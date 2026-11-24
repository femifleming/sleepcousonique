/* wingiehands — the Wingie's octave switches, transpose, and pitch on places tuned under another name.
 *   cc -std=c99 -O2 -DSCQ_HOST -Iengine tests/wingiehands.c engine/scq_core.c engine/scq_places.c -lm */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../engine/scq.h"

static int fails = 0;
static void check(int ok, const char *what, float got)
{
	printf("%s %s (%.4f)\n", ok ? "ok  " : "FAIL", what, got);
	if (!ok) fails++;
}
static void run(void) { float l[128], r[128]; for (int i = 0; i < 4; i++) scq_render(l, r, 128); }
static int load(const char *t) { static char b[2048]; snprintf(b, sizeof b, "%s", t); return scq_load(b, (int)strlen(b), 1); }

int main(void)
{
	/* nodes in the order written: 0 octave1, 1 t (transpose), 2 f (transpose 7st), 3 keys1, 4 out */
	if (load("instrument wingie\nthe t is transpose\nthe f is transpose 7st\noctave1 -> t\noctave1 -> f\nkeys1 -> t.pitch\nt -> out\nf -> out\n") < 0) { puts("FAIL load"); return 1; }
	const char *d = scq_describe();
	int oct = -1, t = -1, f = -1, keys = -1;
	for (int n = 0; n < 16; n++) {
		char key[16]; snprintf(key, sizeof key, "N %d ", n);
		const char *at = strstr(d, key); if (!at) continue;
		char place[32], name[32]; sscanf(at, "N %*d %31s %*s %31s", place, name);
		if (!strcmp(name, "octave1")) oct = n;
		else if (!strcmp(name, "t")) t = n;
		else if (!strcmp(name, "f")) f = n;
		else if (!strcmp(place, "keys")) keys = n;
	}
	if (oct < 0 || t < 0 || f < 0 || keys < 0) { printf("FAIL nodes %d %d %d %d\n%s\n", oct, t, f, keys, d); return 1; }
	float want_sw[3] = { 0.f, 0.5f, 1.f }, want_t[3] = { -1.f, 0.f, 1.f };
	for (int s = -1; s <= 1; s++) {
		scq_control(SCQ_CTL_OCTAVE1, (float)s);
		for (int k = 0; k < 12; k++) scq_control(SCQ_CTL_KEYS1 + k, k == 7 ? 1.f : 0.f);   /* G held */
		run();
		char w[96];
		snprintf(w, sizeof w, "octave1 at %+d reads %.1f", s, want_sw[s + 1]);
		check(fabsf(scq_probe(oct, 0) - want_sw[s + 1]) < 1e-4f, w, scq_probe(oct, 0));
		snprintf(w, sizeof w, "keys1 ignores the switch at %+d: G is 7/12", s);
		check(fabsf(scq_probe(keys, 0) - 7.f / 12.f) < 1e-4f, w, scq_probe(keys, 0));
		snprintf(w, sizeof w, "transpose: G plus %+.0f octave", want_t[s + 1]);
		check(fabsf(scq_probe(t, 0) - (7.f / 12.f + want_t[s + 1])) < 1e-4f, w, scq_probe(t, 0));
		snprintf(w, sizeof w, "transpose 7st: %+.0f fifth", want_t[s + 1]);
		check(fabsf(scq_probe(f, 0) - want_t[s + 1] * 7.f / 12.f) < 1e-3f, w, scq_probe(f, 0));
	}
	/* pitch names the tuning inlet of cave, pitchshift, specshift, formantshift */
	const char *alias[] = { "cave", "pitchshift", "specshift", "formantshift" };
	for (int i = 0; i < 4; i++) {
		char b[256];
		snprintf(b, sizeof b, "instrument wingie\nthe x is %s\nmic.both -> x\nkeys1 -> x.pitch\nx -> out\n", alias[i]);
		int r = load(b);
		char w[96]; snprintf(w, sizeof w, "keys1 -> %s.pitch is understood", alias[i]);
		check(r >= 0 && !strstr(scq_messages(), "E "), w, (float)r);
	}
	printf("%s\n", fails ? "FAILED" : "all good");
	return fails ? 1 : 0;
}
