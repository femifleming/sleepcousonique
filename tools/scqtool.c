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
