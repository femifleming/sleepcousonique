/*
 * Sleepcousonique on the SP-1. See sp1_scq.h.
 *
 * Controls:   F1-F4 lean1-lean4        T1-T4 door1-door4
 *             FFWD / RWD lift +1 / -1  PLAY wake          •• lucid
 *             VOL +/-  volume          •• + VOL +/-  next / previous dream slot
 * USB serial: "SCQ HELLO"  -> "SLEEPCOUSONIQUE <version>"
 *             "SCQ LOAD <bytes> <slot>" + bytes  -> "OK <errors>" / "ERR <why>"
 *             "SCQ SLOT <n>"  -> plays slot n
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
#include "scq.h"
#include "scq_default.h"     /* the first night, generated from examples/first-night.dream */

#define SCQ_VERSION   "0.1.0"
#define SLOTS         5
#define SLOT_BYTES    (16 * 1024)
#define STATE_OFF     (SLOTS * SLOT_BYTES)       /* 0x14000: one page of state */
#define SLOT_MAGIC    0x31514353u                /* "SCQ1" */
#define STATE_MAGIC   0x54514353u                /* "SCQT" */

static const struct device *cdc = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
K_MUTEX_DEFINE(engine_lock);

RING_BUF_DECLARE(rx_ring, 1024);
static char     staging[SCQ_MAX_TEXT];
static int      slot;              /* the slot that is playing */
static uint32_t nights;            /* boots so far: drift's seed */
static bool     link_up;

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

static void state_save(void)
{
	const struct flash_area *fa = area();
	struct state_page st = { STATE_MAGIC, (uint32_t)slot, nights, 0 };
	if (!fa) {
		return;
	}
	flash_area_erase(fa, STATE_OFF, 4096);
	flash_area_write(fa, STATE_OFF, &st, sizeof st);
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
	scq_render(left, right, (int)frames);
	k_mutex_unlock(&engine_lock);
}

/* ------------------------------------------------------------------ USB serial */

static void rx_isr(const struct device *dev, void *user)
{
	ARG_UNUSED(user);
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
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
				if (slot_write(slot, staging, got) != 0) {
					reply("ERR could not save (playing it anyway)\n");
				} else {
					state_save();
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
	ARG_UNUSED(elapsed_ms);
	serial_poll();

	if (sp1_faders_valid()) {
		for (int i = 0; i < 4; i++) {
			scq_control(SCQ_CTL_LEAN1 + i, (float)sp1_fader_raw(i) * (1.f / 4095.f));
		}
	}
	for (int i = 0; i < 4; i++) {
		scq_control(SCQ_CTL_DOOR1 + i, sp1_button_held((enum sp1_button)(SP1_BTN_T1 + i)) ? 1.f : 0.f);
	}
	scq_control(SCQ_CTL_LIFT, sp1_button_held(SP1_BTN_FFWD) ? 1.f : (sp1_button_held(SP1_BTN_RWD) ? -1.f : 0.f));
	scq_control(SCQ_CTL_WAKE, sp1_button_held(SP1_BTN_PLAY) ? 1.f : 0.f);
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

void sp1_scq_play_leds(void)
{
	for (int i = 0; i < 4; i++) {
		sp1_led_set(SP1_ROW_PLAY, i, (uint8_t)(scq_lamp(4 + i) * 255.f));
	}
}
