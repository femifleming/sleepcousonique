/* sweep — every place, every inlet, a fader patched in and the inlet's attenuverter turned
 * from one end to the other, with the fader held, thrown and shaken.
 *
 * The engine is driven the way the editor drives it: the dream is loaded as text, the
 * attenuverter and the panel knob are turned live (scq_set_att / scq_set_knob), the fader is
 * a control (scq_control), and after the storm the dream is loaded again (the editor reloads
 * on every edit, and each place that is still there keeps its state).
 *
 * A case fails when
 *   - any output sample or any outlet of any place is not a finite number, or runs away;
 *   - after the fader and the attenuverter are back where they were, the place doesn't sound
 *     again (silent, or pinned against the output stage), with no restart.
 * One of the moves is poison: the fader's control set to numbers no fader sends (1e30,
 * infinity, not-a-number), to see every place come back from the worst it can be handed.
 *
 *   sweep                 everything, a line per failure, a summary
 *   sweep -v              a line per place
 *   sweep PLACE [INLET]   just that one
 *   sweep -j K/N          the K-th of N equal shares of the places (to run N at once)
 *   sweep --long          the reverbs for three and a half hours of sound (their sample count
 *                         used to stall after about three); takes a minute or two
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../engine/scq.h"

#define MAXP   256
#define MAXIO  12
#define BLK    SCQ_BLOCK

typedef struct {
	char name[24], family[12];
	char in[MAXIO][16], out[MAXIO][16];
	int  nin, nout, pitch, tune, wingie;   /* wingie: a place only Wingie dreams have (catalog I line) */
} pinfo;

static pinfo P[MAXP];
static int   NP;
static int   verbose;

static int split(const char *s, int n, char out[MAXIO][16])
{
	int k = 0, j = 0;
	for (int i = 0; i <= n && k < MAXIO; i++) {
		if (i == n || s[i] == ',' || s[i] == ' ') {
			if (j) { out[k][j] = 0; k++; j = 0; }
		} else if (j < 15) out[k][j++] = s[i];
	}
	return k;
}

static void read_catalog(void)
{
	const char *c = scq_catalog();
	while (*c) {
		const char *e = strchr(c, '\n');
		if (!e) e = c + strlen(c);
		if (c[0] == 'P' && c[1] == ' ' && NP < MAXP) {
			pinfo *p = &P[NP];
			char ins[256] = "";
			memset(p, 0, sizeof *p);
			sscanf(c, "P %23s %11s %*s %*s %255s", p->name, p->family, ins);
			if (strcmp(ins, "|")) p->nin = split(ins, (int)strlen(ins), p->in);
			const char *bar = strstr(c, " | ");
			if (bar && bar < e) {
				const char *o = bar + 3, *bar2 = strstr(o, " | ");
				if (bar2 && bar2 <= e) p->nout = split(o, (int)(bar2 - o), p->out);
			}
			p->pitch = -1;
			for (int i = 0; i < p->nin; i++) if (!strcmp(p->in[i], "pitch")) p->pitch = i;
			NP++;
		} else if (c[0] == 'I' && c[1] == ' ') {
			char nm[24] = "", in1[16] = "";
			int k = sscanf(c, "I %23s %15s", nm, in1);   /* one instrument per line */
			if (k == 2 && !strcmp(in1, "wingie"))
				for (int i = 0; i < NP; i++) if (!strcmp(P[i].name, nm)) P[i].wingie = 1;
		} else if (c[0] == 'O' && c[1] == ' ' && NP > 0) {
			const char *t = strstr(c, "tune=");
			if (t && t < e) P[NP - 1].tune = 1;
		}
		c = *e ? e + 1 : e;
	}
}

static int is_one(const char *w, const char *list)
{
	size_t n = strlen(w);
	for (const char *s = list; *s; ) {
		const char *a = s;
		while (*s && *s != ' ') s++;
		if ((size_t)(s - a) == n && !strncmp(a, w, n)) return 1;
		while (*s == ' ') s++;
	}
	return 0;
}

/* the same feeds as tools/test_places.sh: 1 a sound, 2 a clock, 0 nothing */
static int feed_of(const pinfo *p, const char *in)
{
	if (!strcmp(in, "in") && is_one(p->name, "trigdelay bernoulli trigger")) return 2;
	if (is_one(in, "in a b carrier modulator in1 in2 in3 in4 in5 in6")) return 1;
	if (is_one(in, "strike pluck trig trig1 trig2 cv1 cv2 clock gate next record burst sync hold")) return 2;
	return 0;
}

/* how a case is patched */
enum { ST_FED, ST_BARE, ST_HALF, ST_TUNE_HI, ST_TUNE_LO, ST_SELF, ST_COUNT };
static const char *const ST_NAME[ST_COUNT] = { "fed", "bare", "half", "tune+12", "tune-12", "self" };

