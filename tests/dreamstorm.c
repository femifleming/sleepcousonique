/* dreamstorm — every scene of a dream played as a performer would: line in that sounds like a
 * gig (a drum loop, a voice, a sine, silence, and a burst at full scale), and the hands moved
 * about the whole time, deterministically at random: faders slid and thrown, keys and chords
 * played and let go, the octave switches and the Line/Mic switch flipped, the Mode buttons
 * tapped, held and combined (the firmware's scene combos), on the SP-1 the faders, T1-T4, the
 * rocker, PLAY and the dots.
 *
 * For every scene:
 *   A  reference: the hands at rest (faders at 0.6, nothing held), the drums and the voice in
 *   B  the storm: the hands and the line in at random, with a burst at full scale part way
 *   C  silence in, the hands at rest again
 *   D  the drums and the voice again, the hands at rest
 * It fails when
 *   - any output sample or any outlet of any place is not a finite number (any time);
 *   - in C the output stays pinned against the output stage (a loop that runs away on its own);
 *   - a scene that sounded in A gives nothing at all in D (a place that never came back).
 * It warns (and does not fail) when the engine had to start a place again, when an outlet
 * climbs past anything sane, when D is far quieter than A (some dreams keep what the hands
 * latched: a toggle, a frozen tape), or when the storm spends long against the output stage.
 * Then each dream is loaded and unloaded 50 times between two identical renders: the memory it
 * asks for must stay the same, and both renders must give the same bytes.
 *
 *   dreamstorm [-s SECONDS] [-n RELOADS] [-r ROUNDS] [-R FIRST] [-S SCENE] [-v] file.dream ...
 *     -s  seconds of sound per scene (default 30; A, C and D take 11 of them)
 *     -n  load / unload cycles for the reload test (default 50)
 *     -r  rounds: every scene played this many times, each with its own storm (default 1)
 *     -R  start at this round (to play one storm again: -R 7 -r 1), -S just this scene (1 is the first)
 *     -v  print the moves as they happen (very long); -vv the output level every quarter second too
 * Exit status: 1 if anything failed, else 0.
 *
 *   cc -std=c99 -O2 -Wall -DSCQ_HOST -Iengine tests/dreamstorm.c engine/scq_core.c engine/scq_places.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/resource.h>
#include "../engine/scq.h"

#define SR   SCQ_RATE
#define BLK  SCQ_BLOCK
#define MAXN 96

static int verbose, round_;

/* ------------------------------------------------------------------ deterministic chance */

typedef struct { uint32_t s; } rng;
static uint32_t rnext(rng *r) { uint32_t x = r->s ? r->s : 0x9e3779b9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return r->s = x; }
static float rfloat(rng *r) { return (float)(rnext(r) >> 8) * (1.f / 16777216.f); }
static float rrange(rng *r, float a, float b) { return a + (b - a) * rfloat(r); }
static int   rint_(rng *r, int n) { return (int)(rfloat(r) * (float)n) % n; }
static uint32_t hash_str(const char *s) { uint32_t h = 2166136261u; while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; } return h ? h : 1; }

/* ------------------------------------------------------------------ the line in */

/* (the burst is last: the storm draws its segments from everything before it) */
enum { IN_SILENCE, IN_DRUMS, IN_VOICE, IN_SINE, IN_DRUMVOICE, IN_ALL, IN_CLICKS, IN_RUMBLE, IN_BURST, IN_KINDS };
static const char *const IN_NAME[IN_KINDS] = { "silence", "drums", "voice", "sine", "drums+voice", "all", "clicks", "rumble", "BURST" };

typedef struct {
	rng r;
	long t;
	float gd, gv, gs, gb, gc, gdc;     /* levels of each part, glided (gdc: the rumble's rate) */
	float td, tv, ts, tb, tc, tdc;     /* where they glide to */
	float rph;                         /* the rumble's phase */
	/* drums */
	float bpm; long step_len, step_at; int step; uint16_t kicks, snares;
	float kenv, kph, kf, senv, henv, hprev;
	/* voice */
	float f0, vph, vib, syl, sylt, sylgap, form[3], ftarget[3], ry1[3], ry2[3];
	long syl_left;
	/* sine */
	float sf, sft, sph;
} linein;

static const float VOWELS[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };

static void in_init(linein *g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->r.s = seed;
	g->bpm = rrange(&g->r, 92.f, 132.f);
	g->step_len = (long)(SR * 60.f / g->bpm / 4.f);
	g->kicks = (uint16_t)(0x1111 | (rnext(&g->r) & 0x0404));   /* four on the floor, now and then a ghost */
	g->snares = 0x1010;
	g->f0 = rrange(&g->r, 105.f, 220.f);
	g->sf = g->sft = rrange(&g->r, 80.f, 1500.f);
	for (int k = 0; k < 3; k++) g->form[k] = g->ftarget[k] = VOWELS[0][k];
}

static void in_mix(linein *g, int kind)
{
	g->td = g->tv = g->ts = g->tb = g->tc = g->tdc = 0.f;
	switch (kind) {
	case IN_DRUMS: g->td = 1.f; break;
	case IN_VOICE: g->tv = 1.f; break;
	case IN_SINE: g->ts = 1.f; g->sft = expf(rrange(&g->r, logf(30.f), logf(rfloat(&g->r) < 0.2f ? 20000.f : 3000.f))); break;
	case IN_CLICKS: g->tc = 1.f; break;
	case IN_RUMBLE: g->tdc = rrange(&g->r, 2.f, 9.f); g->tv = 0.5f; break;   /* a knocked cable, a stage rumbling (the input is AC-coupled: no DC) */
	case IN_DRUMVOICE: g->td = 0.8f; g->tv = 0.8f; break;
	case IN_ALL: g->td = 0.7f; g->tv = 0.7f; g->ts = 0.5f; break;
	case IN_BURST: g->tb = 1.f; break;
	default: break;
	}
}

