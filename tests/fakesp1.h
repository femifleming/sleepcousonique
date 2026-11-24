/*
 * A pretend SP-1 for testing the editor's USB link without one: the far end of the serial
 * protocol, as firmware/app/src/sp1_scq.c (handle_line, serial_poll) and the Wakes console
 * (sp1_console.c: the banner, the stored-log replay, the [STATE] status lines) speak it.
 *
 * Only the wire is pretend. The dream it is sent is compiled by the real engine (scq_load),
 * so its "OK <n>" / "ERR <n> mistakes" are the ones the SP-1 would give for the same text.
 *
 * Used in two ways: in-process by tests/SerialTest.cpp (on every platform), and behind a
 * pseudo-terminal by tests/fakesp1_pty.c (macOS / Linux) for the real app binary.
 */
#ifndef FAKESP1_H
#define FAKESP1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum fake_mode {
	FAKE_ON = 0,       /* Sleepcousonique running (ON): answers everything */
	FAKE_STANDBY,      /* Sleepcousonique flashed but switched off, plugged in: console only */
	FAKE_OTHER,        /* another firmware (Wakes, the tape looper...): console, no SCQ */
	FAKE_SILENT,       /* a port that never says anything */
	FAKE_STALL         /* ON, but stops taking bytes 2000 bytes into a dream */
};

#define FAKE_OUT_CAP  (64 * 1024)
#define FAKE_TEXT     (16 * 1024)       /* SCQ_MAX_TEXT: the firmware's staging buffer */
#define FAKE_SLOTS    12                /* the SP-1 has 5, the Wingie 12 */

struct fake_sp1 {
	int      mode;
	char     version[24];               /* what HELLO answers ("1.4.0") */
	int      old_mistakes;              /* > 0: every dream gets "ERR n mistakes" (an older engine) */
	/* device -> host */
	char     out[FAKE_OUT_CAP];
	int      out_n;
	/* the firmware's line-or-bytes parser (sp1_scq.c) */
	char     line[96];
	int      line_n;
	int      want, got, want_slot;
	char     staging[FAKE_TEXT];
	int      slot, watching;
	uint32_t ms, status_ms, hands_ms;
	/* what happened, for the tests */
	int      dreams, loads, tries, hellos, taken;   /* dreams: every one received, kept or not */
	int      last_len, last_slot, last_errors;
	char     slots[FAKE_SLOTS][FAKE_TEXT];
	int      slot_len[FAKE_SLOTS];
	/* a pretend Wingie (fake_wingie): HELLO says WINGIE, 12 slots, its hands as SCQ WHANDS, a
	 * report every second with core1/heap/levels, and opening the port resets it (it doesn't
	 * hear anything for boot_ms) */
	int      wingie;
	uint32_t boot_ms;
	int      measures;                  /* SCQ MEASURE asked */
};

void fake_init(struct fake_sp1 *f, int mode, const char *version);
/* the same, as a Wingie (call before fake_open) */
void fake_wingie(struct fake_sp1 *f);
/* the host opened the port (DTR rose): the banner and a stored log of `log_lines` lines */
void fake_open(struct fake_sp1 *f, int log_lines);
/* host -> device; returns how many bytes it took (FAKE_STALL stops taking them) */
int  fake_feed(struct fake_sp1 *f, const char *bytes, int n);
/* time passes: status lines, the hands while watching, the engine report */
void fake_tick(struct fake_sp1 *f, uint32_t ms);
/* device -> host: takes up to cap bytes of what it has said */
int  fake_take(struct fake_sp1 *f, char *buf, int cap);

#ifdef __cplusplus
}
#endif
#endif
