/*
 * Sleepcousonique on the SP-1. See sp1_scq.h.
 *
 * Controls:   F1-F4 fader1-fader4      T1-T4 button1-button4
 *             FFWD / RWD rocker +1 / -1  PLAY play        •• dots
 *             VOL +/-  volume          •• + VOL +/-  next / previous dream slot
 *             •• + FFWD / RWD  next / previous scene (the rocker never reaches the dream then)
 * USB serial: "SCQ HELLO"  -> "SLEEPCOUSONIQUE <version>"
 *             "SCQ LOAD <bytes> <slot>" + bytes  -> "OK <errors>" / "ERR <why>"
 *             "SCQ SLOT <n>"  -> plays slot n
 *             "SCQ WATCH 1"   -> streams the hands ~30x a second until "SCQ WATCH 0" or the
 *                                port closes:  "SCQ HANDS f1 f2 f3 f4 buttons scene slot leds"
 *                                (faders 0-4095, buttons a bitmask, leds 8 hex bytes)
 * Bluetooth (CONFIG_SP1_BLE): BLE-MIDI SysEx, F0 7D 'S' 'Q' <cmd> ... F7 (sp1_ble.c):
 *             'H'                         -> 'h' "SLEEPCOUSONIQUE <version> <name>"
 *             'B' len(3x7 bits) slot      start a dream (slot 0-4 saves it; 127 plays it, unsaved)
 *             'D' nibbles...              its bytes, high nibble then low
 *             'E'                         -> 'o' "<errors>" / 'x' "<why>"
 *             'S' kind a(2x7) b v(5x7)    live: kind a = cord amount (cord a), t = tune (node a),
 *                                         v = attenuverter (node a, inlet b), p = param (node a,
 *                                         option b), c = control b (the editor's hands)
 *             'G' scene(2x7)              go to a scene
 *             'P'                         -> 'p' (are you there)
 *             •• + T4 turns Bluetooth on and off (kept in the state page).
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sp1_scq.h"
#include "sp1_controls.h"
#include "sp1_led.h"
#include "sp1_power.h"
#include "sp1_batt.h"
#include "sp1_ui_timing.h"
#include "sp1_display.h"
#include "sp1_audio.h"
#include "sp1_meter.h"
#include <cmsis_core.h>
#include "scq.h"
#include "scq_default.h"     /* the first night, generated from examples/first-night.dream */
#if defined(CONFIG_SP1_BLE)
#include "sp1_ble.h"
#endif

#define SCQ_VERSION   "1.0.0"
#define SLOTS         5
#define SLOT_BYTES    (16 * 1024)
#define STATE_OFF     (SLOTS * SLOT_BYTES)       /* 0x14000: one page of state */
#define SLOT_MAGIC    0x31514353u                /* "SCQ1" */
#define STATE_MAGIC   0x54514353u                /* "SCQT" */

static const struct device *cdc = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
K_MUTEX_DEFINE(engine_lock);

RING_BUF_DECLARE(rx_ring, 4096);
static char     staging[SCQ_MAX_TEXT];
static int      slot;              /* the slot that is playing */
static uint32_t nights;            /* boots so far: drift's seed */
static bool     link_up;
static bool     audio_live;        /* ON and the audio thread running: flash writes must pause it */
static volatile uint32_t eng_sum, eng_n, eng_max, eng_skipped;   /* skipped: over budget, played as silence */   /* engine cycles, from the audio thread */
static uint32_t report_ms;
static bool     watching;          /* the editor wants the hands, live */
static bool     ble_wanted = true; /* Bluetooth: on unless turned off with •• + T4 */
static float    remote_ctl[SCQ_CTL_COUNT];   /* the editor's hands, over Bluetooth */
static bool     remote_on[SCQ_CTL_COUNT];
static uint32_t watch_ms;

struct slot_head  { uint32_t magic, len; };
struct state_page { uint32_t magic, slot, nights, pad; };

/* ------------------------------------------------------------------ flash */