/* a two-pole resonator: a formant */
static float reson(float x, float f, float bw, float *y1, float *y2)
{
	float r = expf(-3.14159265f * bw / SR), c = 2.f * r * cosf(6.2831853f * f / SR);
	float y = (1.f - r) * x + c * *y1 - r * r * *y2;
	*y2 = *y1; *y1 = y;
	return y;
}

static void in_render(linein *g, float *l, float *rr, int n)
{
	for (int i = 0; i < n; i++, g->t++) {
		const float a = 0.0015f;                       /* about 15 ms of glide between parts */
		g->gd += (g->td - g->gd) * a; g->gv += (g->tv - g->gv) * a; g->gs += (g->ts - g->gs) * a;
		g->gb = g->tb;                                  /* the burst, the clicks and the dc come in hard */
		g->gc = g->tc; g->gdc = g->tdc;
		/* drums */
		if (g->t >= g->step_at) {
			g->step_at = g->t + g->step_len;
			int s = g->step & 15;
			if (g->kicks & (1u << s)) { g->kenv = 1.f; g->kf = 160.f; }
			if (g->snares & (1u << s)) g->senv = 1.f;
			if (!(s & 1)) g->henv = (s & 2) ? 0.5f : 0.8f;
			g->step++;
		}
		float noise = rfloat(&g->r) * 2.f - 1.f;
		g->kph += g->kf / SR; if (g->kph >= 1.f) g->kph -= 1.f;
		g->kf = 48.f + (g->kf - 48.f) * 0.9993f;
		float kick = sinf(6.2831853f * g->kph) * g->kenv; g->kenv *= 0.99985f;
		float snare = (noise * 0.7f + 0.3f * sinf(6.2831853f * 190.f * (float)g->t / SR)) * g->senv; g->senv *= 0.9995f;
		float hat = (noise - g->hprev) * 0.5f * g->henv; g->hprev = noise; g->henv *= 0.997f;
		float drums = 0.55f * kick + 0.3f * snare + 0.15f * hat;
		/* voice: a buzz through three formants, in syllables */
		if (--g->syl_left <= 0) {
			int v = rint_(&g->r, 5);
			for (int k = 0; k < 3; k++) g->ftarget[k] = VOWELS[v][k] * rrange(&g->r, 0.9f, 1.1f);
			g->syl_left = (long)(SR * rrange(&g->r, 0.12f, 0.45f));
			g->sylt = rfloat(&g->r) < 0.18f ? 0.f : 1.f;   /* a gap between words */
			if (rfloat(&g->r) < 0.2f) g->f0 *= rrange(&g->r, 0.85f, 1.18f);
			if (g->f0 < 90.f) g->f0 = 90.f;
			if (g->f0 > 300.f) g->f0 = 300.f;
		}
		g->syl += (g->sylt - g->syl) * 0.002f;
		g->vib += 5.2f / SR; if (g->vib >= 1.f) g->vib -= 1.f;
		g->vph += g->f0 * (1.f + 0.012f * sinf(6.2831853f * g->vib)) / SR; if (g->vph >= 1.f) g->vph -= 1.f;
		float buzz = (1.f - 2.f * g->vph) + 0.05f * noise;
		float voice = 0.f;
		for (int k = 0; k < 3; k++) {
			g->form[k] += (g->ftarget[k] - g->form[k]) * 0.002f;
			voice += reson(buzz, g->form[k], 60.f + 40.f * k, &g->ry1[k], &g->ry2[k]) * (k == 0 ? 1.f : 0.6f);
		}
		voice *= 0.08f * g->syl;                     /* about -18 dB, a sung line at line level */
		/* sine */
		g->sf += (g->sft - g->sf) * 0.0004f;
		g->sph += g->sf / SR; if (g->sph >= 1.f) g->sph -= 1.f;
		float sine = 0.35f * sinf(6.2831853f * g->sph);
		/* the burst: full scale, square and noise */
		float burst = (g->vph < 0.5f ? 1.f : -1.f) * 0.6f + noise * 0.4f;
		float click = (rnext(&g->r) & 2047u) == 0 ? (rfloat(&g->r) < 0.5f ? 0.95f : -0.95f) : 0.f;   /* single-sample clicks: a crackling cable */
		float rumble = 0.f;
		if (g->gdc > 0.f) { g->rph += g->gdc / SR; if (g->rph >= 1.f) g->rph -= 1.f; rumble = 0.6f * sinf(6.2831853f * g->rph); }
		float m = g->gd * drums + g->gv * voice + g->gs * sine + g->gb * burst + g->gc * click + rumble;
		float d = g->gd * 0.1f * hat + g->gv * 0.05f * voice;        /* the right channel a little apart */
		if (m > 1.f) m = 1.f;
		if (m < -1.f) m = -1.f;                                     /* line in can't go past the converter */
		l[i] = m;
		rr[i] = fmaxf(-1.f, fminf(1.f, m - d));
	}
}

/* ------------------------------------------------------------------ the hands */

