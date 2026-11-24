/*
 * Time (the Wingie collection, 2026-11-24): delays, loops, grains, spectra, reverbs and space.
 * Included by scq_places.c after scq_more_spaces.inc (ring16 comes from there).
 *
 * Every memory is 16-bit, or 4-bit ADPCM where a long memory matters more than the last bits
 * (tapeloop), and none takes more than about 40 KB of the house. The echoes keep every
 * other sample (the mean of each pair): twice the time in the same room, with 12 kHz of
 * bandwidth, which is what the old samplers and Clouds' lo-fi mode did. Times are counted in
 * the place's own samples, so samplerate=half makes every one of them twice as long.
 *
 * Helpers here are tm_ (time) so they never meet the other collections' names.
 */

static inline float tm_in(float x) { return scq_clamp(x, -4.f, 4.f); }   /* a sound inlet, made safe */
static inline float tm_c01(float x) { return scq_clamp(x, 0.f, 1.f); }
static inline int tm_edge(float x, float *prev)
{
	int on = x > 0.5f, e = on && *prev <= 0.5f;
	*prev = on ? 1.f : 0.f;                 /* (never the raw value: a poisoned inlet stays out of the state) */
	return e;
}
static inline float tm_flush(float x) { return scq_abs(x) < 1e-15f ? 0.f : x; }
static inline float tm_frac(float x) { return x - scq_floor(x); }

/* equal-power pan gains for p in -1..1 */
static inline void tm_pan(float p, float *l, float *r)
{
	float a = (scq_clamp(p, -1.f, 1.f) + 1.f) * 0.125f;
	*l = scq_sin2pi(a + 0.25f);
	*r = scq_sin2pi(a);
}

/* A ring that keeps every other sample. h[0] write index, h[1] the first of the pair, h[2] 0/1. */
static inline void tm_h2_put(int16_t *b, uint32_t L, float *h, float x)
{
	if (h[2] == 0.f) { h[1] = x; h[2] = 1.f; return; }
	uint32_t w = (uint32_t)h[0];
	if (w >= L) w = 0;
	b[w] = to16(0.5f * (h[1] + x));
	h[0] = (float)(w + 1 < L ? w + 1 : 0);
	h[2] = 0.f;
}
/* what came in D samples ago (D >= 2), between the kept ones */
static inline float tm_h2_get(const int16_t *b, uint32_t L, const float *h, float D)
{
	float dc = scq_clamp(0.5f * (D + 1.5f - h[2]), 1.f, (float)L - 2.f);
	uint32_t w = (uint32_t)h[0];
	return ring16(b, L, w < L ? w : 0, dc);
}
static inline float tm_h2_max(uint32_t L) { return 2.f * (float)(L - 4); }   /* the longest D */

/* a Schroeder allpass on a 16-bit line */
static inline float tm_ap16(int16_t *line, uint32_t p, float x, float g)
{
	float z = FROM16(line[p]);
	float v = x + g * z;
	line[p] = to16(v);
	return z - g * v;
}

/* a soft knee: straight to 0.7, then bending to 1 (overdubs that pile up) */
static inline float tm_knee(float v)
{
	float a = scq_abs(v);
	if (a < 0.7f) return v;
	float k = 0.7f + 0.3f * scq_tanh((a - 0.7f) * 3.333f);
	return v < 0.f ? -k : k;
}