static int build(char *t, int cap, const pinfo *p, int inlet, int state, float att, int with_att)
{
	int n = 0;
	if (p->wingie) n += snprintf(t + n, (size_t)(cap - n), "instrument wingie\n");
	n += snprintf(t + n, (size_t)(cap - n), "the x is %s", p->name);
	if (state == ST_HALF) n += snprintf(t + n, (size_t)(cap - n), " samplerate=half");
	if (state == ST_TUNE_HI) n += snprintf(t + n, (size_t)(cap - n), " tune=12");
	if (state == ST_TUNE_LO) n += snprintf(t + n, (size_t)(cap - n), " tune=-12");
	if (with_att) n += snprintf(t + n, (size_t)(cap - n), " att.%s=%g", p->in[inlet], (double)att);
	n += snprintf(t + n, (size_t)(cap - n), "\nthe clk is clock\nthe src is vco 3\n");
	if (state != ST_BARE)
		for (int i = 0; i < p->nin; i++) {
			int f = feed_of(p, p->in[i]);
			if (f == 1) n += snprintf(t + n, (size_t)(cap - n), "src -> x.%s\n", p->in[i]);
			if (f == 2) n += snprintf(t + n, (size_t)(cap - n), "clk.eighth -> x.%s\n", p->in[i]);
		}
	if (state == ST_SELF && p->nout > 0)            /* its own sound back into the inlet as well */
		n += snprintf(t + n, (size_t)(cap - n), "x -> x.%s\n", p->in[inlet]);
	n += snprintf(t + n, (size_t)(cap - n), "fader1 -> x.%s\n", p->in[inlet]);
	for (int o = 0; o < p->nout; o++) {
		if (o == 0) n += snprintf(t + n, (size_t)(cap - n), "x -> out\n");
		else n += snprintf(t + n, (size_t)(cap - n), "x.%s -60> out\n", p->out[o]);
	}
	return n;
}

typedef struct { int bad; float peak, outlet; double sum; long count; } heard;   /* outlet: the largest any outlet stood, before the output stage */

static void listen(heard *h, int blocks, int nodes)
{
	float l[BLK], r[BLK];
	for (int b = 0; b < blocks; b++) {
		scq_render(l, r, BLK);
		for (int i = 0; i < BLK; i++) {
			if (!isfinite(l[i]) || !isfinite(r[i])) { h->bad++; continue; }
			float a = fabsf(l[i]);
			if (a > h->peak) h->peak = a;
			h->sum += (double)l[i] * l[i] + (double)r[i] * r[i];
		}
		h->count += BLK;
		for (int n = 0; n < nodes; n++)
			for (int p = 0; p < 4; p++) {
				float v = scq_probe(n, p);
				if (!isfinite(v) || fabsf(v) > 1e9f) h->bad++;      /* (the engine heals a place past 1e9 before the block ends) */
				else if (fabsf(v) > h->outlet) h->outlet = fabsf(v);
			}
	}
}

static float rms(const heard *h) { return h->count ? (float)sqrt(h->sum / (2.0 * (double)h->count)) : 0.f; }

#define SECONDS(s) ((int)((s) * SCQ_RATE / BLK))
#define REST 0.3f      /* where the fader rests (below the half-way mark a pulse inlet listens for) */

/* nothing carried over from the case before: a dream with no place of the same name */
static void forget(void)
{
	static const char nothing[] = "the nobody is value 1\nnobody -> out\n";
	scq_load(nothing, (int)sizeof nothing - 1, 0);
	for (int k = 0; k < SCQ_CTL_COUNT; k++) scq_control(k, 0.f);
	float l[BLK], r[BLK];
	scq_render(l, r, BLK);
}

static const char *const MOVE_NAME[] = { "held", "thrown", "shaken", "poisoned", "att swept" };
enum { MV_HELD, MV_THROWN, MV_SHAKEN, MV_POISON, MV_ATTSWEEP, MV_COUNT };

typedef struct { int cases, fails, nonfinite, stuck; unsigned healed_plain, healed_self, healed_poison; } tally;

static void fail(const pinfo *p, int inlet, int state, float att, int move, const char *why, float a, float b)
{
	printf("FAIL %-14s %-10s %-8s att %+5.1f fader %-9s  %s (%g, %g)\n",
	       p->name, p->in[inlet], ST_NAME[state], (double)att, MOVE_NAME[move], why, (double)a, (double)b);
}