typedef struct {
	int wingie, nf;
	float f[4], ft[4], fstep[4];
	long key_off[24];                    /* block at which a held key (or T1-T4) is let go; 0 = up */
	long mode_off[2];                    /* a Mode button (or PLAY / the dots) held until */
	int octave[2], source;
	int rocker; long rocker_off;
	/* the firmware's combos: one Mode held, the other tapped (scene) or both held (slot) */
	int combo; long combo_t; int combo_first;
	int want_scene;                      /* +1 / -1: the combo asked for a scene */
	long moves;
} hands;

static void hands_rest(hands *h)
{
	for (int k = 0; k < 4; k++) h->f[k] = h->ft[k] = 0.6f, h->fstep[k] = 0.f;
	memset(h->key_off, 0, sizeof h->key_off);
	memset(h->mode_off, 0, sizeof h->mode_off);
	h->octave[0] = h->octave[1] = 0;
	h->source = 1;
	h->rocker = 0; h->rocker_off = 0;
	h->combo = 0; h->want_scene = 0;
}

/* what the engine sees, block by block */
static void hands_send(hands *h, long b)
{
	for (int k = 0; k < h->nf; k++) {
		if (h->fstep[k] != 0.f) {
			h->f[k] += h->fstep[k];
			if ((h->fstep[k] > 0.f && h->f[k] >= h->ft[k]) || (h->fstep[k] < 0.f && h->f[k] <= h->ft[k])) { h->f[k] = h->ft[k]; h->fstep[k] = 0.f; }
		}
		scq_control(SCQ_CTL_LEAN1 + k, h->f[k]);
	}
	if (h->wingie) {
		int m[2] = { h->mode_off[0] > b, h->mode_off[1] > b };
		int both = m[0] && m[1];
		for (int k = 0; k < 24; k++) {
			int down = h->key_off[k] > b && !both;     /* while both Modes are down the dream sees no keys */
			scq_control((k < 12 ? SCQ_CTL_KEYS1 : SCQ_CTL_KEYS2) + (k % 12), down ? 1.f : 0.f);
		}
		/* a combo: the button held first stays held (at least 0.65 s), the second never shows */
		if (h->combo) {
			scq_control(SCQ_CTL_MODE1, h->combo_first == 0 ? 1.f : 0.f);
			scq_control(SCQ_CTL_MODE2, h->combo_first == 1 ? 1.f : 0.f);
		} else {
			scq_control(SCQ_CTL_MODE1, (float)m[0]);
			scq_control(SCQ_CTL_MODE2, (float)m[1]);
		}
		scq_control(SCQ_CTL_OCTAVE1, (float)h->octave[0]);
		scq_control(SCQ_CTL_OCTAVE2, (float)h->octave[1]);
		scq_control(SCQ_CTL_SOURCE, (float)h->source);
	} else {
		for (int k = 0; k < 4; k++) scq_control(SCQ_CTL_DOOR1 + k, h->key_off[k] > b ? 1.f : 0.f);
		scq_control(SCQ_CTL_WAKE, h->mode_off[0] > b ? 1.f : 0.f);
		int dots = h->mode_off[1] > b;
		scq_control(SCQ_CTL_LUCID, dots ? 1.f : 0.f);
		scq_control(SCQ_CTL_LIFT, (h->rocker_off > b && !dots) ? (float)h->rocker : 0.f);   /* dots + rocker is the scene change, not the rocker */
	}
	if (h->combo && b >= h->combo_t) h->combo = 0;
}

#define BLOCKS(s) ((long)((s) * (float)SR / (float)BLK))

static void say_move(long b, const char *what, int a, float x)
{
	if (verbose) printf("        %7.2f s  %s %d %.2f\n", (double)b * BLK / SR, what, a, (double)x);
}

