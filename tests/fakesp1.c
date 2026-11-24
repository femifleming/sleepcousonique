/* A pretend SP-1: see fakesp1.h. The parser below follows firmware/app/src/sp1_scq.c line by
 * line (handle_line, serial_poll); keep the two in step when the protocol changes. */
#include "fakesp1.h"
#include "scq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOTS(f)    ((f)->wingie ? FAKE_SLOTS : 5)
#define SLOT_BYTES  (16 * 1024)
#define SLOT_HEAD   8

static void say(struct fake_sp1 *f, const char *s)
{
	int n = (int)strlen(s);
	if (f->out_n + n > FAKE_OUT_CAP) {
		n = FAKE_OUT_CAP - f->out_n;   /* a host that never reads: the rest is lost, as on USB */
	}
	memcpy(f->out + f->out_n, s, (size_t)n);
	f->out_n += n;
}

void fake_init(struct fake_sp1 *f, int mode, const char *version)
{
	memset(f, 0, sizeof *f);
	f->mode = mode;
	snprintf(f->version, sizeof f->version, "%s", version ? version : "1.4.1");
	f->want = -1;
}

void fake_open(struct fake_sp1 *f, int log_lines)
{
	if (f->mode == FAKE_SILENT) {
		return;
	}
	if (f->wingie) {
		/* the CH340's DTR / RTS reset the ESP32: its ROM speaks, then nothing is heard until it has started */
		say(f, "ets Jul 29 2019 12:21:46\n\nrst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\nload:0x3fff0030,len:1184\nentry 0x400805e4\n");
		f->boot_ms = 600;
		f->want = -1;
		f->line_n = 0;
		return;
	}
	/* sp1_console.c print_banner(), then sp1_logbuf_replay_start() */
	say(f, "\n=== Wakes v0.9.0 ===\nbuilt Nov 19 2026 10:48:00\nresetreas 0x00000000\n");
	say(f, "\n=== STORED LOG: 4096 bytes, oldest first, [+uptime] per line; live output resumes after the end marker ===\n");
	for (int i = 0; i < log_lines; i++) {
		char l[96];
		/* an old STANDBY line in the replay must not be taken for the state now */
		snprintf(l, sizeof l, i == 0 ? "[+00:00:00.%03d] [STANDBY] batt raw=2900  4100mV  lvl=250/255   usb=1 chg=1\n"
		                             : "[+00:00:01.%03d] SCQ slot 1: 512 bytes, 0 mistakes\n", i % 1000);
		say(f, l);
	}
	say(f, "=== STORED LOG: end ===\n");
}

static int load_text(struct fake_sp1 *f)
{
	f->staging[f->got < FAKE_TEXT ? f->got : FAKE_TEXT - 1] = 0;
	if (f->old_mistakes > 0) {
		return f->old_mistakes;
	}
	int e = scq_load(f->staging, f->got, 0);
	/* the device's engine is built for one instrument and refuses a dream for the other */
	if (e == 0 && scq_instrument() != (f->wingie ? SCQ_WINGIE : SCQ_SP1)) {
		e = 1;
	}
	return e;
}

void fake_wingie(struct fake_sp1 *f)
{
	f->wingie = 1;
}

static void handle_line(struct fake_sp1 *f)
{
	char *line = f->line;
	line[f->line_n] = 0;
	if (strncmp(line, "SCQ HELLO", 9) == 0) {
		char r[64];
		snprintf(r, sizeof r, f->wingie ? "SLEEPCOUSONIQUE %s WINGIE\n" : "SLEEPCOUSONIQUE %s\n", f->version);
		say(f, r);
		f->hellos++;
	} else if (strncmp(line, "SCQ MEASURE ", 12) == 0) {
		f->measures++;
		say(f, "SCQ MEASURED 9100 15200 76800 750 0 12000 126976\n");
	} else if (strncmp(line, "SCQ LOAD ", 9) == 0) {
		char *end;
		long n = strtol(line + 9, &end, 10);
		long s = strtol(end, NULL, 10);
		if (n <= 0 || n >= (long)FAKE_TEXT) {
			say(f, "ERR too big\n");
		} else {
			f->want = (int)n;
			f->got = 0;
			f->want_slot = (s >= 0 && s < SLOTS(f)) ? (int)s : f->slot;
		}
	} else if (strncmp(line, "SCQ TRY ", 8) == 0) {
		long n = strtol(line + 8, NULL, 10);
		if (n <= 0 || n >= (long)FAKE_TEXT) {
			say(f, "ERR too big\n");
		} else {
			f->want = (int)n;
			f->got = 0;
			f->want_slot = -1;
		}
	} else if (strncmp(line, "SCQ WATCH ", 10) == 0) {
		f->watching = line[10] == '1';
	} else if (strncmp(line, "SCQ WHO", 7) == 0) {
		say(f, "SCQ ME FAKE0001 0 DREAM\n");
	} else if (strncmp(line, "SCQ SLOT ", 9) == 0) {
		f->slot = ((atoi(line + 9) - 1) % SLOTS(f) + SLOTS(f)) % SLOTS(f);
		say(f, "OK 0\n");
	}
	f->line_n = 0;
}