/* ---- IMA ADPCM: four bits a sample ---- */
static const int16_t TM_STEP[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
	73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
	494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
	2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
	10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
static const int8_t TM_STEPIX[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
typedef struct { int32_t pred, ix; } tm_adpcm;
static inline void tm_adpcm_load(tm_adpcm *a, int16_t pred, int16_t ix)
{
	a->pred = pred;
	a->ix = ix < 0 ? 0 : (ix > 88 ? 88 : ix);
}
static inline float tm_adpcm_dec(tm_adpcm *a, int code)
{
	int32_t ix = a->ix < 0 ? 0 : (a->ix > 88 ? 88 : a->ix);
	int32_t step = TM_STEP[ix], diff = step >> 3;
	if (code & 4) diff += step;
	if (code & 2) diff += step >> 1;
	if (code & 1) diff += step >> 2;
	int32_t p = (code & 8) ? a->pred - diff : a->pred + diff;
	a->pred = p > 32767 ? 32767 : (p < -32768 ? -32768 : p);
	ix += TM_STEPIX[code & 15];
	a->ix = ix < 0 ? 0 : (ix > 88 ? 88 : ix);
	return (float)a->pred * (1.f / 32768.f);
}
static inline int tm_adpcm_enc(tm_adpcm *a, float x)
{
	int32_t ix = a->ix < 0 ? 0 : (a->ix > 88 ? 88 : a->ix);
	int32_t step = TM_STEP[ix], d = (int32_t)(scq_clamp(x, -1.f, 1.f) * 32767.f) - a->pred;
	int code = 0;
	if (d < 0) { code = 8; d = -d; }
	if (d >= step) { code |= 4; d -= step; }
	step >>= 1;
	if (d >= step) { code |= 2; d -= step; }
	step >>= 1;
	if (d >= step) code |= 1;
	tm_adpcm_dec(a, code);
	return code;
}
static inline int tm_nib(const uint8_t *b, uint32_t i) { uint8_t v = b[i >> 1]; return (i & 1u) ? v >> 4 : v & 15; }
static inline void tm_nib_put(uint8_t *b, uint32_t i, int c)
{
	uint8_t v = b[i >> 1];
	b[i >> 1] = (i & 1u) ? (uint8_t)((v & 15) | ((c & 15) << 4)) : (uint8_t)((v & 0xf0) | (c & 15));
}

/* ------------------------------------------------------------------ beauty */

/* beauty — a hallway of mirrors that breathe: eight taps across a span of time, each drifting in
 * level, pan and brightness on its own; spread fans them out, jitter makes their times wander
 * (after ppooll's Beauty.act) */
#define BTY_L 19600u
#define BTY_T 8
#define BTY_F 12      /* per tap: wander cur/target, level cur/target, pan cur/target, tone cur/target, lowpass, timer, delay now */
static uint32_t mem_beauty(float nature) { (void)nature; return BTY_L * sizeof(int16_t) + BTY_T * BTY_F * sizeof(float); }
static void bty_target(node *n, float *t, float jit)
{
	t[1] = scq_noise(&n->rng);
	t[3] = 0.25f + 0.75f * scq_rand01(&n->rng);
	t[5] = scq_noise(&n->rng);
	t[7] = scq_coef_hz(500.f * scq_exp2(5.f * scq_rand01(&n->rng)), SR);   /* 500 Hz to 16 kHz */
	t[9] = (0.2f + 2.2f * scq_rand01(&n->rng)) / (0.35f + jit) * SR;
}
static void init_beauty(node *n)
{
	float *tap = (float *)((int16_t *)n->mem + BTY_L);
	for (int k = 0; k < BTY_T; k++) {
		float *t = tap + k * BTY_F;
		bty_target(n, t, 0.5f);
		t[0] = t[1]; t[2] = t[3]; t[4] = t[5]; t[6] = t[7];
		t[9] *= scq_rand01(&n->rng);
	}
}
static void run_beauty(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	float *tap = (float *)(buf + BTY_L);
	float *h = &n->s[0];                                   /* 0-2 the ring */
	const float maxd = tm_h2_max(BTY_L);
	const float jit = tm_c01(in[4][0]), fb = tm_c01(in[3][0]) * 1.1f;
	float want = 0.02f * SR + tm_c01(in[1][0]) * (maxd - 0.02f * SR);
	if (n->s[3] <= 0.f) { n->s[3] = want; n->s[4] = tm_c01(in[2][0]); }
	n->s[3] += (want - n->s[3]) * 0.03f;                   /* the span glides: tape-like swoops */
	n->s[4] += (tm_c01(in[2][0]) - n->s[4]) * 0.03f;
	const float span = n->s[3], spr = n->s[4];
	const float gc = scq_min((float)B * INV_SR * 4.f, 1.f);   /* the drift glides over a quarter second */
	const float depth = 0.35f + 0.65f * jit;
	float dcur[BTY_T], dstep[BTY_T], gl[BTY_T], gr[BTY_T], cf[BTY_T], lp[BTY_T], lv[BTY_T], lvsum = 0.f;
	for (int k = 0; k < BTY_T; k++) {
		float *t = tap + k * BTY_F;
		t[9] -= (float)B;
		if (!(t[9] > 0.f)) bty_target(n, t, jit);
		for (int j = 0; j < 8; j += 2) t[j] += (t[j + 1] - t[j]) * gc;
		float pos = 1.f - spr * (1.f - (float)(k + 1) * (1.f / BTY_T));
		float d = scq_clamp(2.f + span * pos * (1.f + 0.22f * jit * t[0]), 2.f, maxd);
		if (!(t[10] >= 2.f && t[10] <= maxd)) t[10] = d;
		dcur[k] = t[10];
		dstep[k] = (d - t[10]) / (float)B;
		t[10] = d;
		lv[k] = 1.f - (1.f - t[2]) * depth;
		lvsum += lv[k];
		tm_pan(t[4] * (0.3f + 0.7f * depth), &gl[k], &gr[k]);
		gl[k] *= lv[k]; gr[k] *= lv[k];
		cf[k] = 1.f - (1.f - t[6]) * depth;
		lp[k] = t[8];
	}
	const float fbn = fb * 0.3f / scq_max(lvsum, 1.f), fbl = fb / scq_max(lv[BTY_T - 1], 0.25f);
	float hp = n->s[5], aa = n->s[6];
	LOOP {
		float L = 0.f, R = 0.f, sum = 0.f;
		for (int k = 0; k < BTY_T; k++) {
			float y = tm_h2_get(buf, BTY_L, h, dcur[k]);
			dcur[k] += dstep[k];
			lp[k] += cf[k] * (y - lp[k]);
			L += lp[k] * gl[k];
			R += lp[k] * gr[k];
			sum += lp[k] * lv[k];
		}
		float f = sum * fbn + lp[BTY_T - 1] * lv[BTY_T - 1] * fbl;   /* most of it from the last tap, the rest from all */
		hp += 0.004f * (f - hp);
		float x = scq_tanh(tm_in(in[0][i]) + f - hp);
		aa += 0.7f * (x - aa);
		tm_h2_put(buf, BTY_L, h, aa);
		n->out[0][i] = 0.36f * L;
		n->out[1][i] = 0.36f * R;
	}
	for (int k = 0; k < BTY_T; k++) tap[k * BTY_F + 8] = tm_flush(lp[k]);
	n->s[5] = tm_flush(hp); n->s[6] = tm_flush(aa);
}

/* ------------------------------------------------------------------ pt2399 */

/* pt2399 — the little black chip in the Chloe: companded, a lowpass that closes and grit that
 * grows as the time gets longer, the clock's whine, and a scream when fed back too far (after the
 * Princeton PT2399 echo chip) */
#define PT_L 16320u     /* 340 ms at 48 kHz */
static uint32_t mem_pt2399(float nature) { (void)nature; return PT_L * sizeof(int16_t); }
static void run_pt2399(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	/* s: 0 w, 1 delay, 2 compressor env, 3 its root, 4 expander env, 5-6 pre lowpass, 7-8 svf,
	 * 9 post pole, 10 held sample, 11 hold phase, 12 whine phase, 13 loop highpass, 14 fed back */
	uint32_t w = (uint32_t)n->s[0];
	if (w >= PT_L) w = 0;
	const float dmax = (float)PT_L - 4.f;
	float want = scq_clamp((0.03f + 0.31f * tm_c01(in[1][0])) * SR, 8.f, dmax);
	float d = n->s[1];
	if (!(d >= 8.f && d <= dmax)) d = want;
	const float fb = tm_c01(in[2][0]) * 1.2f;
	/* the chip's clock slows as the time grows: everything is set by r, 1 at 30 ms */
	float r = scq_max(d / (0.03f * SR), 1.f);
	float lr = scq_log2(r);
	float fc = scq_min(11000.f * scq_exp2(-0.65f * lr), 0.4f * SR);
	float pc = scq_coef_hz(fc * 1.3f, SR);
	float g = scq_tan(SCQ_PI * fc * INV_SR), kq = 1.2f;
	float a1 = 1.f / (1.f + g * (g + kq)), a2 = g * a1, a3 = g * a2;
	float sh = scq_clamp(1.6f / r, 0.12f, 1.f);
	float long_ = scq_clamp((r - 1.f) * (1.f / 10.3f), 0.f, 1.f);
	float qstep = scq_exp2(-(13.f - 7.f * long_));
	float wf = scq_min(1300.f / scq_sqrt_from(d * INV_SR, 0.2f), 0.45f * SR) * INV_SR;
	float wamp = 0.006f * long_ * long_, namp = 0.002f + 0.004f * long_;
	const float glide = 1.f / (0.06f * SR);
	float ec = n->s[2], sq = n->s[3], ee = n->s[4], p1 = n->s[5], p2 = n->s[6], ic1 = n->s[7], ic2 = n->s[8];
	float p3 = n->s[9], held = n->s[10], hph = n->s[11], wph = n->s[12], hp = n->s[13], back = n->s[14];
	LOOP {
		float x = scq_tanh(tm_in(in[0][i]) + back * fb);          /* the op-amp in front clips */
		float ax = scq_abs(x);
		ec += (ax > ec ? 0.02f : 0.0015f) * (ax - ec);
		sq = scq_sqrt_from(ec + 0.002f, sq);
		float c = x * (0.25f / sq);                                 /* compressor: 2 to 1 */
		p1 += pc * (c - p1); p2 += pc * (p1 - p2);
		wph += wf; wph -= scq_floor(wph);
		float v = p2 + wamp * (1.f + 3.f * ec) * scq_sin2pi(wph) + namp * scq_noise(&n->rng);
		v = scq_floor(v / qstep + 0.5f) * qstep;                   /* the converter's few bits */
		d += (want - d) * glide;                                    /* turning time bends the pitch */
		float y = ring16(buf, PT_L, w, d);
		buf[w] = to16(v);
		if (++w >= PT_L) w = 0;
		hph += sh;
		if (hph >= 1.f) { hph -= 1.f; held = y; }                  /* the slow clock holds */
		float v3 = held - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		p3 += pc * (v2 - p3);
		float ay = scq_abs(p3);
		ee += (ay > ee ? 0.02f : 0.0015f) * (ay - ee);
		float z = p3 * scq_min(ee * 16.f, 8.f);                    /* expander: back up again */
		hp += 0.005f * (z - hp);
		back = z - hp;
		n->out[0][i] = z;
	}
	n->s[0] = (float)w; n->s[1] = d; n->s[2] = tm_flush(ec); n->s[3] = sq; n->s[4] = tm_flush(ee);
	n->s[5] = tm_flush(p1); n->s[6] = tm_flush(p2); n->s[7] = tm_flush(ic1); n->s[8] = tm_flush(ic2);
	n->s[9] = tm_flush(p3); n->s[10] = held; n->s[11] = hph; n->s[12] = wph; n->s[13] = tm_flush(hp); n->s[14] = tm_flush(back);
}

/* ------------------------------------------------------------------ dub */

/* dub — the engineer's hand on the send: only while throw is up does the sound go into the echo;
 * inside the loop a spring splash, a saturating stage and a tone that cuts both ends, so it can
 * run away and be pulled back (after King Tubby, Lee Perry and the sound systems; Space Echo and
 * Echoplex) */
#define DUB_L 19000u
static const uint16_t DUB_APL[3] = { 113, 241, 347 };
#define DUB_APN (113u + 241u + 347u)
static uint32_t mem_dub(float nature) { (void)nature; return (DUB_L + DUB_APN) * sizeof(int16_t); }
static void run_dub(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem, *sp = buf + DUB_L;
	float *h = &n->s[0];
	/* s: 0-2 ring, 3 time, 4 send, 5-6 lowpass, 7 highpass, 8 anti-alias, 9 splash count, 10-15 chirp */
	const float maxd = tm_h2_max(DUB_L);
	float want = 0.05f * SR + tm_c01(in[2][0]) * (maxd - 0.05f * SR);
	float t = n->s[3];
	if (!(t >= 2.f && t <= maxd)) t = want;
	float tone = tm_c01(in[4][0]);
	float lpc = scq_coef_hz(500.f * scq_exp2(tone * 3.7f), SR), hpc = scq_coef_hz(35.f * scq_exp2(tone * 3.f), SR);
	float fb = tm_c01(in[3][0]);
	const float glide = 1.f / (0.25f * SR), sc = scq_coef_time(0.004f, SR);
	float send = n->s[4], l1 = n->s[5], l2 = n->s[6], hp = n->s[7], aa = n->s[8];
	uint32_t cnt = (uint32_t)n->s[9];
	float *ch = &n->s[10];
	LOOP {
		float x = tm_in(in[0][i]);
		send += (tm_c01(in[1][i]) - send) * sc;
		t += (want - t) * glide;                               /* a twist of the time knob: the pitch swoops */
		float y = tm_h2_get(buf, DUB_L, h, t);
		l1 += lpc * (y - l1); l2 += lpc * (l1 - l2);
		hp += hpc * (l2 - hp);
		float v = scq_tanh(1.25f * (l2 - hp));
		float u = v;
		for (int k = 0; k < 6; k++) {                          /* a spring's chirp */
			float o = -0.55f * u + ch[k];
			ch[k] = u + 0.55f * o;
			u = o;
		}
		int16_t *ap = sp;
		for (int k = 0; k < 3; k++) {                          /* and its splash */
			u = tm_ap16(ap, cnt % DUB_APL[k], u, 0.6f);
			ap += DUB_APL[k];
		}
		if (++cnt >= 113u * 241u * 347u) cnt = 0;
		v = 0.6f * v + 0.4f * u;
		float wv = scq_tanh(x * send + fb * v);
		aa += 0.7f * (wv - aa);
		tm_h2_put(buf, DUB_L, h, aa);
		n->out[0][i] = x + y;
		n->out[1][i] = y;
	}
	n->s[3] = t; n->s[4] = send; n->s[5] = tm_flush(l1); n->s[6] = tm_flush(l2); n->s[7] = tm_flush(hp);
	n->s[8] = tm_flush(aa); n->s[9] = (float)cnt;
	for (int k = 0; k < 6; k++) ch[k] = tm_flush(ch[k]);
}

/* ------------------------------------------------------------------ reverse */

/* reverse — the tape turned over: each slice of what you play comes back backwards, two heads
 * crossfading so there is no seam; feedback= sends the reversed sound round again (after the
 * reverse delays of the Eventide H3000, Boss RV-2 and Line 6 DL4) */
#define REV_L 19600u
static uint32_t mem_reverse(float nature) { (void)nature; return REV_L * sizeof(int16_t); }
static void run_reverse(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	float *h = &n->s[0];
	const float smax = (float)REV_L - 8.f;
	float want = scq_min(0.03f * SR + tm_c01(in[1][0]) * (smax - 0.03f * SR), smax);
	float S = n->s[4];
	if (!(S >= 8.f && S <= smax)) S = want;
	S += (want - S) * 0.1f;
	const float mix = tm_c01(in[2][0]), fbo = scq_clamp(n->opt[0], 0.f, 0.95f), inv = 1.f / S;
	float p = n->s[3], aa = n->s[5], wet = n->s[6];
	LOOP {
		float x = tm_in(in[0][i]);
		float v = scq_tanh(x + fbo * wet);
		aa += 0.7f * (v - aa);
		tm_h2_put(buf, REV_L, h, aa);
		p += inv;
		if (p >= 1.f) p -= 1.f;
		float q = p + 0.5f;
		if (q >= 1.f) q -= 1.f;
		float e0 = scq_sin2pi(0.5f * p), e1 = scq_sin2pi(0.5f * q);
		wet = tm_h2_get(buf, REV_L, h, 2.f + 2.f * S * p) * e0 * e0
		    + tm_h2_get(buf, REV_L, h, 2.f + 2.f * S * q) * e1 * e1;
		n->out[0][i] = x + (wet - x) * mix;
	}
	n->s[3] = p; n->s[4] = S; n->s[5] = tm_flush(aa); n->s[6] = tm_flush(wet);
}

/* ------------------------------------------------------------------ stutter */

/* stutter — the record skips: a pulse catches the last slice and repeats it, each repeat quieter
 * by decay (held high, it never fades); shrink= shortens and drop= (semitones) bends every
 * repeat (after Ableton's Beat Repeat, the Akai MPC's note repeat, jungle edits) */
#define STU_L 19200u
static uint32_t mem_stutter(float nature) { (void)nature; return STU_L * sizeof(int16_t); }
static void run_stutter(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	/* s: 0 w, 1 level, 2 pos, 3 len, 4 rate, 5 start, 6 active, 7 trig, 8 level heard */
	uint32_t w = (uint32_t)n->s[0];
	if (w >= STU_L) w = 0;
	float lvl = n->s[1], pos = n->s[2], len = n->s[3], rate = n->s[4], start = n->s[5], ls = n->s[8];
	int active = n->s[6] > 0.5f;
	const float shrink = scq_clamp(n->opt[0], 0.f, 0.9f), drop = scq_exp2(scq_clamp(n->opt[1], -24.f, 24.f) * (1.f / 12.f));
	const float decay = tm_c01(in[3][0]);
	const float L0 = scq_clamp(SR * (0.01f + 0.39f * tm_c01(in[2][0])), 96.f, (float)STU_L - 4.f);
	if (!(start >= 0.f && start < (float)STU_L)) start = 0.f;
	if (!(len >= 1.f && len <= (float)STU_L)) { len = 96.f; active = 0; }
	LOOP {
		float x = tm_in(in[0][i]);
		if (tm_edge(in[1][i], &n->s[7])) {
			start = (float)w - L0;
			if (start < 0.f) start += (float)STU_L;
			pos = 0.f; len = L0; rate = 1.f; lvl = 1.f; active = 1;
		}
		float rep = 0.f;
		if (active) {
			float rp = start + pos;
			if (rp >= (float)STU_L) rp -= (float)STU_L;
			uint32_t r0 = (uint32_t)rp, r1 = r0 + 1 < STU_L ? r0 + 1 : 0;
			if (r0 >= STU_L) r0 = 0;
			float fr = rp - (float)r0;
			float y = FROM16(buf[r0]) + (FROM16(buf[r1]) - FROM16(buf[r0])) * fr;
			float env = scq_clamp(scq_min(pos, len - pos) * (1.f / 48.f), 0.f, 1.f);
			rep = y * env;
			pos += rate;
			if (pos >= len) {
				pos -= len;
				len = scq_max(len * (1.f - shrink), 96.f);
				if (pos >= len) pos = 0.f;
				rate = scq_clamp(rate * drop, 0.0625f, 8.f);
				if (rate * len > (float)STU_L - 4.f) rate = ((float)STU_L - 4.f) / len;
				if (in[1][i] <= 0.5f) lvl *= decay;            /* held high: it keeps on */
				if (lvl < 0.01f) { active = 0; lvl = 0.f; }
			}
		} else {
			buf[w] = to16(x);
			if (++w >= STU_L) w = 0;
		}
		ls += ((active ? lvl : 0.f) - ls) * 0.01f;
		n->out[0][i] = x + (rep - x) * ls;
	}
	n->s[0] = (float)w; n->s[1] = lvl; n->s[2] = pos; n->s[3] = len; n->s[4] = rate; n->s[5] = start;
	n->s[6] = active ? 1.f : 0.f; n->s[8] = tm_flush(ls);
}
static void init_stutter(node *n) { n->s[3] = 96.f; n->s[4] = 1.f; }

/* ------------------------------------------------------------------ looper */

/* looper — a loop pedal: hold record to catch a loop (its length is how long you held), overdub
 * to add to it, speed to play it slower, faster or backwards (negative), erase to start again;
 * position is where it is (0 to 1), end pulses at each turn (after the Echoplex Digital Pro,
 * Boomerang and the Lexicon JamMan) */
#define LOOP_L 20000u
static uint32_t mem_looper(float nature) { (void)nature; return LOOP_L * sizeof(int16_t); }
static void run_looper(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	/* s: 0 mode (0 empty, 1 recording, 2 playing), 1 length, 2 pos, 3 record, 4 erase, 5 end pulse, 6 overdub, 7 recorded */
	int mode = (int)n->s[0];
	uint32_t len = (uint32_t)n->s[1], rc = (uint32_t)n->s[7];
	float pos = n->s[2], pt = n->s[5], ov = n->s[6];
	if (len > LOOP_L || rc > LOOP_L || mode < 0 || mode > 2 || (mode == 2 && len < 2)) { mode = 0; len = 0; rc = 0; }
	if (!(pos >= 0.f && pos < (float)(len ? len : 1))) pos = 0.f;
	LOOP {
		float x = tm_in(in[0][i]);
		if (tm_edge(in[4][i], &n->s[4])) { mode = 0; len = 0; pos = 0.f; }
		int rec = in[1][i] > 0.5f;
		if (rec && n->s[3] <= 0.5f) { mode = 1; rc = 0; }
		n->s[3] = rec ? 1.f : 0.f;
		float y = 0.f;
		if (mode == 1) {
			if (rec && rc < LOOP_L) buf[rc++] = to16(x);
			else {                                     /* the end folds into the start: no click at the seam */
				uint32_t F = rc / 4 < 240u ? rc / 4 : 240u;
				for (uint32_t j = 0; j < F; j++) {
					float t = ((float)j + 0.5f) / (float)F;
					buf[j] = to16(FROM16(buf[j]) * t + FROM16(buf[rc - F + j]) * (1.f - t));
				}
				len = rc - F;
				mode = len >= 2 ? 2 : 0;
				pos = 0.f;
			}
		}
		ov += ((in[2][i] > 0.5f ? 1.f : 0.f) - ov) * 0.005f;
		if (mode == 2) {
			uint32_t r0 = (uint32_t)pos;
			if (r0 >= len) r0 = 0;
			uint32_t r1 = r0 + 1 < len ? r0 + 1 : 0;
			float fr = pos - (float)r0;
			y = FROM16(buf[r0]) + (FROM16(buf[r1]) - FROM16(buf[r0])) * fr;
			if (ov > 0.001f) buf[r0] = to16(tm_knee(FROM16(buf[r0]) + x * ov));
			float sp = scq_clamp(in[3][i], -4.f, 4.f);
			y *= scq_min(scq_abs(sp) * 20.f, 1.f);         /* a stopped tape is silent, not a held level */
			pos += sp;
			if (pos >= (float)len || pos < 0.f) {
				pos -= (float)len * scq_floor(pos / (float)len);
				if (!(pos >= 0.f && pos < (float)len)) pos = 0.f;
				pt = PULSE_SAMPLES;
			}
		}
		n->out[0][i] = y;
		n->out[1][i] = mode == 2 ? pos / (float)len : 0.f;
		n->out[2][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = (float)mode; n->s[1] = (float)len; n->s[2] = pos; n->s[5] = pt; n->s[6] = tm_flush(ov); n->s[7] = (float)rc;
}

/* ------------------------------------------------------------------ tapeloop */

/* tapeloop — six seconds of tape in a loop, for the voice: punch records over what is there
 * (some of the old stays), age wears it (each pass duller, quieter, more hiss, more wow and
 * flutter), speed runs the motor faster or slower (octaves: up is higher). Left is the play head
 * just after the record head, right a second head further round. The tape is 4-bit ADPCM, so
 * six seconds fit; seconds= makes it shorter and clearer (after Frippertronics, Eno's Discreet
 * Music, William Basinski, Pauline Oliveros' tape delays) */
#define TL_N   76800u
#define TL_BLK 256u
#define TL_NB  (TL_N / TL_BLK)
typedef struct {
	tm_adpcm old, rec, hl, hr;      /* the record head reads the old tape and writes the new; two play heads */
	uint32_t a;                     /* the record head's cell */
	float yl0, yl1, yr0, yr1;       /* the last two cells each play head read */
	float acc, cnt, age;            /* what came in since the last cell; the generation lowpass */
} tl_state;
static uint32_t mem_tapeloop(float nature) { (void)nature; return TL_N / 2 + TL_NB * 2 * sizeof(int16_t) + sizeof(tl_state); }
static void tl_step(node *n, uint8_t *tape, int16_t *hdr, tl_state *st, float keep, float ca, float punch)
{
	uint32_t a = st->a < TL_N ? st->a : 0;
	uint32_t gl = 8u * TL_BLK;
	uint32_t gr = (uint32_t)(scq_clamp(n->opt[1], 0.05f, 0.95f) * (float)TL_NB + 0.5f) * TL_BLK;
	if (gr < TL_BLK) gr = TL_BLK;
	if (a % TL_BLK == 0) {
		uint32_t b = a / TL_BLK;
		tm_adpcm_load(&st->old, hdr[2 * b], hdr[2 * b + 1]);
		hdr[2 * b] = (int16_t)st->rec.pred; hdr[2 * b + 1] = (int16_t)st->rec.ix;
	}
	float old = tm_adpcm_dec(&st->old, tm_nib(tape, a));
	float x = st->cnt > 0.f ? st->acc / st->cnt : 0.f;
	st->acc = 0.f; st->cnt = 0.f;
	st->age += ca * (old - st->age);
	float nv = st->age * keep * (1.f - 0.45f * punch) + 0.75f * scq_tanh(1.5f * x) * punch;
	tm_nib_put(tape, a, tm_adpcm_enc(&st->rec, nv));
	uint32_t cl = (a + TL_N - gl) % TL_N, cr = (a + TL_N - gr) % TL_N;
	if (cl % TL_BLK == 0) tm_adpcm_load(&st->hl, hdr[2 * (cl / TL_BLK)], hdr[2 * (cl / TL_BLK) + 1]);
	if (cr % TL_BLK == 0) tm_adpcm_load(&st->hr, hdr[2 * (cr / TL_BLK)], hdr[2 * (cr / TL_BLK) + 1]);
	st->yl0 = st->yl1; st->yl1 = tm_adpcm_dec(&st->hl, tm_nib(tape, cl));
	st->yr0 = st->yr1; st->yr1 = tm_adpcm_dec(&st->hr, tm_nib(tape, cr));
	st->a = a + 1 < TL_N ? a + 1 : 0;
}
static void run_tapeloop(node *n, const float *const in[MAX_IN])
{
	uint8_t *tape = (uint8_t *)n->mem;
	int16_t *hdr = (int16_t *)(tape + TL_N / 2);
	tl_state *st = (tl_state *)(hdr + 2 * TL_NB);
	/* s: 0 between cells, 1 wow phase, 2 flutter phase, 3 wow wander, 4 its target, 5-6 hiss,
	 * 7 punch, 8-9 playback lowpass */
	const float secs = scq_clamp(n->opt[0], 0.5f, 12.f), crate = (float)TL_N / secs;
	const float age = tm_c01(in[3][0]);
	const float keep = 1.f - 0.07f * age, ca = 1.f - 0.5f * age;
	n->s[1] = tm_frac(n->s[1] + 0.55f * (float)B * INV_SR);
	n->s[2] = tm_frac(n->s[2] + 7.3f * (float)B * INV_SR);
	if (scq_rand01(&n->rng) < (float)B * INV_SR) n->s[4] = scq_noise(&n->rng);
	n->s[3] += (n->s[4] - n->s[3]) * scq_min((float)B * INV_SR * 1.5f, 1.f);
	float wob = (0.0012f + 0.006f * age) * (0.6f * scq_sin2pi(n->s[1]) + 0.4f * n->s[3])
	          + (0.0004f + 0.002f * age) * scq_sin2pi(n->s[2]);
	float spd = scq_exp2(scq_clamp(in[2][0], -2.f, 1.f));
	float v = crate * INV_SR * spd * (1.f + wob);
	float pc = scq_coef_hz(scq_min(0.42f * crate * spd, 0.45f * SR) * (1.f - 0.4f * age), SR);
	float hiss = 0.0025f + 0.012f * age;
	float fr = n->s[0], h1 = n->s[5], h2 = n->s[6], punch = n->s[7], ol = n->s[8], orr = n->s[9];
	if (!(fr >= 0.f && fr < 1.f)) fr = 0.f;
	LOOP {
		float x = tm_in(in[0][i]);
		punch += (tm_c01(in[1][i]) - punch) * 0.002f;
		st->acc += x; st->cnt += 1.f;
		fr += v;
		while (fr >= 1.f) { fr -= 1.f; tl_step(n, tape, hdr, st, keep, ca, punch); }
		float yl = st->yl0 + (st->yl1 - st->yl0) * fr, yr = st->yr0 + (st->yr1 - st->yr0) * fr;
		ol += pc * (yl - ol); orr += pc * (yr - orr);
		h1 += 0.5f * (scq_noise(&n->rng) - h1); h2 += 0.5f * (scq_noise(&n->rng) - h2);
		n->out[0][i] = ol + hiss * h1;
		n->out[1][i] = orr + hiss * h2;
	}
	n->s[0] = fr; n->s[5] = h1; n->s[6] = h2; n->s[7] = tm_flush(punch); n->s[8] = tm_flush(ol); n->s[9] = tm_flush(orr);
}

/* ------------------------------------------------------------------ eis */

/* eis — the cistern under Fort Worden: four delays whose times wander slowly, feeding each other
 * through a matrix and turning around the room; rate is how fast they wander, depth how far
 * (after Pauline Oliveros' Expanded Instrument System) */
static const uint16_t EIS_LEN[4] = { 3109, 4337, 5623, 6733 };   /* at a quarter of the rate: up to 0.56 s */
static const float EIS_MUL[4] = { 1.f, 1.37f, 0.73f, 1.91f };
#define EIS_TOTAL (3109u + 4337u + 5623u + 6733u)
static uint32_t mem_eis(float nature) { (void)nature; return EIS_TOTAL * sizeof(int16_t); }
static void run_eis(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem, *line[4];
	uint32_t off = 0;
	for (int k = 0; k < 4; k++) { line[k] = base + off; off += EIS_LEN[k]; }
	/* s: 0-1 count, 2 sub, 3 sum in, 4-5 anti-alias, 6-9 wander phases, 10-13 damping,
	 * 14-15 last out, 16-17 out now, 18 turning, 19-20 smoothing */
	uint32_t w = count_get(&n->s[0], &n->s[1]);
	float hz = 0.05f * scq_exp2(scq_clamp(in[1][0], 0.f, 4.f) * 2.f);
	float depth = 0.3f * tm_c01(in[2][0]), fb = tm_c01(in[3][0]);
	n->s[18] = tm_frac(n->s[18] + 0.5f * hz * (float)B * INV_SR);
	float gl[4], gr[4];
	for (int k = 0; k < 4; k++) {
		tm_pan(scq_sin2pi(n->s[18] + 0.25f * (float)k), &gl[k], &gr[k]);
		gl[k] *= 0.55f; gr[k] *= 0.55f;
	}
	const float dph = hz * 4.f * INV_SR;
	float sub = n->s[2], acc = n->s[3], a1 = n->s[4], a2 = n->s[5], pl = n->s[19], pr = n->s[20];
	LOOP {
		float x = tm_in(in[0][i]);
		a1 += 0.45f * (x - a1); a2 += 0.45f * (a1 - a2);
		acc += a2;
		sub += 1.f;
		if (sub >= 4.f) {                                  /* the network runs at a quarter of the rate */
			sub = 0.f;
			float xin = acc * 0.25f, y[4], f[4], m = 0.f;
			acc = 0.f;
			for (int k = 0; k < 4; k++) {
				float *ph = &n->s[6 + k];
				*ph = tm_frac(*ph + dph * EIS_MUL[k]);
				float t = 0.66f * (float)EIS_LEN[k] * (1.f + depth * scq_sin2pi(*ph));
				y[k] = ring16(line[k], EIS_LEN[k], w % EIS_LEN[k], scq_clamp(t, 1.f, (float)EIS_LEN[k] - 2.f));
				n->s[10 + k] += 0.6f * (y[k] - n->s[10 + k]);
				m += n->s[10 + k];
			}
			m *= 0.5f;
			for (int k = 0; k < 4; k++) f[k] = n->s[10 + k] - m;          /* Householder: every line hears the others */
			for (int k = 0; k < 4; k++)
				line[k][w % EIS_LEN[k]] = to16(scq_tanh(((k & 1) ? -0.7f : 0.7f) * xin + fb * f[(k + 1) & 3]));
			if (++w >= 1u << 30) w = 0;
			n->s[14] = n->s[16]; n->s[15] = n->s[17];
			n->s[16] = y[0] * gl[0] + y[1] * gl[1] + y[2] * gl[2] + y[3] * gl[3];
			n->s[17] = y[0] * gr[0] + y[1] * gr[1] + y[2] * gr[2] + y[3] * gr[3];
		}
		float t = sub * 0.25f;
		pl += 0.5f * (n->s[14] + (n->s[16] - n->s[14]) * t - pl);
		pr += 0.5f * (n->s[15] + (n->s[17] - n->s[15]) * t - pr);
		n->out[0][i] = pl;
		n->out[1][i] = pr;
	}
	count_put(w, &n->s[0], &n->s[1]);
	n->s[2] = sub; n->s[3] = acc; n->s[4] = tm_flush(a1); n->s[5] = tm_flush(a2);
	n->s[19] = tm_flush(pl); n->s[20] = tm_flush(pr);
	for (int k = 10; k < 18; k++) n->s[k] = tm_flush(n->s[k]);
}

/* ------------------------------------------------------------------ shimmer */

/* shimmer — a cathedral where every echo climbs: a reverb whose tail passes through an
 * octave-up pitch shifter on its way round (interval= in semitones); shimmer is how much climbs
 * (after Brian Eno and Daniel Lanois' shimmer, the Eventide H8000 and Valhalla Shimmer) */
static const uint16_t SHM_AP[4] = { 142, 107, 379, 277 };
static const uint16_t SHM_T[4] = { 2203, 2621, 3109, 3677 };
#define SHM_PS 4096u
#define SHM_TOTAL (142u + 107u + 379u + 277u + 2203u + 2621u + 3109u + 3677u + SHM_PS)
static uint32_t mem_shimmer(float nature) { (void)nature; return SHM_TOTAL * sizeof(int16_t) + 9 * sizeof(uint32_t); }
static void run_shimmer(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem, *ap[4], *tk[4], *ps;
	uint32_t off = 0;
	for (int k = 0; k < 4; k++) { ap[k] = base + off; off += SHM_AP[k]; }
	for (int k = 0; k < 4; k++) { tk[k] = base + off; off += SHM_T[k]; }
	ps = base + off;
	uint32_t *wi = (uint32_t *)(ps + SHM_PS);     /* write places: 4 allpasses, 4 tank lines, the shifter */
	for (int k = 0; k < 4; k++) {
		if (wi[k] >= SHM_AP[k]) wi[k] = 0;
		if (wi[4 + k] >= SHM_T[k]) wi[4 + k] = 0;
	}
	/* s: 2-5 damping, 6 shifter phase, 7-8 wander, 9 bandwidth, 10 shifted lowpass, 11 its highpass */
	const float size = tm_c01(in[1][0]), sh = tm_c01(in[2][0]);
	float rt = 1.5f * scq_exp2(size * 4.3f), g[4];
	for (int k = 0; k < 4; k++) g[k] = scq_exp2(-9.966f * (float)SHM_T[k] / (rt * SR));
	const float ratio = scq_exp2(scq_clamp(n->opt[0], -24.f, 24.f) * (1.f / 12.f));
	const float win = 2000.f, dph = (1.f - ratio) / win;
	n->s[7] = tm_frac(n->s[7] + 0.31f * (float)B * INV_SR);
	n->s[8] = tm_frac(n->s[8] + 0.47f * (float)B * INV_SR);
	const float m0 = 8.f * scq_sin2pi(n->s[7]), m1 = 8.f * scq_sin2pi(n->s[8]);
	float *dm = &n->s[2];
	float php = n->s[6], bw = n->s[9], slp = n->s[10], shp = n->s[11];
	LOOP {
		float x = tm_in(in[0][i]) * 0.5f;
		bw += 0.6f * (x - bw);
		float u = bw;
		for (int k = 0; k < 4; k++) {
			u = tm_ap16(ap[k], wi[k], u, k < 2 ? 0.7f : 0.6f);
			if (++wi[k] >= SHM_AP[k]) wi[k] = 0;
		}
		uint32_t p[4];
		float o[4];
		for (int k = 0; k < 4; k++) p[k] = wi[4 + k];
		o[0] = ring16(tk[0], SHM_T[0], p[0], (float)SHM_T[0] - 12.f + m0);
		o[1] = ring16(tk[1], SHM_T[1], p[1], (float)SHM_T[1] - 12.f + m1);
		o[2] = FROM16(tk[2][p[2]]);
		o[3] = FROM16(tk[3][p[3]]);
		for (int k = 0; k < 4; k++) dm[k] += 0.55f * (o[k] - dm[k]);
		/* the climb: two heads sweeping through the last 2000 samples of the tail */
		uint32_t pw = wi[8] & (SHM_PS - 1);
		ps[pw] = to16(0.25f * (o[0] + o[1] + o[2] + o[3]));
		php += dph;
		php -= scq_floor(php);
		float up = 0.f;
		for (int k = 0; k < 2; k++) {
			float q = php + 0.5f * (float)k;
			q -= scq_floor(q);
			float e = scq_sin2pi(0.5f * q);
			up += ring16(ps, SHM_PS, pw, 4.f + q * win) * e * e;
		}
		slp += 0.5f * (up - slp);
		shp += 0.01f * (slp - shp);
		float inj = u + sh * 0.5f * (slp - shp);
		float a = dm[0], b = dm[1], c = dm[2], d = dm[3];
		float h0 = 0.5f * (a + b + c + d), h1 = 0.5f * (a - b + c - d), h2 = 0.5f * (a + b - c - d), h3 = 0.5f * (a - b - c + d);
		tk[0][p[0]] = to16(tm_knee(g[0] * h0 + inj));
		tk[1][p[1]] = to16(tm_knee(g[1] * h1 - inj));
		tk[2][p[2]] = to16(tm_knee(g[2] * h2 + inj));
		tk[3][p[3]] = to16(tm_knee(g[3] * h3 - inj));
		for (int k = 0; k < 4; k++) if (++wi[4 + k] >= SHM_T[k]) wi[4 + k] = 0;
		wi[8] = (pw + 1) & (SHM_PS - 1);
		n->out[0][i] = 0.7f * (o[0] + 0.6f * o[2] - 0.3f * o[3]);
		n->out[1][i] = 0.7f * (o[1] + 0.6f * o[3] - 0.3f * o[2]);
	}
	for (int k = 0; k < 4; k++) dm[k] = tm_flush(dm[k]);
	n->s[6] = php; n->s[9] = tm_flush(bw); n->s[10] = tm_flush(slp); n->s[11] = tm_flush(shp);
}

/* ------------------------------------------------------------------ cloud */

/* a 4-point Hermite read from a 16-bit ring of length L at position p (0 <= p < L): clean
 * transposition, without linear interpolation's dulling */
static inline float tm_herm16(const int16_t *b, uint32_t L, float p)
{
	uint32_t i0 = (uint32_t)p;
	if (i0 >= L) i0 = 0;
	float f = p - (float)i0;
	uint32_t im = i0 ? i0 - 1 : L - 1, i1 = i0 + 1 < L ? i0 + 1 : 0, i2 = i1 + 1 < L ? i1 + 1 : 0;
	float xm = FROM16(b[im]), x0 = FROM16(b[i0]), x1 = FROM16(b[i1]), x2 = FROM16(b[i2]);
	float c1 = 0.5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
	return ((c3 * f + c2) * f + c1) * f + x0;
}

/* cloud — a texture cloud: grains of the live sound, up to 24 at once, each a Hann window of what
 * just happened, read at full resolution with Hermite interpolation; freeze holds the buffer,
 * density is how many overlap (none at 0, a continuous wash at the top), size how long (12 ms to
 * half a second), pitch transposes (octaves), spread scatters them in time, position and across
 * the stereo field. The buffer is 0.4 s at full rate, longer at lower samplerates. Heavy at full
 * density: at home on the Wingie (after Curtis Roads' Microsound, Barry Truax's granulation) */
#define CLD_L 19200u
#define CLD_G 24
#define CLD_F 6       /* per grain: pos, inc, phase, dphase, left, right */
static uint32_t mem_cloud(float nature) { (void)nature; return CLD_L * sizeof(int16_t) + CLD_G * CLD_F * sizeof(float); }
static void run_cloud(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	float *gr = (float *)(buf + CLD_L);
	/* s: 0 w, 1 timer */
	uint32_t w = (uint32_t)n->s[0];
	if (w >= CLD_L) w = 0;
	const int frozen = in[1][0] > 0.5f;
	const float dens = tm_c01(in[2][0]), spread = tm_c01(in[5][0]);
	const float ratio = scq_exp2(scq_clamp(in[4][0], -3.f, 3.f));
	float glen = SR * 0.012f * scq_exp2(tm_c01(in[3][0]) * 5.4f);
	glen = scq_min(glen, 0.4f * (float)CLD_L / scq_max(ratio * 1.06f, 1.f));
	/* density counts overlapping grains (a quarter to 20), so a long grain never runs out of voices */
	const float over = 0.25f * scq_exp2(dens * 6.3f);
	const float rate = over * SR / scq_max(glen, 16.f);
	const float norm = 0.75f / scq_sqrt_from(scq_max(over, 1.f), 2.f);
	float timer = n->s[1];
	if (!(timer < SR * 8.f)) timer = 0.f;
	LOOP {
		if (!frozen) {
			buf[w] = to16(tm_in(in[0][i]));
			if (++w >= CLD_L) w = 0;
		}
		timer -= 1.f;
		if (timer <= 0.f) {
			timer += SR / rate * (1.f + (0.1f + 0.9f * spread) * 0.9f * scq_noise(&n->rng));
			if (dens > 0.01f)
				for (int g = 0; g < CLD_G; g++) {
					float *q = gr + g * CLD_F;
					if (q[3] != 0.f) continue;
					float inc = ratio * scq_exp2(spread * 0.04f * scq_noise(&n->rng));
					float need = frozen ? 4.f : glen * scq_max(inc - 1.f, 0.f) + 4.f;
					float room = (float)CLD_L - glen * (frozen ? inc : scq_max(inc, 1.f)) - 8.f;
					float back = need + scq_rand01(&n->rng) * spread * scq_max(room - need, 0.f);
					if (back > (float)CLD_L - 8.f) back = (float)CLD_L - 8.f;
					float p = (float)w - back;
					while (p < 0.f) p += (float)CLD_L;
					q[0] = p; q[1] = inc; q[2] = 0.f; q[3] = 1.f / scq_max(glen, 16.f);
					tm_pan(spread * scq_noise(&n->rng), &q[4], &q[5]);
					q[4] *= norm; q[5] *= norm;
					break;
				}
		}
		float L = 0.f, R = 0.f;
		for (int g = 0; g < CLD_G; g++) {
			float *q = gr + g * CLD_F;
			if (q[3] == 0.f) continue;
			float e = scq_sin2pi(0.5f * q[2]);
			float y = tm_herm16(buf, CLD_L, q[0]) * e * e;           /* Hann */
			L += y * q[4]; R += y * q[5];
			q[0] += q[1];
			if (q[0] >= (float)CLD_L) q[0] -= (float)CLD_L;
			q[2] += q[3];
			if (q[2] >= 1.f) q[3] = 0.f;
		}
		n->out[0][i] = L;
		n->out[1][i] = R;
	}
	n->s[0] = (float)w; n->s[1] = timer;
}

/* ------------------------------------------------------------------ concat */

/* concat — a mosaic of your own sound: record cuts what comes in into 25 ms slices (a pulse takes
 * one, a held gate keeps cutting; the last 16 are kept, 16-bit), each known by its loudness and
 * its brightness (zero crossings); the slice nearest the bright and loud you ask for plays,
 * crossfading (equal power) into the next; follow asks with the live sound's own features and
 * matches its loudness: the input re-voiced by the corpus; trig picks again now. Slices are
 * longer at lower samplerates (after Coalescence, Diemo Schwarz's CataRT, Bob Sturm's MATConcat) */
#define CC_SL 1200u
#define CC_NS 16u
typedef struct { float loud, bright; } cc_slice;
typedef struct { int32_t slice, pos; float gain; } cc_voice;
typedef struct {
	cc_slice sl[CC_NS];
	cc_voice v[3];
	uint32_t valid;
	int32_t rec, rpos, next, last, hop;
	float sumsq, zc, prevx;           /* the slice being cut */
	float ms, zr, lprev;              /* the live sound */
} cc_state;
static uint32_t mem_concat(float nature) { (void)nature; return CC_NS * CC_SL * sizeof(int16_t) + sizeof(cc_state); }
static void init_concat(node *n)
{
	cc_state *st = (cc_state *)((int16_t *)n->mem + CC_NS * CC_SL);
	st->rec = -1; st->last = -1;
	for (int k = 0; k < 3; k++) st->v[k].slice = -1;
}
static inline float cc_bright(float zcr) { return tm_c01(scq_log2(scq_max(zcr * SR * 0.5f, 1.f) * (1.f / 50.f)) * 0.125f); }
static inline void cc_begin(cc_state *st)
{
	st->rec = st->next; st->next = (st->next + 1) % (int32_t)CC_NS;
	st->valid &= ~(1u << st->rec);
	st->rpos = 0; st->sumsq = 0.f; st->zc = 0.f;
}
static void run_concat(node *n, const float *const in[MAX_IN])
{
	int16_t *data = (int16_t *)n->mem;
	cc_state *st = (cc_state *)(data + CC_NS * CC_SL);
	if (st->rec >= (int32_t)CC_NS || st->rec < -1 || st->next < 0 || st->next >= (int32_t)CC_NS
	    || st->rpos < 0 || st->rpos >= (int32_t)CC_SL) { st->rec = -1; st->next = 0; st->rpos = 0; }
	const float tb_in = tm_c01(in[2][0]), tl_in = tm_c01(in[3][0]), follow = tm_c01(in[4][0]);
	const float cl = 1.f / (0.05f * SR);
	LOOP {
		float x = tm_in(in[0][i]);
		int cross = (x >= 0.f) != (st->lprev >= 0.f);
		st->ms += cl * (x * x - st->ms);
		st->zr += cl * ((cross ? 1.f : 0.f) - st->zr);
		st->lprev = x;
		/* cutting */
		int rec = in[1][i] > 0.5f;
		if (tm_edge(in[1][i], &n->s[0]) && st->rec < 0) cc_begin(st);
		if (st->rec >= 0) {
			data[(uint32_t)st->rec * CC_SL + (uint32_t)st->rpos] = to16(x);
			st->sumsq += x * x;
			if ((x >= 0.f) != (st->prevx >= 0.f)) st->zc += 1.f;
			st->prevx = x;
			if (++st->rpos >= (int32_t)CC_SL) {
				cc_slice *s = &st->sl[st->rec];
				s->loud = tm_c01(1.f + 0.05f * scq_log2(st->sumsq * (1.f / CC_SL) + 1e-12f));
				s->bright = cc_bright(st->zc * (1.f / CC_SL));
				st->valid |= 1u << st->rec;
				st->rec = -1;
				if (rec) cc_begin(st);                          /* still held: the next slice */
			}
		}
		/* choosing */
		if (tm_edge(in[5][i], &n->s[1])) st->hop = 0;
		if (--st->hop <= 0) {
			st->hop = (int32_t)CC_SL / 2;
			float liveb = cc_bright(st->zr), livel = tm_c01(1.f + 0.05f * scq_log2(st->ms + 1e-12f));
			float tb = tb_in + (liveb - tb_in) * follow, tl = tl_in + (livel - tl_in) * follow;
			int best = -1;
			float bd = 1e9f;
			for (int s = 0; s < (int)CC_NS; s++) {
				if (!(st->valid & (1u << s))) continue;
				float db = st->sl[s].bright - tb, dl = st->sl[s].loud - tl;
				float dd = db * db + dl * dl + (s == st->last ? 0.004f : 0.f) + 0.002f * scq_rand01(&n->rng);
				if (dd < bd) { bd = dd; best = s; }
			}
			if (best >= 0) {
				int vi = 0;
				for (int k = 0; k < 3; k++) {
					if (st->v[k].slice < 0) { vi = k; break; }
					if (st->v[k].pos > st->v[vi].pos) vi = k;
				}
				cc_voice *v = &st->v[vi];
				v->slice = best; v->pos = 0;
				float srms = scq_exp2((st->sl[best].loud - 1.f) * 10.f);
				float g = scq_clamp(scq_sqrt_from(st->ms, 0.1f) / scq_max(srms, 1e-4f), 0.f, 4.f);
				v->gain = 1.f + (g - 1.f) * follow;
				st->last = best;
			}
		}
		/* playing */
		float y = 0.f;
		for (int k = 0; k < 3; k++) {
			cc_voice *v = &st->v[k];
			if (v->slice < 0 || v->slice >= (int32_t)CC_NS || v->pos < 0) { v->slice = -1; continue; }
			float s = FROM16(data[(uint32_t)v->slice * CC_SL + (uint32_t)v->pos]);
			y += s * scq_sin2pi((float)v->pos * (0.5f / CC_SL)) * v->gain;
			if (++v->pos >= (int32_t)CC_SL) v->slice = -1;
		}
		n->out[0][i] = y;
	}
	st->ms = tm_flush(st->ms); st->zr = tm_flush(st->zr);
}

/* ------------------------------------------------------------------ the spectral pair */

/* A 512-point short-time Fourier transform (94 Hz bins), hop 128, Hann in and out. The work of each hop is cut
 * into four stages run a quarter of a hop apart, so every block costs about the same: 0 the
 * forward transform, 1-2 half the bins each, 3 the inverse and the overlap-add. 512 real points
 * are one 256-point complex transform plus a split. Phases are counted in turns. */
#define SP_N 512
#define SP_H 128
#define SP_M 256
#define SP_K 257
#define SP_BITS 8
typedef struct {
	float win[SP_N], ring[SP_N], z[SP_N], X[2 * SP_K];
	float mag[SP_K], dev[SP_K], prev[SP_K], acc[SP_K], fmag[SP_K], fdev[SP_K];
	float ola[SP_N], outq[SP_H];
	float tw[SP_M];          /* cos, sin of 2 pi j / M, j < M/2 */
	float sw[2 * SP_K];      /* cos, sin of 2 pi k / N, k <= M */
	uint8_t rev[SP_M];
	uint32_t rw, cnt, ready, frames, capture;
	float fx, gate;          /* specfreeze: how frozen it sounds; the gate */
} sp_state;
static uint32_t mem_spectral(float nature) { (void)nature; return sizeof(sp_state); }

/* sin and cos to float precision for the tables (made once, when the place is built) */
static float sp_sin(float x)       /* 0 <= x <= pi */
{
	if (x > 0.5f * SCQ_PI) x = SCQ_PI - x;
	float x2 = x * x;
	return x * (1.f - x2 / 6.f * (1.f - x2 / 20.f * (1.f - x2 / 42.f * (1.f - x2 / 72.f * (1.f - x2 / 110.f)))));
}
static void sp_make(sp_state *st)
{
	for (int j = 0; j < SP_N; j++) {
		float s = sp_sin(SCQ_PI * (float)j / SP_N);
		st->win[j] = s * s;                                                                     /* Hann */
	}
	for (int j = 0; j < SP_M / 2; j++) {
		float a = 2.f * SCQ_PI * (float)j / SP_M;
		st->tw[2 * j] = a <= 0.5f * SCQ_PI ? sp_sin(0.5f * SCQ_PI - a) : -sp_sin(a - 0.5f * SCQ_PI);
		st->tw[2 * j + 1] = sp_sin(a);
	}
	for (int k = 0; k < SP_K; k++) {
		float a = 2.f * SCQ_PI * (float)k / SP_N;
		st->sw[2 * k] = a <= 0.5f * SCQ_PI ? sp_sin(0.5f * SCQ_PI - a) : -sp_sin(a - 0.5f * SCQ_PI);
		st->sw[2 * k + 1] = sp_sin(a);
	}
	for (int j = 0; j < SP_M; j++) {
		int r = 0;
		for (int b = 0; b < SP_BITS; b++) if (j & (1 << b)) r |= 1 << (SP_BITS - 1 - b);
		st->rev[j] = (uint8_t)r;
	}
	st->ready = 1;
}
static void sp_fft(float *z, const float *tw, const uint8_t *rev, int inverse)
{
	for (int i = 0; i < SP_M; i++) {
		int j = rev[i];
		if (j > i) {
			float t = z[2 * i]; z[2 * i] = z[2 * j]; z[2 * j] = t;
			t = z[2 * i + 1]; z[2 * i + 1] = z[2 * j + 1]; z[2 * j + 1] = t;
		}
	}
	const float sg = inverse ? 1.f : -1.f;
	for (int len = 2; len <= SP_M; len <<= 1) {
		int half = len >> 1, step = SP_M / len;
		for (int s = 0; s < SP_M; s += len)
			for (int k = 0; k < half; k++) {
				float wr = tw[2 * k * step], wi = sg * tw[2 * k * step + 1];
				float *a = z + 2 * (s + k), *b = z + 2 * (s + k + half);
				float tr = b[0] * wr - b[1] * wi, ti = b[0] * wi + b[1] * wr;
				b[0] = a[0] - tr; b[1] = a[1] - ti;
				a[0] += tr; a[1] += ti;
			}
	}
}
/* atan2 in turns, -0.5..0.5 */
static inline float sp_atan2t(float y, float x)
{
	float ax = scq_abs(x), ay = scq_abs(y), mx = scq_max(ax, ay);
	if (!(mx > 1e-30f)) return 0.f;
	float a = scq_min(ax, ay) / mx, s = a * a;
	float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
	if (ay > ax) r = 1.57079637f - r;
	if (x < 0.f) r = 3.14159274f - r;
	if (y < 0.f) r = -r;
	return r * 0.159154943f;
}

enum { SP_SMEAR, SP_FREEZE };
static void sp_bins(node *n, sp_state *st, int k0, int k1, int mode, float c, float q, float leak)
{
	float *X = st->X;
	for (int k = k0; k < k1; k++) {
		float re = X[2 * k], im = X[2 * k + 1];
		float m2 = re * re + im * im;
		float m = scq_sqrt_from(m2, scq_abs(re) + scq_abs(im));
		float ph = sp_atan2t(im, re);
		float d = ph - st->prev[k] - (float)(k & 3) * 0.25f;
		d -= scq_floor(d + 0.5f);
		st->prev[k] = ph;
		float mo, adv, jit = 0.f;
		if (mode == SP_SMEAR) {
			st->mag[k] += c * (m - st->mag[k]);
			st->dev[k] += c * (d - st->dev[k]);
			if (c >= 0.999f) { st->acc[k] = ph; continue; }      /* no blur: the sound as it was */
			float lo = k > 0 ? st->mag[k - 1] : st->mag[k], hi = k < SP_K - 1 ? st->mag[k + 1] : st->mag[k];
			mo = st->mag[k] + q * (0.5f * (lo + hi) - st->mag[k]);
			adv = st->dev[k];
		} else {
			if (st->capture) {
				if (st->capture == 4) { st->fmag[k] = m; st->fdev[k] = d; }
				else { st->fmag[k] += 0.5f * (m - st->fmag[k]); st->fdev[k] += 0.5f * (d - st->fdev[k]); }
			} else if (st->gate > 0.5f) {
				st->fmag[k] += leak * (m - st->fmag[k]);
				st->fdev[k] += leak * (d - st->fdev[k]);
			}
			float fx = st->fx;
			if (fx <= 0.0001f) { st->acc[k] = ph; continue; }
			mo = m + (st->fmag[k] - m) * fx;
			adv = d + (st->fdev[k] - d) * fx;
			jit = fx * 0.03f * scq_noise(&n->rng);     /* the phases drift around where they belong, never away */
		}
		float a = st->acc[k] + (float)(k & 3) * 0.25f + adv;
		a -= scq_floor(a);
		st->acc[k] = a;
		if (k == 0 || k == SP_M) { X[2 * k] = 0.f; X[2 * k + 1] = 0.f; continue; }   /* (nothing to sing at DC or Nyquist) */
		X[2 * k] = mo * scq_sin2pi(a + jit + 0.25f);
		X[2 * k + 1] = mo * scq_sin2pi(a + jit);
	}
}
static void sp_stage(node *n, sp_state *st, int stage, int mode, float c, float q, float leak)
{
	float *z = st->z, *X = st->X;
	const float *sw = st->sw;
	if (stage == 0) { sp_fft(z, st->tw, st->rev, 0); return; }
	if (stage == 1) {
		for (int k = 0; k <= SP_M; k++) {               /* the split: M complex points into M + 1 real bins */
			int a = k & (SP_M - 1), b = (SP_M - k) & (SP_M - 1);
			float zkr = z[2 * a], zki = z[2 * a + 1], zmr = z[2 * b], zmi = z[2 * b + 1];
			float fer = 0.5f * (zkr + zmr), fei = 0.5f * (zki - zmi);
			float for_ = 0.5f * (zki + zmi), foi = -0.5f * (zkr - zmr);
			float cs = sw[2 * k], sn = sw[2 * k + 1];
			X[2 * k] = fer + cs * for_ + sn * foi;
			X[2 * k + 1] = fei + cs * foi - sn * for_;
		}
		sp_bins(n, st, 0, SP_K / 2, mode, c, q, leak);
		return;
	}
	if (stage == 2) {
		sp_bins(n, st, SP_K / 2, SP_K, mode, c, q, leak);
		X[1] = 0.f; X[2 * SP_M + 1] = 0.f;
		for (int k = 0; k < SP_M; k++) {                /* and back */
			int m = SP_M - k;
			float xkr = X[2 * k], xki = X[2 * k + 1], xmr = X[2 * m], xmi = X[2 * m + 1];
			float fer = 0.5f * (xkr + xmr), fei = 0.5f * (xki - xmi);
			float dr = 0.5f * (xkr - xmr), di = 0.5f * (xki + xmi);
			float cs = sw[2 * k], sn = sw[2 * k + 1];
			float for_ = dr * cs - di * sn, foi = dr * sn + di * cs;
			z[2 * k] = fer - foi;
			z[2 * k + 1] = fei + for_;
		}
		return;
	}
	sp_fft(z, st->tw, st->rev, 1);
	const float sc = (1.f / SP_M) * (2.f / 3.f);
	for (int j = 0; j < SP_N; j++) st->ola[j] += z[j] * st->win[j] * sc;
}
static void sp_run(node *n, const float *const in[MAX_IN], int mode)
{
	sp_state *st = (sp_state *)n->mem;
	if (!st->ready) sp_make(st);
	float c = 1.f, q = 0.f, leak = 0.f;
	const float frames = SR / (float)SP_H;
	if (mode == SP_SMEAR) {
		float blur = tm_c01(in[1][0]);
		if (blur > 0.001f) c = scq_coef_time(0.015f * scq_exp2(blur * 8.5f), frames);
		q = 0.3f * blur;
	} else {
		float bl = tm_c01(in[2][0]);
		leak = bl * bl * scq_coef_time(0.4f, frames);
	}
	if (st->rw >= SP_N) st->rw = 0;
	if (st->cnt >= SP_H) st->cnt = 0;
	int i = 0;
	while (i < B) {
		if (st->cnt == 0) {                          /* a hop: the next SP_H out, and the last SP_N in to work on */
			for (int j = 0; j < SP_H; j++) st->outq[j] = st->ola[j];
			for (int j = 0; j < SP_N - SP_H; j++) st->ola[j] = st->ola[j + SP_H];
			for (int j = SP_N - SP_H; j < SP_N; j++) st->ola[j] = 0.f;
			for (int j = 0; j < SP_N; j++) st->z[j] = st->ring[(st->rw + (uint32_t)j) & (SP_N - 1)] * st->win[j];
			if (mode == SP_FREEZE) {
				float gt = in[1][i] > 0.5f ? 1.f : 0.f;
				if (gt > 0.5f && st->gate <= 0.5f) st->capture = 4;
				else if (st->capture) st->capture--;
				st->gate = gt;
				st->fx += ((gt > 0.5f ? 1.f : 0.f) - st->fx) * 0.25f;
				if (st->fx < 0.0005f && gt <= 0.5f) st->fx = 0.f;
			}
		}
		int chunk = B - i < (int)(SP_H - st->cnt) ? B - i : (int)(SP_H - st->cnt);
		for (int s = 0; s < 4; s++) {
			uint32_t at = (uint32_t)s * (SP_H / 4);
			if (at >= st->cnt && at < st->cnt + (uint32_t)chunk) sp_stage(n, st, s, mode, c, q, leak);
		}
		for (int j = 0; j < chunk; j++, i++) {
			st->ring[st->rw] = tm_in(in[0][i]);
			st->rw = (st->rw + 1) & (SP_N - 1);
			n->out[0][i] = st->outq[st->cnt + (uint32_t)j];
		}
		st->cnt += (uint32_t)chunk;
		if (st->cnt >= SP_H) st->cnt = 0;
	}
}
static void init_spectral(node *n) { sp_make((sp_state *)n->mem); }

/* smear — the photograph left out in the rain: the spectrum blurred in time (and a little across
 * frequency), its partials carried on by a phase vocoder; blur from none to seconds. Heavy: at
 * home on the Wingie, or samplerate=half on the SP-1 (after Ableton's Spectral Time, ppooll's
 * spectral acts, Trevor Wishart's blur) */
static void run_smear(node *n, const float *const in[MAX_IN]) { sp_run(n, in, SP_SMEAR); }

/* specfreeze — a held breath that never ends: while freeze is up, the spectrum is caught and
 * sung on with drifting phases; blend lets the live sound seep into what is held. Heavy: at home
 * on the Wingie, or samplerate=half on the SP-1 (after ppooll's spectral freeze, the Disting EX's
 * Spectral Freeze, the GRM Freeze) */
static void run_specfreeze(node *n, const float *const in[MAX_IN]) { sp_run(n, in, SP_FREEZE); }

/* ------------------------------------------------------------------ cavern */

/* cavern — the limestone cave under the drone: eight long lines, slowly moving, feeding back
 * through a Hadamard matrix; size is how long (3 s to a minute), dark how quickly the highs go,
 * and hold makes it ring for ever while nothing new comes in (after Jot's feedback delay
 * networks, the Lexicon 224, Eventide's Blackhole) */
static const uint16_t CAV_LEN[8] = { 1327, 1601, 1867, 2063, 2333, 2557, 2797, 3011 };
static const uint16_t CAV_AP[4] = { 241, 367, 557, 683 };
#define CAV_TOTAL (1327u + 1601u + 1867u + 2063u + 2333u + 2557u + 2797u + 3011u + 241u + 367u + 557u + 683u)
static uint32_t mem_cavern(float nature) { (void)nature; return CAV_TOTAL * sizeof(int16_t) + 12 * sizeof(uint32_t); }
static void run_cavern(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem, *line[8], *ap[4];
	uint32_t off = 0;
	for (int k = 0; k < 8; k++) { line[k] = base + off; off += CAV_LEN[k]; }
	for (int k = 0; k < 4; k++) { ap[k] = base + off; off += CAV_AP[k]; }
	uint32_t *wi = (uint32_t *)(base + off);      /* each line's and allpass's write place */
	/* s: 0-7 damping, 8-15 wander phases, 16 size, 17 hold, 18 dark, 19 input highpass */
	n->s[16] += (tm_c01(in[1][0]) - n->s[16]) * 0.02f;
	n->s[17] += ((in[3][0] > 0.5f ? 1.f : 0.f) - n->s[17]) * 0.05f;
	n->s[18] += (tm_c01(in[2][0]) - n->s[18]) * 0.05f;
	const float size = n->s[16], hold = n->s[17];
	float rt = 3.f * scq_exp2(size * 4.3f);
	rt = scq_exp2(scq_log2(rt) + hold * (scq_log2(600.f) - scq_log2(rt)));
	float cd = scq_coef_hz(9000.f * scq_exp2(-4.f * n->s[18]), SR);
	cd += (1.f - cd) * hold;                      /* held: nothing is lost */
	const float scale = 0.55f + 0.43f * size, ing = 1.f - hold;
	float dl[8], g[8];
	uint32_t di[8];
	for (int k = 0; k < 8; k++) {
		float *ph = &n->s[8 + k];
		*ph = tm_frac(*ph + (0.07f + 0.031f * (float)k) * (float)B * INV_SR);
		dl[k] = ((float)CAV_LEN[k] - 16.f) * scale + ((k == 0 || k == 5) ? 12.f * scq_sin2pi(*ph) : 0.f);   /* two lines wander, the rest stand */
		di[k] = (uint32_t)dl[k];
		g[k] = scq_exp2(-9.966f * dl[k] / (rt * SR)) * 0.35355339f;
		if (wi[k] >= CAV_LEN[k]) wi[k] = 0;
	}
	for (int k = 0; k < 4; k++) if (wi[8 + k] >= CAV_AP[k]) wi[8 + k] = 0;
	float dm[8], hp = n->s[19];
	uint32_t w[12];
	for (int k = 0; k < 8; k++) dm[k] = n->s[k];
	for (int k = 0; k < 12; k++) w[k] = wi[k];
	LOOP {
		float x = tm_in(in[0][i]) * 0.5f * ing;
		hp += 0.003f * (x - hp);
		x -= hp;
		for (int k = 0; k < 4; k++) {
			x = tm_ap16(ap[k], w[8 + k], x, 0.65f);
			if (++w[8 + k] >= CAV_AP[k]) w[8 + k] = 0;
		}
		float o[8], v[8];
		for (int k = 0; k < 8; k++) {
			if (k == 0 || k == 5) o[k] = ring16(line[k], CAV_LEN[k], w[k], dl[k]);
			else { uint32_t r = w[k] >= di[k] ? w[k] - di[k] : w[k] + CAV_LEN[k] - di[k]; o[k] = FROM16(line[k][r]); }
			dm[k] += cd * (o[k] - dm[k]);
			v[k] = dm[k];
		}
		for (int s = 1; s < 8; s <<= 1)              /* a fast Walsh-Hadamard transform */
			for (int j = 0; j < 8; j += s << 1)
				for (int k = j; k < j + s; k++) { float a = v[k], b = v[k + s]; v[k] = a + b; v[k + s] = a - b; }
		for (int k = 0; k < 8; k++) {
			line[k][w[k]] = to16(tm_knee(g[k] * v[k] + ((k & 1) ? -x : x)));
			if (++w[k] >= CAV_LEN[k]) w[k] = 0;
		}
		n->out[0][i] = 0.4f * (o[0] - o[2] + o[4] - o[6]);
		n->out[1][i] = 0.4f * (o[1] - o[3] + o[5] - o[7]);
	}
	for (int k = 0; k < 8; k++) n->s[k] = tm_flush(dm[k]);
	for (int k = 0; k < 12; k++) wi[k] = w[k];
	n->s[19] = tm_flush(hp);
}

/* ------------------------------------------------------------------ doppler */

/* doppler — the train passing the platform: a sound moving along a line in front of you;
 * position (-1 left to 1 right, 60 m each way) puts it there, speed sends it past again and again
 * (up to 60 m/s), distance is how far the line is (1 to 40 m). The delay to each ear does the
 * rest: the pitch falls as it passes, it fades and dulls with distance, the far ear hears it later
 * and darker (after Doppler, John Chowning's moving sources, Xenakis' trajectories and Bernhard
 * Leitner's sound spaces) */
#define DOP_L 12288u
static uint32_t mem_doppler(float nature) { (void)nature; return DOP_L * sizeof(int16_t); }
static void run_doppler(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	/* s: 0 w, 1 travel, 2-3 delay to each ear, 4-5 air, 6-7 gains, 8 started, 9-10 air coefficients */
	uint32_t w = (uint32_t)n->s[0];
	if (w >= DOP_L) w = 0;
	float spd = tm_c01(in[3][0]), v = 60.f * spd * spd;
	float X = n->s[1] + v * (float)B * INV_SR;
	if (!(X > -60.f && X < 60.f)) X = X >= 60.f && X < 1e6f ? X - 120.f : 0.f;
	n->s[1] = X;
	float Xt = X + 60.f * scq_clamp(in[1][0], -1.f, 1.f);
	Xt -= 120.f * scq_floor((Xt + 60.f) * (1.f / 120.f));
	float D = 1.f + 39.f * tm_c01(in[2][0]) * tm_c01(in[2][0]);
	float dt[2], gt[2], ct[2];
	for (int e = 0; e < 2; e++) {
		float dx = Xt - (e ? 0.09f : -0.09f);
		float r = scq_sqrt_from(dx * dx + D * D, scq_abs(dx) + D);
		dt[e] = scq_clamp(r * (1.f / 343.f) * SR, 2.f, (float)DOP_L - 4.f);
		float side = scq_max((e ? -Xt : Xt) / r, 0.f);           /* how far round the head it has to go */
		gt[e] = scq_min(1.2f / (1.f + r * (1.f / 3.f)), 1.f) * (1.f - 0.25f * side);
		ct[e] = scq_coef_hz(18000.f / (1.f + r * (1.f / 12.f)) * (1.f - 0.6f * side), SR);
	}
	if (n->s[8] == 0.f) { n->s[2] = dt[0]; n->s[3] = dt[1]; n->s[6] = gt[0]; n->s[7] = gt[1]; n->s[8] = 1.f; }
	float step[2], gs[2], d[2], g[2];
	for (int e = 0; e < 2; e++) {
		d[e] = scq_clamp(n->s[2 + e], 2.f, (float)DOP_L - 4.f);
		float ch = scq_clamp(dt[e] - d[e], -0.9f * (float)B, 0.9f * (float)B);   /* never faster than sound */
		step[e] = ch / (float)B;
		g[e] = n->s[6 + e];
		gs[e] = (gt[e] - g[e]) / (float)B;
	}
	float a0 = n->s[4], a1 = n->s[5];
	LOOP {
		buf[w] = to16(tm_in(in[0][i]));
		float yl = ring16(buf, DOP_L, w, d[0]), yr = ring16(buf, DOP_L, w, d[1]);
		if (++w >= DOP_L) w = 0;
		d[0] += step[0]; d[1] += step[1];
		g[0] += gs[0]; g[1] += gs[1];
		a0 += ct[0] * (yl - a0); a1 += ct[1] * (yr - a1);
		n->out[0][i] = a0 * g[0];
		n->out[1][i] = a1 * g[1];
	}
	n->s[0] = (float)w; n->s[2] = d[0]; n->s[3] = d[1]; n->s[4] = tm_flush(a0); n->s[5] = tm_flush(a1);
	n->s[6] = g[0]; n->s[7] = g[1];
}

/* ------------------------------------------------------------------ haas */

/* haas — two windows onto the same street: the lows stay in the middle, the highs open into two
 * complementary combs (what one side gains the other loses, so in mono it all comes back) and a
 * small lead for the left; width from mono to wide (after Helmut Haas, Michael Gerzon's
 * shufflers, the old pseudo-stereo tricks) */
#define HAAS_L 1024u
static uint32_t mem_haas(float nature) { (void)nature; return HAAS_L * sizeof(int16_t); }
static void run_haas(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	if (w >= HAAS_L) w = 0;
	float lo = n->s[1], wd = n->s[2];
	const float cl = scq_coef_hz(250.f, SR);
	const float dc = scq_min(0.0093f * SR, (float)HAAS_L - 2.f), want = tm_c01(in[1][0]);
	const float hz = scq_min(0.0007f * SR, (float)HAAS_L - 2.f);
	const float ws = (want - wd) / (float)B;
	LOOP {
		float x = tm_in(in[0][i]);
		lo += cl * (x - lo);
		float hi = x - lo;
		buf[w] = to16(0.5f * hi);
		float hd = 2.f * ring16(buf, HAAS_L, w, dc), hh = 2.f * ring16(buf, HAAS_L, w, 1.f + wd * hz);
		if (++w >= HAAS_L) w = 0;
		wd += ws;
		float nrm = 1.f / (1.f + 0.2f * wd);
		n->out[0][i] = lo + (hi + 0.7f * wd * hd) * nrm;
		n->out[1][i] = lo + (hi + (hh - hi) * wd - 0.7f * wd * hd) * nrm;
	}
	n->s[0] = (float)w; n->s[1] = tm_flush(lo); n->s[2] = want;
}

/* ------------------------------------------------------------------ swarm */

/* swarm — a cloud of insects over the water: many tiny grains of the live sound (full rate, Hermite-read,
 * smooth windows), up to twenty at once, scattered in time, pitch and space by a logistic map; low chaos keeps them in a pattern,
 * high chaos sets them loose; density is how many, pitch where they sit (octaves) (after Xenakis'
 * clouds of sound grains, Agostino Di Scipio's chaotic granulation, the Ciat-Lonbarde Cocoquantus) */
#define SW_L 8192u
#define SW_G 20
#define SW_F 6        /* per grain: pos, inc, phase, dphase, left, right */
static uint32_t mem_swarm(float nature) { (void)nature; return SW_L * sizeof(int16_t) + SW_G * SW_F * sizeof(float); }
static void init_swarm(node *n) { n->s[2] = 0.37f; }
static void run_swarm(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	float *gr = (float *)(buf + SW_L);
	uint32_t w = (uint32_t)n->s[0] & (SW_L - 1);
	float timer = n->s[1], mx = n->s[2];
	if (!(timer < SR)) timer = 0.f;
	const float dens = tm_c01(in[1][0]), chaos = tm_c01(in[2][0]);
	const float rate = 4.f * scq_exp2(dens * 7.f), rr = 3.45f + 0.55f * chaos;
	const float base = scq_clamp(in[3][0], -3.f, 3.f), wide = 0.3f + 2.2f * chaos;
	const float norm = 0.9f / scq_sqrt_from(scq_max(rate * 0.02f, 1.f), 2.f);
	LOOP {
		buf[w] = to16(tm_in(in[0][i]));
		w = (w + 1) & (SW_L - 1);
		timer -= 1.f;
		if (timer <= 0.f) {
			float x1, x2, x3;
			if (!(mx > 0.001f && mx < 0.999f)) mx = 0.1f + 0.8f * scq_rand01(&n->rng);
			mx = rr * mx * (1.f - mx); x1 = mx;
			mx = rr * mx * (1.f - mx); x2 = mx;
			mx = rr * mx * (1.f - mx); x3 = mx;
			timer += SR / rate * (0.25f + 1.5f * x1);
			for (int g = 0; g < SW_G; g++) {
				float *q = gr + g * SW_F;
				if (q[3] != 0.f) continue;
				float dur = SR * (0.004f + 0.036f * x1 * x1);
				float ratio = scq_min(scq_exp2(base + (x2 - 0.5f) * wide), 4.f);
				float back = SR * (0.003f + 0.1f * x3);
				if (ratio > 1.f) back = scq_max(back, dur * (ratio - 1.f) + 4.f);
				back = scq_min(back, (float)SW_L - dur - 8.f);
				float p = (float)w - back;
				while (p < 0.f) p += (float)SW_L;
				q[0] = p; q[1] = ratio; q[2] = 0.f; q[3] = 1.f / scq_max(dur, 8.f);
				tm_pan(2.f * tm_frac(x3 * 5.3f) - 1.f, &q[4], &q[5]);
				q[4] *= norm; q[5] *= norm;
				break;
			}
		}
		float L = 0.f, R = 0.f;
		for (int g = 0; g < SW_G; g++) {
			float *q = gr + g * SW_F;
			if (q[3] == 0.f) continue;
			float p = q[2], e;
			if (p < 0.15f) { e = scq_sin2pi(p * 1.6667f); e *= e; }   /* a quick raised-cosine rise */
			else { e = (1.f - p) * 1.17647f; e *= e; }            /* and a smooth fall away */
			float y = tm_herm16(buf, SW_L, q[0]) * e;
			L += y * q[4]; R += y * q[5];
			q[0] += q[1];
			if (q[0] >= (float)SW_L) q[0] -= (float)SW_L;
			q[2] += q[3];
			if (q[2] >= 1.f) q[3] = 0.f;
		}
		n->out[0][i] = L;
		n->out[1][i] = R;
	}
	n->s[0] = (float)w; n->s[1] = timer; n->s[2] = mx;
}