static const struct flash_area *area(void)
{
	static const struct flash_area *fa;
	if (!fa && flash_area_open(FIXED_PARTITION_ID(scq_partition), &fa) != 0) {
		fa = NULL;
	}
	return fa;
}

static int slot_read(int s, char *buf, int cap)
{
	const struct flash_area *fa = area();
	struct slot_head h;
	if (!fa || flash_area_read(fa, (off_t)s * SLOT_BYTES, &h, sizeof h) != 0) {
		return -1;
	}
	if (h.magic != SLOT_MAGIC || h.len == 0 || h.len > (uint32_t)cap ||
	    h.len > SLOT_BYTES - sizeof h) {
		return -1;
	}
	if (flash_area_read(fa, (off_t)s * SLOT_BYTES + sizeof h, buf, h.len) != 0) {
		return -1;
	}
	return (int)h.len;
}

/* ⚠️ Erasing a page stops the CPU for ~85 ms, audio thread included: the stream
 * underruns and sp1_audio recovers. Only ever done on purpose (a dream arriving). */
static int slot_write(int s, const char *text, int len)
{
	const struct flash_area *fa = area();
	if (!fa || len <= 0 || len > SLOT_BYTES - (int)sizeof(struct slot_head)) {
		return -1;
	}
	off_t off = (off_t)s * SLOT_BYTES;
	if (flash_area_erase(fa, off, SLOT_BYTES) != 0) {
		return -1;
	}
	struct slot_head h = { SLOT_MAGIC, (uint32_t)len };
	if (flash_area_write(fa, off, &h, sizeof h) != 0) {
		return -1;
	}
	/* writes must be word multiples: pad the tail */
	int body = len & ~3;
	if (body && flash_area_write(fa, off + sizeof h, text, body) != 0) {
		return -1;
	}
	if (len & 3) {
		uint8_t tail[4] = { 0xff, 0xff, 0xff, 0xff };
		memcpy(tail, text + body, (size_t)(len & 3));
		if (flash_area_write(fa, off + sizeof h + body, tail, 4) != 0) {
			return -1;
		}
	}
	return 0;
}

/* ⚠️ A flash erase stops the CPU for ~85 ms per page. With the audio running that
 * underruns the I2S stream, and from there it may never recover (seen on hardware:
 * silence after the first dream arrived). So every write pauses the audio and brings it
 * back up exactly as entering ON does. */
static void audio_pause(void)
{
	if (audio_live) {
		sp1_audio_stop();
	}
}

static void audio_resume(void)
{
	if (audio_live) {
		(void)sp1_audio_start();
	}
}

static void state_write(void)
{
	const struct flash_area *fa = area();
	struct state_page st = { STATE_MAGIC, (uint32_t)slot, nights, ble_wanted ? 0u : 1u };
	if (!fa) {
		return;
	}
	flash_area_erase(fa, STATE_OFF, 4096);
	flash_area_write(fa, STATE_OFF, &st, sizeof st);
}

static void state_save(void)
{
	audio_pause();
	state_write();
	audio_resume();
}

/* ------------------------------------------------------------------ the engine */

static int load_text(const char *text, int len)
{
	k_mutex_lock(&engine_lock, K_FOREVER);
	int errors = scq_load(text, len, nights);
	k_mutex_unlock(&engine_lock);
	return errors;
}

static void play_slot(int s)
{
	slot = (s % SLOTS + SLOTS) % SLOTS;
	int len = slot_read(slot, staging, sizeof staging);
	if (len > 0) {
		int e = load_text(staging, len);
		printk("SCQ slot %d: %d bytes, %d mistakes\n", slot + 1, len, e);
	} else {
		load_text(SCQ_DEFAULT_DREAM, (int)strlen(SCQ_DEFAULT_DREAM));
		printk("SCQ slot %d is empty: playing the first night\n", slot + 1);
	}
}