/* one case: quiet reference, the storm, then everything back and a reload */
static void one(const pinfo *p, int inlet, int state, float att, int has_att, int move, tally *T)
{
	char text[2048];
	int len = build(text, (int)sizeof text, p, inlet, state, 1.f, 0);
	heard ref = { 0 }, storm = { 0 }, after = { 0 }, skip = { 0 };
	const uint32_t healed0 = scq_healed();
	T->cases++;

	forget();
	if (scq_load(text, len, 0)) { printf("FAIL %s.%s: the test dream doesn't load:\n%s%s", p->name, p->in[inlet], text, scq_messages()); T->fails++; return; }
	int nodes = scq_node_count();
	scq_control(SCQ_CTL_LEAN1, REST);
	listen(&skip, SECONDS(1.0), nodes);
	listen(&ref, SECONDS(2.5), nodes);
	if (skip.bad || ref.bad) { fail(p, inlet, state, att, move, "not a number before anything was moved", (float)(skip.bad + ref.bad), ref.peak); T->nonfinite++; T->fails++; return; }

	/* the storm */
	if (has_att) scq_set_att(0, inlet, att);
	switch (move) {
	case MV_HELD:
		scq_control(SCQ_CTL_LEAN1, 1.f);
		listen(&storm, SECONDS(2.0), nodes);
		break;
	case MV_THROWN:                                   /* slammed between the ends */
		for (int k = 0; k < 12; k++) {
			scq_control(SCQ_CTL_LEAN1, (k & 1) ? 0.f : 1.f);
			listen(&storm, SECONDS(0.12), nodes);
		}
		scq_control(SCQ_CTL_LEAN1, 1.f);
		listen(&storm, SECONDS(0.6), nodes);
		break;
	case MV_SHAKEN:                                   /* a new end every block, then held */
		for (int k = 0; k < SECONDS(0.5); k++) {
			scq_control(SCQ_CTL_LEAN1, (k & 1) ? 0.f : 1.f);
			listen(&storm, 1, nodes);
		}
		scq_control(SCQ_CTL_LEAN1, 1.f);
		listen(&storm, SECONDS(1.0), nodes);
		break;
	case MV_POISON: {                                 /* what no fader can send: the place must still come back */
		static const float bad[] = { 1e30f, -1e30f, 3e38f, -3e38f, 0.f, 0.f, 0.f };
		float v[7];
		for (int k = 0; k < 7; k++) v[k] = bad[k];
		v[4] = (float)INFINITY; v[5] = -(float)INFINITY; v[6] = (float)NAN;
		for (int k = 0; k < 7; k++) {
			scq_control(SCQ_CTL_LEAN1, v[k]);
			listen(&storm, SECONDS(0.1), nodes);
			scq_control(SCQ_CTL_LEAN1, 1.f);
			listen(&storm, SECONDS(0.05), nodes);
		}
		break;
	}
	case MV_ATTSWEEP:                                 /* the fader up, the attenuverter dragged end to end */
		scq_control(SCQ_CTL_LEAN1, 1.f);
		for (int k = 0; k <= 200 && has_att; k++) {
			float a = att * (float)cos(6.2831853 * 2.0 * k / 200.0);
			scq_set_att(0, inlet, a);
			listen(&storm, SECONDS(0.01), nodes);
		}
		if (has_att) scq_set_att(0, inlet, att);
		listen(&storm, SECONDS(0.5), nodes);
		break;
	}

	/* back where it was; the editor writes the knob into the dream and loads it again */
	if (has_att) scq_set_att(0, inlet, 1.f);
	scq_control(SCQ_CTL_LEAN1, REST);
	scq_load(text, len, 0);
	skip.bad = 0;
	listen(&skip, SECONDS(1.5), nodes);
	listen(&after, SECONDS(2.5), nodes);

	/* Not a number: never, at any time. Coming back: what was sounding before sounds again
	 * (not silent), and isn't pinned against the output stage (a place that rang up to
	 * thousands of times full scale is as dead as a silent one). A sub-audio place (tune=-12:
	 * one wave takes longer than the window) is only asked not to be pinned. */
	int failed = 0;
	if (storm.bad) { fail(p, inlet, state, att, move, "not a number during", (float)storm.bad, storm.peak); T->nonfinite++; failed = 1; }
	if (skip.bad || after.bad) { fail(p, inlet, state, att, move, "not a number after", (float)(skip.bad + after.bad), after.peak); T->nonfinite++; failed = 1; }
	else {
		float a = rms(&ref), b = rms(&after);
		int silent = state != ST_TUNE_LO && a > 0.002f && b < a * 0.05f;
		int pinned = after.outlet > 30.f && after.outlet > ref.outlet * 30.f;      /* outlets far past full scale, where they weren't */
		if (silent || pinned) { fail(p, inlet, state, att, move, silent ? "silent afterwards" : "pinned afterwards", a, b); T->stuck++; failed = 1; }
	}
	if (failed) T->fails++;
	/* how often the engine had to start a place again: never for a fader and an attenuverter
	 * alone; a place fed its own sound, or poisoned, is what the net is for */
	{
		unsigned h = (unsigned)(scq_healed() - healed0);
		if (move == MV_POISON) T->healed_poison += h;
		else if (state == ST_SELF) T->healed_self += h;
		else {
			if (h) printf("NOTE %-14s %-10s %-8s att %+5.1f fader %-9s  started again %u times\n", p->name, p->in[inlet], ST_NAME[state], (double)att, MOVE_NAME[move], h);
			T->healed_plain += h;
		}
	}
}