/* one random move (b: the block it happens at) */
static void hands_move(hands *h, rng *r, long b)
{
	float p = rfloat(r);
	h->moves++;
	if (p < 0.17f) {                                       /* a fader slid */
		int k = rint_(r, h->nf);
		h->ft[k] = rfloat(r);
		float dur = rrange(r, 0.2f, 3.f);
		h->fstep[k] = (h->ft[k] - h->f[k]) / (float)BLOCKS(dur);
		if (h->fstep[k] == 0.f) h->f[k] = h->ft[k];
		say_move(b, "slide", k + 1, h->ft[k]);
	} else if (p < 0.27f) {                                /* a fader thrown */
		int k = rint_(r, h->nf);
		h->f[k] = h->ft[k] = rfloat(r) < 0.5f ? (float)rint_(r, 2) : rfloat(r);
		h->fstep[k] = 0.f;
		say_move(b, "throw", k + 1, h->f[k]);
	} else if (p < 0.47f) {                                /* a key (or a track button) */
		int k = h->wingie ? rint_(r, 24) : rint_(r, 4);
		h->key_off[k] = b + BLOCKS(rfloat(r) < 0.4f ? rrange(r, 0.04f, 0.15f) : rrange(r, 0.2f, 2.f));
		say_move(b, "key", k + 1, 0);
	} else if (p < 0.56f) {                                /* a chord, held, on one keyboard */
		int row = h->wingie ? rint_(r, 2) * 12 : 0, n = 2 + rint_(r, 3), span = h->wingie ? 12 : 4;
		long off = b + BLOCKS(rrange(r, 0.4f, 3.f));
		for (int i = 0; i < n; i++) h->key_off[row + rint_(r, span)] = off;
		say_move(b, "chord", n, 0);
	} else if (p < 0.62f) {                                /* everything let go */
		for (int k = 0; k < 24; k++) if (h->key_off[k] > b) h->key_off[k] = b;
		say_move(b, "let go", 0, 0);
	} else if (p < 0.69f) {
		if (h->wingie) { int k = rint_(r, 2); h->octave[k] = rint_(r, 3) - 1; say_move(b, "octave", k + 1, (float)h->octave[k]); }
		else { h->rocker = rint_(r, 2) ? 1 : -1; h->rocker_off = b + BLOCKS(rrange(r, 0.05f, 1.f)); say_move(b, "rocker", h->rocker, 0); }
	} else if (p < 0.72f) {
		if (h->wingie) { h->source ^= 1; say_move(b, "source", h->source, 0); }
		else { h->mode_off[1] = b + BLOCKS(rrange(r, 0.1f, 1.5f)); say_move(b, "dots", 0, 0); }
	} else if (p < 0.82f) {                                /* a Mode button (PLAY) tapped */
		int k = h->wingie ? rint_(r, 2) : 0;
		if (h->mode_off[k ^ 1] <= b) { h->mode_off[k] = b + BLOCKS(rrange(r, 0.04f, 0.12f)); say_move(b, "tap", k + 1, 0); }
	} else if (p < 0.91f) {                                /* held */
		int k = h->wingie ? rint_(r, 2) : 0;
		if (h->mode_off[k ^ 1] <= b) { h->mode_off[k] = b + BLOCKS(rrange(r, 0.3f, 3.f)); say_move(b, "hold", k + 1, 0); }
	} else if (p < 0.95f && h->wingie && !h->combo) {     /* one held, the other tapped: a scene */
		int first = rint_(r, 2);
		h->combo = 1; h->combo_first = first; h->combo_t = b + BLOCKS(0.7f);
		h->mode_off[first] = b + BLOCKS(0.4f);
		h->mode_off[first ^ 1] = b + BLOCKS(0.25f);
		h->want_scene = first == 0 ? +1 : -1;              /* mode1 held + mode2 tap: next */
		say_move(b, "combo scene", h->want_scene, 0);
	} else if (p < 0.97f && h->wingie && !h->combo) {     /* both held and a bottom key: a slot (the firmware's; the dream sees nothing) */
		int first = rint_(r, 2);
		h->combo = 1; h->combo_first = first; h->combo_t = b + BLOCKS(0.9f);
		h->mode_off[0] = h->mode_off[1] = b + BLOCKS(0.6f);
		h->key_off[rint_(r, 12)] = b + BLOCKS(0.2f);
		say_move(b, "combo slot", 0, 0);
	} else if (!h->wingie && p < 0.97f) {                 /* the dots and the rocker: a scene */
		h->mode_off[1] = b + BLOCKS(0.5f);
		h->rocker = rint_(r, 2) ? 1 : -1; h->rocker_off = b + BLOCKS(0.2f);
		h->want_scene = h->rocker;
		say_move(b, "dots + rocker", h->rocker, 0);
	} else {                                               /* every fader thrown at once */
		for (int k = 0; k < h->nf; k++) { h->f[k] = h->ft[k] = (float)rint_(r, 2); h->fstep[k] = 0.f; }
		say_move(b, "all faders thrown", 0, 0);
	}
}

/* ------------------------------------------------------------------ listening */

typedef struct {
	double sum, dc; long n; float peak; long hot;   /* hot: samples at the output stage's ceiling */
	long bad;                                    /* output samples or outlets not a number */
} ear;

static int nodes;
static char node_place[MAXN][24], node_name[MAXN][32];
static float node_max[MAXN];
static int node_bad[MAXN];

static void read_nodes(void)
{
	nodes = scq_node_count();
	if (nodes > MAXN) nodes = MAXN;
	for (int k = 0; k < MAXN; k++) { node_place[k][0] = node_name[k][0] = 0; }
	const char *d = scq_describe();
	while (*d) {
		int id; char place[24], name[32];
		if (d[0] == 'N' && sscanf(d, "N %d %23s %*s %31s", &id, place, name) == 3 && id >= 0 && id < MAXN) {
			snprintf(node_place[id], sizeof node_place[id], "%s", place);
			snprintf(node_name[id], sizeof node_name[id], "%s", name);
		}
		const char *e = strchr(d, '\n');
		if (!e) break;
		d = e + 1;
	}
}

static void hear(ear *e, const float *l, const float *r, int n)
{
	for (int i = 0; i < n; i++) {
		if (!isfinite(l[i]) || !isfinite(r[i])) { e->bad++; continue; }
		float a = fmaxf(fabsf(l[i]), fabsf(r[i]));
		if (a > e->peak) e->peak = a;
		if (a > 0.97f) e->hot++;
		e->sum += (double)l[i] * l[i] + (double)r[i] * r[i];
		e->dc += 0.5 * ((double)l[i] + r[i]);
	}
	e->n += n;
	for (int k = 0; k < nodes; k++)
		for (int p = 0; p < 4; p++) {
			float v = scq_probe(k, p);
			if (!isfinite(v)) { node_bad[k]++; e->bad++; }
			else if (fabsf(v) > node_max[k]) node_max[k] = fabsf(v);
		}
}

static float db(const ear *e) { double m = e->n ? e->sum / (2.0 * (double)e->n) : 0; return m > 1e-14 ? (float)(10.0 * log10(m)) : -140.f; }
static float rms(const ear *e) { return e->n ? (float)sqrt(e->sum / (2.0 * (double)e->n)) : 0.f; }