int fake_feed(struct fake_sp1 *f, const char *bytes, int n)
{
	if (f->mode == FAKE_SILENT || f->mode == FAKE_STANDBY || f->mode == FAKE_OTHER) {
		return n;                 /* taken by the USB stack, never answered */
	}
	if (f->boot_ms > 0) {
		return n;                 /* a Wingie starting again: the bytes are lost */
	}
	for (int i = 0; i < n; i++) {
		char c = bytes[i];
		if (f->want >= 0) {
			if (f->mode == FAKE_STALL && f->got >= 2000) {
				return i;         /* stops reading: the host's writes back up */
			}
			f->staging[f->got++] = c;
			f->taken++;
			if (f->got < f->want) {
				continue;
			}
			f->want = -1;
			f->dreams++;
			int e = load_text(f);
			f->last_len = f->got;
			f->last_errors = e;
			char r[80];
			if (e == 0 && f->want_slot < 0) {
				f->tries++;
				say(f, "OK 0\n");
			} else if (e == 0) {
				f->slot = f->want_slot;
				f->last_slot = f->slot;
				if (f->got > SLOT_BYTES - SLOT_HEAD) {
					say(f, "ERR could not save (playing it anyway)\n");
				} else {
					memcpy(f->slots[f->slot], f->staging, (size_t)f->got);
					f->slot_len[f->slot] = f->got;
					f->loads++;
					snprintf(r, sizeof r, "OK %d\n", e);
					say(f, r);
				}
				snprintf(r, sizeof r, "SCQ received %d bytes into slot %d\n", f->got, f->slot + 1);
				say(f, r);
			} else {
				snprintf(r, sizeof r, "ERR %d mistakes\n", e);
				say(f, r);
			}
			continue;
		}
		if (c == '\n' || c == '\r') {
			if (f->line_n) {
				handle_line(f);
			}
		} else if (f->line_n < (int)sizeof f->line - 1) {
			f->line[f->line_n++] = c;
		}
	}
	return n;
}

void fake_tick(struct fake_sp1 *f, uint32_t ms)
{
	if (f->mode == FAKE_SILENT) {
		return;
	}
	f->ms += ms;
	if (f->wingie) {
		if (f->boot_ms > 0) {
			f->boot_ms = ms >= f->boot_ms ? 0 : f->boot_ms - ms;
			if (f->boot_ms == 0) {
				say(f, "Sleepcousonique for the Wingie: ready\n");
			}
			return;
		}
		if (f->mode != FAKE_ON && f->mode != FAKE_STALL) {
			return;
		}
		if ((f->status_ms += ms) >= 1000u) {
			/* the once-a-second report: the load wanders, so the meters' history has something to draw */
			f->status_ms = 0;
			int s = (int)((f->ms / 1000u) % 20u);
			int avg = 30 + (s < 10 ? s : 20 - s), max = avg + 9 + (s % 3) * 4;
			int dropped = (f->ms / 1000u) % 30u == 7u ? 2 : 0;
			char r[300];
			snprintf(r, sizeof r, "SCQ engine avg=%d%% max=%d%%  places=7 scene=1 slot=%d  load=%d.4 peak=%d.1 dropped=%d of 1500 "
			         "mem=12000/126976 core1=%d%% heap=%d inL=%d inR=%d outL=-18 outR=-19\n",
			         avg, max, f->slot + 1, avg, max, dropped, 4 + s % 3, 123456 - s * 100, -42 + s, -40 + s / 2);
			say(f, r);
		}
		if (f->watching && (f->hands_ms += ms) >= 32u) {
			char r[120];
			f->hands_ms = 0;
			snprintf(r, sizeof r, "SCQ WHANDS 4095 2048 1000 011 800 1 -1 1 0 2 %d 40 ff\n", f->slot + 1);
			say(f, r);
		}
		return;
	}
	uint32_t period = f->mode == FAKE_STANDBY ? 1000u : 5000u;     /* sp1_ui_timing.h */
	if ((f->status_ms += ms) >= period) {
		f->status_ms = 0;
		say(f, f->mode == FAKE_STANDBY ? "[STANDBY] batt raw=2900  4100mV  lvl=250/255   usb=1 chg=1\n"
		                               : "[ON] batt raw=2900  4100mV  lvl=250/255   usb=1 chg=1\n");
		if (f->mode == FAKE_ON || f->mode == FAKE_STALL) {
			say(f, "SCQ engine avg=41% max=57%  places=12 scene=1 slot=1  load=41.0 peak=57.0 dropped=0 of 7500 mem=20000/126976\n");
		}
	}
	if (f->watching && (f->mode == FAKE_ON || f->mode == FAKE_STALL) && (f->hands_ms += ms) >= 32u) {
		f->hands_ms = 0;
		say(f, "SCQ HANDS 4095 2048 0 1000 11 2 3 ff80000000000040\n");
	}
}

int fake_take(struct fake_sp1 *f, char *buf, int cap)
{
	int n = f->out_n < cap ? f->out_n : cap;
	memcpy(buf, f->out, (size_t)n);
	memmove(f->out, f->out + n, (size_t)(f->out_n - n));
	f->out_n -= n;
	return n;
}
