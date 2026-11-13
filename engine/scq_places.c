/*
 * The places. Each one has a fixed nature (Nabra: no knobs); you change it only by
 * running something into its inlets. Natures are tuned from the 60 Hz hum.
 *
 * Signals: sound is about -1..1, current is about 0..1, a pulse is 0 or 1.
 * Pitch inlets are in octaves (1.0 = one octave up). Any signal goes into any inlet.
 */
#include "scq_int.h"

world W;

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

static inline int natural(float nature, int lo, int hi)
{
	int n = (int)(nature + 0.5f);
	return n < lo ? lo : (n > hi ? hi : n);
}

/* ------------------------------------------------------------------ sources */

/* aira — the fountain labeled aira: a pure tone at nature x 60 Hz */
static void run_aira(node *n, const float *const in[MAX_IN])
{
	float base = 60.f * n->nature * scq_exp2(in[0][0]), ph = n->s[0];   /* pitch per block */
	LOOP {
		float f = base * (1.f + in[1][i]);
		ph += f * INV_SR;
		ph -= scq_floor(ph);
		n->out[0][i] = scq_sin2pi(ph);
	}
	n->s[0] = ph;
}

/* twin — two partials, 60n and 60(n+1): their difference is always the hum */
static void run_twin(node *n, const float *const in[MAX_IN])
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

/* flood — water rising: noise through a resonant lowpass that climbs with the level */
static void run_flood(node *n, const float *const in[MAX_IN])
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

/* mountains — dark umber peaks: a sine wavefolder */
static void run_mountains(node *n, const float *const in[MAX_IN])
{
	float base = 1.f + 0.5f * (n->nature - 1.f);
	LOOP {
		float g = base + scq_max(in[1][i], 0.f) * 8.f;
		n->out[0][i] = scq_sin2pi(0.25f * in[0][i] * g);
	}
}

/* window — a lamp shining on a photocell (the lowpass gate) */
static void run_window(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], l1 = n->s[1], l2 = n->s[2];
	const float att = 1.f / (0.002f * SR), rel = 1.f / (0.25f * SR);
	float c = scq_coef_hz(20.f * scq_exp2(v * 10.f), SR);       /* colour per block */
	LOOP {
		float t = scq_clamp(in[1][i], 0.f, 1.f);
		v += (t - v) * (t > v ? att : rel * (0.3f + v));
		l1 += c * (in[0][i] - l1);
		l2 += c * (l1 - l2);
		n->out[0][i] = l2 * v;
	}
	n->s[0] = v; n->s[1] = l1; n->s[2] = l2;
}

/* basement — the house show downstairs: a resonant lowpass, warm */
static void run_basement(node *n, const float *const in[MAX_IN])
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

/* horns — horn-loaded speakers: eight fixed resonant bands (Serge's resonant EQ) */
static const float HORN_HZ[8] = { 61.f, 115.f, 218.f, 411.f, 777.f, 1470.f, 2800.f, 5200.f };
static uint32_t mem_horns(float nature) { (void)nature; return 8 * 5 * sizeof(float); }
static void init_horns(node *n)
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
static void run_horns(node *n, const float *const in[MAX_IN])
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

/* lighter — the lighter that explodes: a flick of fire and drive */
static void run_lighter(node *n, const float *const in[MAX_IN])
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

/* copper — the grounding rod: blocks DC, lets the music through */
static void run_copper(node *n, const float *const in[MAX_IN])
{
	float lp = n->s[0], c = scq_coef_hz(20.f * n->nature, SR);
	LOOP {
		lp += c * (in[0][i] - lp);
		n->out[0][i] = in[0][i] - lp;
	}
	n->s[0] = lp;
}

/* blanket — puts the fire out: a plain amplifier */
static void run_blanket(node *n, const float *const in[MAX_IN])
{
	LOOP n->out[0][i] = in[0][i] * in[1][i];
}

/* crystal — the crystal matrix: four in, four out, turned by an angle */
static void run_crystal(node *n, const float *const in[MAX_IN])
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