/* ------------------------------------------------------------------ one dream */

static void forget(void)
{
	static const char nothing[] = "the nobody is value 0\nnobody -> out\n";
	scq_load(nothing, (int)sizeof nothing - 1, 0);
	for (int k = 0; k < SCQ_CTL_COUNT; k++) scq_control(k, 0.f);
	scq_input(0, 0);
	float l[BLK], r[BLK];
	scq_render(l, r, BLK);
}

typedef struct { int scenes, fails, warns; } tally;

static int scene_titles(const char *text, char titles[SCQ_MAX_SCENES][64])
{
	int n = 0;
	const char *p = text, *prev = 0; int prevlen = 0;
	while (*p) {
		const char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		int dashes = len >= 3;
		for (int i = 0; i < len; i++) if (p[i] != '-' && p[i] != '\r') { dashes = 0; break; }
		if (dashes && prev && prevlen > 0 && n < SCQ_MAX_SCENES) {
			int k = prevlen < 63 ? prevlen : 63;
			memcpy(titles[n], prev, (size_t)k); titles[n][k] = 0;
			n++;
		}
		prev = p; prevlen = len;
		if (!e) break;
		p = e + 1;
	}
	return n;
}

/* render with the hands and the line in, `blocks` long */
typedef struct { linein *g; hands *h; rng *r; long *b; int storm; float move_rate; } play;

static ear *in_ear;                     /* when set, the line in is listened to as well */

static int render_blocks(play *pl, long blocks, ear *e, int scene, int *visits, unsigned *slots)
{
	float il[BLK], ir[BLK], l[BLK], r[BLK];
	int back_at = -1; long back_b = 0;
	for (long k = 0; k < blocks; k++, (*pl->b)++) {
		long b = *pl->b;
		if (pl->storm && rfloat(pl->r) < pl->move_rate) hands_move(pl->h, pl->r, b);
		if (pl->h->want_scene) {                       /* the firmware's combo: the next / previous scene */
			scq_goto_scene(scq_scene() + pl->h->want_scene);
			pl->h->want_scene = 0;
		}
		hands_send(pl->h, b);
		in_render(pl->g, il, ir, BLK);
		if (in_ear) for (int i = 0; i < BLK; i++) { in_ear->sum += (double)il[i] * il[i] + (double)ir[i] * ir[i]; in_ear->n++; }
		scq_input(il, ir);
		scq_render(l, r, BLK);
		scq_input(0, 0);
		int s, st;
		if (scq_wanted_slot(&s, &st)) (*slots)++;
		hear(e, l, r, BLK);
		if (verbose > 1) {                        /* -vv: the output, a line every quarter second */
			static double acc; static long cnt;
			for (int i = 0; i < BLK; i++) acc += (double)l[i] * l[i];
			if (++cnt == BLOCKS(0.25f)) {
				printf("        %7.2f s  out %6.1f dB  faders %.2f %.2f %.2f %.2f  modes %.0f %.0f  octaves %+.0f %+.0f  keys ", (double)b * BLK / SR,
				       10.0 * log10(acc / (double)(cnt * BLK) + 1e-14), (double)scq_get_control(SCQ_CTL_LEAN1), (double)scq_get_control(SCQ_CTL_LEAN2),
				       (double)scq_get_control(SCQ_CTL_LEAN3), (double)scq_get_control(SCQ_CTL_LEAN4), (double)scq_get_control(SCQ_CTL_MODE1),
				       (double)scq_get_control(SCQ_CTL_MODE2), (double)scq_get_control(SCQ_CTL_OCTAVE1), (double)scq_get_control(SCQ_CTL_OCTAVE2));
				for (int k = 0; k < 24; k++) putchar(scq_get_control(SCQ_CTL_KEYS1 + k) > 0.5f ? (k < 12 ? 'a' + k : 'A' + k - 12) : '.');
				printf("  scene %d", scq_scene());
				if (verbose > 2) for (int k = 0; k < nodes && k < 4; k++) printf("  %s %.3g", node_name[k], (double)scq_probe(k, 0));
				printf("\n");
				acc = 0; cnt = 0;
			}
		}
		/* somewhere else (the dream's own scene place, or a combo): stay a second, then come back */
		if (scq_scene() != scene) {
			if (back_at < 0) { back_at = scq_scene(); back_b = b + BLOCKS(1.0f); (*visits)++; read_nodes(); }
			else if (b >= back_b) { scq_goto_scene(scene); }
		} else if (back_at >= 0) { back_at = -1; read_nodes(); }
	}
	/* make sure we end where we began */
	for (int k = 0; k < 200 && scq_scene() != scene; k++) {
		if (k == 0) scq_goto_scene(scene);
		hands_send(pl->h, *pl->b);
		scq_render(l, r, BLK);
		hear(e, l, r, BLK);
	}
	read_nodes();
	return 0;
}

