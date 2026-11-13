/* scqtool — check and render .dream files from the command line.
 *   scqtool check file.dream            messages + description
 *   scqtool render file.dream out.wav 8 [scene] [lean1 lean2 lean3 lean4]
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

static void wav(const char *path, const float *l, const float *r, int n)
{
	FILE *f = fopen(path, "wb");
	int data = n * 4, rate = SCQ_RATE, byterate = rate * 4;
	short ch = 2, bits = 16, align = 4, fmt = 1;
	int sz = 36 + data, fmtlen = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	fwrite(&fmtlen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
	fwrite(&rate, 4, 1, f); fwrite(&byterate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
	for (int i = 0; i < n; i++) {
		short a = (short)(l[i] * 32000.f), b = (short)(r[i] * 32000.f);
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
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
	if (argc < 3) { fprintf(stderr, "usage: scqtool check|render|stats file.dream ...\n"); return 2; }
	int len;
	char *text = slurp(argv[2], &len);
	int errors = scq_load(text, len, 0);
	printf("%s", scq_messages());

	if (!strcmp(argv[1], "check")) {
		printf("%s", scq_describe());
		return errors ? 1 : 0;
	}
	if (!strcmp(argv[1], "render")) {
		float secs = argc > 4 ? atof(argv[4]) : 8.f;
		if (argc > 5) { scq_goto_scene(atoi(argv[5])); }
		for (int k = 0; k < 4; k++) scq_control(k, argc > 6 + k ? atof(argv[6 + k]) : 0.5f);
		int n = (int)(secs * SCQ_RATE) / SCQ_BLOCK * SCQ_BLOCK;
		float *l = calloc(n, 4), *r = calloc(n, 4);
		for (int i = 0; i < n; i += 128) {
			int m = n - i < 128 ? n - i : 128;
			scq_render(l + i, r + i, m);
		}
		wav(argv[3], l, r, n);
		return 0;
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