void sp1_scq_render(float *left, float *right, uint32_t frames)
{
	if (k_mutex_lock(&engine_lock, K_NO_WAIT) != 0) {
		memset(left, 0, frames * sizeof(float));
		memset(right, 0, frames * sizeof(float));
		return;
	}
	/* Over budget: a dream that takes longer to render than the block lasts starves the
	 * I2S queue, and the stream errors out and never stays up (heard as near-silence). So
	 * after a block that used more than 90% of its time, this one is silence: the stream
	 * keeps running and the overload is heard as dropouts, not as a dead SP-1. */
	static uint32_t last_c;
	const uint32_t budget = 1333u * frames;
	if (last_c > budget / 10u * 9u) {
		last_c = 0u;
		k_mutex_unlock(&engine_lock);
		memset(left, 0, frames * sizeof(float));
		memset(right, 0, frames * sizeof(float));
		eng_skipped++;
		return;
	}
	const uint32_t c0 = DWT->CYCCNT;
	scq_render(left, right, (int)frames);
	const uint32_t c = DWT->CYCCNT - c0;
	last_c = c;
	k_mutex_unlock(&engine_lock);
	eng_sum += c;
	eng_n++;
	if (c > eng_max) {
		eng_max = c;
	}
}

/* ------------------------------------------------------------------ USB serial */

/* Never drop a byte: when our ring is nearly full, stop taking data and let USB hold the
 * rest back (the host just waits); serial_poll() turns receiving back on once it has room.
 * Reading less than is there, with receiving still on, would make the CDC driver re-run
 * this callback in a loop on the system work queue, above the audio. */
static volatile bool rx_paused;
static void rx_isr(const struct device *dev, void *user)
{
	ARG_UNUSED(user);
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			break;
		}
		uint32_t room = ring_buf_space_get(&rx_ring);
		if (room < 64u) {
			uart_irq_rx_disable(dev);
			rx_paused = true;
			break;
		}
		uint8_t buf[64];
		int n = uart_fifo_read(dev, buf, sizeof buf);
		if (n > 0) {
			ring_buf_put(&rx_ring, buf, (uint32_t)n);
		}
	}
}

static void reply(const char *s)
{
	if (!link_up) {
		return;
	}
	while (*s) {
		uart_poll_out(cdc, (unsigned char)*s++);
	}
}

/* A dream in staging[0..len): play it, and keep it in `target` (0-4), or with target 127
 * just play it (live from the editor: no flash write, so no gap in the sound). */
static int accept_dream(int len, int target, char *msg, int cap)
{
	int e = load_text(staging, len);
	if (e != 0) {
		snprintf(msg, (size_t)cap, "ERR %d mistakes\n", e);
		play_slot(slot);              /* back to what was there */
		return -1;
	}
	if (target == 127) {
		snprintf(msg, (size_t)cap, "OK 0\n");
		return 0;
	}
	slot = target;
	audio_pause();                    /* one pause for both writes */
	int wr = slot_write(slot, staging, len);
	if (wr == 0) {
		state_write();
	}
	audio_resume();
	printk("SCQ received %d bytes into slot %d\n", len, slot + 1);
	if (wr != 0) {
		snprintf(msg, (size_t)cap, "ERR could not save (playing it anyway)\n");
		return -1;
	}
	snprintf(msg, (size_t)cap, "OK %d\n", e);
	return 0;
}

/* line-or-bytes parser */
static char  line[96];
static int   line_n;
static int   want = -1, got, want_slot;

static void handle_line(void)
{
	line[line_n] = 0;
	if (strncmp(line, "SCQ HELLO", 9) == 0) {
		reply("SLEEPCOUSONIQUE " SCQ_VERSION "\n");
	} else if (strncmp(line, "SCQ LOAD ", 9) == 0) {
		char *end;
		long n = strtol(line + 9, &end, 10);
		long s = strtol(end, NULL, 10);
		if (n <= 0 || n >= (long)sizeof staging) {
			reply("ERR too big\n");
		} else {
			want = (int)n;
			got = 0;
			want_slot = (s >= 0 && s < SLOTS) ? (int)s : slot;
		}
	} else if (strncmp(line, "SCQ WATCH ", 10) == 0) {
		watching = line[10] == '1';
		watch_ms = 0u;
	} else if (strncmp(line, "SCQ SLOT ", 9) == 0) {
		play_slot(atoi(line + 9) - 1);
		state_save();
		reply("OK 0\n");
	}
	line_n = 0;
}