static void one_scene(const char *file, const char *text, int len, int scene, int nsc, const char *title, int wingie, float seconds, tally *T)
{
	char base[64];
	const char *slash = strrchr(file, '/');
	snprintf(base, sizeof base, "%s", slash ? slash + 1 : file);
	char *dot = strstr(base, ".dream"); if (dot) *dot = 0;

	forget();
	scq_load(text, len, 0);
	float il[BLK], ir[BLK], l[BLK], r[BLK];
	scq_goto_scene(scene);
	for (int k = 0; k < 200 && scq_scene() != scene; k++) { scq_render(l, r, BLK); }
	uint32_t used, total;
	scq_memory(&used, &total);
	float load = scq_scene_load(scene);
	read_nodes();
	memset(node_max, 0, sizeof node_max);
	memset(node_bad, 0, sizeof node_bad);
	const uint32_t healed0 = scq_healed();

	linein g; hands h; rng r_;
	uint32_t seed = hash_str(file) ^ (uint32_t)(scene * 7919 + 1) ^ (uint32_t)round_ * 0x9e3779b9u;
	in_init(&g, seed);
	r_.s = seed * 2654435761u + 12345u;
	memset(&h, 0, sizeof h);
	h.wingie = wingie; h.nf = wingie ? 3 : 4;
	hands_rest(&h);
	long b = 0;
	int visits = 0; unsigned slots = 0;
	play pl = { &g, &h, &r_, &b, 0, 0.f };

	/* A: the reference */
	ear skip = { 0 }, A = { 0 }, B = { 0 }, burst = { 0 }, C = { 0 }, D = { 0 };
	in_mix(&g, IN_DRUMVOICE);
	render_blocks(&pl, BLOCKS(1.f), &skip, scene, &visits, &slots);
	ear IN = { 0 };
	in_ear = &IN;
	render_blocks(&pl, BLOCKS(3.f), &A, scene, &visits, &slots);
	in_ear = 0;

	/* B: the storm */
	float storm = seconds - 11.f;
	if (storm < 3.f) storm = 3.f;
	long storm_b = BLOCKS(storm), burst_at = storm_b * 2 / 5, burst_len = BLOCKS(0.5f);
	pl.storm = 1; pl.move_rate = 1.f / (float)BLOCKS(0.35f);    /* a move every 0.35 s or so */
	long done = 0;
	while (done < storm_b) {
		long seg;
		if (done < burst_at && done + BLOCKS(3.f) > burst_at) {   /* the burst comes when it comes */
			seg = burst_at - done;
			if (seg > 0) { in_mix(&g, rint_(&r_, IN_BURST)); render_blocks(&pl, seg, &B, scene, &visits, &slots); done += seg; }
			in_mix(&g, IN_BURST);
			if (verbose) printf("        %7.2f s  BURST\n", (double)b * BLK / SR);
			render_blocks(&pl, burst_len, &B, scene, &visits, &slots); done += burst_len;
			in_mix(&g, IN_DRUMVOICE);
			render_blocks(&pl, BLOCKS(2.f), &burst, scene, &visits, &slots); done += BLOCKS(2.f);
			continue;
		}
		int kind = rint_(&r_, IN_BURST);
		seg = BLOCKS(rrange(&r_, 0.4f, 3.f));
		if (seg > storm_b - done) seg = storm_b - done;
		in_mix(&g, kind);
		if (verbose) printf("        %7.2f s  line in: %s\n", (double)b * BLK / SR, IN_NAME[kind]);
		render_blocks(&pl, seg, &B, scene, &visits, &slots);
		done += seg;
	}

	/* C: silence in, hands at rest; D: the drums and the voice again */
	pl.storm = 0;
	hands_rest(&h);
	in_mix(&g, IN_SILENCE);
	ear cskip = { 0 };
	render_blocks(&pl, BLOCKS(1.f), &cskip, scene, &visits, &slots);
	render_blocks(&pl, BLOCKS(2.f), &C, scene, &visits, &slots);
	in_mix(&g, IN_DRUMVOICE);
	render_blocks(&pl, BLOCKS(1.f), &cskip, scene, &visits, &slots);
	render_blocks(&pl, BLOCKS(3.f), &D, scene, &visits, &slots);
	(void)il; (void)ir;

	unsigned healed = (unsigned)(scq_healed() - healed0);
	char rtag[16] = "";
	if (round_) snprintf(rtag, sizeof rtag, " r%d", round_);
	long bad = skip.bad + A.bad + B.bad + burst.bad + C.bad + cskip.bad + D.bad;
	char why[512] = "", warn[512] = "";
	int failed = 0, warned = 0;
	if (bad) {
		int worst = -1;
		for (int k = 0; k < nodes; k++) if (node_bad[k] && (worst < 0 || node_bad[k] > node_bad[worst])) worst = k;
		snprintf(why + strlen(why), sizeof why - strlen(why), " NOT A NUMBER %ld times%s%s%s", bad,
		         worst >= 0 ? " (first in " : "", worst >= 0 ? node_place[worst] : "", worst >= 0 ? ")" : "");
		failed = 1;
	}
	/* pinned: more than half the samples against the output stage's ceiling with nothing coming in
	 * (a loud drone is not pinned: a full sine sits at 0.64 rms and touches the ceiling rarely) */
	if (C.n && (float)C.hot / (float)C.n > 0.5f) { snprintf(why + strlen(why), sizeof why - strlen(why), " pinned with silence in (%.0f%% at the ceiling, %.2f rms)", 100.0 * C.hot / C.n, (double)rms(&C)); failed = 1; }
	else if (rms(&C) > 0.3f && rms(&C) > 2.f * rms(&A)) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " grows with silence in (%.2f rms, %.2f with the line in)", (double)rms(&C), (double)rms(&A)); warned = 1; }
	if (C.n && fabs(C.dc / (double)C.n) > 0.1) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " DC at the output (%.2f with silence in)", C.dc / (double)C.n); warned = 1; }
	if (rms(&A) > 1e-3f && rms(&D) < 1e-5f) { snprintf(why + strlen(why), sizeof why - strlen(why), " silent after the storm (A %.1f dB, D %.1f dB)", (double)db(&A), (double)db(&D)); failed = 1; }
	else if (rms(&A) > 1e-3f && rms(&D) < rms(&A) / 30.f) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " much quieter after (A %.1f dB, D %.1f dB)", (double)db(&A), (double)db(&D)); warned = 1; }
	if (rms(&A) < 1e-4f && rms(&D) < 1e-4f && rms(&B) < 1e-4f) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " never sounds"); warned = 1; }
	if (burst.n && (float)burst.hot / (float)burst.n > 0.5f) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " against the ceiling 2 s after the burst (%.0f%%)", 100.0 * burst.hot / burst.n); warned = 1; }
	if (B.n && (float)B.hot / (float)B.n > 0.2f) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " storm %.0f%% against the ceiling", 100.0 * B.hot / B.n); warned = 1; }
	if (healed) {
		int worst = 0;
		for (int k = 1; k < nodes; k++) if (node_max[k] > node_max[worst]) worst = k;
		snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " %u places started again (largest outlet: %s '%s' %.3g)", healed, node_place[worst], node_name[worst], (double)node_max[worst]);
		warned = 1;
	}
	{
		int worst = -1;
		for (int k = 0; k < nodes; k++) if (node_max[k] > 1000.f && (worst < 0 || node_max[k] > node_max[worst])) worst = k;
		if (worst >= 0 && !healed) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " outlet %s '%s' reached %.3g", node_place[worst], node_name[worst], (double)node_max[worst]); warned = 1; }
	}
	if (!wingie && load > 1.f) { snprintf(warn + strlen(warn), sizeof warn - strlen(warn), " SP-1 load %.2f", (double)load); warned = 1; }
	if (wingie && (load > 0.85f || used > total)) { snprintf(why + strlen(why), sizeof why - strlen(why), " too heavy for the Wingie (load %.2f, %u of %u bytes)", (double)load, used, total); failed = 1; }

	printf("%s %-28s %d/%d%-4s %-30.30s load %.2f mem %5.1fK  in %5.1f A %6.1f  storm %6.1f peak %.2f hot %4.1f%%  silence %6.1f  D %6.1f dB  moves %ld scenes+%d%s%s%s%s\n",
	       failed ? "FAIL" : warned ? "warn" : "ok  ", base, scene + 1, nsc, rtag, title, (double)load, used / 1024.0, (double)db(&IN),
	       (double)db(&A), (double)db(&B), (double)B.peak, B.n ? 100.0 * B.hot / B.n : 0.0, (double)db(&C), (double)db(&D),
	       h.moves, visits, slots ? " slot asks" : "", failed ? " --" : "", why, warn);
	fflush(stdout);
	if (failed || verbose) {                          /* where the sound stops: every place's outlets as they stand */
		for (int k = 0; k < nodes; k++)
			printf("        %-14s %-12s %10.4g %10.4g %10.4g %10.4g  (largest %.4g)\n", node_place[k], node_name[k],
			       (double)scq_probe(k, 0), (double)scq_probe(k, 1), (double)scq_probe(k, 2), (double)scq_probe(k, 3), (double)node_max[k]);
	}
	T->scenes++;
	if (failed) T->fails++;
	if (warned) T->warns++;
}