/* tunnel — tubular architecture, tubular acoustics: a tuned comb */
static uint32_t mem_tunnel(float nature) { (void)nature; return 2048 * sizeof(float); }
static void run_tunnel(node *n, const float *const in[MAX_IN])
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

/* hallway — the long grey hallway: a delay. nature x 50 ms, up to a second */
static uint32_t mem_hallway(float nature) { return (uint32_t)natural(nature, 1, 20) * 2400 * sizeof(int16_t); }
static void run_hallway(node *n, const float *const in[MAX_IN])
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
static void init_hallway(node *n) { n->s[1] = 1.f; }

/* warehouse — a big room with wooden floors: four-line feedback reverb */
static const uint16_t WH_LEN[4] = { 1447, 1811, 2137, 2551 };
static uint32_t wh_len(float nature, int k)
{
	float sc = 0.6f + 0.2f * scq_clamp(nature, 1.f, 4.f);
	return (uint32_t)((float)WH_LEN[k] * sc);
}
static uint32_t mem_warehouse(float nature)
{
	uint32_t t = 0;
	for (int k = 0; k < 4; k++) t += wh_len(nature, k);
	return t * sizeof(int16_t);
}
static void run_warehouse(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem;
	int16_t *line[4];
	uint32_t len[4], off = 0;
	for (int k = 0; k < 4; k++) { len[k] = wh_len(n->nature, k); line[k] = base + off; off += len[k]; }
	float g = 0.5f * (0.55f + 0.44f * scq_clamp(in[1][0], 0.f, 1.f));
	uint32_t w = (uint32_t)n->s[0];
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
	n->s[0] = (float)w;
}

/* bell — the cargo train's deep bell: four struck modes */
static const float BELL_R[4] = { 1.f, 2.76f, 5.40f, 8.93f };
static const float BELL_A[4] = { 1.f, 0.6f, 0.4f, 0.25f };
static void run_bell(node *n, const float *const in[MAX_IN])
{
	/* s: 0..3 y1, 4..7 y2, 8 prev strike */
	float f0 = 120.f * n->nature * scq_exp2(in[1][0]);
	float T = 0.3f + 4.f * scq_clamp(in[2][0], 0.f, 1.f);
	float c1[4], c2[4], g[4];
	for (int k = 0; k < 4; k++) {
		float f = scq_min(f0 * BELL_R[k], 18000.f);
		float r = scq_exp2(-1.442695f / (T / (1.f + (float)k * 0.7f) * SR));
		c1[k] = 2.f * r * scq_sin2pi(f * INV_SR + 0.25f);
		c2[k] = -r * r;
		g[k] = scq_sin2pi(f * INV_SR) * 0.3f * BELL_A[k];   /* impulse peak is 1/sin(w) */
	}
	LOOP {
		float x = in[0][i] - n->s[8];
		n->s[8] = in[0][i];
		float sum = 0.f;
		for (int k = 0; k < 4; k++) {
			float y = g[k] * x + c1[k] * n->s[k] + c2[k] * n->s[4 + k];
			n->s[4 + k] = n->s[k];
			n->s[k] = y;
			sum += y;
		}
		n->out[0][i] = sum;
	}
}