static void serial_poll(void)
{
	uint8_t c;
	if (rx_paused && ring_buf_space_get(&rx_ring) > 1024u) {
		rx_paused = false;
		uart_irq_rx_enable(cdc);
	}
	while (ring_buf_get(&rx_ring, &c, 1) == 1) {
		if (want >= 0) {
			staging[got++] = (char)c;
			if (got < want) {
				continue;
			}
			want = -1;
			char msg[48];
			accept_dream(got, want_slot, msg, sizeof msg);
			reply(msg);
			continue;
		}
		if (c == '\n' || c == '\r') {
			if (line_n) {
				handle_line();
			}
		} else if (line_n < (int)sizeof line - 1) {
			line[line_n++] = (char)c;
		}
	}
}

/* ------------------------------------------------------------------ Bluetooth */

#if defined(CONFIG_SP1_BLE)
static int  ble_want = -1, ble_got, ble_slot;

static void ble_reply(char kind, const char *text)
{
	uint8_t out[96] = { 0x7D, 'S', 'Q', (uint8_t)kind };
	int n = 4;
	while (*text && n < (int)sizeof out) {
		out[n++] = (uint8_t)(*text++ & 0x7f);
	}
	sp1_ble_send(out, n);
}

static float unpack_float(const uint8_t *b)
{
	uint32_t u = (uint32_t)b[0] | ((uint32_t)b[1] << 7) | ((uint32_t)b[2] << 14) | ((uint32_t)b[3] << 21) | ((uint32_t)b[4] << 28);
	float f;
	memcpy(&f, &u, 4);
	return f;
}

static void ble_poll(void)
{
	static uint8_t m[SP1_BLE_MAX_MSG];
	int n;
	while ((n = sp1_ble_take(m, sizeof m)) > 0) {
		if (n < 4 || m[0] != 0x7D || m[1] != 'S' || m[2] != 'Q') {
			continue;
		}
		const uint8_t *p = m + 4;
		const int len = n - 4;
		switch (m[3]) {
		case 'H': {
			char t[64];
			snprintf(t, sizeof t, "SLEEPCOUSONIQUE " SCQ_VERSION " %s", sp1_ble_name());
			ble_reply('h', t);
		} break;
		case 'P': ble_reply('p', ""); break;
		case 'B':
			if (len >= 4) {
				int total = p[0] | (p[1] << 7) | (p[2] << 14);
				ble_want = total > 0 && total < (int)sizeof staging ? total : -1;
				ble_got = 0;
				ble_slot = p[3] == 127 ? 127 : (p[3] < SLOTS ? p[3] : slot);
				if (ble_want < 0) {
					ble_reply('x', "too big");
				}
			}
			break;
		case 'D':
			for (int i = 0; ble_want >= 0 && i + 1 < len && ble_got < ble_want; i += 2) {
				staging[ble_got++] = (char)(((p[i] & 0x0f) << 4) | (p[i + 1] & 0x0f));
			}
			break;
		case 'E':
			if (ble_want >= 0 && ble_got == ble_want) {
				char msg[48];
				int r = accept_dream(ble_got, ble_slot, msg, sizeof msg);
				char *nl = strchr(msg, '\n');
				if (nl) { *nl = 0; }
				ble_reply(r == 0 ? 'o' : 'x', msg);
			} else {
				ble_reply('x', "the dream arrived incomplete");
			}
			ble_want = -1;
			break;
		case 'S':
			if (len >= 9) {
				const int a = p[1] | (p[2] << 7), b = p[3];
				const float v = unpack_float(p + 4);
				k_mutex_lock(&engine_lock, K_FOREVER);
				switch (p[0]) {
				case 'a': scq_set_amount(a, v); break;
				case 't': scq_set_tune(a, v); break;
				case 'v': scq_set_att(a, b, v); break;
				case 'p': scq_set_param(a, b, v); break;
				case 'c':
					if (b < SCQ_CTL_COUNT) {
						remote_ctl[b] = v;
						remote_on[b] = true;
					}
					break;
				default: break;
				}
				k_mutex_unlock(&engine_lock);
			}
			break;
		case 'G':
			if (len >= 2) {
				k_mutex_lock(&engine_lock, K_FOREVER);
				scq_goto_scene(p[0] | (p[1] << 7));
				k_mutex_unlock(&engine_lock);
			}
			break;
		default: break;
		}
	}
}
#endif