/* the same two seconds of every scene, hands and line in fixed: a fingerprint of the dream */
static uint64_t fingerprint(const char *text, int len, int nsc, uint32_t *mem)
{
	uint64_t h = 1469598103934665603ull;
	float il[BLK], ir[BLK], l[BLK], r[BLK];
	forget();
	scq_load(text, len, 0);
	for (int s = 0; s < nsc; s++) {
		scq_goto_scene(s);
		for (int k = 0; k < 200 && scq_scene() != s; k++) scq_render(l, r, BLK);
		uint32_t used; scq_memory(&used, 0);
		mem[s] = used;
		linein g; in_init(&g, 777u + (uint32_t)s);
		in_mix(&g, IN_ALL);
		for (int k = 0; k < SCQ_CTL_COUNT; k++) scq_control(k, 0.f);
		for (int k = 0; k < 4; k++) scq_control(SCQ_CTL_LEAN1 + k, 0.6f);
		for (long b = 0; b < BLOCKS(2.f); b++) {
			scq_control(SCQ_CTL_KEYS1 + 4, (b / 400) & 1 ? 1.f : 0.f);
			scq_control(SCQ_CTL_DOOR1, (b / 400) & 1 ? 1.f : 0.f);
			in_render(&g, il, ir, BLK);
			scq_input(il, ir);
			scq_render(l, r, BLK);
			scq_input(0, 0);
			const unsigned char *p = (const unsigned char *)l;
			for (size_t i = 0; i < sizeof l; i++) { h ^= p[i]; h *= 1099511628211ull; }
			p = (const unsigned char *)r;
			for (size_t i = 0; i < sizeof r; i++) { h ^= p[i]; h *= 1099511628211ull; }
		}
	}
	return h;
}

static char *slurp(const char *path, int *len)
{
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = malloc((size_t)n + 1);
	if (!b) { fclose(f); return 0; }
	n = (long)fread(b, 1, (size_t)n, f);
	fclose(f);
	b[n] = 0;
	*len = (int)n;
	return b;
}

