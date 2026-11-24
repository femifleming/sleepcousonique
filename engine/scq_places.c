/*
 * The places. Each one has a fixed nature (Nabra: no knobs); you change it only by
 * running something into its inlets. Natures are tuned from the 60 Hz hum.
 *
 * Signals: sound is about -1..1, current is about 0..1, a pulse is 0 or 1.
 * Pitch inlets are in octaves (1.0 = one octave up). Any signal goes into any inlet.
 */
#include "scq_int.h"

world W = { .nb = SCQ_BLOCK, .sr = (float)SCQ_RATE, .isr = 1.f / (float)SCQ_RATE };

/* Place code sees the block and the rate of the place being run: a place at samplerate=half
 * runs half as many samples at half the rate (the engine decimates its inlets and holds its
 * outlets back up to full rate). */
/* Loops stay loops (see scq_core.c): the SP-1's C library clears and copies a byte at a time, and
 * at -O3 the compiler would hand it every `LOOP out = 0` and every block copy in here. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("no-tree-loop-distribute-patterns")
#endif

#undef B
#undef SR
#undef INV_SR
#define B      (W.nb)
#define SR     (W.sr)
#define INV_SR (W.isr)

#define PULSE_SAMPLES 96   /* 2 ms */

static inline int edge(float x, float *prev)
{
	int e = (x > 0.5f && *prev <= 0.5f);
	*prev = x;
	return e;
}

static inline int16_t to16(float x)
{
	x = scq_clamp(x, -1.f, 1.f) * 32767.f;
	return (int16_t)x;
}
#define FROM16(v) ((float)(v) * (1.f / 32767.f))

/* A sample count that runs to 2^30 (the reverbs' write position), kept in two state floats.
 * One float holds whole numbers only to 2^24: past that the count lost its low bits, and at
 * 2^29 (about three hours of sound) adding a block rounded back to where it was, so the place
 * wrote the same 32 samples for ever. */
static inline uint32_t count_get(const float *lo, const float *hi) { return (uint32_t)*lo | ((uint32_t)*hi << 16); }
static inline void count_put(uint32_t w, float *lo, float *hi) { *lo = (float)(w & 0xffffu); *hi = (float)(w >> 16); }

static inline int natural(float nature, int lo, int hi)
{
	int n = (int)(nature + 0.5f);
	return n < lo ? lo : (n > hi ? hi : n);
}

/* ------------------------------------------------------------------ sources */

/* sine — the fountain labeled aira: a pure tone at nature x 60 Hz */
static void run_sine(node *n, const float *const in[MAX_IN])
{
	float base = 60.f * n->nature * scq_exp2(in[0][0]) * (n->opt[0] != 0.f ? 0.01f : 1.f), ph = n->s[0];   /* pitch per block; range=audio|lfo */
	if (!(n->patched & 2u)) {                    /* no fm: the bare sine */
		const float dt = base * INV_SR;
		LOOP { n->out[0][i] = scq_sin2pi(ph); ph += dt; ph -= scq_floor(ph); }
		n->s[0] = ph;
		return;
	}
	LOOP {
		float f = base * (1.f + in[1][i]);
		ph += f * INV_SR;
		ph -= scq_floor(ph);
		n->out[0][i] = scq_sin2pi(ph);
	}
	n->s[0] = ph;
}

/* crossfm — two partials, 60n and 60(n+1): their difference is always the hum */
static void run_crossfm(node *n, const float *const in[MAX_IN])
{
	float pa = n->s[0], pb = n->s[1];
	float k = scq_exp2(in[0][0]);                                /* pitch per block */
	float ia = 60.f * n->nature * k * INV_SR, ib = 60.f * (n->nature + 1.f) * k * INV_SR;
	LOOP {
		pb += ib; pb -= scq_floor(pb);
		float b = scq_sin2pi(pb);
		pa += ia; pa -= scq_floor(pa);
		float a = scq_sin2pi(pa + in[1][i] * b * 0.5f);
		a = scq_sin2pi(0.25f * a * (1.f + scq_max(in[2][i], 0.f) * 7.f));
		n->out[0][i] = 0.5f * (a + b);
		n->out[1][i] = a;
		n->out[2][i] = b;
	}
	n->s[0] = pa; n->s[1] = pb;
}