/* elevator — the lift into the night sky: a tone that rises forever */
static void run_elevator(node *n, const float *const in[MAX_IN])
{
	/* s: 0 phase of the lowest octave, 8 position within the octave. Every partial is an
	 * octave of the lowest, so one sine and cosine give them all by doubling the angle:
	 * sin 2x = 2 sin x cos x, cos 2x = cos^2 x - sin^2 x */
	float pos = n->s[8], ph = n->s[0];
	pos += in[0][0] * (float)B * INV_SR;
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

/* moss — a golden square of moss in a night pond: it blooms into grains */
#define MOSS_LEN 12000
#define MOSS_G   6
static uint32_t mem_moss(float nature) { (void)nature; return MOSS_LEN * sizeof(int16_t) + MOSS_G * 4 * sizeof(float); }
static void init_moss(node *n)
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
static void run_moss(node *n, const float *const in[MAX_IN])
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
			uint32_t p = (uint32_t)q[0] % MOSS_LEN;
			float e = scq_sin2pi(q[2] * 0.5f);
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

/* frog — frogs in a grid: chirping FM voices. nature = how many (1-8) */
static const float FROG_H[8] = { 9.f, 10.f, 12.f, 15.f, 16.f, 18.f, 20.f, 24.f };
static uint32_t mem_frog(float nature) { (void)nature; return 8 * 5 * sizeof(float); }
static void init_frog(node *n)
{
	float *m = (float *)n->mem;
	for (int k = 0; k < 8; k++) m[k * 5 + 2] = scq_rand01(&n->rng) * 2.f * SR;
}
static void run_frog(node *n, const float *const in[MAX_IN])
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

/* stairs — the spiral staircase (Serge's universal slope): rises, falls, follows.
 * Patch its end into its start and it climbs forever. */
static void run_stairs(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], mode = n->s[1], et = n->s[3];
	const float up = 1.f / (0.001f * scq_exp2(scq_clamp(in[2][0], 0.f, 1.f) * 13.3f) * SR);   /* rates per block */
	const float dn = 1.f / (0.001f * scq_exp2(scq_clamp(in[3][0], 0.f, 1.f) * 13.3f) * SR);
	LOOP {
		float st = in[1][i];
		if (edge(st, &n->s[2])) mode = 1.f;
		if (mode == 1.f) {
			v += up;
			if (v >= 1.f) { v = 1.f; mode = 2.f; }
		} else if (mode == 2.f) {
			v -= dn;
			if (v <= 0.f) {
				v = 0.f; mode = 0.f; et = PULSE_SAMPLES;
				if (st > 0.5f) mode = 1.f;
			}
		} else {
			float t = in[0][i];
			v = t > v ? scq_min(t, v + up) : scq_max(t, v - dn);
		}
		n->out[0][i] = v;
		n->out[1][i] = et > 0.f ? 1.f : 0.f;
		if (et > 0.f) et -= 1.f;
	}
	n->s[0] = v; n->s[1] = mode; n->s[3] = et;
}

/* at rest the slope has "ended": one end pulse at the start, so end -> start begins to climb */
static void init_stairs(node *n) { n->s[3] = PULSE_SAMPLES; }

/* jakes — they look like people but aren't: smooth wandering random */
static void run_jakes(node *n, const float *const in[MAX_IN])
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

/* connors — the other ones: stepped random, held until the next pulse */
static void run_connors(node *n, const float *const in[MAX_IN])
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

/* escalator — steps that move smoothly (Serge's smooth & stepped) */
static void run_escalator(node *n, const float *const in[MAX_IN])
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

/* train — the train and its stations. Stations are written in semitones. */
static uint32_t mem_train(float nature) { (void)nature; return 33 * sizeof(float); }
static void init_train(node *n)
{
	float *m = (float *)n->mem;
	static const float def[8] = { 0, 3, 7, 10, 12, 10, 7, 3 };
	if (m[32] == 0.f) {                 /* no stations written: a default route */
		int c = natural(n->nature, 1, 32);
		for (int k = 0; k < c; k++) m[k] = def[k % 8];
		m[32] = (float)c;
	}
}
static void run_train(node *n, const float *const in[MAX_IN])
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

