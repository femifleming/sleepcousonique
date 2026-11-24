/* pitchcheck — does a Wingie keyboard actually move every place's pitch?
 * For every place with a pitch inlet: keys1 -> x.pitch, a sound in at mic, render with the C key
 * held and again with the G key held; report the places whose output doesn't change.
 *   cc -std=c99 -O2 -DSCQ_HOST -Iengine tests/pitchcheck.c engine/scq_core.c engine/scq_places.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../engine/scq.h"

static float render(const char *text, int key, float *out, int frames)
{
	static char buf[4096];
	snprintf(buf, sizeof buf, "%s", text);
	if (scq_load(buf, (int)strlen(buf), 1) < 0) return -1;
	float inl[128], inr[128], l[128], r[128];
	static double ph = 0;
	ph = 0;
	for (int k = 0; k < 12; k++) scq_control(SCQ_CTL_KEYS1 + k, 0.f);
	for (int k = 0; k < 3; k++) scq_control(k, 0.5f);
	int done = 0;
	double e = 0;
	while (done < frames) {
		for (int i = 0; i < 128; i++) {             /* a saw with some noise, into the mics */
			ph += 220.0 / SCQ_RATE; if (ph >= 1) ph -= 1;
			inl[i] = inr[i] = (float)(ph * 2 - 1) * 0.4f + 0.05f * ((rand() & 1023) / 512.f - 1.f);
		}
		if (done == 2048) scq_control(SCQ_CTL_KEYS1 + key, 1.f);   /* the key goes down after a moment */
		scq_input(inl, inr);
		scq_render(l, r, 128);
		for (int i = 0; i < 128 && done + i < frames; i++) { out[done + i] = l[i] + r[i]; e += (double)out[done + i] * out[done + i]; }
		done += 128;
	}
	return (float)sqrt(e / frames);
}

int main(int argc, char **argv)
{
	const char *cat = scq_catalog();
	enum { N = SCQ_RATE * 2 };
	static float a[N], b[N];
	int silent = 0, still = 0, checked = 0;
	const char *p = cat;
	while ((p = strstr(p, "P ")) != NULL) {
		if (p != cat && p[-1] != '\n') { p++; continue; }
		char name[40], fam[24], ins[256] = "";
		int cost, nat;
		if (sscanf(p, "P %39s %23s %d %d %255s", name, fam, &cost, &nat, ins) < 5) { p++; continue; }
		const char *eol = strchr(p, '\n');
		p = eol ? eol : p + strlen(p);
		if (!strcmp(fam, "hands") || !strcmp(fam, "ears")) continue;
		if (!strcmp(ins, "|")) continue;
		char *tok = strtok(ins, ",");
		for (; tok; tok = strtok(NULL, ",")) {
			if (strcmp(tok, "pitch")) continue;
			char d[1024];
			snprintf(d, sizeof d, "instrument wingie\nthe c is clock 4\nthe x is %s\nmic.both -> x\nc.eighth -> x\nkeys1 -> x.pitch\nx -> out\n", name);
			float ea = render(d, 0, a, N), eb = render(d, 7, b, N);
			if (ea < 0) { printf("refused     %s\n", name); continue; }
			checked++;
			double diff = 0, ref = 0;
			for (int i = N / 2; i < N; i++) { diff += (a[i] - b[i]) * (a[i] - b[i]); ref += a[i] * a[i] + b[i] * b[i]; }
			double rel = ref > 1e-12 ? sqrt(diff / ref) : 0;
			if (ea < 1e-4 && eb < 1e-4) { printf("silent      %s\n", name); silent++; }
			else if (rel < 0.02) { printf("no change   %s  (%.4f)\n", name, rel); still++; }
			else if (argc > 1) printf("ok          %s  (%.3f)\n", name, rel);
		}
	}
	printf("%d places with a pitch inlet: %d don't move with the keys, %d silent\n", checked, still, silent);
	return still ? 1 : 0;
}
