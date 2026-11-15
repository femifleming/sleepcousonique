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
 *             "SCQ MODE IFACE" / "SCQ MODE DREAM" -> interface mode (a USB sound card) or dreams
 *             "SCQ UNIT <n>"  -> this SP-1 is unit n of an array (shown on the LEDs, kept)
 *             "SCQ WHO"       -> "SCQ ME <usb serial> <unit> <IFACE|DREAM>"
 *             "SCQ SHOW"      -> flashes the LEDs (which one is this?)
 *             "SCQ WATCH 1"   -> streams the hands ~30x a second until "SCQ WATCH 0" or the
 *                                port closes:  "SCQ HANDS f1 f2 f3 f4 buttons scene slot leds"
 *                                (faders 0-4095, buttons a bitmask, leds 8 hex bytes)
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
#if defined(CONFIG_SP1_SCQ_MIDI)
#include "sp1_midi_usb.h"
#include "usb_rt_parse.h"
#endif
#if defined(CONFIG_SP1_SCQ_IFACE)
#include "sp1_iface.h"
#include "sp1_usbd.h"
#include <zephyr/drivers/hwinfo.h>
#endif

#define SCQ_VERSION   "1.2.0"
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
static uint32_t watch_ms;

struct slot_head  { uint32_t magic, len; };
struct state_page { uint32_t magic, slot, nights, unit; };   /* unit: was pad (0) */
static uint32_t unit;              /* this SP-1's number in an array, 0 = none yet */
static bool     iface_mode;        /* interface mode: the SP-1 is a USB sound card */
static uint32_t show_ms;           /* "SCQ SHOW": flash the LEDs this long */

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
	struct state_page st = { STATE_MAGIC, (uint32_t)slot, nights, unit };
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

static void iface_leave(void);