/* pufferfish — the wet gilled box that plays quantized notes */
static const float PF_SCALE[5][9] = {
	{ 7, 0, .169925f, .321928f, .415037f, .584963f, .736966f, .906891f },          /* just major  */
	{ 7, 0, .169925f, .263034f, .415037f, .584963f, .678072f, .847997f },          /* just minor  */
	{ 5, 0, .169925f, .321928f, .584963f, .736966f },                              /* pentatonic  */
	{ 8, 0, .169925f, .321928f, .459432f, .584963f, .700440f, .807355f, .906891f }, /* otonal 8-15 */
	{ 8, 0, .093109f, .192645f, .299560f, .415037f, .540568f, .678072f, .830075f }, /* utonal      */
};
static void run_pufferfish(node *n, const float *const in[MAX_IN])
{
	const float *sc = PF_SCALE[natural(n->nature, 1, 5) - 1];
	int cnt = (int)sc[0];
	float last = n->s[0], pt = n->s[1];
	LOOP {
		float x = in[0][i], o = scq_floor(x), fr = x - o;
		float best = 0.f, bd = 2.f;
		for (int k = 0; k < cnt; k++) {
			float d = scq_abs(fr - sc[1 + k]);
			if (d < bd) { bd = d; best = sc[1 + k]; }
		}
		if (1.f - fr < bd) best = 1.f;
		float q = o + best;
		if (q != last) { last = q; pt = PULSE_SAMPLES; }
		n->out[0][i] = q;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = last; n->s[1] = pt;
}

/* cat — the cat that teaches expressions you mirror back: learns a gesture, repeats it */
#define CAT_LEN 6000
static uint32_t mem_cat(float nature) { (void)nature; return CAT_LEN * sizeof(int16_t); }
static void run_cat(node *n, const float *const in[MAX_IN])
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

/* magician — shushes you into a trance: a ducker and a slow swing */
static void run_magician(node *n, const float *const in[MAX_IN])
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

/* bottle — a kept value (the water bottle). bottle 40 = 0.40 */
static void run_bottle(node *n, const float *const in[MAX_IN])
{
	(void)in;
	LOOP n->out[0][i] = n->nature * 0.01f;
}

/* ------------------------------------------------------------------ hands */

static void run_lean(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int k = natural(n->nature, 1, 4) - 1;
	float v = n->s[0], t = W.ctl[SCQ_CTL_LEAN1 + k];
	LOOP { v += (t - v) * 0.004f; n->out[0][i] = v; }
	n->s[0] = v;
}
static void run_door(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_DOOR1 + natural(n->nature, 1, 4) - 1];
	LOOP n->out[0][i] = v;
}
static void run_lift(node *n, const float *const in[MAX_IN])
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
static void run_wake(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_WAKE];
	LOOP n->out[0][i] = v;
}
static void run_lucid(node *n, const float *const in[MAX_IN])
{
	(void)in;
	float v = W.ctl[SCQ_CTL_LUCID];
	LOOP n->out[0][i] = v;
}

/* ------------------------------------------------------------------ ears */

static void run_left(node *n, const float *const in[MAX_IN])  { (void)n; LOOP W.outL[i] += in[0][i]; }
static void run_right(node *n, const float *const in[MAX_IN]) { (void)n; LOOP W.outR[i] += in[0][i]; }
static void run_ears(node *n, const float *const in[MAX_IN])  { (void)n; LOOP { W.outL[i] += in[0][i]; W.outR[i] += in[0][i]; } }

/* lamp — red while charging, then white: the eight LEDs */
static void run_lamp(node *n, const float *const in[MAX_IN])
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

/* journey — riding between scenes. Sometimes you miss your stop. */
static void run_journey(node *n, const float *const in[MAX_IN])
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

#define P(name, fam, flags, cost, nat, ins, defs, outs, run, init, mem, help) \
	{ name, fam, flags, cost, nat, ins, defs, outs, run, init, mem, help }

