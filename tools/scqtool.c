/* scqtool — check and render .dream files from the command line.
 *   scqtool check file.dream            messages + description
 *   scqtool stats file.dream            render 4 s of every scene, report level / NaN
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../engine/scq.h"

static char *slurp(const char *path, int *len)
{
	FILE *f = fopen(path, "rb");
	if (!f) { perror(path); exit(1); }
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = malloc(n + 1);
	fread(b, 1, n, f);
	fclose(f);
	b[n] = 0;
	*len = (int)n;
	return b;
}

static int scene_count(void)
{
	const char *d = scq_describe();
	int k, n;
	if (sscanf(d, "S %d %d", &k, &n) == 2) return n;
	return 1;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "catalog")) { printf("%s", scq_catalog()); return 0; }
	if (argc < 3) { fprintf(stderr, "usage: scqtool check|stats file.dream  |  scqtool catalog\n"); return 2; }
	int len;
	char *text = slurp(argv[2], &len);
	int errors = scq_load(text, len, 0);
	printf("%s", scq_messages());

	if (!strcmp(argv[1], "check")) {
		printf("%s", scq_describe());
		return errors ? 1 : 0;
	}
	if (!strcmp(argv[1], "probe")) {
		/* 4 s with a clock tapping the doors; every outlet of every place watched */
		float l[128], r[128], mx[48][4] = { { 0 } };
		int bad[48][4] = { { 0 } };
		for (int k = 0; k < 4; k++) scq_control(k, 0.5f);
		for (int i = 0; i < 4 * SCQ_RATE; i += 128) {
			float t = (float)i / SCQ_RATE;
			scq_control(4, fmodf(t, 0.5f) < 0.1f ? 1.f : 0.f);
			scq_render(l, r, 128);
			for (int n = 0; n < 48; n++)
				for (int p = 0; p < 4; p++) {
					float v = scq_probe(n, p);
					if (isnan(v) || isinf(v)) bad[n][p] = 1;
					else if (fabsf(v) > mx[n][p]) mx[n][p] = fabsf(v);
				}
		}
		int fails = 0;
		const char *d = scq_describe();
		for (int n = 0; n < 48; n++) {
			char key[16];
			snprintf(key, sizeof key, "\nN %d ", n);
			const char *at = strstr(d, key);
			if (!at && !(n == 0 && !strncmp(d, "N 0 ", 4))) continue;
			char place[32] = "", name[32] = "";
			sscanf(at ? at + 1 : d, "N %*d %31s %*s %31s", place, name);
			printf("%-12s %-10s", place, name);
			for (int p = 0; p < 4; p++) {
				printf("  %s%.3f", bad[n][p] ? "NaN!" : "", mx[n][p]);
				if (bad[n][p] || mx[n][p] > 50.f) fails++;
			}
			printf("\n");
		}
		return fails ? 1 : 0;
	}
	if (!strcmp(argv[1], "stats")) {
		int ns = scene_count(), bad = 0;
		for (int s = 0; s < ns; s++) {
			scq_load(text, len, 0);
			scq_goto_scene(s);
			for (int k = 0; k < 4; k++) scq_control(k, 0.5f);
			int n = 4 * SCQ_RATE;
			float l[128], r[128], peak = 0, sum = 0;
			int nan = 0;
			for (int i = 0; i < n; i += 128) {
				/* tap the doors and the rocker now and then */
				float t = (float)i / SCQ_RATE;
				scq_control(4, fmodf(t, 1.0f) < 0.1f ? 1.f : 0.f);
				scq_render(l, r, 128);
				for (int j = 0; j < 128; j++) {
					if (isnan(l[j]) || isnan(r[j])) nan++;
					float a = fabsf(l[j]) > fabsf(r[j]) ? fabsf(l[j]) : fabsf(r[j]);
					if (a > peak) peak = a;
					sum += l[j] * l[j] + r[j] * r[j];
				}
			}
			const char *d = scq_describe();
			const char *m = strstr(d, "\nM ");
			float rms = sqrtf(sum / (2.f * n));
			printf("scene %d  peak %.3f  rms %.4f  nan %d  %s", scq_scene(), peak, rms, nan, m ? m + 1 : "\n");
			if (nan || peak < 0.0005f) bad++;
		}
		return bad ? 1 : 0;
	}
	return 2;
}