/* three and a half hours of a reverb fed a tick every second: the last ten minutes must still
 * ring like the first ten */
static int long_run(void)
{
	static const char *const names[] = { "reverb", "plate", "diffuser", "reverblite" };
	int fails = 0;
	for (int k = 0; k < 4; k++) {
		char text[256];
		int len = snprintf(text, sizeof text, "the x is %s\nthe clk is clock\nclk.bar -> x\nx -> out\n", names[k]);
		forget();
		if (scq_load(text, len, 0)) { printf("FAIL %s: doesn't load\n", names[k]); fails++; continue; }
		const long total = (long)(3.5 * 3600) * SCQ_RATE / BLK, win = 600L * SCQ_RATE / BLK;
		float l[BLK], r[BLK];
		double first = 0, last = 0;
		int bad = 0;
		for (long b = 0; b < total; b++) {
			scq_render(l, r, BLK);
			double e = 0;
			for (int i = 0; i < BLK; i++) { if (!isfinite(l[i])) bad++; e += (double)l[i] * l[i]; }
			if (b < win) first += e;
			if (b >= total - win) last += e;
		}
		int ok = !bad && first > 0 && last > first * 0.97 && last < first * 1.03;   /* (energy: within 1.5% in level) */
		printf("%-10s first ten minutes %.6g, last ten minutes %.6g  %s\n", names[k], sqrt(first / (double)(win * BLK)), sqrt(last / (double)(win * BLK)), ok ? "ok" : "FAIL");
		fflush(stdout);
		if (!ok) fails++;
	}
	return fails;
}

int main(int argc, char **argv)
{
	const char *only = 0, *only_in = 0;
	int share = 0, shares = 1;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) verbose = 1;
		else if (!strcmp(argv[i], "--long")) return long_run() ? 1 : 0;
		else if (!strcmp(argv[i], "-j") && i + 1 < argc) { if (sscanf(argv[++i], "%d/%d", &share, &shares) != 2 || shares < 1 || share < 1 || share > shares) { fprintf(stderr, "-j K/N, K from 1 to N\n"); return 2; } }
		else if (!only) only = argv[i];
		else only_in = argv[i];
	}
	read_catalog();
	tally T = { 0 };
	int places = 0, inlets = 0;
	for (int k = 0; k < NP; k++) {
		const pinfo *p = &P[k];
		if (only && strcmp(only, p->name)) continue;
		if (!strcmp(p->family, "hands") || !strcmp(p->family, "ears") || p->nin == 0) continue;
		if (k % shares != share - (share > 0)) continue;
		int has_att = !strcmp(p->family, "source") || p->pitch >= 0;       /* place_has_att() */
		int before = T.fails;
		places++;
		for (int in = 0; in < p->nin; in++) {
			if (only_in && strcmp(only_in, p->in[in])) continue;
			inlets++;
			float top = !strcmp(p->in[in], "pitch") ? 10.f : 2.f;           /* the knob's ends */
			static const float AT[] = { 1.f, -1.f, 0.5f, 0.f, -0.5f };
			for (int state = 0; state < ST_COUNT; state++) {
				if ((state == ST_TUNE_HI || state == ST_TUNE_LO) && (!p->tune || in != p->pitch)) continue;
				if (state == ST_SELF && p->nout == 0) continue;
				if (!has_att) {                  /* no attenuverter: the fader alone, at full */
					for (int mv = 0; mv < MV_ATTSWEEP; mv++) one(p, in, state, 1.f, 0, mv, &T);
					continue;
				}
				for (int a = 0; a < (int)(sizeof AT / sizeof AT[0]); a++)
					for (int mv = 0; mv < MV_COUNT; mv++) {
						if (mv == MV_ATTSWEEP && a > 1) continue;
						if (mv != MV_HELD && a > 1) continue;      /* the ends get every move */
						one(p, in, state, AT[a] * top, 1, mv, &T);
					}
			}
		}
		if (verbose) printf("%-14s %s\n", p->name, T.fails == before ? "ok" : "FAIL");
		fflush(stdout);
	}
	printf("%d places, %d inlets, %d cases: %d failed (%d not a number, %d didn't come back)\n",
	       places, inlets, T.cases, T.fails, T.nonfinite, T.stuck);
	printf("places started again by the engine: %u with a fader and an attenuverter alone, %u fed their own sound, %u poisoned\n",
	       T.healed_plain, T.healed_self, T.healed_poison);
	return T.fails ? 1 : 0;
}