/* ------------------------------------------------------------------ hands and lamp */

void sp1_scq_init(void)
{
	const struct flash_area *fa = area();
	struct state_page st;
	if (fa && flash_area_read(fa, STATE_OFF, &st, sizeof st) == 0 && st.magic == STATE_MAGIC) {
		slot = (int)(st.slot % SLOTS);
		nights = st.nights + 1;
		ble_wanted = (st.pad & 1u) == 0u;
	}
	if (device_is_ready(cdc)) {
		uart_irq_callback_set(cdc, rx_isr);
		uart_irq_rx_enable(cdc);
		link_up = true;
	}
	for (int i = 0; i < SCQ_CTL_COUNT; i++) {
		scq_control(i, 0.f);
	}
	play_slot(slot);
	state_save();             /* one more night */
#if defined(CONFIG_SP1_BLE)
	sp1_ble_set(ble_wanted);
#endif
}

void sp1_scq_tick(uint32_t elapsed_ms, bool fnc)
{
	audio_live = true;             /* the ON loop only runs with the audio started */
	serial_poll();
#if defined(CONFIG_SP1_BLE)
	ble_poll();
	/* •• + T4: Bluetooth on / off; the track row says which (all four lit: on) */
	if (fnc && sp1_button_pressed(SP1_BTN_T4)) {
		(void)sp1_shift_used();
		ble_wanted = !ble_wanted;
		sp1_ble_set(ble_wanted);
		const uint8_t lv[4] = { ble_wanted ? 255 : 0, ble_wanted ? 255 : 0, ble_wanted ? 255 : 0, 255 };
		sp1_display_transient(lv, 900);
		state_save();
	}
#endif

	/* the editor is watching: send the hands, about 30 times a second. Stops when the port
	 * closes (DTR low), so nothing is written to a computer that isn't listening. */
	if (watching) {
		uint32_t dtr = 1u;
		(void)uart_line_ctrl_get(cdc, UART_LINE_CTRL_DTR, &dtr);
		if (!dtr) {
			watching = false;
		} else if ((watch_ms += elapsed_ms) >= 32u) {
			watch_ms = 0u;
			static const enum sp1_button bits[] = {
				SP1_BTN_T1, SP1_BTN_T2, SP1_BTN_T3, SP1_BTN_T4, SP1_BTN_PLAY,
				SP1_BTN_COUNT /* fnc */, SP1_BTN_FFWD, SP1_BTN_RWD, SP1_BTN_VOL_UP, SP1_BTN_VOL_DOWN,
			};
			uint32_t mask = 0u;
			for (int b = 0; b < (int)ARRAY_SIZE(bits); b++) {
				bool held = bits[b] == SP1_BTN_COUNT ? fnc : sp1_button_held(bits[b]);
				if (held) {
					mask |= 1u << b;
				}
			}
			char msg[96];
			int n = snprintf(msg, sizeof msg, "SCQ HANDS %u %u %u %u %x %d %d ",
					 sp1_fader_raw(0), sp1_fader_raw(1), sp1_fader_raw(2), sp1_fader_raw(3),
					 (unsigned)mask, scq_scene() + 1, slot + 1);
			for (int i = 0; i < 8 && n < (int)sizeof msg - 4; i++) {
				n += snprintf(msg + n, sizeof msg - (size_t)n, "%02x", (unsigned)(scq_lamp(i) * 255.f));
			}
			snprintf(msg + n, sizeof msg - (size_t)n, "\n");
			reply(msg);
		}
	}

	/* the audio watchdog: blocks failing and none arriving for a second -> restart the
	 * whole audio path, exactly as entering ON does */
	{
		static uint32_t last_blocks, last_fails, stuck_ms;
		struct sp1_audio_stats as;
		sp1_audio_stats(&as);
		if (as.blocks == last_blocks && as.write_fails != last_fails) {
			stuck_ms += elapsed_ms;
		} else {
			stuck_ms = 0u;
		}
		last_blocks = as.blocks;
		last_fails = as.write_fails;
		if (stuck_ms > 1000u) {
			stuck_ms = 0u;
			printk("SCQ audio stuck: restarting it\n");
			audio_pause();
			audio_resume();
		}
	}

	if ((report_ms += elapsed_ms) >= 5000u) {
		report_ms = 0u;
		const uint32_t n = eng_n, sum = eng_sum, mx = eng_max;
		eng_n = 0u; eng_sum = 0u; eng_max = 0u;
		/* the audio block is SP1 audio frames at 64 MHz: 1333 cycles per frame */
		const uint32_t budget = 1333u * (uint32_t)CONFIG_SP1_AUDIO_BLOCK_FRAMES;
		const uint32_t skipped = eng_skipped;
		eng_skipped = 0u;
		printk("SCQ engine avg=%u%% max=%u%%  places=%d scene=%d slot=%d%s\n",
		       n ? (unsigned)((uint64_t)sum * 100u / ((uint64_t)n * budget)) : 0u,
		       (unsigned)((uint64_t)mx * 100u / budget),
		       scq_node_count(), scq_scene() + 1, slot + 1,
		       skipped ? "  OVER BUDGET: blocks dropped" : "");
		if (skipped) {
			printk("SCQ over budget: %u blocks played as silence in 5 s; the dream is too heavy\n", (unsigned)skipped);
		}
	}

	/* the faders: the SP-1's own, unless the editor sent one over Bluetooth; moving the
	 * hardware fader takes it back */
	if (sp1_faders_valid()) {
		static int last_raw[4] = { -1, -1, -1, -1 };
		for (int i = 0; i < 4; i++) {
			const int raw = (int)sp1_fader_raw(i);
			if (last_raw[i] >= 0 && abs(raw - last_raw[i]) > 60) {
				remote_on[SCQ_CTL_LEAN1 + i] = false;
			}
			last_raw[i] = raw;
			scq_control(SCQ_CTL_LEAN1 + i, remote_on[SCQ_CTL_LEAN1 + i] ? remote_ctl[SCQ_CTL_LEAN1 + i]
										      : (float)raw * (1.f / 4095.f));
		}
	}
	for (int i = 0; i < 4; i++) {
		const bool held = sp1_button_held((enum sp1_button)(SP1_BTN_T1 + i));
		const float remote = remote_on[SCQ_CTL_DOOR1 + i] ? remote_ctl[SCQ_CTL_DOOR1 + i] : 0.f;
		/* T4 with •• is the Bluetooth switch, not button4 */
		scq_control(SCQ_CTL_DOOR1 + i, (fnc && i == 3) ? 0.f : (held ? 1.f : remote));
	}
	/* •• + the rocker: the next / previous scene. While •• is held the rocker belongs to the
	 * scenes and never reaches `lift`, so lift stays free to patch. */
	static bool rocker_taken;
	if (fnc && (sp1_button_pressed(SP1_BTN_FFWD) || sp1_button_pressed(SP1_BTN_RWD))) {
		(void)sp1_shift_used();
		rocker_taken = true;
		k_mutex_lock(&engine_lock, K_FOREVER);
		int to = scq_scene() + (sp1_button_pressed(SP1_BTN_FFWD) ? 1 : -1);
		scq_goto_scene(to);
		k_mutex_unlock(&engine_lock);
		/* show where you are: a bar of (scene % 4) + 1 on the track row */
		const int s = ((to % 4) + 4) % 4;
		const uint8_t lv[4] = { 255, s >= 1 ? 255 : 0, s >= 2 ? 255 : 0, s >= 3 ? 255 : 0 };
		sp1_display_transient(lv, 600);
		printk("SCQ scene %d\n", to);
	}
	if (rocker_taken && !sp1_button_held(SP1_BTN_FFWD) && !sp1_button_held(SP1_BTN_RWD)) {
		rocker_taken = false;
	}
	scq_control(SCQ_CTL_LIFT, rocker_taken ? 0.f :
		    (sp1_button_held(SP1_BTN_FFWD) ? 1.f : (sp1_button_held(SP1_BTN_RWD) ? -1.f : 0.f)));
	scq_control(SCQ_CTL_WAKE, sp1_button_held(SP1_BTN_PLAY) ? 1.f
				  : (remote_on[SCQ_CTL_WAKE] ? remote_ctl[SCQ_CTL_WAKE] : 0.f));
	scq_control(SCQ_CTL_LUCID, fnc ? 1.f : 0.f);

	/* •• + VOL: another dream slot (VOL alone stays the volume, in main.c) */
	if (fnc && (sp1_button_pressed(SP1_BTN_VOL_UP) || sp1_button_pressed(SP1_BTN_VOL_DOWN))) {
		(void)sp1_shift_used();
		play_slot(slot + (sp1_button_pressed(SP1_BTN_VOL_UP) ? 1 : -1));
		state_save();
	}

	/* a journey happened in the audio thread: compile the next scene here */
	k_mutex_lock(&engine_lock, K_FOREVER);
	scq_service();
	k_mutex_unlock(&engine_lock);
}

