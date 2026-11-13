/* sp1bench — measures what dreams really cost on a connected SP-1.
 *
 *   sp1bench /dev/cu.usbmodem101 a.dream b.dream ...
 *
 * Each dream is sent to slot 5 (so slot 1 is left alone), then the firmware's own audio
 * report ("AUD ... cyc avg=X%") is read: the second report after loading, so the window
 * is entirely the new dream. Prints one line per dream: average and peak load in percent.
 * Ends with "SCQ SLOT 1" so the SP-1 goes back to what it was playing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

static int fd;

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static int read_line(char *buf, int cap, double deadline)
{
	int n = 0;
	while (now() < deadline) {
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, 50) <= 0) continue;
		char c;
		if (read(fd, &c, 1) != 1) continue;
		if (c == '\r') continue;
		if (c == '\n') { buf[n] = 0; return n; }
		if (n < cap - 1) buf[n++] = c;
	}
	buf[n] = 0;
	return -1;
}

static void write_all(const char *d, size_t n)
{
	while (n) {
		ssize_t w = write(fd, d, n);
		if (w < 0) { if (errno == EAGAIN) { usleep(2000); continue; } return; }
		d += w; n -= (size_t)w;
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: sp1bench /dev/cu.usbmodemX a.dream ...\n"); return 2; }
	fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) { perror(argv[1]); return 1; }
	struct termios t;
	tcgetattr(fd, &t);
	cfmakeraw(&t);
	cfsetspeed(&t, B115200);
	t.c_cflag |= CLOCAL | CREAD;
	tcsetattr(fd, TCSANOW, &t);
	tcflush(fd, TCIOFLUSH);
	usleep(300000);

	char line[512];
	/* opening the port replays the stored log: let it finish */
	double quiet = now() + 15.0;
	while (now() < quiet) {
		if (read_line(line, sizeof line, now() + 1.5) < 0) break;
		if (strstr(line, "STORED LOG: end")) { usleep(300000); break; }
	}
	tcflush(fd, TCIFLUSH);
	for (int a = 2; a < argc; a++) {
		FILE *f = fopen(argv[a], "rb");
		if (!f) { perror(argv[a]); continue; }
		char body[16384];
		size_t n = fread(body, 1, sizeof body, f);
		fclose(f);
		char head[64];
		int hn = snprintf(head, sizeof head, "SCQ LOAD %zu 4\n", n);
		write_all(head, (size_t)hn);
		write_all(body, n);

		/* wait for OK, then skip one AUD report and keep the next */
		double end = now() + 20.0;
		int ok = 0, auds = 0;
		double avg = -1, max = -1;
		end = now() + 25.0;
		while (now() < end && auds < 3) {
			if (read_line(line, sizeof line, end) < 0) break;
			if (!strncmp(line, "OK", 2)) ok = 1;
			if (!strncmp(line, "ERR", 3)) { printf("%-28s %s\n", argv[a], line); break; }
			char *c = strstr(line, "cyc avg=");
			if (ok && c) {
				auds++;
				if (auds >= 2) {
					avg = atof(c + 8);
					char *m = strstr(line, "max=");
					if (m) max = atof(m + 4);
					const char *base = strrchr(argv[a], '/');
					printf("%-16s %s\n", base ? base + 1 : argv[a], strstr(line, "blk="));
					fflush(stdout);
				}
			}
		}
		if (avg < 0 && ok) printf("%-28s (no report)\n", argv[a]);
	}
	write_all("SCQ SLOT 1\n", 11);
	usleep(500000);
	close(fd);
	return 0;
}