static long maxrss(void) { struct rusage u; getrusage(RUSAGE_SELF, &u); return u.ru_maxrss; }

int main(int argc, char **argv)
{
	float seconds = 30.f;
	int reloads = 50, files = 0, rounds = 1, first_round = 0, only_scene = 0;
	tally T = { 0 };
	int dream_fails = 0, dream_warns = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-s") && i + 1 < argc) { seconds = (float)atof(argv[++i]); continue; }
		if (!strcmp(argv[i], "-n") && i + 1 < argc) { reloads = atoi(argv[++i]); continue; }
		if (!strcmp(argv[i], "-R") && i + 1 < argc) { first_round = atoi(argv[++i]); continue; }
		if (!strcmp(argv[i], "-S") && i + 1 < argc) { only_scene = atoi(argv[++i]); continue; }
		if (!strcmp(argv[i], "-r") && i + 1 < argc) { rounds = atoi(argv[++i]); if (rounds < 1) rounds = 1; continue; }
		if (!strcmp(argv[i], "-v")) { verbose = 1; continue; }
		if (!strcmp(argv[i], "-vv")) { verbose = 2; continue; }
		if (!strcmp(argv[i], "-vvv")) { verbose = 3; continue; }
		const char *file = argv[i];
		int len;
		char *text = slurp(file, &len);
		files++;
		if (!text) { printf("FAIL %s: can't read it\n", file); dream_fails++; continue; }
		forget();
		int errors = scq_load(text, len, 0);
		const char *msgs = scq_messages();
		int warnings = 0;
		for (const char *m = msgs; *m; ) { if (m[0] == 'W' && m[1] == ' ') warnings++; const char *e = strchr(m, '\n'); if (!e) break; m = e + 1; }
		if (errors) { printf("FAIL %s: %d mistakes\n%s", file, errors, msgs); dream_fails++; free(text); continue; }
		int wingie = scq_instrument() == SCQ_WINGIE;
		int nsc = scq_scene_count();
		char titles[SCQ_MAX_SCENES][64];
		int nt = scene_titles(text, titles);
		for (int s = nt; s < nsc; s++) snprintf(titles[s], sizeof titles[s], "(scene %d)", s + 1);
		if (nsc < 1) nsc = 1;
		if (nt == 0 && nsc == 1) snprintf(titles[0], sizeof titles[0], "(one scene)");
		if (warnings) { printf("warn %s: %d warnings from the journal\n%s", file, warnings, msgs); dream_warns++; }

		for (round_ = first_round; round_ < first_round + rounds; round_++)
			for (int s = 0; s < nsc; s++) if (!only_scene || s + 1 == only_scene) one_scene(file, text, len, s, nsc, titles[s], wingie, seconds, &T);

		/* loaded and unloaded again and again: nothing grows, nothing changes */
		uint32_t mem1[SCQ_MAX_SCENES] = { 0 }, mem2[SCQ_MAX_SCENES] = { 0 };
		long rss0 = maxrss();
		uint64_t h1 = fingerprint(text, len, nsc, mem1);
		static const char other[] = "the a is sine 3\nthe b is delay 4\na -> b -> out\n";
		float l[BLK], r[BLK];
		uint32_t rng_s = hash_str(file);
		int memdrift = 0;
		for (int k = 0; k < reloads; k++) {
			scq_load(text, len, 0);
			uint32_t used; scq_memory(&used, 0);
			if (scq_scene() == 0 && used != mem1[0]) memdrift++;
			int blocks = 1 + (int)(rng_s % 40u); rng_s = rng_s * 1664525u + 1013904223u;
			for (int b = 0; b < blocks; b++) scq_render(l, r, BLK);
			if (k & 1) scq_load(other, (int)sizeof other - 1, 0); else forget();
			scq_render(l, r, BLK);
		}
		uint64_t h2 = fingerprint(text, len, nsc, mem2);
		long rss1 = maxrss();
		int memsame = !memcmp(mem1, mem2, sizeof mem1) && !memdrift;
		int same = h1 == h2, grew = rss1 > rss0 + 4096 * 1024;
		/* a dream that leaves things to chance (a maybe cord, scene.miss) draws from the engine's one
		 * chance generator, which scq_load seeds only the first time: it goes on from load to load,
		 * so such a dream is not expected to give the same bytes twice */
		int chance = strstr(text, "scene.miss") != 0;
		for (const char *q = text; *q && !chance; q++)
			if (*q == '?') { const char *e = q + 1; while (*e >= '0' && *e <= '9') e++; if (*e == '>') chance = 1; }
		int bad = !memsame || grew || (!same && !chance);
		printf("%s %-28s reload x%d: %s, %s, memory %s\n", bad ? "FAIL" : same ? "ok  " : "warn", (strrchr(file, '/') ? strrchr(file, '/') + 1 : file),
		       reloads, same ? "same bytes" : chance ? "different bytes (it uses chance: ?> cords or scene.miss)" : "DIFFERENT BYTES",
		       memsame ? "same pool" : "POOL CHANGED", grew ? "GREW" : "steady");
		if (bad) dream_fails++;
		else if (!same) dream_warns++;
		fflush(stdout);
		free(text);
	}
	printf("dreamstorm: %d dreams, %d scenes: %d scenes failed, %d warned; %d dream-level failures, %d with journal warnings\n",
	       files, T.scenes, T.fails, T.warns, dream_fails, dream_warns);
	return (T.fails || dream_fails) ? 1 : 0;
}