static int load_text(const char *text, int len)
{
	iface_leave();                 /* the dream takes its pool back from the sound card */
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

#if defined(CONFIG_SP1_SCQ_MIDI)
/* ---- USB-MIDI in: the dream's midi / midicc / midiclock places ----
 * feldd's patched class calls sp1_midi_usb_rx() in the USB INTERRUPT with whole 4-byte
 * USB-MIDI packets. Each valid message goes, as plain MIDI bytes, into a single-producer /
 * single-consumer ring; the audio thread hands them to the engine (scq_midi) under the
 * engine lock, just before it renders. A full ring drops bytes (counted), never blocks. */
#define MIDI_RING 512u                       /* a power of two */
static uint8_t midi_ring[MIDI_RING];
static atomic_t midi_head, midi_tail;        /* head: the interrupt writes; tail: audio */
static uint32_t midi_dropped;

static void midi_put(const uint8_t *b, uint32_t n)
{
	const uint32_t h = (uint32_t)atomic_get(&midi_head), t = (uint32_t)atomic_get(&midi_tail);
	if (MIDI_RING - (h - t) < n) { midi_dropped++; return; }
	for (uint32_t i = 0; i < n; i++) midi_ring[(h + i) & (MIDI_RING - 1u)] = b[i];
	atomic_set(&midi_head, (atomic_val_t)(h + n));
}

void sp1_midi_usb_rx(const uint8_t *data, size_t len)
{
	for (size_t i = 0; i + 4u <= len; i += 4u) {
		uint8_t msg[3];
		const uint8_t n = usb_midi_extract_voice(&data[i], msg);
		if (n != 0u) { midi_put(msg, n); continue; }
		uint8_t rt = usb_midi_extract_rt(&data[i]);
		/* the same four real-time bytes under CIN 0x5, as some hosts send them (Wakes M5b) */
		if (rt == 0u && (data[i] & 0x0Fu) == 0x5u &&
		    (data[i + 1u] == 0xF8u || data[i + 1u] == 0xFAu || data[i + 1u] == 0xFBu || data[i + 1u] == 0xFCu)) {
			rt = data[i + 1u];
		}
		if (rt != 0u) midi_put(&rt, 1u);
	}
}

/* Port down (unplugged, suspended): let go of every held note, as a keyboard pulled out would. */
void sp1_midi_usb_port(bool up)
{
	if (!up) {
		static const uint8_t all_off[] = { 0xB0, 123, 0, 0xFC };
		midi_put(all_off, sizeof(all_off));
	}
}

static void midi_drain(void)                 /* audio thread, engine lock held */
{
	const uint32_t t = (uint32_t)atomic_get(&midi_tail), h = (uint32_t)atomic_get(&midi_head);
	if (h == t) return;
	const uint32_t a = t & (MIDI_RING - 1u), n = h - t;
	const uint32_t first = n < MIDI_RING - a ? n : MIDI_RING - a;
	scq_midi(&midi_ring[a], (int)first);
	if (n > first) scq_midi(&midi_ring[0], (int)(n - first));
	atomic_set(&midi_tail, (atomic_val_t)h);
}
#else
static inline void midi_drain(void) {}
#endif

void sp1_scq_render(float *left, float *right, uint32_t frames)
{
#if defined(CONFIG_SP1_SCQ_IFACE)
	if (sp1_iface_on()) {          /* interface mode: play what the computer sends */
		sp1_iface_render(left, right, frames);
		return;
	}
#endif
	if (k_mutex_lock(&engine_lock, K_NO_WAIT) != 0) {
		memset(left, 0, frames * sizeof(float));
		memset(right, 0, frames * sizeof(float));
		return;
	}
	/* Over budget: a dream that takes longer to render than the block lasts starves the
	 * I2S queue, and the stream errors out and never stays up (heard as near-silence). So
	 * after a block that used more than 90% of its time, this one is silence: the stream
	 * keeps running and the overload is heard as dropouts, not as a dead SP-1. */
	midi_drain();                            /* even in a skipped block: a note-off must land */
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
/* ------------------------------------------------------------------ interface mode */

static void iface_enter(void)
{
#if defined(CONFIG_SP1_SCQ_IFACE)
	if (iface_mode) {
		return;
	}
	k_mutex_lock(&engine_lock, K_FOREVER);
	uint32_t bytes = 0u;
	void *mem = scq_sleep(&bytes);
	k_mutex_unlock(&engine_lock);
	sp1_iface_start(mem, bytes);
	sp1_usbd_threads_above_audio(true);   /* no ISO OUT packet may wait behind audio */
	iface_mode = true;
	printk("SCQ interface mode: a USB sound card%s (unit %u)\n",
	       sp1_iface_streaming() ? ", the computer is sending" : "", (unsigned)unit);
#endif
}

static void iface_leave(void)
{
#if defined(CONFIG_SP1_SCQ_IFACE)
	if (!iface_mode) {
		return;
	}
	iface_mode = false;
	sp1_iface_stop();
	sp1_usbd_threads_above_audio(false);  /* dreams: audio first again */
	printk("SCQ dream mode\n");
#endif
}

static void dream_again(void)
{
	iface_leave();
	play_slot(slot);               /* the engine wakes with the dream that was playing */
}

static void usb_serial(char *out, size_t cap)
{
	out[0] = 0;
#if defined(CONFIG_SP1_SCQ_IFACE) && defined(CONFIG_HWINFO)
	static const char hex[] = "0123456789ABCDEF";
	uint8_t id[CONFIG_USBD_HWINFO_DEVID_LENGTH];
	ssize_t n = hwinfo_get_device_id(id, sizeof id);   /* the same string USB reports */
	size_t k = 0;
	for (ssize_t i = 0; i < n && k + 2 < cap; i++) {
		out[k++] = hex[id[i] >> 4];
		out[k++] = hex[id[i] & 0xF];
	}
	out[k] = 0;
#else
	ARG_UNUSED(cap);
#endif
}

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
	} else if (strncmp(line, "SCQ MODE ", 9) == 0) {
		if (line[9] == 'I') {
			iface_enter();
		} else {
			dream_again();
		}
		reply(iface_mode ? "OK IFACE\n" : "OK DREAM\n");
	} else if (strncmp(line, "SCQ UNIT ", 9) == 0) {
		long u = strtol(line + 9, NULL, 10);
		unit = (u >= 0 && u <= 16) ? (uint32_t)u : 0u;
		show_ms = 1500u;
		state_save();
		reply("OK 0\n");
	} else if (strncmp(line, "SCQ SHOW", 8) == 0) {
		show_ms = 3000u;
		reply("OK 0\n");
	} else if (strncmp(line, "SCQ WHO", 7) == 0) {
		char sn[40], out[80];
		usb_serial(sn, sizeof sn);
		snprintf(out, sizeof out, "SCQ ME %s %u %s\n", sn[0] ? sn : "-", (unsigned)unit,
			 iface_mode ? "IFACE" : "DREAM");
		reply(out);
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
			int e = load_text(staging, got);
			if (e == 0) {
				/* the playing dream is the new one; keep it */
				slot = want_slot;
				audio_pause();                       /* one pause for both writes */
				int wr = slot_write(slot, staging, got);
				if (wr == 0) {
					state_write();
				}
				audio_resume();
				if (wr != 0) {
					reply("ERR could not save (playing it anyway)\n");
				} else {
					char ok[24];
					snprintf(ok, sizeof ok, "OK %d\n", e);
					reply(ok);
				}
				printk("SCQ received %d bytes into slot %d\n", got, slot + 1);
			} else {
				char err[40];
				snprintf(err, sizeof err, "ERR %d mistakes\n", e);
				reply(err);
				play_slot(slot);      /* back to what was there */
			}
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

/* ------------------------------------------------------------------ hands and lamp */

void sp1_scq_init(void)
{
	const struct flash_area *fa = area();
	struct state_page st;
	if (fa && flash_area_read(fa, STATE_OFF, &st, sizeof st) == 0 && st.magic == STATE_MAGIC) {
		slot = (int)(st.slot % SLOTS);
		nights = st.nights + 1;
		unit = st.unit <= 16u ? st.unit : 0u;
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
}

void sp1_scq_tick(uint32_t elapsed_ms, bool fnc)
{
	audio_live = true;             /* the ON loop only runs with the audio started */
	serial_poll();

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
#if defined(CONFIG_SP1_SCQ_IFACE)
		if (iface_mode) {
			struct sp1_iface_stats is;
			sp1_iface_stats(&is);
			printk("SCQ iface unit=%u %s packets=%u fill=%u under=%u over=%u follow=%dppm\n",
			       (unsigned)unit, is.streaming ? "streaming" : "idle", (unsigned)is.packets,
			       (unsigned)is.fill, (unsigned)is.under, (unsigned)is.over, (int)is.ratio_ppm);
		}
#endif
		printk("SCQ engine avg=%u%% max=%u%%  places=%d scene=%d slot=%d%s\n",
		       n ? (unsigned)((uint64_t)sum * 100u / ((uint64_t)n * budget)) : 0u,
		       (unsigned)((uint64_t)mx * 100u / budget),
		       scq_node_count(), scq_scene() + 1, slot + 1,
		       skipped ? "  OVER BUDGET: blocks dropped" : "");
		if (skipped) {
			printk("SCQ over budget: %u blocks played as silence in 5 s; the dream is too heavy\n", (unsigned)skipped);
		}
	}

	if (sp1_faders_valid()) {
		for (int i = 0; i < 4; i++) {
			scq_control(SCQ_CTL_LEAN1 + i, (float)sp1_fader_raw(i) * (1.f / 4095.f));
		}
	}
	for (int i = 0; i < 4; i++) {
		scq_control(SCQ_CTL_DOOR1 + i, sp1_button_held((enum sp1_button)(SP1_BTN_T1 + i)) ? 1.f : 0.f);
	}
	/* •• + the rocker: the next / previous scene. While •• is held the rocker belongs to the
	 * scenes and never reaches `lift`, so lift stays free to patch. */
	static bool rocker_taken;
	if (fnc && !iface_mode && (sp1_button_pressed(SP1_BTN_FFWD) || sp1_button_pressed(SP1_BTN_RWD))) {
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
	scq_control(SCQ_CTL_WAKE, sp1_button_held(SP1_BTN_PLAY) ? 1.f : 0.f);
	scq_control(SCQ_CTL_LUCID, fnc ? 1.f : 0.f);

	/* •• + PLAY held 1 s: interface mode (a USB sound card) <-> dream mode. A hold, so a quick
	 * •• + PLAY still reaches the dream as `dots` and `play`. */
	{
		static uint32_t mode_ms;
		static bool mode_done;
		if (fnc && sp1_button_held(SP1_BTN_PLAY)) {
			(void)sp1_shift_used();         /* touching PLAY: never the power-off */
			mode_ms += elapsed_ms;
			if (mode_ms >= 1000u && !mode_done) {
				mode_done = true;
				if (iface_mode) {
					dream_again();
				} else {
					iface_enter();
				}
				show_ms = 1200u;
			}
		} else {
			mode_ms = 0u;
			mode_done = false;
		}
	}
	if (show_ms) {
		show_ms = show_ms > elapsed_ms ? show_ms - elapsed_ms : 0u;
	}

	/* •• + VOL: another dream slot (VOL alone stays the volume, in main.c) */
	if (fnc && (sp1_button_pressed(SP1_BTN_VOL_UP) || sp1_button_pressed(SP1_BTN_VOL_DOWN))) {
		(void)sp1_shift_used();
		play_slot(slot + (sp1_button_pressed(SP1_BTN_VOL_UP) ? 1 : -1));
		state_save();
	}

	/* a journey happened in the audio thread: compile the next scene here (never while the
	 * engine is asleep: its pool is the sound card's ring) */
	if (iface_mode) {
		return;
	}
	k_mutex_lock(&engine_lock, K_FOREVER);
	scq_service();
	k_mutex_unlock(&engine_lock);
}

void sp1_scq_track_leds(uint8_t out[4])
{
	if (show_ms) {                 /* "which one is this?": all four blink */
		const uint8_t v = ((show_ms / 150u) & 1u) ? 255u : 0u;
		out[0] = out[1] = out[2] = out[3] = v;
		return;
	}
	if (iface_mode) {              /* the unit number, in binary (1-15), T1 the lowest bit */
		const uint32_t u = unit;
		for (int i = 0; i < 4; i++) {
			out[i] = (u >> i) & 1u ? 200u : (u == 0u ? 24u : 0u);
		}
		return;
	}
	for (int i = 0; i < 4; i++) {
		out[i] = (uint8_t)(scq_lamp(i) * 255.f);
	}
}

void sp1_scq_play_leds(uint32_t elapsed_ms)
{
	static uint32_t idle_ms, breath_ms;
	sp1_meter_tick();              /* keeps the console's meter= honest */
#if defined(CONFIG_SP1_SCQ_IFACE)
	if (iface_mode) {              /* the level: left on the two LEDs nearest ••, right on the two by PLAY */
		const float l = sp1_iface_level(0), r = sp1_iface_level(1);
		const bool live = sp1_iface_streaming();
		sp1_led_set(SP1_ROW_PLAY, 0, (uint8_t)(l > 0.5f ? 255 : (live ? 20 : 0)));
		sp1_led_set(SP1_ROW_PLAY, 1, (uint8_t)(l * 255.f));
		sp1_led_set(SP1_ROW_PLAY, 2, (uint8_t)(r * 255.f));
		sp1_led_set(SP1_ROW_PLAY, 3, (uint8_t)(r > 0.5f ? 255 : (live ? 20 : 0)));
		return;
	}
#endif
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