const place SCQ_PLACES[] = {
P("aira",      FAM_SOURCE, PF_DRIFT, 100, 3, "pitch fm", "0 0", "out", run_aira, 0, 0,
  "the fountain labeled aira: a pure tone, nature x 60 Hz"),
P("twin",      FAM_SOURCE, PF_DRIFT, 207, 3, "pitch index timbre", "0 0 0", "out a b", run_twin, 0, 0,
  "two partials, 60n and 60(n+1); index bends a with b, timbre folds it. Their difference is the hum"),
P("rain",      FAM_SOURCE, PF_DRIFT, 60, 1, "density", "0", "out pulse", run_rain, 0, 0,
  "drops on tiny resonators, 8 x nature per second; pulse marks each drop"),
P("thunder",   FAM_SOURCE, 0, 113, 1, "strike", "0", "out", run_thunder, 0, 0,
  "a rumble when struck, and on its own about nature times a minute"),
P("flood",     FAM_SOURCE, PF_DRIFT, 87, 1, "level", "0.5", "out", run_flood, 0, 0,
  "rising water: noise whose band climbs with the level"),
P("wind",      FAM_SOURCE, PF_DRIFT, 87, 1, "gust", "0.3", "out", run_wind, 0, 0,
  "a cold draft: soft noise through a wandering band"),
P("frog",      FAM_SOURCE, 0, 140, 3, "wake pitch", "0 0", "out pulse", run_frog, init_frog, mem_frog,
  "nature frogs (1-8) croaking on their own; a pulse into wake makes them all croak"),
P("elevator",  FAM_SOURCE, PF_DRIFT, 240, 1, "rate pitch", "0.1 0", "out", run_elevator, 0, 0,
  "the lift into the night sky: rises forever (rate in octaves per second, negative falls)"),
P("bell",      FAM_SOURCE, PF_DRIFT, 220, 1, "strike pitch decay", "0 0 0.5", "out", run_bell, 0, 0,
  "the cargo train's deep bell: four modes, struck by edges or by sound"),
P("mountains", FAM_SHAPE, PF_DRIFT, 87, 1, "in fold", "0 0", "out", run_mountains, 0, 0,
  "dark umber peaks: a wavefolder. More fold, more peaks"),
P("window",    FAM_SHAPE, 0, 73, 1, "in light", "0 1", "out", run_window, 0, 0,
  "a lamp on a photocell (lowpass gate). Pulse the light for a pluck"),
P("basement",  FAM_SHAPE, PF_DRIFT, 100, 1, "in cut res", "0 0 0.2", "out band", run_basement, 0, 0,
  "the house show downstairs: a warm resonant lowpass at nature x 480 Hz"),
P("horns",     FAM_SHAPE, PF_DRIFT, 47, 1, "in band1 band2 band3 band4 band5 band6 band7 band8",
  "0 0 0 0 0 0 0 0 0", "out", run_horns, init_horns, mem_horns,
  "horn-loaded speakers: eight fixed resonant bands; patch into a band to raise it"),
P("lighter",   FAM_SHAPE, 0, 100, 1, "in flick", "0 0", "out", run_lighter, 0, 0,
  "the lighter that explodes: a flick of fire and drive"),
P("copper",    FAM_SHAPE, 0, 33, 1, "in", "0", "out", run_copper, 0, 0,
  "the grounding rod: takes the DC out"),
P("blanket",   FAM_SHAPE, 0, 33, 1, "in gain", "0 1", "out", run_blanket, 0, 0,
  "a blanket over the sound: plain amplifier"),
P("crystal",   FAM_SHAPE, 0, 87, 0, "in1 in2 in3 in4 turn", "0 0 0 0 0", "out1 out2 out3 out4", run_crystal, 0, 0,
  "the crystal matrix: four in, four out, mixed by turning (nature sets the resting angle)"),
P("tunnel",    FAM_SPACE, PF_DRIFT, 113, 2, "in pitch decay", "0 0 0.5", "out", run_tunnel, 0, mem_tunnel,
  "tubular acoustics: a comb tuned to nature x 60 Hz. Pulses pluck it"),
P("hallway",   FAM_SPACE, PF_DRIFT, 180, 6, "in length feedback", "0 1 0.3", "out", run_hallway, init_hallway, mem_hallway,
  "the long grey hallway: a delay, nature x 50 ms long"),
P("warehouse", FAM_SPACE, PF_DRIFT, 353, 2, "in size", "0 0.7", "out l r", run_warehouse, 0, mem_warehouse,
  "a big room with wooden floors: reverb"),
P("moss",      FAM_SPACE, 0, 287, 1, "in bloom", "0 0.3", "out", run_moss, init_moss, mem_moss,
  "golden moss in a night pond: grains of what you plant, or of itself"),
P("stairs",    FAM_CURRENT, 0, 100, 1, "in start rise fall", "0 0 0.3 0.5", "out end", run_stairs, init_stairs, 0,
  "the spiral staircase (universal slope): rises and falls when started, follows otherwise; end into start cycles"),
P("jakes",     FAM_CURRENT, PF_DRIFT, 100, 1, "rate follow", "0 0", "out", run_jakes, 0, 0,
  "they look like people but aren't: smooth wandering random, or an imitation of follow"),
P("connors",   FAM_CURRENT, PF_DRIFT, 47, 1, "clock", "0", "out pulse", run_connors, 0, 0,
  "stepped random, new on each clock pulse (or nature / 2 times a second on its own)"),
P("escalator", FAM_CURRENT, 0, 60, 1, "in clock glide", "0 0 0.2", "out", run_escalator, 0, 0,
  "smooth & stepped: holds the input on each pulse and glides to it"),
P("train",     FAM_CURRENT, 0, 127, 8, "next back reset", "0 0 0", "out pulse step", run_train, init_train, mem_train,
  "the train and its stations (semitones): the train route is train 8: 0 3 7 10"),
P("pufferfish",FAM_CURRENT, 0, 207, 1, "in", "0", "out pulse", run_pufferfish, 0, 0,
  "plays quantized notes: just major, just minor, pentatonic, otonal, utonal (nature 1-5)"),
P("cat",       FAM_CURRENT, 0, 33, 1, "in learn", "0 0", "out", run_cat, 0, mem_cat,
  "learns a gesture while learn is held, then mirrors it back, over and over (up to 8 s)"),
P("magician",  FAM_CURRENT, PF_DRIFT, 113, 1, "in shush", "0 0", "out trance", run_magician, 0, 0,
  "shushes the sound when shush is loud; trance is a slow swing"),
P("bottle",    FAM_CURRENT, 0, 20, 0, "", "", "out", run_bottle, 0, 0,
  "a kept value: bottle 40 is 0.40"),
#include "scq_more_table.inc"
P("lean",      FAM_HANDS, PF_SINGLE | PF_COUNTED, 20, 1, "", "", "out", run_lean, 0, 0,
  "faders: lean1 - lean4, 0 to 1"),
P("door",      FAM_HANDS, PF_SINGLE | PF_COUNTED, 20, 1, "", "", "out", run_door, 0, 0,
  "track buttons: door1 - door4, 1 while held"),
P("lift",      FAM_HANDS, PF_SINGLE, 20, 0, "", "", "out up down", run_lift, 0, 0,
  "the rocker: out is -1, 0 or 1; up and down pulse"),
P("wake",      FAM_HANDS, PF_SINGLE, 20, 0, "", "", "out", run_wake, 0, 0,
  "PLAY, 1 while held"),
P("lucid",     FAM_HANDS, PF_SINGLE, 20, 0, "", "", "out", run_lucid, 0, 0,
  "the dots button, 1 while held"),
P("left",      FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_left, 0, 0, "left ear"),
P("right",     FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_right, 0, 0, "right ear"),
P("ears",      FAM_EARS, PF_SINGLE, 20, 0, "in", "0", "", run_ears, 0, 0, "both ears"),
P("lamp",      FAM_EARS, PF_SINGLE, 40, 0, "in track play", "0 0 0", "out", run_lamp, 0, 0,
  "the eight LEDs: sound lights both rows as a bar; track and play light one row each"),
P("journey",   FAM_EARS, PF_SINGLE, 20, 0, "next back miss", "0 0 0", "", run_journey, 0, 0,
  "rides to the next or last scene on a pulse; miss is the chance of the wrong stop"),
};
const int SCQ_NPLACES = (int)(sizeof(SCQ_PLACES) / sizeof(SCQ_PLACES[0]));

/* port lookup: index of `port` in the place's ins (out=0) or outs (out=1), -1 if absent */
int place_port(const place *p, int out, const char *port, int len)
{
	const char *s = out ? p->outs : p->ins;
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
