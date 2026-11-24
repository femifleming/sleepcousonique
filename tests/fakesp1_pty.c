/* fakesp1 [on|standby|other|silent|stall] [version] [--log N] [--old N] [--save FILE] [--wingie]
 *
 * A pretend SP-1 (tests/fakesp1.c) behind a pseudo-terminal: prints the port's path on the first
 * line, then plays the SP-1's side of the USB serial link until it is killed (or, with --once,
 * after the first dream). Point the editor at it with SCQ_PORTS=<path>:
 *
 *     ./build/fakesp1 on > port.txt &  SCQ_PORTS=$(head -1 port.txt) Sleepcousonique --send x.dream
 *
 * --save FILE writes the last dream it was sent, byte for byte, so a test can compare. */
#define _XOPEN_SOURCE 600
#define _DARWIN_C_SOURCE
#ifndef SCQ_FIRMWARE_VERSION
#define SCQ_FIRMWARE_VERSION "1.5.0"
#endif
#include "fakesp1.h"
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static uint32_t ms_now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

static struct fake_sp1 f;

int main(int argc, char **argv)
{
	int mode = FAKE_ON, log_lines = 20, once = 0, wingie = 0;
	const char *version = SCQ_FIRMWARE_VERSION, *save = NULL;   /* the firmware this editor comes with */
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "on")) mode = FAKE_ON;
		else if (!strcmp(argv[i], "standby")) mode = FAKE_STANDBY;
		else if (!strcmp(argv[i], "other")) mode = FAKE_OTHER;
		else if (!strcmp(argv[i], "silent")) mode = FAKE_SILENT;
		else if (!strcmp(argv[i], "stall")) mode = FAKE_STALL;
		else if (!strcmp(argv[i], "--log") && i + 1 < argc) log_lines = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--old") && i + 1 < argc) f.old_mistakes = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
		else if (!strcmp(argv[i], "--once")) once = 1;
		else if (!strcmp(argv[i], "--wingie")) wingie = 1;   /* a pretend Wingie instead */
		else version = argv[i];
	}
	int old = f.old_mistakes;
	fake_init(&f, mode, version);
	f.old_mistakes = old;
	if (wingie) {
		fake_wingie(&f);
	}

	int m = posix_openpt(O_RDWR | O_NOCTTY);
	if (m < 0 || grantpt(m) || unlockpt(m)) {
		perror("pty");
		return 1;
	}
	const char *name = ptsname(m);
	/* hold the far end open too, raw, so the line discipline never echoes our output back to
	 * us and closing the editor's end doesn't hang the master up */
	int keep = open(name, O_RDWR | O_NOCTTY);
	struct termios t;
	tcgetattr(keep, &t);
	cfmakeraw(&t);
	tcsetattr(keep, TCSANOW, &t);
	printf("%s\n", name);
	fflush(stdout);
	signal(SIGPIPE, SIG_IGN);
	fcntl(m, F_SETFL, fcntl(m, F_GETFL) | O_NONBLOCK);

	/* no DTR on a pty: the banner and the log come at once, and again whenever the editor has
	 * been quiet for 3 s (a new connection, as near as we can tell) */
	fake_open(&f, log_lines);
	uint32_t last = ms_now(), heard = last;
	char pend[4096], out[1024];
	int pend_n = 0, out_n = 0, loads = 0;
	for (;;) {
		struct pollfd p = { m, POLLIN, 0 };
		poll(&p, 1, 5);
		uint32_t t2 = ms_now();
		fake_tick(&f, t2 - last);
		last = t2;
		if (pend_n == 0) {
			int n = (int)read(m, pend, sizeof pend);
			if (n > 0) {
				if (t2 - heard > 3000u) {
					fake_open(&f, log_lines);
				}
				heard = t2;
				pend_n = n;
			}
		}
		if (pend_n > 0) {
			int k = fake_feed(&f, pend, pend_n);   /* a stalled SP-1 takes nothing more */
			memmove(pend, pend + k, (size_t)(pend_n - k));
			pend_n -= k;
		}
		/* what it says goes out as fast as the editor's end takes it */
		if (out_n == 0) {
			out_n = fake_take(&f, out, sizeof out);
		}
		if (out_n > 0) {
			int w = (int)write(m, out, (size_t)out_n);
			if (w > 0) {
				memmove(out, out + w, (size_t)(out_n - w));
				out_n -= w;
			}
		}
		if (f.dreams != loads) {
			loads = f.dreams;
			if (save) {
				FILE *o = fopen(save, "wb");
				if (o) {
					fwrite(f.staging, 1, (size_t)f.last_len, o);
					fclose(o);
				}
			}
			fprintf(stderr, "fakesp1: a dream of %d bytes, %d mistakes, slot %d\n", f.last_len, f.last_errors, f.last_slot + 1);
			if (once) {
				usleep(300000);
				return 0;
			}
		}
	}
}