/* rain — drops: random strikes on tiny resonators. nature = drops per second / 8 */
static void run_rain(node *n, const float *const in[MAX_IN])
{
	/* s: 0 y1, 1 y2, 2 c1, 3 c2, 4 pulse timer */
	float y1 = n->s[0], y2 = n->s[1], c1 = n->s[2], c2 = n->s[3], pt = n->s[4];
	const float chance = 8.f * n->nature * (1.f + scq_max(in[0][0], 0.f) * 8.f) * INV_SR;
	LOOP {
		float x = 0.f;
		if (scq_rand01(&n->rng) < chance) {
			float f = 1800.f + 4200.f * scq_rand01(&n->rng);
			float r = 0.995f;
			c1 = 2.f * r * scq_sin2pi(f * INV_SR + 0.25f);
			c2 = -r * r;
			x = (0.2f + 0.8f * scq_rand01(&n->rng)) * 0.15f;
			pt = PULSE_SAMPLES;
		}
		float y = x + c1 * y1 + c2 * y2;
		y2 = y1; y1 = y;
		n->out[0][i] = y;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = y1; n->s[1] = y2; n->s[2] = c1; n->s[3] = c2; n->s[4] = pt;
}

/* thunder — rare rumbles, or when struck. nature = how often (per minute) */
static void run_thunder(node *n, const float *const in[MAX_IN])
{
	float env = n->s[0], lp = n->s[1], lp2 = n->s[2];
	float decay = 1.f - 1.f / (2.5f * SR);
	LOOP {
		int strike = edge(in[0][i], &n->s[3]);
		if (scq_rand01(&n->rng) < n->nature / (60.f * SR)) strike = 1;
		if (strike) env = 1.f;
		float w = scq_noise(&n->rng);
		lp += 0.02f * (w - lp);
		lp2 += 0.02f * (lp - lp2);
		float crack = w * env * env * env * env * env * env * env * env * 0.3f;
		n->out[0][i] = lp2 * env * 6.f + crack;
		env *= decay;
	}
	n->s[0] = env; n->s[1] = lp; n->s[2] = lp2;
}

/* noisesweep — water rising: noise through a resonant lowpass that climbs with the level */
static void run_noisesweep(node *n, const float *const in[MAX_IN])
{
	float ic1 = n->s[0], ic2 = n->s[1], wander = n->s[2], wt = n->s[3];
	float level = scq_clamp(in[0][0], 0.f, 1.f);
	if (scq_rand01(&n->rng) < 0.002f) wt = scq_rand01(&n->rng);
	wander += 0.001f * (wt - wander);
	float fc = scq_min(80.f * n->nature * scq_exp2(level * 6.f + wander * 1.5f), 12000.f);
	float g = scq_tan(SCQ_PI * fc * INV_SR), k = 0.6f;
	float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
	LOOP {
		float x = scq_noise(&n->rng) * scq_clamp(in[0][i], 0.f, 1.f);
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		n->out[0][i] = v2 * 0.8f;
	}
	n->s[0] = ic1; n->s[1] = ic2; n->s[2] = wander; n->s[3] = wt;
}

/* wind — a cold draft: soft noise through a wandering band */
static void run_wind(node *n, const float *const in[MAX_IN])
{
	float ic1 = n->s[0], ic2 = n->s[1], wander = n->s[2], wt = n->s[3], pk = n->s[4];
	if (scq_rand01(&n->rng) < 0.003f) wt = scq_rand01(&n->rng);
	wander += 0.0015f * (wt - wander);
	float fc = 250.f * n->nature * scq_exp2(wander * 2.5f);
	float g = scq_tan(SCQ_PI * scq_min(fc, 10000.f) * INV_SR), k = 0.5f;
	float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
	LOOP {
		float w = scq_noise(&n->rng);
		pk += 0.3f * (w - pk);
		float x = pk * (0.25f + scq_max(in[0][i], 0.f));
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		n->out[0][i] = v1 * 1.5f;
	}
	n->s[0] = ic1; n->s[1] = ic2; n->s[2] = wander; n->s[3] = wt; n->s[4] = pk;
}

/* ------------------------------------------------------------------ shapes */

/* folder — dark umber peaks: a sine wavefolder */
static void run_folder(node *n, const float *const in[MAX_IN])
{
	float base = 1.f + 0.5f * (n->nature - 1.f);
	LOOP {
		float g = base + scq_max(in[1][i], 0.f) * 8.f;
		n->out[0][i] = scq_sin2pi(0.25f * in[0][i] * g);
	}
}

/* lpg — a lamp shining on a photocell (the lowpass gate) */
/* run_lpg: in scq_more_classic.inc (shared with dusg / duallpg) */

/* lowpass — the house show downstairs: a resonant lowpass, warm */
static void run_lowpass(node *n, const float *const in[MAX_IN])
{
	float ic1 = n->s[0], ic2 = n->s[1];
	float fc = scq_clamp(480.f * n->nature * scq_exp2(in[1][0]), 20.f, 16000.f);
	float g = scq_tan(SCQ_PI * fc * INV_SR);
	float k = 2.f - 1.96f * scq_clamp(in[2][0], 0.f, 1.f);
	float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
	LOOP {
		float x = scq_tanh(in[0][i]);
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		n->out[0][i] = v2;
		n->out[1][i] = v1;
	}
	n->s[0] = ic1; n->s[1] = ic2;
}

/* filterbank — horn-loaded speakers: eight fixed resonant bands (Serge's resonant EQ) */
static const float HORN_HZ[8] = { 61.f, 115.f, 218.f, 411.f, 777.f, 1470.f, 2800.f, 5200.f };
static uint32_t mem_filterbank(float nature) { (void)nature; return 8 * 5 * sizeof(float); }
static void init_filterbank(node *n)
{
	float *m = (float *)n->mem;
	for (int b = 0; b < 8; b++) {
		float fc = scq_min(HORN_HZ[b] * n->nature, 15000.f);
		float g = scq_tan(SCQ_PI * fc * INV_SR), k = 0.2f;
		m[b * 5 + 0] = 1.f / (1.f + g * (g + k));
		m[b * 5 + 1] = g * m[b * 5 + 0];
		m[b * 5 + 2] = g * m[b * 5 + 1];
	}
}
static void run_filterbank(node *n, const float *const in[MAX_IN])
{
	float *m = (float *)n->mem;
	LOOP n->out[0][i] = in[0][i];
	for (int b = 0; b < 8; b++) {
		if (!(n->patched & (1u << (b + 1)))) continue;   /* an empty band is flat */
		float *f = m + b * 5, ic1 = f[3], ic2 = f[4];
		LOOP {
			float v3 = in[0][i] - ic2;
			float v1 = f[0] * ic1 + f[1] * v3;
			float v2 = ic2 + f[1] * ic1 + f[2] * v3;
			ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
			n->out[0][i] += in[b + 1][i] * v1 * 0.4f;
		}
		f[3] = ic1; f[4] = ic2;
	}
}

/* saturator — the lighter that explodes: a flick of fire and drive */
static void run_saturator(node *n, const float *const in[MAX_IN])
{
	float flame = n->s[0];
	float decay = 1.f - 1.f / (0.4f * SR);
	LOOP {
		if (edge(in[1][i], &n->s[1])) flame = 1.f;
		float drive = 1.f + (n->nature - 1.f) * 2.f + flame * 20.f;
		n->out[0][i] = scq_tanh(in[0][i] * drive) + scq_noise(&n->rng) * flame * flame * 0.3f;
		flame *= decay;
	}
	n->s[0] = flame;
}

/* dcblock — the grounding rod: blocks DC, lets the music through */
static void run_dcblock(node *n, const float *const in[MAX_IN])
{
	float lp = n->s[0], c = scq_coef_hz(20.f * n->nature, SR);
	LOOP {
		lp += c * (in[0][i] - lp);
		n->out[0][i] = in[0][i] - lp;
	}
	n->s[0] = lp;
}

/* vca — puts the fire out: a plain amplifier */
static void run_vca(node *n, const float *const in[MAX_IN])
{
	if (n->opt[0] != 0.f) LOOP { float g = scq_max(in[1][i], 0.f); n->out[0][i] = in[0][i] * g * g * g; }   /* response=exp */
	else LOOP n->out[0][i] = in[0][i] * in[1][i];
}

/* matrix — the crystal matrix: four in, four out, turned by an angle */
static void run_matrix(node *n, const float *const in[MAX_IN])
{
	float th = n->nature * 0.0625f + in[4][0] * 0.25f;        /* in turns */
	float c = scq_sin2pi(th + 0.25f), s = scq_sin2pi(th);
	LOOP {
		float a = in[0][i], b = in[1][i], d = in[2][i], e = in[3][i];
		float a1 = c * a - s * b, b1 = s * a + c * b;
		float d1 = c * d - s * e, e1 = s * d + c * e;
		n->out[0][i] = c * a1 - s * d1;
		n->out[2][i] = s * a1 + c * d1;
		n->out[1][i] = c * b1 - s * e1;
		n->out[3][i] = s * b1 + c * e1;
	}
}

/* ------------------------------------------------------------------ spaces */

/* comb — tubular architecture, tubular acoustics: a tuned comb */
static uint32_t mem_comb(float nature) { (void)nature; return 2048 * sizeof(float); }
static void run_comb(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float lp = n->s[1];
	float f = 60.f * n->nature * scq_exp2(in[1][0]);
	float period = scq_clamp(SR / scq_max(f, 1.f), 2.f, 2000.f);
	float fb = 0.8f + 0.199f * scq_clamp(in[2][0], 0.f, 1.f);
	LOOP {
		float rp = (float)w - period;
		if (rp < 0.f) rp += 2048.f;
		uint32_t r0 = (uint32_t)rp;
		float fr = rp - (float)r0;
		float y = buf[r0 & 2047] + (buf[(r0 + 1) & 2047] - buf[r0 & 2047]) * fr;
		lp += 0.5f * (y - lp);
		float v = in[0][i] + fb * lp;
		buf[w] = scq_tanh(v);
		w = (w + 1) & 2047;
		n->out[0][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = lp;
}

/* delay — the long grey hallway: a delay. nature x 50 ms, up to a second */
static uint32_t mem_delay(float nature) { return (uint32_t)natural(nature, 1, 20) * 2400 * sizeof(int16_t); }
static void run_delay(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t L = n->memlen / sizeof(int16_t);
	uint32_t w = (uint32_t)n->s[0];
	float len = n->s[1];
	float c = 1.f / (0.08f * SR);
	LOOP {
		len += (scq_clamp(in[1][i], 0.01f, 1.f) - len) * c;
		float d = 1.f + len * (float)(L - 3);
		float rp = (float)w - d;
		if (rp < 0.f) rp += (float)L;
		uint32_t r0 = (uint32_t)rp, r1 = r0 + 1 < L ? r0 + 1 : 0;
		float fr = rp - (float)r0;
		float y = FROM16(buf[r0]) + (FROM16(buf[r1]) - FROM16(buf[r0])) * fr;
		buf[w] = to16(scq_tanh(in[0][i] + y * scq_clamp(in[2][i], 0.f, 0.98f)));
		if (++w >= L) w = 0;
		n->out[0][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = len;
}
static void init_delay(node *n) { n->s[1] = 1.f; }

/* reverb — a big room with wooden floors: four-line feedback reverb */
static const uint16_t WH_LEN[4] = { 1447, 1811, 2137, 2551 };
static uint32_t wh_len(float nature, int k)
{
	float sc = 0.6f + 0.2f * scq_clamp(nature, 1.f, 4.f);
	return (uint32_t)((float)WH_LEN[k] * sc);
}
static uint32_t mem_reverb(float nature)
{
	uint32_t t = 0;
	for (int k = 0; k < 4; k++) t += wh_len(nature, k);
	return t * sizeof(int16_t);
}
static void run_reverb(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem;
	int16_t *line[4];
	uint32_t len[4], off = 0;
	for (int k = 0; k < 4; k++) { len[k] = wh_len(n->nature, k); line[k] = base + off; off += len[k]; }
	float g = 0.5f * (0.55f + 0.44f * scq_clamp(in[1][0], 0.f, 1.f));
	uint32_t w = count_get(&n->s[0], &n->s[5]);
	float *damp = &n->s[1];
	LOOP {
		float o[4];
		for (int k = 0; k < 4; k++) {
			uint32_t p = w % len[k];
			float y = FROM16(line[k][p]);
			damp[k] += 0.45f * (y - damp[k]);
			o[k] = damp[k];
		}
		float h0 = o[0] + o[1] + o[2] + o[3], h1 = o[0] - o[1] + o[2] - o[3];
		float h2 = o[0] + o[1] - o[2] - o[3], h3 = o[0] - o[1] - o[2] + o[3];
		float x = in[0][i] * 0.5f;
		line[0][w % len[0]] = to16(x + g * h0);
		line[1][w % len[1]] = to16(x + g * h1);
		line[2][w % len[2]] = to16(g * h2 - x);
		line[3][w % len[3]] = to16(g * h3);
		w++;
		if (w >= 1u << 30) w = 0;
		n->out[1][i] = o[0] + o[2];
		n->out[2][i] = o[1] + o[3];
		n->out[0][i] = 0.5f * (n->out[1][i] + n->out[2][i]);
	}
	count_put(w, &n->s[0], &n->s[5]);
}

/* One mode of a struck thing (the bell, the marimba): a point that turns by the mode's
 * frequency and shrinks by its decay every sample; what you hear is how high it stands.
 *
 * It used to be the textbook two-pole resonator, y = g x + c1 y1 + c2 y2. That one keeps the
 * last two heights, and what two heights mean depends on the frequency: the same pair that is a
 * quiet ripple at a high pitch is the start of an enormous swing at a low one (by 1 / sin of the
 * angle, tens of thousands at a few hertz). So a pitch thrown down over many octaves while the
 * mode still rang (a fader through att.pitch=10) multiplied what was ringing, every time, until
 * the outlet was thousands of times full scale: the output stage clipped it flat (silence), it
 * took minutes to die away, and past the largest float it became not-a-number for good. A turn
 * keeps its size whatever the angle becomes, so here nothing can grow but what is struck in.
 * Left alone (a pitch that doesn't move) the two are the same filter: the same poles (the angle
 * is taken from the same cosine) and the same response to a strike. */
typedef struct { float rc, rs, gc, gs; } mode_c;
static inline mode_c mode_make(float f, float r, float amp)
{
	mode_c m;
	const float w = f * INV_SR;
	const float c = scq_sin2pi(w + 0.25f), sn = scq_sin2pi(w);
	float s = scq_sqrt_from(1.f - c * c, scq_abs(sn));    /* the sine that belongs to that cosine */
	if (s < 1e-4f) s = 1e-4f;                              /* (below a hertz or so: still a turn) */
	const float g = sn * amp / s;                          /* a strike of 1 rings to amp, as before */
	m.rc = r * c; m.rs = r * s; m.gc = g * c; m.gs = g * s;
	return m;
}
/* re, im: the point; x: the strike. Returns the height. */
static inline float mode_run(const mode_c *m, float *re, float *im, float x)
{
	const float a = *re, b = *im;
	*re = m->rc * a - m->rs * b + m->gc * x;
	return *im = m->rs * a + m->rc * b + m->gs * x;
}

/* bell — the cargo train's deep bell: four struck modes */
static const float BELL_R[4] = { 1.f, 2.76f, 5.40f, 8.93f };
static const float BELL_A[4] = { 1.f, 0.6f, 0.4f, 0.25f };
static void run_bell(node *n, const float *const in[MAX_IN])
{
	/* s: 0..3 height, 4..7 the other half of each point, 8 prev strike */
	float f0 = 120.f * n->nature * scq_exp2(in[1][0]);
	float T = 0.3f + 4.f * scq_clamp(in[2][0], 0.f, 1.f);
	mode_c m[4];
	for (int k = 0; k < 4; k++) {
		float f = scq_min(f0 * BELL_R[k], 18000.f);
		float r = scq_exp2(-1.442695f / (T / (1.f + (float)k * 0.7f) * SR));
		m[k] = mode_make(f, r, 0.3f * BELL_A[k]);
	}
	LOOP {
		float x = in[0][i] - n->s[8];
		n->s[8] = in[0][i];
		float sum = 0.f;
		for (int k = 0; k < 4; k++) sum += mode_run(&m[k], &n->s[4 + k], &n->s[k], x);
		n->out[0][i] = sum;
	}
}

/* shepard — the lift into the night sky: a tone that rises forever */
static void run_shepard(node *n, const float *const in[MAX_IN])
{
	/* s: 0 phase of the lowest octave, 8 position within the octave. Every partial is an
	 * octave of the lowest, so one sine and cosine give them all by doubling the angle:
	 * sin 2x = 2 sin x cos x, cos 2x = cos^2 x - sin^2 x */
	float pos = n->s[8], ph = n->s[0];
	pos += scq_clamp(in[0][0] * (float)B * INV_SR, -1.f, 1.f);   /* an octave a block at most: one step of the ladder */
	if (pos >= 1.f) { pos -= 1.f; ph = 2.f * ph; ph -= scq_floor(ph); }   /* the ladder steps down an octave */
	if (pos < 0.f)  { pos += 1.f; ph = 0.5f * ph; }
	n->s[8] = pos;
	float k0 = scq_exp2(in[1][0]);
	float inc = 30.f * n->nature * scq_exp2(pos) * k0 * INV_SR;
	float amp[8];
	for (int k = 0; k < 8; k++) {
		float oct = (float)k + pos;
		float s = scq_sin2pi(oct / 16.f);
		amp[k] = (inc * scq_exp2((float)k) < 0.45f) ? s * s * 0.3f : 0.f;
	}
	LOOP {
		ph += inc; ph -= scq_floor(ph);
		float s = scq_sin2pi(ph), c = scq_sin2pi(ph + 0.25f), sum = amp[0] * s;
		for (int k = 1; k < 8; k++) {
			float s2 = 2.f * s * c, c2 = c * c - s * s;
			s = s2; c = c2;
			sum += amp[k] * s;
		}
		n->out[0][i] = sum;
	}
	n->s[0] = ph;
}

/* granular — a golden square of moss in a night pond: it blooms into grains */
#define MOSS_LEN 12000
#define MOSS_G   6
static uint32_t mem_granular(float nature) { (void)nature; return MOSS_LEN * sizeof(int16_t) + MOSS_G * 4 * sizeof(float); }
static void init_granular(node *n)
{
	int16_t *buf = (int16_t *)n->mem;
	float f = 60.f * n->nature * 4.f;
	for (int i = 0; i < MOSS_LEN; i++) {        /* a golden tone, for when nothing is planted */
		float t = (float)i * INV_SR;
		float v = 0.5f * scq_sin2pi(f * t) + 0.3f * scq_sin2pi(f * 1.25f * t) + 0.2f * scq_sin2pi(f * 1.5f * t);
		buf[i] = to16(v * 0.6f);
	}
}
static const float MOSS_RATE[6] = { 1.f, 2.f, 0.5f, 1.5f, 1.f, 0.75f };
static void run_granular(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	float *gr = (float *)(buf + MOSS_LEN);  /* per grain: pos, rate, env phase, env inc */
	uint32_t w = (uint32_t)n->s[0];
	int planted = n->patched & 1;
	LOOP {
		if (planted) {
			buf[w] = to16(in[0][i]);
			if (++w >= MOSS_LEN) w = 0;
		}
		float bloom = scq_clamp(in[1][i], 0.f, 1.f);
		if (scq_rand01(&n->rng) < bloom * bloom * 0.004f + 0.0002f) {
			for (int g = 0; g < MOSS_G; g++) {
				float *q = gr + g * 4;
				if (q[3] != 0.f) continue;
				float dur = (0.03f + 0.09f * scq_rand01(&n->rng)) * SR;
				q[0] = (float)w - dur * 2.f - scq_rand01(&n->rng) * (float)(MOSS_LEN / 2);
				while (q[0] < 0.f) q[0] += (float)MOSS_LEN;
				q[1] = MOSS_RATE[scq_rng(&n->rng) % 6];
				q[2] = 0.f;
				q[3] = 1.f / dur;
				break;
			}
		}
		float sum = 0.f;
		for (int g = 0; g < MOSS_G; g++) {
			float *q = gr + g * 4;
			if (q[3] == 0.f) continue;
			uint32_t p = (uint32_t)q[0];                   /* kept inside the buffer below */
			float e = 4.f * q[2] * (1.f - q[2]);           /* a soft hump, no sine */
			sum += FROM16(buf[p]) * e * e;
			q[0] += q[1];
			if (q[0] >= (float)MOSS_LEN) q[0] -= (float)MOSS_LEN;
			q[2] += q[3];
			if (q[2] >= 1.f) q[3] = 0.f;
		}
		n->out[0][i] = sum * 0.6f;
	}
	n->s[0] = (float)w;
}

/* frogs — frogs in a grid: chirping FM voices. nature = how many (1-8) */
static const float FROG_H[8] = { 9.f, 10.f, 12.f, 15.f, 16.f, 18.f, 20.f, 24.f };
static uint32_t mem_frogs(float nature) { (void)nature; return 8 * 5 * sizeof(float); }
static void init_frogs(node *n)
{
	float *m = (float *)n->mem;
	for (int k = 0; k < 8; k++) m[k * 5 + 2] = scq_rand01(&n->rng) * 2.f * SR;
}
static void run_frogs(node *n, const float *const in[MAX_IN])
{
	float *m = (float *)n->mem;   /* per frog: phase, mod phase, timer, freq, env */
	int count = natural(n->nature, 1, 8);
	float k0 = scq_exp2(in[1][0]);
	int woke = 0;
	LOOP if (edge(in[0][i], &n->s[0])) woke = 1;
	float pt = n->s[1];
	LOOP { n->out[0][i] = 0.f; }
	for (int k = 0; k < count; k++) {
		float *f = m + k * 5;
		f[2] -= (float)B;
		if (f[2] <= 0.f || woke) {
			f[2] = (0.3f + 2.2f * scq_rand01(&n->rng)) * SR;
			f[3] = 60.f * FROG_H[k] * k0;
			f[4] = 0.4f;
			pt = PULSE_SAMPLES;
		}
		if (f[4] < 0.0005f) continue;
		float ph = f[0], mp = f[1], fr = f[3], env = f[4];
		LOOP {
			mp += fr * 2.01f * INV_SR; mp -= scq_floor(mp);
			ph += fr * INV_SR; ph -= scq_floor(ph);
			n->out[0][i] += env * scq_sin2pi(ph + 0.25f * scq_sin2pi(mp));
			env *= 0.9993f;
			fr *= 0.99998f;
		}
		f[0] = ph; f[1] = mp; f[3] = fr; f[4] = env;
	}
	LOOP {
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[1] = pt;
}

/* ------------------------------------------------------------------ currents */

/* slope — the spiral staircase (Serge's universal slope): rises, falls, follows.
 * Patch its end into its start and it climbs forever. */
/* run_slope: in scq_more_classic.inc (shared with dusg / duallpg) */

/* at rest the slope has "ended": one end pulse at the start, so end -> start begins to climb */
static void init_slope(node *n) { n->s[3] = PULSE_SAMPLES; }

/* smoothrandom — they look like people but aren't: smooth wandering random */
static void run_smoothrandom(node *n, const float *const in[MAX_IN])
{
	float from = n->s[0], to = n->s[1], ph = n->s[2];
	float f = 0.3f * n->nature * scq_exp2(scq_clamp(in[0][0], 0.f, 1.f) * 6.f);
	int imitate = n->patched & 2;
	LOOP {
		ph += f * INV_SR;
		if (ph >= 1.f) { ph -= 1.f; from = to; to = scq_rand01(&n->rng); }
		float e = 0.5f - 0.5f * scq_sin2pi(ph * 0.5f + 0.25f);
		float v = from + (to - from) * e;
		n->out[0][i] = imitate ? in[1][i] + (v - 0.5f) * 0.3f : v;
	}
	n->s[0] = from; n->s[1] = to; n->s[2] = ph;
}

/* steprandom — the other ones: stepped random, held until the next pulse */
static void run_steprandom(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], ph = n->s[1], pt = n->s[3];
	int clocked = n->patched & 1;
	LOOP {
		int step;
		if (clocked) step = edge(in[0][i], &n->s[2]);
		else {
			ph += n->nature * 0.5f * INV_SR;
			step = ph >= 1.f;
			if (step) ph -= 1.f;
		}
		if (step) { v = scq_rand01(&n->rng); pt = PULSE_SAMPLES; }
		n->out[0][i] = v;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = v; n->s[1] = ph; n->s[3] = pt;
}

/* glide — steps that move smoothly (Serge's smooth & stepped) */
static void run_glide(node *n, const float *const in[MAX_IN])
{
	float held = n->s[0], v = n->s[1];
	const float c = scq_coef_time(scq_clamp(in[2][0], 0.f, 1.f) * 1.f, SR);   /* glide per block */
	LOOP {
		if (edge(in[1][i], &n->s[2])) held = in[0][i];
		v += (held - v) * c;
		n->out[0][i] = v;
	}
	n->s[0] = held; n->s[1] = v;
}

/* seq — the train and its stations. Stations are written in semitones. */
static uint32_t mem_seq(float nature) { (void)nature; return 33 * sizeof(float); }
static void init_seq(node *n)
{
	float *m = (float *)n->mem;
	static const float def[8] = { 0, 3, 7, 10, 12, 10, 7, 3 };
	if (m[32] == 0.f) {                 /* no stations written: a default route */
		int c = natural(n->nature, 1, 32);
		for (int k = 0; k < c; k++) m[k] = def[k % 8];
		m[32] = (float)c;
	}
}
static void run_seq(node *n, const float *const in[MAX_IN])
{
	float *m = (float *)n->mem;
	int count = (int)m[32];
	int at = (int)n->s[0];
	float pt = n->s[4];
	if (count < 1) count = 1;
	LOOP {
		int moved = 0;
		if (edge(in[0][i], &n->s[1])) { at = (at + 1) % count; moved = 1; }
		if (edge(in[1][i], &n->s[2])) { at = (at + count - 1) % count; moved = 1; }
		if (edge(in[2][i], &n->s[3])) { at = 0; moved = 1; }
		if (moved) pt = PULSE_SAMPLES;
		n->out[0][i] = m[at] / 12.f;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		n->out[2][i] = count > 1 ? (float)at / (float)(count - 1) : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = (float)at; n->s[4] = pt;
}

/* quantizer — pitch quantizer over just, tempered, equal and microtonal scales.
 * The scale is the nature (1-38), or the scale= switch. Pitch in octaves; root transposes. */
#include "scq_scales.inc"
static float qz_snap(const float *d, float y)
{
	float per = d[0] * (1.f / 1200.f);
	int cnt = (int)d[1];
	float k = scq_floor(y / per), r = (y - k * per) * 1200.f;     /* cents into the period */
	float q;
	if (cnt < 0) {                                                 /* equal steps */
		float step = d[0] / (float)(-cnt);
		q = scq_floor(r / step + 0.5f) * step;
	} else {
		float bd = d[0] - r;                                       /* the next period's root */
		q = d[0];
		for (int j = 0; j < cnt; j++) {
			float e = scq_abs(r - d[2 + j]);
			if (e < bd) { bd = e; q = d[2 + j]; }
		}
	}
	return k * per + q * (1.f / 1200.f);
}
static void run_quantizer(node *n, const float *const in[MAX_IN])
{
	int sc = n->opt[0] != 0.f ? (int)n->opt[0] - 1 : natural(n->nature, 1, QZ_COUNT) - 1;
	const float *d = sc < QZ_COUNT ? QZ_DATA + QZ_AT[sc] : scq_tuning(sc - QZ_COUNT);   /* past the built-ins: the dream's tunings */
	if (!d) d = QZ_DATA + QZ_AT[5];                                                    /* (chromatic) */
	float last = n->s[0], pt = n->s[1], lastin = n->s[2], q = n->s[3];
	float root = in[1][0];
	LOOP {
		float x = in[0][i];
		if (x != lastin || i == 0) { lastin = x; q = root + qz_snap(d, x - root); }   /* only when it moves */
		if (q != last) { last = q; pt = PULSE_SAMPLES; }
		n->out[0][i] = q;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = last; n->s[1] = pt; n->s[2] = lastin; n->s[3] = q;
}

/* gesture — the cat that teaches expressions you mirror back: learns a gesture, repeats it */
#define CAT_LEN 6000
static uint32_t mem_gesture(float nature) { (void)nature; return CAT_LEN * sizeof(int16_t); }
static void run_gesture(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	/* s: 0 learning, 1 length, 2 play pos, 3 decimation, 4 smooth */
	int learning = in[1][0] > 0.5f;
	if (learning && n->s[0] == 0.f) { n->s[1] = 0.f; n->s[3] = 0.f; }
	n->s[0] = learning ? 1.f : 0.f;
	float target;
	if (learning) {
		if (n->s[3] == 0.f && n->s[1] < (float)CAT_LEN)
			buf[(int)n->s[1]++] = to16(scq_clamp(in[0][0], -1.f, 1.f));
		target = in[0][0];
	} else if (n->s[1] > 0.f) {
		target = FROM16(buf[(int)n->s[2]]);
		if (n->s[3] == 0.f && (n->s[2] += 1.f) >= n->s[1]) n->s[2] = 0.f;
	} else target = in[0][0];
	n->s[3] = n->s[3] >= 3.f ? 0.f : n->s[3] + 1.f;
	float v = n->s[4];
	LOOP {
		v += (target - v) * 0.02f;
		n->out[0][i] = v;
	}
	n->s[4] = v;
}

/* ducker — shushes you into a trance: a ducker and a slow swing */
static void run_ducker(node *n, const float *const in[MAX_IN])
{
	float env = n->s[0], ph = n->s[1];
	float att = scq_coef_time(0.005f, SR), rel = scq_coef_time(0.3f, SR);
	LOOP {
		float a = scq_abs(in[1][i]);
		env += (a - env) * (a > env ? att : rel);
		n->out[0][i] = in[0][i] * (1.f - scq_min(1.f, env * 2.f));
		ph += 0.05f * n->nature * INV_SR;
		ph -= scq_floor(ph);
		n->out[1][i] = 0.5f + 0.5f * scq_sin2pi(ph);
	}
	n->s[0] = env; n->s[1] = ph;
}

/* value — a kept value (the water bottle). bottle 40 = 0.40 */
static void run_value(node *n, const float *const in[MAX_IN])
{
	(void)in;
	LOOP n->out[0][i] = n->nature * 0.01f;
}

/* ------------------------------------------------------------------ hands */

static void run_fader(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int k = natural(n->nature, 1, 4) - 1;
	float v = n->s[0], t = W.ctl[SCQ_CTL_LEAN1 + k];
	if (v + (t - v) * 0.004f == v) { LOOP n->out[0][i] = v; return; }     /* at rest: nothing to smooth */
	LOOP { v += (t - v) * 0.004f; n->out[0][i] = v; }
	n->s[0] = v;
}
static void run_button(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_DOOR1 + natural(n->nature, 1, 4) - 1];
	LOOP n->out[0][i] = v;
}
static void run_rocker(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_LIFT];
	float up = n->s[1], dn = n->s[2];
	if (v > 0.5f && n->s[0] <= 0.5f) up = PULSE_SAMPLES;
	if (v < -0.5f && n->s[0] >= -0.5f) dn = PULSE_SAMPLES;
	n->s[0] = v;
	LOOP {
		n->out[0][i] = v;
		n->out[1][i] = up > 0.f ? 1.f : 0.f;
		n->out[2][i] = dn > 0.f ? 1.f : 0.f;
		if (up > 0.f) up -= 1.f;
		if (dn > 0.f) dn -= 1.f;
	}
	n->s[1] = up; n->s[2] = dn;
}
static void run_play(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_WAKE];
	LOOP n->out[0][i] = v;
}
static void run_dots(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_LUCID];
	LOOP n->out[0][i] = v;
}

/* ------------------------------------------------------------------ the Wingie's hands */

/* mic — the sound coming in: the Wingie's two microphones (or line in), the editor's audio input */
static void run_mic(node *n, const float *const in[MAX_IN])
{
	(void)in;
	LOOP {
		float l = W.inL[i], r = W.inR[i];
		n->out[0][i] = l; n->out[1][i] = r; n->out[2][i] = 0.5f * (l + r);
	}
}

/* keys1 / keys2 — a keyboard: pitch of the last key pressed (octaves; keys2 sits an octave
 * above, as on the stock Wingie; the octave switch adds), gate while any key is held, a pulse
 * at every press, and which key (0 for C .. 1 for B) */
static void run_keys(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int side = natural(n->nature, 1, 2) - 1;
	int base = side ? SCQ_CTL_KEYS2 : SCQ_CTL_KEYS1;
	uint32_t held = 0, prev = (uint32_t)n->s[0];
	for (int k = 0; k < 12; k++) if (W.ctl[base + k] > 0.5f) held |= 1u << k;
	float last = n->s[1], pt = n->s[2];
	uint32_t pressed = held & ~prev;
	if (pressed) { for (int k = 11; k >= 0; k--) if (pressed & (1u << k)) last = (float)k; pt = PULSE_SAMPLES; }
	float oct = W.ctl[(side ? SCQ_CTL_OCTAVE2 : SCQ_CTL_OCTAVE1)];
	float pitch = (last + 12.f * oct) / 12.f + (float)side;
	float gate = held ? 1.f : 0.f;
	LOOP {
		n->out[0][i] = pitch; n->out[1][i] = gate;
		n->out[2][i] = pt > 0.f ? 1.f : 0.f;
		n->out[3][i] = last / 11.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = (float)held; n->s[1] = last; n->s[2] = pt;
}

/* key — one key: key 1-12 are the left keyboard C..B, 13-24 the right. 1 while held, a pulse on press */
static void run_key(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int k = natural(n->nature, 1, 24) - 1;
	float v = W.ctl[SCQ_CTL_KEYS1 + k] > 0.5f ? 1.f : 0.f, pt = n->s[1];
	if (v > 0.5f && n->s[0] <= 0.5f) pt = PULSE_SAMPLES;
	n->s[0] = v;
	LOOP { n->out[0][i] = v; n->out[1][i] = pt > 0.f ? 1.f : 0.f; if (pt > 0.f) pt -= 1.f; }
	n->s[1] = pt;
}

/* mode1 / mode2 — the Mode buttons: 1 while held; tap pulses when one is let go quickly (< 0.6 s) */
static void run_mode(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_MODE1 + natural(n->nature, 1, 2) - 1] > 0.5f ? 1.f : 0.f;
	float held = n->s[1], pt = n->s[2];
	if (v > 0.5f) held += (float)B;
	else { if (n->s[0] > 0.5f && held < 0.6f * SR) pt = PULSE_SAMPLES; held = 0.f; }
	n->s[0] = v;
	LOOP { n->out[0][i] = v; n->out[1][i] = pt > 0.f ? 1.f : 0.f; if (pt > 0.f) pt -= 1.f; }
	n->s[1] = held; n->s[2] = pt;
}

/* octave1 / octave2 — the octave switches: -1, 0 or 1 */
static void run_octave(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_OCTAVE1 + natural(n->nature, 1, 2) - 1];
	LOOP n->out[0][i] = v;
}

/* source — the source switch: 0 microphones, 1 line in */
static void run_source(node *n, const float *const in[MAX_IN])
{
	(void)in; (void)n;
	float v = W.ctl[SCQ_CTL_SOURCE];
	LOOP n->out[0][i] = v;
}

/* lights — the Wingie's two LEDs. They glow red at rest; 0..1 climbs red, purple, yellow, white */
static void run_lights(node *n, const float *const in[MAX_IN])
{
	float a = n->s[0], b = n->s[1];
	float x = scq_clamp(scq_abs(in[0][0]), 0.f, 1.f), y = scq_clamp(scq_abs(in[1][0]), 0.f, 1.f);
	a += (x - a) * 0.3f; b += (y - b) * 0.3f;
	n->s[0] = a; n->s[1] = b;
	W.lamp[0] = a; W.lamp[1] = b;
}

/* ------------------------------------------------------------------ ears */

static void run_left(node *n, const float *const in[MAX_IN])  { (void)n; LOOP W.outL[i] += in[0][i]; }
static void run_right(node *n, const float *const in[MAX_IN]) { (void)n; LOOP W.outR[i] += in[0][i]; }
static void run_out(node *n, const float *const in[MAX_IN])  { (void)n; LOOP { W.outL[i] += in[0][i]; W.outR[i] += in[0][i]; } }

/* leds — red while charging, then white: the eight LEDs */
static void run_leds(node *n, const float *const in[MAX_IN])
{
	float env = n->s[0], t1 = n->s[1], t2 = n->s[2];
	float att = scq_coef_time(0.01f, SR), rel = scq_coef_time(0.2f, SR);
	LOOP {
		float a = scq_abs(in[0][i]);
		env += (a - env) * (a > env ? att : rel);
		n->out[0][i] = env;
	}
	t1 += (scq_clamp(in[1][0], 0.f, 1.f) - t1) * 0.05f;
	t2 += (scq_clamp(in[2][0], 0.f, 1.f) - t2) * 0.05f;
	n->s[0] = env; n->s[1] = t1; n->s[2] = t2;
	/* the sound lights both rows as one bar; track / play light their own row */
	float bar = scq_clamp(env * 1.5f, 0.f, 1.f) * 8.f;
	for (int k = 0; k < 8; k++) W.lamp[k] = scq_clamp(bar - (float)k, 0.f, 1.f);
	if (n->patched & 2) for (int k = 0; k < 4; k++) W.lamp[k] = scq_clamp(t1 * 4.f - (float)k, 0.f, 1.f);
	if (n->patched & 4) for (int k = 0; k < 4; k++) W.lamp[4 + k] = scq_clamp(t2 * 4.f - (float)k, 0.f, 1.f);
}

/* scene — riding between scenes. Sometimes you miss your stop. */
static void run_scene(node *n, const float *const in[MAX_IN])
{
	LOOP {
		if (edge(in[0][i], &n->s[0])) W.want_next = 1;
		if (edge(in[1][i], &n->s[1])) W.want_back = 1;
	}
	W.miss = scq_clamp(in[2][0], 0.f, 1.f);
}

/* ------------------------------------------------------------------ the table */

/* ------------------------------------------------------------------ the rest of modular history */
#include "scq_more_sources.inc"
#include "scq_more_shapes.inc"
#include "scq_more_spaces.inc"
#include "scq_more_currents.inc"
#include "scq_more_classic.inc"
#include "scq_more_simple.inc"
#include "scq_midi.inc"
#include "scq_room.inc"
/* the Wingie collection (2026-11-24): its resonator, the Coco, the fish ideas, and a lot of processing */
#include "scq_wingie_places.inc"
#include "scq_proc_time.inc"
#include "scq_proc_color.inc"
#include "scq_proc_grain.inc"
#include "scq_proc_spectral.inc"

#define P(name, fam, flags, cost, nat, ins, defs, outs, run, init, mem, help) \
	{ name, fam, flags, cost, nat, ins, defs, outs, run, init, mem, help }

const place SCQ_PLACES[] = {
P("sine",    FAM_SOURCE, PF_DRIFT, 87, 3, "pitch fm", "0 0", "out", run_sine, 0, 0,
  "sine oscillator at nature x 60 Hz"),
P("crossfm", FAM_SOURCE, PF_DRIFT, 207, 3, "pitch index fold", "0 0 0", "out a b", run_crossfm, 0, 0,
  "two sines on harmonics n and n+1; index frequency-modulates a with b, fold wavefolds a; out is both mixed"),
P("rain",    FAM_SOURCE, PF_DRIFT, 60, 1, "density", "0", "out pulse", run_rain, 0, 0,
  "rain: random impulses ringing tiny resonators, 8 x nature per second; pulse marks each drop"),
P("thunder", FAM_SOURCE, 0, 113, 1, "strike", "0", "out", run_thunder, 0, 0,
  "thunder: a filtered noise rumble when struck, and on its own about nature times a minute"),
P("noisesweep", FAM_SOURCE, PF_DRIFT, 73, 1, "level", "0.5", "out", run_noisesweep, 0, 0,
  "noise through a resonant bandpass whose frequency climbs with level"),
P("wind",    FAM_SOURCE, PF_DRIFT, 73, 1, "gust", "0.3", "out", run_wind, 0, 0,
  "wind: soft noise through a slowly wandering bandpass"),
P("frogs",   FAM_SOURCE, 0, 460, 3, "trig pitch", "0 0", "out pulse", run_frogs, init_frogs, mem_frogs,
  "nature (1-8) chirping FM voices on random timers; a pulse into trig makes them all chirp"),
P("shepard", FAM_SOURCE, PF_DRIFT, 247, 1, "rate pitch", "0.1 0", "out", run_shepard, 0, 0,
  "Shepard-Risset glissando: rises forever (rate in octaves per second, negative falls)"),
P("bell",    FAM_SOURCE, PF_DRIFT, 180, 1, "strike pitch decay", "0 0 0.5", "out", run_bell, 0, 0,
  "modal bell: four inharmonic modes, struck by pulses or by sound"),
P("folder",  FAM_SHAPE, PF_DRIFT, 87, 1, "in fold", "0 0", "out", run_folder, 0, 0,
  "wavefolder; more fold, more folds"),
P("lpg",     FAM_SHAPE, 0, 73, 1, "in level", "0 1", "out", run_lpg, 0, 0,
  "lowpass gate (vactrol-style); pulse level for a pluck"),
P("lowpass", FAM_SHAPE, PF_DRIFT, 87, 1, "in cutoff res", "0 0 0.2", "out band", run_lowpass, 0, 0,
  "resonant 2-pole lowpass at nature x 480 Hz, with a bandpass out"),
P("filterbank", FAM_SHAPE, PF_DRIFT, 47, 1, "in band1 band2 band3 band4 band5 band6 band7 band8",
  "0 0 0 0 0 0 0 0 0", "out", run_filterbank, init_filterbank, mem_filterbank,
  "fixed filter bank: eight resonant bands; patch into a band to raise it"),
P("saturator", FAM_SHAPE, 0, 100, 1, "in burst", "0 0", "out", run_saturator, 0, 0,
  "tanh saturator; a pulse into burst kicks the drive up with a crackle that dies away"),
P("dcblock", FAM_SHAPE, 0, 33, 1, "in", "0", "out", run_dcblock, 0, 0,
  "DC blocker (highpass at nature x 20 Hz)"),
P("vca",     FAM_SHAPE, 0, 33, 1, "in gain", "0 1", "out", run_vca, 0, 0,
  "VCA: in times gain"),
P("matrix",  FAM_SHAPE, 0, 87, 0, "in1 in2 in3 in4 rotate", "0 0 0 0 0", "out1 out2 out3 out4", run_matrix, 0, 0,
  "4 x 4 rotation matrix mixer; rotate turns it (nature sets the resting angle)"),
P("comb",    FAM_SPACE, PF_DRIFT, 113, 2, "in pitch decay", "0 0 0.5", "out", run_comb, 0, mem_comb,
  "comb filter tuned to nature x 60 Hz; pulses pluck it"),
P("delay",   FAM_SPACE, PF_DRIFT, 180, 6, "in time feedback", "0 1 0.3", "out", run_delay, init_delay, mem_delay,
  "delay, nature x 50 ms long; time scales it"),
P("reverb",  FAM_SPACE, PF_DRIFT, 327, 2, "in size", "0 0.7", "out l r", run_reverb, 0, mem_reverb,
  "stereo room reverb"),
P("granular", FAM_SPACE, 0, 220, 1, "in density", "0 0.3", "out", run_granular, init_granular, mem_granular,
  "granular: grains of what you patch in, or of its own buffer"),
P("slope",   FAM_CURRENT, 0, 87, 1, "in trig rise fall", "0 0 0.3 0.5", "out end", run_slope, init_slope, 0,
  "slope / function generator: rises and falls when triggered, follows in otherwise; end into trig cycles (after Serge DUSG)"),
P("smoothrandom", FAM_CURRENT, PF_DRIFT, 100, 1, "rate follow", "0 0", "out", run_smoothrandom, 0, 0,
  "smooth random voltage; with follow patched, it wanders around that instead"),
P("steprandom", FAM_CURRENT, PF_DRIFT, 47, 1, "clock", "0", "out pulse", run_steprandom, 0, 0,
  "stepped random, a new value on each clock pulse (or nature / 2 times a second on its own)"),
P("glide",   FAM_CURRENT, 0, 47, 1, "in clock time", "0 0 0.2", "out", run_glide, 0, 0,
  "sample & hold with glide: holds in on each clock pulse and slides to it (after Serge Smooth & Stepped)"),
P("seq",     FAM_CURRENT, PF_KEEPMEM, 127, 8, "next back reset", "0 0 0", "out pulse step", run_seq, init_seq, mem_seq,
  "step sequencer of semitones: seq 8: 0 3 7 10"),
P("quantizer", FAM_CURRENT, 0, 60, 1, "in root", "0 0", "out pulse", run_quantizer, 0, 0,
  "pitch quantizer (octaves in, root transposes); the scale is the nature or scale=: 1 just major, 2 just minor, 3 just pentatonic, 4 otonal 8-15, 5 utonal, 6 chromatic, 7 major, 8 minor, 9 dorian, 10 phrygian, 11 lydian, 12 mixolydian, 13 locrian, 14 harmonic minor, 15 melodic minor, 16 major pentatonic, 17 minor pentatonic, 18 blues, 19 whole tone, 20 diminished, 21 quarter tones (24-TET), 22 19-TET, 23 22-TET, 24 31-TET, 25 53-TET, 26 Bohlen-Pierce, 27-29 Carlos alpha / beta / gamma, 30 slendro, 31 pelog, 32 maqam rast, 33 maqam bayati, 34 Partch 43, 35 just chromatic, 36 Pythagorean, 37 quarter-comma meantone, 38 Werckmeister III"),
P("gesture", FAM_CURRENT, 0, 33, 1, "in record", "0 0", "out", run_gesture, 0, mem_gesture,
  "gesture recorder: records in while record is held, then loops it (up to 8 s)"),
P("ducker",  FAM_CURRENT, PF_DRIFT, 140, 1, "in side", "0 0", "out lfo", run_ducker, 0, 0,
  "ducker: turns in down when side is loud; lfo is a slow sine"),
P("value",   FAM_CURRENT, 0, 20, 0, "", "", "out", run_value, 0, 0,
  "a fixed value: value 40 is 0.40"),
#include "scq_more_table.inc"
#include "scq_wingie_places_table.inc"
#include "scq_proc_time_table.inc"
#include "scq_proc_color_table.inc"
#include "scq_proc_grain_table.inc"
#include "scq_proc_spectral_table.inc"
P("midi",    FAM_HANDS, 0, 30, 0, "", "", "pitch gate velocity trig", run_midi, 0, 0,
  "MIDI to CV: pitch (octaves from middle C), gate, velocity, trig on each note; channel=0 is any, voice=1-4 picks one voice of a chord (0 = mono, last note)"),
P("midicc",  FAM_HANDS, 0, 22, 0, "", "", "out", run_midicc, 0, 0,
  "one MIDI controller, 0 to 1: cc=0-127, 128 = pitch bend (-1 to 1), 129 = aftertouch; channel=0 is any"),
P("midiclock", FAM_HANDS, 0, 22, 0, "", "", "clock reset run", run_midiclock, 0, 0,
  "MIDI clock: clock pulses every div ticks (24 = quarter notes, 6 = sixteenths), reset on start, run is 1 while playing"),
P("crowd",   FAM_CURRENT, 0, 30, 0, "", "", "out count near strength", run_crowd, 0, 0,
  "the Bluetooth devices near the computer: out is how full the room is (0 to 1, the count over max=), count the plain number, near the strongest signal, strength all of them together; rest= is the count it holds where there is no radio (the SP-1 on its own)"),
P("visitors", FAM_CURRENT, 0, 30, 0, "", "", "arrive leave pulse voice", run_visitors, 0, 0,
  "the same devices as they come and go: a pulse when one arrives, when one leaves and on every signal heard; voice is the number (0 to 1) of the one heard last"),
P("fader",   FAM_HANDS, PF_SINGLE | PF_COUNTED, 20, 1, "", "", "out", run_fader, 0, 0,
  "faders: fader1 - fader4, 0 to 1 (the Wingie: fader1 Mix, fader2 Decay, fader3 Volume)"),
P("mic",     FAM_HANDS, PF_SINGLE | PF_WINGIE, 20, 0, "", "", "left right both", run_mic, 0, 0,
  "the Wingie's microphones (or line in, by its source switch): left, right and both mixed"),
P("keys",    FAM_HANDS, PF_SINGLE | PF_COUNTED | PF_WINGIE, 30, 1, "", "", "pitch gate trig key", run_keys, 0, 0,
  "a Wingie keyboard: keys1 left, keys2 right (an octave up). pitch of the last key in octaves with the octave switch, gate while any is held, trig on each press, key 0 (C) to 1 (B)"),
P("key",     FAM_HANDS, PF_WINGIE, 20, 1, "", "", "out trig", run_key, 0, 0,
  "one Wingie key: key 1-12 the left keyboard C to B, 13-24 the right; 1 while held, trig on press"),
P("mode",    FAM_HANDS, PF_SINGLE | PF_COUNTED | PF_WINGIE, 20, 1, "", "", "out tap", run_mode, 0, 0,
  "the Wingie's Mode buttons: mode1 left, mode2 right; 1 while held, tap pulses on a quick press"),
P("octave",  FAM_HANDS, PF_SINGLE | PF_COUNTED | PF_WINGIE, 20, 1, "", "", "out", run_octave, 0, 0,
  "the Wingie's octave switches: octave1 left, octave2 right; -1, 0 or 1"),
P("source",  FAM_HANDS, PF_SINGLE | PF_WINGIE, 20, 0, "", "", "out", run_source, 0, 0,
  "the Wingie's source switch: 0 microphones, 1 line in"),
P("button",  FAM_HANDS, PF_SINGLE | PF_COUNTED | PF_SP1, 20, 1, "", "", "out", run_button, 0, 0,
  "track buttons: button1 - button4, 1 while held"),
P("rocker",  FAM_HANDS, PF_SINGLE | PF_SP1, 20, 0, "", "", "out up down", run_rocker, 0, 0,
  "the rocker: out is -1, 0 or 1; up and down pulse"),
P("play",    FAM_HANDS, PF_SINGLE | PF_SP1, 20, 0, "", "", "out", run_play, 0, 0,
  "PLAY, 1 while held"),
P("dots",    FAM_HANDS, PF_SINGLE | PF_SP1, 20, 0, "", "", "out", run_dots, 0, 0,
  "the dots button, 1 while held"),
P("left",    FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_left, 0, 0, "left output"),
P("right",   FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_right, 0, 0, "right output"),
P("out",     FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_out, 0, 0, "both outputs"),
P("leds",    FAM_EARS, PF_SINGLE | PF_SP1, 40, 0, "in track play", "0 0 0", "out", run_leds, 0, 0,
  "the eight LEDs: sound lights both rows as a bar; track and play light one row each"),
P("lights",  FAM_EARS, PF_SINGLE | PF_WINGIE, 20, 0, "left right", "0 0", "", run_lights, 0, 0,
  "the Wingie's two LEDs: red at rest, then purple, yellow and white as what comes in climbs to 1"),
P("scene",   FAM_EARS, PF_SINGLE, 20, 0, "next back miss", "0 0 0", "", run_scene, 0, 0,
  "goes to the next or previous scene on a pulse; miss is the chance of landing on a random one"),
};
const int SCQ_NPLACES = (int)(sizeof(SCQ_PLACES) / sizeof(SCQ_PLACES[0]));

/* what the places were called before the rename, and their old ports in the same order */
typedef struct { const char *now, *was, *ins, *outs; } place_was;
static const place_was WAS[] = {
#include "scq_was.inc"
};
#define NWAS ((int)(sizeof(WAS) / sizeof(WAS[0])))

static int same(const char *a, int n, const char *b)
{
	int k = 0;
	while (k < n && b[k] && a[k] == b[k]) k++;
	return k == n && !b[k];
}

/* switches and params: "name=a|b|c" (a is the default) and "name=number" */
static int port_in(const char *s, const char *port, int len);

#define PITCH_OPTS "tune=0"   /* every place with a pitch inlet: its base pitch knob, in octaves */
static const struct { const char *place, *opts; } OPTS[] = {
	{ "sine",       "range=audio|lfo " PITCH_OPTS },
	{ "vco",        "range=audio|lfo " PITCH_OPTS },
	{ "sinesaw",    "range=audio|lfo " PITCH_OPTS },
	{ "square",     "range=audio|lfo " PITCH_OPTS },
	{ "lfo",        "range=lfo|audio" },
	{ "vca",        "response=lin|exp" },
	{ "quadvca",    "response=lin|exp" },
	{ "lpg",        "mode=both|lowpass|vca" },
	{ "duallpg",    "mode1=both|lowpass|vca mode2=both|lowpass|vca" },
	{ "slope",      "cycle=off|on range=lfo|audio curve=lin|exp|log" },
	{ "dusg",       "cycle1=off|on cycle2=off|on range=lfo|audio curve=lin|exp|log" },
	{ "dualprocessor", "mode1=lag|slew mode2=lag|slew" },
	{ "slew",       "shape=lin|exp" },
	{ "ad",         "loop=off|on range=lfo|audio curve=exp|lin" },
	{ "ar",         "curve=exp|lin" },
	{ "decay",      "curve=exp|lin" },
	{ "stepseq",    "dir=forward|backward|pingpong|random" },
	{ "trigger",    "length=2" },
	{ "difftone",   "ratio=1.2 " PITCH_OPTS },
	{ "midi",       "channel=0 voice=0" },
	{ "midicc",     "channel=0 cc=1" },
	{ "midiclock",  "div=6" },
	{ "crowd",      "max=20 rest=0" },
#include "scq_wingie_places_opts.inc"
#include "scq_proc_time_opts.inc"
#include "scq_proc_color_opts.inc"
#include "scq_proc_grain_opts.inc"
#include "scq_proc_spectral_opts.inc"
	{ "quantizer",  "scale=nature|justmajor|justminor|justpentatonic|otonal|utonal|chromatic|major|minor|dorian|phrygian|lydian|mixolydian|locrian|harmonicminor|melodicminor|majorpentatonic|minorpentatonic|blues|wholetone|diminished|quartertone|tet19|tet22|tet31|tet53|bohlenpierce|alpha|beta|gamma|slendro|pelog|rast|bayati|partch|justchromatic|pythagorean|meantone|werckmeister" },
};

const char *place_opts(const char *name)
{
	for (int i = 0; i < (int)(sizeof(OPTS) / sizeof(OPTS[0])); i++) {
		const char *a = OPTS[i].place, *b = name;
		while (*a && *a == *b) a++, b++;
		if (!*a && !*b) return OPTS[i].opts;
	}
	for (int i = 0; i < SCQ_NPLACES; i++) {
		const char *a = SCQ_PLACES[i].name, *b = name;
		while (*a && *a == *b) a++, b++;
		if (!*a && !*b) return port_in(SCQ_PLACES[i].ins, "pitch", 5) >= 0 ? PITCH_OPTS : "";
	}
	return "";
}

int place_has_att(const place *p)
{
	return p->family == FAM_SOURCE || port_in(p->ins, "pitch", 5) >= 0;
}

const char *place_renamed(const char *old, int len)
{
	for (int i = 0; i < NWAS; i++)
		if (same(old, len, WAS[i].was) && !same(old, len, WAS[i].now)) return WAS[i].now;
	return 0;
}

/* port lookup: index of `port` in the place's ins (out=0) or outs (out=1), -1 if absent.
 * Old port names (before the rename) still work. */
int place_port(const place *p, int out, const char *port, int len)
{
	int k = port_in(out ? p->outs : p->ins, port, len);
	if (k >= 0) return k;
	for (int i = 0; i < NWAS; i++) {
		const char *a = WAS[i].now, *b = p->name;
		while (*a && *a == *b) a++, b++;
		if (*a || *b) continue;
		const char *old = out ? WAS[i].outs : WAS[i].ins;
		return *old ? port_in(old, port, len) : -1;
	}
	return -1;
}

static int port_in(const char *s, const char *port, int len)
{
	int idx = 0;
	while (*s) {
		while (*s == ' ') s++;
		if (!*s) break;
		const char *w = s;
		while (*s && *s != ' ') s++;
		int wl = (int)(s - w);
		if (wl == len) {
			int k = 0;
			while (k < len && w[k] == port[k]) k++;
			if (k == len) return idx;
		}
		idx++;
	}
	return -1;
}

int place_nports(const char *s)
{
	int n = 0;
	while (*s) {
		while (*s == ' ') s++;
		if (!*s) break;
		n++;
		while (*s && *s != ' ') s++;
	}
	return n;
}