void sp1_scq_track_leds(uint8_t out[4])
{
	for (int i = 0; i < 4; i++) {
		out[i] = (uint8_t)(scq_lamp(i) * 255.f);
	}
}

void sp1_scq_play_leds(uint32_t elapsed_ms)
{
	static uint32_t idle_ms, breath_ms;
	sp1_meter_tick();              /* keeps the console's meter= honest */
	bool lit = false;
	for (int i = 0; i < 4; i++) {
		if (scq_lamp(4 + i) > 0.02f) {
			lit = true;
		}
	}
	idle_ms = lit ? 0u : (idle_ms < 60000u ? idle_ms + elapsed_ms : idle_ms);

	/* plugged in and the dream leaves the play row dark: the charge bar, as in STANDBY */
	if (sp1_usb_present() && idle_ms > 1000u) {
		uint32_t b = SP1_STANDBY_BAR_LEVEL;
		if (sp1_charging()) {
			breath_ms = (breath_ms + elapsed_ms) % SP1_STANDBY_BREATH_MS;
			const uint32_t half = SP1_STANDBY_BREATH_MS / 2u;
			const uint32_t tri = breath_ms < half ? breath_ms : SP1_STANDBY_BREATH_MS - breath_ms;
			const uint32_t depth_max = (SP1_STANDBY_BREATH_DEPTH * SP1_STANDBY_BAR_LEVEL) / 255u;
			b = SP1_STANDBY_BAR_LEVEL - (depth_max - (depth_max * tri) / half);
		}
		sp1_led_bar(SP1_ROW_PLAY, sp1_batt_level(), (uint8_t)b);
		return;
	}
	for (int i = 0; i < 4; i++) {
		sp1_led_set(SP1_ROW_PLAY, i, (uint8_t)(scq_lamp(4 + i) * 255.f));
	}
}
