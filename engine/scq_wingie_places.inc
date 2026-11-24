/*
 * The Wingie collection: the Wingie2's resonator and what grew around it. Ciat-Lonbarde's Coco
 * and Quantussy, and the fish ideas: relabi, dust, Lucier's room, Xenakis's glissandi, the
 * talking drum, Four Bit Modulo, the Double Knot, the Benjolin, Rollz-5, Probee.
 * Included by scq_places.c. Every helper here is called wg_ so nothing meets another file's.
 */

/* ------------------------------------------------------------------ the Wingie's resonator maths */

/* One mode of the Wingie2 (Meng Qi's Faust "controlledModalRotation"): a coupled-form rotation,
 * rho = 0.001^(1/(t60 SR)), the output scaled by 2/rho, coefficients held for a block, and the
 * anti-feedback guard: a mode whose energy passed 1 in the last block has its input turned down
 * by that much and its rho held to the guard's (t60 0.1 s) for the next. */
#define WG_N 9
typedef struct { float q, p, s, c, rs, rc, in, peak, g, gs, hz; } wg_mode;

/* 0.001^(1/(t60 SR)), to the last bit that matters (1 - rho is ~1e-5: scq_exp2 can't) */
static inline float wg_rho(float t60)
{
	float a = -6.9077553f / (scq_max(t60, 0.02f) * SR);
	return 1.f + a * (1.f + a * (0.5f + a * (1.f / 6.f)));
}

/* sin and cos of 2 pi hz / sr, from the half angle (good to 1e-6 up to near sr / 2, and exact
 * enough near 0 that a 16 Hz mode is still a 16 Hz mode) */
static inline void wg_sincos(float hz, float sr, float *s, float *c)
{
	float h = SCQ_PI * hz / sr, h2 = h * h;
	float sh = h * (1.f - h2 * (1.f / 6.f) * (1.f - h2 * (1.f / 20.f) * (1.f - h2 * (1.f / 42.f) * (1.f - h2 * (1.f / 72.f)))));
	float ch = 1.f - h2 * 0.5f * (1.f - h2 * (1.f / 12.f) * (1.f - h2 * (1.f / 30.f) * (1.f - h2 * (1.f / 56.f))));
	*s = 2.f * sh * ch;
	*c = 1.f - 2.f * sh * sh;
}

/* tune a mode; above 16 kHz (or near Nyquist at a low samplerate) it is muted, as on the Wingie */
static inline void wg_tune(wg_mode *m, float hz)
{
	float top = scq_min(16000.f, 0.45f * SR);
	m->g = hz <= top ? 1.f : 0.f;                 /* (not-a-number: muted) */
	hz = scq_clamp(hz, 16.f, top);
	if (hz == m->hz) return;
	m->hz = hz;
	wg_sincos(hz, SR, &m->s, &m->c);
}

/* the block's coefficients and the guard */
static void wg_block(wg_mode *m, float rho_user)
{
	const float guard = wg_rho(0.1f);            /* stock 0.998435 at 44.1 kHz */
	float gk = scq_min(1.f, (float)B / (0.05f * SR));   /* the stock 50 ms mute */
	for (int k = 0; k < WG_N; k++) {
		wg_mode *d = &m[k];
		float rho = rho_user;
		if (d->peak > 1.f) { d->in = 1.f / d->peak; rho = scq_min(rho_user, guard); }
		else d->in = 1.f;
		if (d->q * d->q + d->p * d->p < 1e-24f) { d->q = 0.f; d->p = 0.f; }   /* rung out: no denormals */
		d->peak = 0.f;
		d->gs += (d->g - d->gs) * gk;
		d->rs = rho * d->s; d->rc = rho * d->c;
	}
}

/* one sample through all nine: the odd modes (1st, 3rd ...) and the even ones summed apart */
static inline void wg_tick(wg_mode *m, float x, float *odd, float *even)
{
	float o = 0.f, e = 0.f;
	for (int k = 0; k < WG_N; k++) {
		wg_mode *d = &m[k];
		float q = d->rs * d->p + d->rc * d->q;
		float p = d->in * x + d->rc * d->p - d->rs * d->q;
		d->q = q; d->p = p;
		float en = q * q + p * p;
		if (en > d->peak) d->peak = en;
		if (k & 1) e += q * d->gs; else o += q * d->gs;
	}
	*odd = o; *even = e;
}

static inline void wg_strike(wg_mode *m, float a) { for (int k = 0; k < WG_N; k++) m[k].p += a * m[k].g; }

/* the stock input chain: dc block, x 0.0625 (lin2LogGain 0.25), one-pole lowpass at 4 kHz.
 * st: x1, y1, lp */
static inline float wg_in(float x, float *st, float lpa)
{
	x = scq_clamp(x, -4.f, 4.f);
	float y = x - st[0] + 0.995f * st[1];
	st[0] = x; st[1] = y;
	st[2] += lpa * (y * 0.0625f - st[2]);
	return st[2];
}
static inline float wg_lpa(void) { return 1.f - scq_exp2(-1.442695f * 2.f * SCQ_PI * 4000.f * INV_SR); }
static inline void wg_flush(float *v) { if (!(scq_abs(*v) > 1e-20f)) *v = 0.f; }

/* the stock output stage: x preClip (0.0613), Faust cubicnl, x postClip (0.6806); then 1.5 so it
 * sits with the other places (the Wingie's own volume stage comes after) */
static inline float wg_out(float w)
{
	float y = scq_clamp(w * 0.0613f * 1.0471285f, -1.f, 1.f);
	return (y - y * y * y * (1.f / 3.f)) * (0.6806f * 1.5f);
}

/* log2(x) for x > 0, within 0.005 (scq_log2 in scq_math.h comes out one too high as of
 * 2026-11-24; this one is its own so a fix there can't move it) */
static inline float wg_log2(float x)
{
	union { float f; int32_t i; } u;
	if (!(x > 1e-30f)) return -100.f;
	u.f = x;
	float e = (float)(((u.i >> 23) & 255) - 128);
	u.i = (u.i & ~(255 << 23)) | (127 << 23);
	float m = u.f;
	return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}

/* the stock decay fader: 0.1 + 9.9 x^2.11 seconds */
static inline float wg_decay(float x)
{
	x = scq_clamp(x, 0.f, 1.f);
	return 0.1f + 9.9f * scq_exp2(2.11f * wg_log2(x));
}

static const float WG_BAR[WG_N] = { 1.f, 2.7778f, 5.4444f, 9.f, 13.444f, 18.778f, 25.f, 32.111f, 40.111f };
static const float WG_MEMBRANE[WG_N] = { 1.f, 1.593f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.6f };
static const float WG_CAVE[3][WG_N] = {
	{ 62, 115, 218, 411, 777, 1500, 2800, 5200, 11000 },     /* Low  */
	{ 205, 304, 370, 523, 540, 800, 913, 1568, 2400 },       /* Mid  */
	{ 212, 425, 531, 637, 1168, 2017, 2336, 2654, 3693 },    /* High */
};

static uint32_t mem_wg_bank(float nature) { (void)nature; return WG_N * sizeof(wg_mode); }

/* a bank run on its input with a strike inlet: the common body of wingie and cave.
 * s: 0-2 input chain, 3 strike edge */
static void wg_run_bank(node *n, wg_mode *m, const float *x, const float *strike, float rho_user)
{
	wg_block(m, rho_user);
	const float sc = 2.f / rho_user, lpa = wg_lpa();
	LOOP {
		if (edge(strike[i], &n->s[3])) wg_strike(m, 0.3f);
		float o, e;
		wg_tick(m, wg_in(x[i], &n->s[0], lpa), &o, &e);
		n->out[0][i] = wg_out((o + e) * sc);
		n->out[1][i] = wg_out(o * sc);
		n->out[2][i] = wg_out(e * sc);
	}
	for (int k = 0; k < 3; k++) wg_flush(&n->s[k]);
}

/* wingie — the bird with two keyboards: nine modes ringing on what comes in, the Wingie2's own
 * resonator to the last coefficient (after Meng Qi's Wingie2) */
static void run_wingie(node *n, const float *const in[MAX_IN])
{
	wg_mode *m = (wg_mode *)n->mem;
	float base = 60.f * n->nature * scq_exp2(in[1][0]);
	int shape = (int)n->opt[0];
	float ratio = scq_clamp(n->opt[1], 0.25f, 4.f), rk = 1.f;
	for (int k = 0; k < WG_N; k++) {
		float hz;
		switch (shape) {
		case 1:  hz = base * WG_BAR[k]; break;
		case 2:  hz = base * WG_MEMBRANE[k]; break;
		case 3:  hz = base * WG_CAVE[0][k] * (1.f / 62.f); break;
		case 4:  hz = base * rk; rk *= ratio; break;
		default: hz = base * (float)(k + 1); break;
		}
		wg_tune(&m[k], hz);
	}
	wg_run_bank(n, m, in[0], in[3], wg_rho(wg_decay(in[2][0])));
}

/* cave — three caves under the Wingie: its factory cave tunings as a resonator (after Meng Qi's Wingie2 cave mode) */
static void run_cave(node *n, const float *const in[MAX_IN])
{
	wg_mode *m = (wg_mode *)n->mem;
	int b = natural(n->nature, 1, 3) - 1;
	float k2 = scq_exp2(scq_clamp(in[2][0], -6.f, 6.f));
	for (int k = 0; k < WG_N; k++) wg_tune(&m[k], WG_CAVE[b][k] * k2);
	wg_run_bank(n, m, in[0], in[3], wg_rho(wg_decay(in[1][0])));
}

/* ------------------------------------------------------------------ Coco and Quantussy */

/* The Coco: a fixed 8-bit buffer whose sampler clock is the delay time. Slower is longer and
 * lower; nothing is interpolated (the held step is the grit). Full clock is 24 kHz. */
#define WG_COCO_HZ 24000.f
typedef struct { float pos, dir, last, held, acc, cnt; } wg_coco;

static uint32_t wg_coco_len(float seconds) { return (uint32_t)(scq_clamp(seconds, 0.05f, 1.6f) * WG_COCO_HZ); }

static inline int8_t wg_q8(float x)
{
	x = scq_clamp(x, -1.f, 1.f) * 127.f;
	return (int8_t)(x >= 0.f ? x + 0.5f : x - 0.5f);
}

static inline void wg_coco_get(wg_coco *c, const float *s) { c->pos = s[0]; c->dir = s[1]; c->last = s[2]; c->held = s[3]; c->acc = s[4]; c->cnt = s[5]; }
static inline void wg_coco_put(const wg_coco *c, float *s) { s[0] = c->pos; s[1] = c->dir; s[2] = c->last; s[3] = c->held; s[4] = c->acc; s[5] = c->cnt; }

/* one sample: step is buffer cells per sample (rate x 24 kHz / SR) */
static inline float wg_coco_tick(wg_coco *c, int8_t *buf, uint32_t len, float x, float step, float fb, int frozen)
{
	const float L = (float)len;
	c->acc += scq_clamp(x, -2.f, 2.f); c->cnt += 1.f;
	float pos = c->pos + step * (c->dir < 0.f ? -1.f : 1.f);
	if (pos >= L) pos -= L;
	if (pos < 0.f) pos += L;
	if (!(pos >= 0.f && pos < L)) pos = 0.f;
	c->pos = pos;
	uint32_t idx = (uint32_t)pos;
	if (idx >= len) idx = 0;
	if ((float)idx != c->last) {               /* a clock tick: read the old, write the new */
		c->last = (float)idx;
		c->held = (float)buf[idx] * (1.f / 127.f);
		if (!frozen) buf[idx] = wg_q8(c->acc / c->cnt + c->held * fb);
		c->acc = 0.f; c->cnt = 0.f;
	}
	return c->held;
}

static inline float wg_coco_step(float rate) { return scq_exp2(6.f * (scq_clamp(rate, 0.f, 1.1667f) - 1.f)) * WG_COCO_HZ * INV_SR; }

/* coco — a coconut on a string: Ciat-Lonbarde's Coco, an 8-bit delay whose clock is the time;
 * skip jumps the head, flip turns it round, freeze stops the writing (after Peter Blasser's Coco)
 * s: 0-5 the coco, 6 skip edge, 7 flip edge */
static uint32_t mem_coco(float nature) { return wg_coco_len(nature); }
static void init_coco(node *n) { n->s[1] = 1.f; }
static void run_coco(node *n, const float *const in[MAX_IN])
{
	int8_t *buf = (int8_t *)n->mem;
	uint32_t len = n->memlen;
	wg_coco c;
	wg_coco_get(&c, n->s);
	float step = wg_coco_step(in[1][0]);
	float fb = scq_clamp(in[2][0], 0.f, 1.1f);
	int frozen = in[3][0] > 0.5f;
	LOOP {
		if (edge(in[4][i], &n->s[6])) c.pos = scq_rand01(&n->rng) * (float)(len - 1);
		if (edge(in[5][i], &n->s[7])) c.dir = c.dir < 0.f ? 1.f : -1.f;
		float y = wg_coco_tick(&c, buf, len, in[0][i], step, fb, frozen);
		float pan = scq_clamp(in[6][i], 0.f, 1.f);
		n->out[0][i] = y;
		n->out[1][i] = y * pan;
		n->out[2][i] = y * (1.f - pan);
	}
	wg_coco_put(&c, n->s);
	wg_flush(&n->s[4]);
}

/* The Quantussy's five petals: triangle cores, each pushed faster or slower by the sign of its
 * neighbours (FLIP). Returns the triangle (-1..1) of a phase. */
static const float WG_PETAL_HZ[5] = { 0.11f, 0.17f, 0.23f, 0.31f, 0.43f };
static inline float wg_tri(float ph) { return ph < 0.5f ? ph * 4.f - 1.f : 3.f - ph * 4.f; }
static inline void wg_petals(float *ph, float speed_dt, float couple)
{
	float t[5];
	for (int k = 0; k < 5; k++) t[k] = wg_tri(ph[k]);
	for (int k = 0; k < 5; k++) {
		float nx = t[k == 4 ? 0 : k + 1], pv = t[k == 0 ? 4 : k - 1];
		float r = WG_PETAL_HZ[k] * (1.f + couple * ((nx > 0.f ? 0.6f : -0.4f) + (pv > 0.f ? -0.15f : 0.15f)));
		float p = ph[k] + r * speed_dt;
		ph[k] = p - scq_floor(p);
	}
}

/* cocoquantus — two coconuts and a flower: two Cocos and a five-petal Quantussy that wobbles their
 * clocks and swings each one's verso / inverso across (after Peter Blasser's Cocoquantus)
 * s: 0-5 coco L, 6-11 coco R, 12-16 petals, 17 skip edge, 18 flip edge, 19-20 last petal 3 / 4 */
static uint32_t mem_cocoquantus(float nature) { return 2u * wg_coco_len(scq_clamp(nature, 0.05f, 0.8f)); }
static void init_cocoquantus(node *n)
{
	n->s[1] = 1.f; n->s[7] = 1.f;
	for (int k = 0; k < 5; k++) n->s[12 + k] = 0.2f * (float)k;
}
static void run_cocoquantus(node *n, const float *const in[MAX_IN])
{
	uint32_t len = n->memlen / 2;
	int8_t *bl = (int8_t *)n->mem, *br = bl + len;
	wg_coco a, b;
	wg_coco_get(&a, &n->s[0]);
	wg_coco_get(&b, &n->s[6]);
	/* the petals, once a block */
	float speed = scq_clamp(in[5][0], 0.f, 1.f);
	float ph[5];
	for (int k = 0; k < 5; k++) ph[k] = n->s[12 + k];
	wg_petals(ph, 12.f * speed * speed * (float)B * INV_SR, 0.6f);
	float t[5];
	for (int k = 0; k < 5; k++) { n->s[12 + k] = ph[k]; t[k] = wg_tri(ph[k]); }
	float depth = scq_min(1.f, speed * 4.f) * 0.8f;
	float rate = in[2][0];
	float sa = wg_coco_step(rate) * (1.f + 0.12f * depth * t[1]);
	float sb = wg_coco_step(rate) * (1.f + 0.12f * depth * t[2]) * 0.997f;
	float va = 0.5f + 0.5f * depth * t[0], vb = 0.5f - 0.5f * depth * t[3];   /* each coco's verso share */
	float fb = scq_clamp(in[3][0], 0.f, 1.1f);
	int frozen = in[4][0] > 0.5f;
	int automode = (int)n->opt[0];                 /* auto=off|skip|flip|both: the petals' peaks do it */
	if ((automode & 1) && n->s[19] < 0.95f && t[3] >= 0.95f) { a.pos = scq_rand01(&n->rng) * (float)(len - 1); b.pos = scq_rand01(&n->rng) * (float)(len - 1); }
	if ((automode & 2) && n->s[20] < 0.95f && t[4] >= 0.95f) { a.dir = -a.dir; b.dir = -b.dir; }
	n->s[19] = t[3]; n->s[20] = t[4];
	const int stereo = (n->patched & 2u) != 0;
	float petal = 0.5f + 0.5f * t[2];
	LOOP {
		if (edge(in[6][i], &n->s[17])) { a.pos = scq_rand01(&n->rng) * (float)(len - 1); b.pos = scq_rand01(&n->rng) * (float)(len - 1); }
		if (edge(in[7][i], &n->s[18])) { a.dir = a.dir < 0.f ? 1.f : -1.f; b.dir = b.dir < 0.f ? 1.f : -1.f; }
		float xl = in[0][i], xr = stereo ? in[1][i] : xl;
		float ya = wg_coco_tick(&a, bl, len, xl, sa, fb, frozen);
		float yb = wg_coco_tick(&b, br, len, xr, sb, fb, frozen);
		n->out[0][i] = ya * va + yb * (1.f - vb);
		n->out[1][i] = ya * (1.f - va) + yb * vb;
		n->out[2][i] = petal;
	}
	wg_coco_put(&a, &n->s[0]);
	wg_coco_put(&b, &n->s[6]);
	wg_flush(&n->s[4]); wg_flush(&n->s[10]);
}

/* petals — a flower of five triangles: the Quantussy alone, each petal hurried or held back by
 * its neighbours' signs; form=tri|square|pulse (pulse: a pulse at every petal's crest, a lurching
 * rhythm) (after Peter Blasser's Quantussy)
 * s: 0-4 phases, 5-8 pulse timers */
static void init_petals(node *n) { for (int k = 0; k < 5; k++) n->s[k] = 0.2f * (float)k; }
static void run_petals(node *n, const float *const in[MAX_IN])
{
	float ph[5];
	for (int k = 0; k < 5; k++) ph[k] = n->s[k];
	float speed = n->nature * scq_exp2(scq_clamp(in[0][0], -4.f, 1.f) * 12.f) * INV_SR;
	float couple = scq_clamp(in[1][0], 0.f, 1.f);
	int form = (int)n->opt[0];
	float pt[4] = { n->s[5], n->s[6], n->s[7], n->s[8] };
	LOOP {
		float before[4] = { ph[0], ph[1], ph[2], ph[3] };
		wg_petals(ph, speed, couple);
		for (int k = 0; k < 4; k++) {
			float v;
			if (form == 0) v = 0.5f + 0.5f * wg_tri(ph[k]);
			else if (form == 1) v = ph[k] < 0.5f ? 1.f : 0.f;
			else {
				if (before[k] < 0.5f && ph[k] >= 0.5f) pt[k] = PULSE_SAMPLES;
				v = pt[k] > 0.f ? 1.f : 0.f;
				if (pt[k] > 0.f) pt[k] -= 1.f;
			}
			n->out[k][i] = v;
		}
	}
	for (int k = 0; k < 5; k++) n->s[k] = ph[k];
	for (int k = 0; k < 4; k++) n->s[5 + k] = pt[k];
}

/* ------------------------------------------------------------------ the fish ideas: rhythm and chance */

/* relabi — almost a beat: four slow sines at irrational ratios summed; a pulse at every upward
 * zero crossing, as strong as the crossing is steep (after John Berndt's relabi)
 * s: 0-3 phases, 4-7 drift, 8 last sum, 9 strength, 10 pulse timer */
static const float WG_RELABI[4] = { 1.f, 1.618034f, 1.414214f, 0.75f };
static void init_relabi(node *n) { n->s[1] = 0.31f; n->s[2] = 0.62f; n->s[3] = 0.13f; }
static void run_relabi(node *n, const float *const in[MAX_IN])
{
	float base = 1.5f * n->nature * scq_exp2(8.f * (scq_clamp(in[0][0], 0.f, 1.f) - 0.5f));
	float smear = scq_clamp(in[1][0], 0.f, 1.f);
	float dt = (float)B * INV_SR, inc[4], ph[4];
	for (int k = 0; k < 4; k++) {
		float d = n->s[4 + k];
		d += (scq_noise(&n->rng) * 4.f - d * 0.5f) * dt * smear * 8.f;
		d = scq_clamp(d, -0.4f, 0.4f) * (smear > 0.f ? 1.f : 0.f);
		n->s[4 + k] = d;
		inc[k] = base * WG_RELABI[k] * (1.f + d) * INV_SR;
		ph[k] = n->s[k];
	}
	float prev = n->s[8], strength = n->s[9], pt = n->s[10];
	const float norm = 1.f / (base * 2.5f);
	LOOP {
		float sum = 0.f;
		for (int k = 0; k < 4; k++) { sum += scq_sin2pi(ph[k]); ph[k] += inc[k]; ph[k] -= scq_floor(ph[k]); }
		sum *= 0.25f;
		if (prev <= 0.f && sum > 0.f) {
			strength = scq_clamp((sum - prev) * SR * norm, 0.15f, 1.f);
			pt = PULSE_SAMPLES;
		}
		prev = sum;
		n->out[0][i] = pt > 0.f ? 1.f : 0.f;
		n->out[1][i] = 0.5f + 0.5f * sum;
		n->out[2][i] = strength;
		if (pt > 0.f) pt -= 1.f;
	}
	for (int k = 0; k < 4; k++) n->s[k] = ph[k];
	n->s[8] = prev; n->s[9] = strength; n->s[10] = pt;
}

/* dust — motes in a sunbeam: random impulses of random height, nature a second on average
 * (density 0.5), 1/64 to 64 times that (after SuperCollider's Dust)
 * s: 0 pulse timer */
static void run_dust(node *n, const float *const in[MAX_IN])
{
	float pt = n->s[0];
	LOOP {
		float chance = n->nature * scq_exp2(12.f * (scq_clamp(in[0][i], 0.f, 1.f) - 0.5f)) * INV_SR;
		float y = 0.f;
		if (scq_rand01(&n->rng) < chance) { y = scq_rand01(&n->rng); pt = PULSE_SAMPLES; }
		n->out[0][i] = y;
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = pt;
}

/* haze — a heat haze: on each trigger it drifts to a new random value between lo and hi, taking
 * glide (0..1 is 0..2 s) to get there; untriggered it wanders on its own every nature seconds or so
 * s: 0 value, 1 step, 2 samples left, 3 trig edge */
static void run_haze(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], inc = n->s[1], left = n->s[2];
	const int own = !(n->patched & 1u);
	const float chance = own ? INV_SR / scq_max(n->nature, 0.05f) : 0.f;
	LOOP {
		int t = edge(in[0][i], &n->s[3]);
		if (own && scq_rand01(&n->rng) < chance) t = 1;
		if (t) {
			float lo = scq_clamp(in[1][i], -8.f, 8.f), hi = scq_clamp(in[2][i], -8.f, 8.f);
			float to = lo + (hi - lo) * scq_rand01(&n->rng);
			float g = scq_clamp(in[3][i], 0.f, 1.f);
			left = scq_max(1.f, 2.f * g * g * SR);
			inc = (to - v) / left;
		}
		if (left > 0.f) { v += inc; left -= 1.f; }
		n->out[0][i] = v;
	}
	if (!scq_sane(v)) v = 0.f;
	n->s[0] = v; n->s[1] = inc; n->s[2] = left;
}

/* probee — a bee among four flowers: on every trigger four new random values, held;
 * spread=uniform|gauss|bimodal, width narrows them around the middle (after the Probee Max for Live device)
 * s: 0-3 values, 4 trig edge */
static float wg_probee_draw(uint32_t *r, int spread)
{
	if (spread == 1) {
		float g = (scq_rand01(r) + scq_rand01(r) + scq_rand01(r) + scq_rand01(r)) * 0.25f;
		return scq_clamp(0.5f + (g - 0.5f) * 1.6f, 0.f, 1.f);
	}
	if (spread == 2) {
		float g = (scq_rand01(r) + scq_rand01(r) + scq_rand01(r)) * (1.f / 3.f) - 0.5f;
		return scq_clamp((scq_rand01(r) < 0.5f ? 0.18f : 0.82f) + g * 0.35f, 0.f, 1.f);
	}
	return scq_rand01(r);
}
static void init_probee(node *n) { for (int k = 0; k < 4; k++) n->s[k] = scq_rand01(&n->rng); }
static void run_probee(node *n, const float *const in[MAX_IN])
{
	int spread = (int)n->opt[0];
	LOOP {
		if (edge(in[0][i], &n->s[4])) {
			float w = scq_clamp(in[1][i], 0.f, 1.f);
			for (int k = 0; k < 4; k++) n->s[k] = 0.5f + (wg_probee_draw(&n->rng, spread) - 0.5f) * w;
		}
		for (int k = 0; k < 4; k++) n->out[k][i] = n->s[k];
	}
}

/* rollz — five marbles in a bowl: a ring of five pulse oscillators; each fire pulls its
 * neighbours forward and pushes the far ones back, so the rhythm rolls and lurches instead of
 * keeping a grid. Four lanes come out; the fifth roller is heard only in how it pulls the others
 * (after Peter Blasser's Rollz-5)
 * s: 0-4 phases, 5-8 pulse timers */
static const float WG_ROLL_K[5] = { 0.f, 0.37f, -0.23f, 0.61f, -0.41f };
static void init_rollz(node *n) { for (int k = 0; k < 5; k++) n->s[k] = scq_rand01(&n->rng); }
static void run_rollz(node *n, const float *const in[MAX_IN])
{
	float ph[5], inc[5], pt[4];
	float hz = n->nature * scq_exp2(6.f * (scq_clamp(in[0][0], 0.f, 1.f) - 0.5f));
	float spread = scq_clamp(in[1][0], 0.f, 1.f);
	float drama = scq_clamp(in[2][0], 0.f, 1.f);
	for (int k = 0; k < 5; k++) { ph[k] = n->s[k]; inc[k] = hz * (1.f + spread * WG_ROLL_K[k]) * INV_SR; }
	for (int k = 0; k < 4; k++) pt[k] = n->s[5 + k];
	const float near = 0.45f * drama, far = -0.3f * drama;
	LOOP {
		unsigned fired = 0;
		for (int k = 0; k < 5; k++) {                /* each rolls faster downhill of the next one */
			ph[k] += inc[k] * (1.f + 1.2f * drama * (ph[k == 4 ? 0 : k + 1] - 0.5f));
			if (ph[k] >= 1.f) { ph[k] -= scq_floor(ph[k]); fired |= 1u << k; }
		}
		if (fired) {
			for (int j = 0; j < 5; j++) {
				if (!(fired & (1u << j))) continue;
				if (j < 4) pt[j] = PULSE_SAMPLES;
				for (int k = 0; k < 5; k++) {
					if (k == j || (fired & (1u << k))) continue;
					int d = k - j; if (d < 0) d = -d;
					float c = (d == 1 || d == 4) ? near : far;   /* neighbours on the ring, or across it */
					ph[k] = scq_clamp(ph[k] + c * ph[k], 0.f, 0.9999f);
				}
			}
		}
		for (int k = 0; k < 4; k++) { n->out[k][i] = pt[k] > 0.f ? 1.f : 0.f; if (pt[k] > 0.f) pt[k] -= 1.f; }
	}
	for (int k = 0; k < 5; k++) n->s[k] = ph[k];
	for (int k = 0; k < 4; k++) n->s[5 + k] = pt[k];
}

/* ------------------------------------------------------------------ Four Bit Modulo Phase Triplex */

/* fourbit — the underpass at 41.814011, -71.406527: 64 bits taken from the sign of what comes in,
 * played by three loops of 64, 68 and 72 steps (violin, piano, cello) that drift out of phase;
 * movement=one|two|three: two gives each loop new material at every turn, three cycles loop
 * lengths of 3, 7, 8 and 4 beats, then drifts, then disintegrates into silence (reset brings it
 * back) (after Four Bit Modulo Phase Triplex for Violin, Piano and Cello, 2023)
 * mem: pattern 2 words, capture 2 words.
 * s: 0-2 positions, 3-5 rotations, 6-8 pulse timers, 9 seq index, 10 seq passes, 11 collapse,
 *    12 capturing (bits + 1), 13 capture timer, 14 internal clock phase, 15-17 edges */
static uint32_t mem_fourbit(float nature) { (void)nature; return 4 * sizeof(uint32_t); }
static void init_fourbit(node *n)
{
	uint32_t *pat = (uint32_t *)n->mem;
	if (pat[0] | pat[1]) return;
	uint32_t r = 41814011u ^ 71406527u;
	for (int b = 0; b < 64; b++) if (scq_rand01(&r) > 0.5f) pat[b >> 5] |= 1u << (b & 31);
}
static inline int wg_bit(const uint32_t *pat, int b) { b &= 63; return (int)((pat[b >> 5] >> (b & 31)) & 1u); }
static const int WG_FB_LEN[3] = { 64, 68, 72 };
static const int WG_FB_BEATS[4] = { 3, 7, 8, 4 };
static const int WG_FB_MAT[6] = { 0, 3, 5, 7, -2, -5 };
static void run_fourbit(node *n, const float *const in[MAX_IN])
{
	uint32_t *pat = (uint32_t *)n->mem;
	int mv = (int)n->opt[0];
	float *s = n->s;
	const int own = !(n->patched & 4u);
	float sps = (mv == 1 ? 12.f : mv == 2 ? 8.6667f : 8.f) * scq_clamp(n->nature, 0.05f, 16.f);   /* 16ths at 120 / triplets / 130 BPM */
	LOOP {
		if (edge(in[3][i], &s[17])) {
			for (int k = 0; k < 3; k++) { s[k] = 0.f; s[3 + k] = 0.f; }
			s[9] = 0.f; s[10] = 0.f; s[11] = 0.f;
		}
		if (edge(in[1][i], &s[15]) && s[12] == 0.f) { s[12] = 1.f; s[13] = 0.f; pat[2] = pat[3] = 0; }
		if (s[12] > 0.f) {                           /* capturing: a bit every 31 ms, two seconds in all */
			s[13] -= 1.f;
			if (s[13] <= 0.f) {
				int b = (int)s[12] - 1;
				if (in[0][i] > 0.f) pat[2 + (b >> 5)] |= 1u << (b & 31);
				s[13] = 0.031f * SR;
				s[12] += 1.f;
				if (s[12] > 64.f) {
					if (pat[2] | pat[3]) { pat[0] = pat[2]; pat[1] = pat[3]; }
					s[12] = 0.f;
					for (int k = 0; k < 3; k++) s[k] = 0.f;
				}
			}
		}
		int step;
		if (own) { s[14] += sps * INV_SR; step = s[14] >= 1.f; if (step) s[14] -= 1.f; }
		else step = edge(in[2][i], &s[16]);
		if (step && s[11] < 1.f) {
			int stage = (int)s[10];
			for (int v = 0; v < 3; v++) {
				int len = mv == 2 ? WG_FB_BEATS[(int)s[9] & 3] * 4 : WG_FB_LEN[v];
				int pos = (int)s[v] % len;
				int adv = 1;
				if (mv == 2 && stage >= 3) {        /* the tempo drifts: a voice now and then rushes or drags */
					float r = scq_rand01(&n->rng);
					if (r < 0.04f) adv = 0; else if (r < 0.08f) adv = 2;
				}
				int play = wg_bit(pat, pos + (int)s[3 + v]);
				if (mv == 2 && stage >= 6 && !play && scq_rand01(&n->rng) < 0.08f) play = 1;
				if (mv == 2 && stage >= 9 && scq_rand01(&n->rng) < s[11]) play = 0;
				if (play) s[6 + v] = PULSE_SAMPLES;
				pos += adv;
				if (pos >= len) {
					pos -= len;
					if (mv == 1) s[3 + v] = (float)(WG_FB_MAT[scq_rng(&n->rng) % 6u] + 64);
					if (mv == 2 && v == 0) {                /* the violin turns the sequence of loop lengths */
						s[9] += 1.f;
						if (s[9] >= 4.f) { s[9] = 0.f; s[10] = scq_min(s[10] + 1.f, 99.f); }
					}
				}
				s[v] = (float)pos;
			}
			if (mv == 2 && stage >= 9) s[11] = scq_min(1.f, s[11] + 1.f / 350.f);   /* about 40 s to dissolve */
		}
		float any = 0.f;
		for (int v = 0; v < 3; v++) {
			float o = s[6 + v] > 0.f ? 1.f : 0.f;
			n->out[v][i] = o;
			any = scq_max(any, o);
			if (s[6 + v] > 0.f) s[6 + v] -= 1.f;
		}
		n->out[3][i] = any;
	}
}

/* ------------------------------------------------------------------ Double Knot, Benjolin */

/* knot — two knots in one rope: two voices, each a triangle-to-square oscillator stepped by an
 * 8-bit shift register (XOR feedback, lock holds the loop), an envelope and a VCA; cross lets
 * each voice's register clock the other (after the Lorre Mill Double Knot)
 * s: 0-1 osc phases, 2-3 registers, 4-5 clock phases, 6-7 envelopes, 8-9 attacking,
 *    10-11 pulse timers, 12 clock edge, 13-14 steps (semitones) */
static const float WG_KNOT_STEP[8] = { 0.f, 2.f, 3.f, 5.f, 7.f, 10.f, 12.f, 15.f };
static void init_knot(node *n) { n->s[2] = 180.f; n->s[3] = 45.f; }
static void wg_knot_clock(node *n, int v, float lock)
{
	float *s = n->s;
	uint32_t reg = (uint32_t)s[2 + v] & 0xffu;
	uint32_t last = (reg >> 7) & 1u;
	uint32_t data = s[1 - v] < 0.5f ? 1u : 0u;             /* the other oscillator's square */
	uint32_t nb = scq_rand01(&n->rng) < lock ? last : (last ^ data);
	reg = ((reg << 1) | nb) & 0xffu;
	s[2 + v] = (float)reg;
	s[13 + v] = WG_KNOT_STEP[reg & 7u];
	if (reg & 1u) { s[8 + v] = 1.f; s[10 + v] = PULSE_SAMPLES; }
}
static void run_knot(node *n, const float *const in[MAX_IN])
{
	float *s = n->s;
	float base = 60.f * n->nature * scq_exp2(in[1][0]);
	float rate = 0.5f * scq_exp2(5.f * scq_clamp(in[2][0], 0.f, 1.f));
	float lock = scq_clamp(in[3][0], 0.f, 1.f);
	float cross = scq_clamp(in[4][0], 0.f, 1.f);
	float shape = scq_clamp(in[5][0], 0.f, 1.f);
	float dsec = 0.03f * scq_exp2(6.f * scq_clamp(in[6][0], 0.f, 1.f));
	float dk = scq_exp2(-1.442695f * 6.9f / (dsec * SR));
	float ak = 1.f / (0.002f * SR);
	float drive = 1.f + 9.f * shape, makeup = 1.3f - 0.3f * shape;
	const int ext = (n->patched & 1u) != 0;
	float top = 0.45f * SR;
	LOOP {
		int ca = 0, cb = 0;
		if (ext) ca = edge(in[0][i], &s[12]);
		else { s[4] += rate * INV_SR; if (s[4] >= 1.f) { s[4] -= scq_floor(s[4]); ca = 1; } }
		s[5] += rate * 0.75f * INV_SR;
		if (s[5] >= 1.f) { s[5] -= scq_floor(s[5]); cb = 1; }
		if (ca) {
			wg_knot_clock(n, 0, lock);
			if (((uint32_t)s[2] & 1u) && scq_rand01(&n->rng) < cross) cb = 1;
		}
		if (cb) {
			wg_knot_clock(n, 1, lock);
			if (!ca && ((uint32_t)s[3] & 1u) && scq_rand01(&n->rng) < cross * 0.5f) wg_knot_clock(n, 0, lock);
		}
		float y[2];
		for (int v = 0; v < 2; v++) {
			float f = scq_min(base * (v ? 1.5f : 1.f) * scq_exp2(s[13 + v] * (1.f / 12.f)), top);
			float p = s[v] + f * INV_SR;
			s[v] = p - scq_floor(p);
			float tri = 1.f - 4.f * scq_abs(s[v] - 0.5f);
			float e = s[6 + v];
			if (s[8 + v] > 0.f) { e += ak; if (e >= 1.f) { e = 1.f; s[8 + v] = 0.f; } }
			else e *= dk;
			s[6 + v] = e;
			y[v] = scq_tanh(tri * drive) * makeup * e * e;
		}
		n->out[0][i] = 0.7f * (y[0] + 0.35f * y[1]);
		n->out[1][i] = 0.7f * (y[1] + 0.35f * y[0]);
		for (int v = 0; v < 2; v++) { n->out[2 + v][i] = s[10 + v] > 0.f ? 1.f : 0.f; if (s[10 + v] > 0.f) s[10 + v] -= 1.f; }
	}
	wg_flush(&s[6]); wg_flush(&s[7]);
}

/* benjolin — a little box of bad intentions: two oscillators, the rungler between them, a twin-peak
 * filter on their comparison, chaos to taste (after Rob Hordijk's Benjolin)
 * s: 0-1 phases, 2 register, 3 rungler, 4-7 the two filters */
static void init_benjolin(node *n) { n->s[2] = (float)(n->rng & 0xffu); }
/* a TPT state-variable filter's three coefficients for a cutoff and a damping */
static inline void wg_svf(float hz, float k, float *a)
{
	float g = scq_tan(SCQ_PI * scq_clamp(hz, 10.f, 0.4f * SR) * INV_SR);
	a[0] = 1.f / (1.f + g * (g + k)); a[1] = g * a[0]; a[2] = g * a[1];
}
static void run_benjolin(node *n, const float *const in[MAX_IN])
{
	float *s = n->s;
	float f1 = 60.f * n->nature * scq_exp2(in[0][0]);
	float f2 = f1 * scq_exp2(6.f * (scq_clamp(in[1][0], 0.f, 1.f) - 0.5f));
	float chaos = scq_clamp(in[2][0], 0.f, 1.f);
	float fc = 60.f * scq_exp2(8.f * scq_clamp(in[3][0], 0.f, 1.f));
	float k = 2.f - 1.9f * scq_clamp(in[4][0], 0.f, 1.f);
	int loop = in[5][0] > 0.5f;
	float top = 0.45f * SR;
	float run = s[3];
	float m1 = scq_exp2(chaos * 3.f * (run - 0.5f)), m2 = scq_exp2(chaos * 4.f * (0.5f - run));
	float ca[3], cb[3];
	float fcr = fc * scq_exp2(chaos * 2.5f * (run - 0.5f));
	wg_svf(fcr, k, ca); wg_svf(fcr * 2.5f, k, cb);
	LOOP {
		float p1 = s[0] + scq_min(f1 * m1, top) * INV_SR;
		s[0] = p1 - scq_floor(p1);
		float tri1 = 1.f - 4.f * scq_abs(s[0] - 0.5f);
		float p2 = s[1] + scq_min(f2 * m2 * (1.f + 0.5f * chaos * tri1), top) * INV_SR;
		int clock = p2 >= 1.f;
		s[1] = p2 - scq_floor(p2);
		if (clock) {                                 /* osc 2 clocks the rungler; osc 1's square is the data */
			uint32_t reg = (uint32_t)s[2] & 0xffu, last = (reg >> 7) & 1u;
			uint32_t nb = loop ? last : ((s[0] < 0.5f ? 1u : 0u) ^ last);
			reg = ((reg << 1) | nb) & 0xffu;
			s[2] = (float)reg;
			float nr = (float)(reg & 7u) * (1.f / 7.f);
			if (nr != run) {
				run = nr;
				m1 = scq_exp2(chaos * 3.f * (run - 0.5f)); m2 = scq_exp2(chaos * 4.f * (0.5f - run));
				fcr = fc * scq_exp2(chaos * 2.5f * (run - 0.5f));
				wg_svf(fcr, k, ca); wg_svf(fcr * 2.5f, k, cb);
			}
		}
		float tri2 = 1.f - 4.f * scq_abs(s[1] - 0.5f);
		float x = tri1 > tri2 ? 0.5f : -0.5f;        /* the comparator: the Benjolin's PWM */
		/* two resonant peaks a tenth and a bit apart, the upper one turned over: a notch between */
		float v3 = x - s[5];
		float v1 = ca[0] * s[4] + ca[1] * v3;
		float v2 = s[5] + ca[1] * s[4] + ca[2] * v3;
		s[4] = 2.f * v1 - s[4]; s[5] = 2.f * v2 - s[5];
		float y = k * v1 + 0.3f * v2;
		v3 = x - s[7];
		v1 = cb[0] * s[6] + cb[1] * v3;
		v2 = s[7] + cb[1] * s[6] + cb[2] * v3;
		s[6] = 2.f * v1 - s[6]; s[7] = 2.f * v2 - s[7];
		y -= 0.8f * k * v1;
		n->out[0][i] = scq_tanh(y * 1.2f);
		n->out[1][i] = tri1;
		n->out[2][i] = s[1] < 0.5f ? 1.f : 0.f;
		n->out[3][i] = run;
	}
	s[3] = run;
	for (int f = 4; f < 8; f++) wg_flush(&s[f]);
}

/* ------------------------------------------------------------------ resonators with ideas */

/* listen — tuned to the ground: while listen is held it listens to what comes in (a constant-Q
 * Goertzel bank, a semitone apart from 50 Hz to 3.2 kHz, every bin running a little each sample
 * so nothing spikes); when listen lets go it picks the place's nine strongest resonances, one a
 * block, and tunes its nine Wingie modes to them; locked pulses when they are in
 * (after Tuned to the Ground, for the Wingie2)
 * mem: 9 modes, 73 bins, 9 chosen. s: 0-2 input chain, 3 strike edge, 4 listen edge,
 *    5 picking (0 no, 1..9 the one being picked), 6 decimator count, 7 decimator sum, 8-9 anti-alias,
 *    10 the rate the bins were set for, 11 pulse timer, 12 found, 13 the strongest */
#define WG_LBINS 73
#define WG_LDEC  6
#define WG_LQ    14.f  /* cycles a bin listens for (a triangle window over them: sidelobes 26 dB down) */
typedef struct { float s1, s2, k, pw, cnt, len, inv; } wg_bin;   /* inv: 2 / len, for the window */
static uint32_t mem_listen(float nature) { (void)nature; return WG_N * sizeof(wg_mode) + WG_LBINS * sizeof(wg_bin) + WG_N * sizeof(float); }
static void run_listen(node *n, const float *const in[MAX_IN])
{
	wg_mode *m = (wg_mode *)n->mem;
	wg_bin *bin = (wg_bin *)(m + WG_N);
	float *chosen = (float *)(bin + WG_LBINS);
	float *s = n->s;
	float fd = SR / (float)WG_LDEC;
	if (s[10] != SR) {                              /* the bins, for this rate */
		s[10] = SR;
		for (int b = 0; b < WG_LBINS; b++) {
			float hz = 50.f * scq_exp2((float)b * (1.f / 12.f)), sn, cs;
			wg_sincos(hz, fd, &sn, &cs);
			bin[b].k = 2.f * cs;
			bin[b].len = hz < 0.45f * fd ? (float)(int)(WG_LQ * fd / hz + 0.5f) : 0.f;
			bin[b].inv = bin[b].len > 0.f ? 2.f / bin[b].len : 0.f;
			bin[b].s1 = bin[b].s2 = bin[b].pw = bin[b].cnt = 0.f;
		}
	}
	/* the tuning: the place's, or (before it has heard one) the Low cave moved by the nature */
	if (s[5] == 0.f && !(chosen[0] >= 16.f)) for (int k = 0; k < WG_N; k++) chosen[k] = WG_CAVE[0][k] * scq_clamp(n->nature, 0.1f, 8.f);
	int listening = in[1][0] > 0.5f;
	int was = s[4] > 0.5f;
	s[4] = listening ? 1.f : 0.f;
	if (listening && !was) {
		for (int b = 0; b < WG_LBINS; b++) bin[b].s1 = bin[b].s2 = bin[b].pw = bin[b].cnt = 0.f;
		s[5] = 0.f;
	}
	if (!listening && was) { s[5] = 1.f; s[12] = 0.f; }
	if (s[5] >= 1.f) {                              /* pick one resonance this block */
		int j = (int)s[5] - 1, best = -1;
		float bp = 0.f;
		for (int b = 1; b < WG_LBINS - 1; b++) {
			float p = bin[b].pw;
			if (!(p > bp) || p < bin[b - 1].pw || p < bin[b + 1].pw || bin[b].len == 0.f) continue;
			int near = 0;
			for (int c = 0; c < j; c++) { float d = (float)b - chosen[c]; if (d < 2.f && d > -2.f) near = 1; }
			if (!near) { bp = p; best = b; }
		}
		if (best >= 0 && bp > 1e-12f && (j == 0 || bp > s[13] * 0.01f)) {
			chosen[j] = (float)best;                 /* a bin for now: Hz when all are in */
			s[12] = (float)(j + 1);
			if (j == 0) s[13] = bp;
			s[5] += 1.f;
		}
		if ((int)s[12] < j + 1 || s[5] > (float)WG_N) {
			int found = (int)s[12];
			if (found > 0) {
				float hz[WG_N];
				for (int c = 0; c < found; c++) {
					int b = (int)chosen[c];
					float a = wg_log2(bin[b - 1].pw + 1e-30f), mid = wg_log2(bin[b].pw + 1e-30f), z = wg_log2(bin[b + 1].pw + 1e-30f);
					float d = a - 2.f * mid + z;
					float off = d < -1e-6f || d > 1e-6f ? scq_clamp(0.5f * (a - z) / d, -0.5f, 0.5f) : 0.f;
					hz[c] = 50.f * scq_exp2(((float)b + off) * (1.f / 12.f));
				}
				for (int c = found; c < WG_N; c++) hz[c] = hz[c % found] * scq_exp2((float)(c / found));
				for (int a = 1; a < WG_N; a++) {        /* low to high */
					float v = hz[a]; int b = a - 1;
					while (b >= 0 && hz[b] > v) { hz[b + 1] = hz[b]; b--; }
					hz[b + 1] = v;
				}
				for (int c = 0; c < WG_N; c++) chosen[c] = hz[c];
				s[11] = PULSE_SAMPLES;
				wg_strike(m, 0.25f);
			} else for (int k = 0; k < WG_N; k++) chosen[k] = m[k].hz;   /* heard nothing: as it was */
			s[5] = 0.f;
		}
	}
	if (s[5] == 0.f) for (int k = 0; k < WG_N; k++) wg_tune(&m[k], chosen[k]);
	float rho = wg_rho(wg_decay(in[2][0]));
	wg_block(m, rho);
	const float sc = 2.f / rho, lpa = wg_lpa(), aa = scq_coef_hz(0.35f * fd, SR);
	LOOP {
		if (edge(in[3][i], &s[3])) wg_strike(m, 0.3f);
		float x = in[0][i];
		if (listening) {                             /* the ear: anti-alias, decimate, every bin a step */
			s[8] += aa * (scq_clamp(x, -4.f, 4.f) - s[8]);
			s[9] += aa * (s[8] - s[9]);
			s[7] += s[9];
			s[6] += 1.f;
			if (s[6] >= (float)WG_LDEC) {
				float d = s[7] * (1.f / (float)WG_LDEC);
				s[6] = 0.f; s[7] = 0.f;
				for (int b = 0; b < WG_LBINS; b++) {
					wg_bin *g = &bin[b];
					if (g->len == 0.f) continue;
					float s0 = d * (1.f - scq_abs(g->cnt * g->inv - 1.f)) + g->k * g->s1 - g->s2;
					g->s2 = g->s1; g->s1 = s0;
					g->cnt += 1.f;
					if (g->cnt >= g->len) {
						g->pw += (g->s1 * g->s1 + g->s2 * g->s2 - g->k * g->s1 * g->s2) / g->len;
						if (!(g->pw < 1e20f)) g->pw = 1e20f;
						g->s1 = g->s2 = g->cnt = 0.f;
					}
				}
			}
		}
		float o, e;
		wg_tick(m, wg_in(x, &s[0], lpa), &o, &e);
		n->out[0][i] = wg_out((o + e) * sc);
		n->out[1][i] = s[11] > 0.f ? 1.f : 0.f;
		if (s[11] > 0.f) s[11] -= 1.f;
	}
	for (int k = 0; k < 3; k++) wg_flush(&s[k]);
	wg_flush(&s[8]); wg_flush(&s[9]);
}

/* talkingdrum — the drum under the arm: a membrane whose pitch is squeezed; mode=rise climbs with
 * how loud what comes in is, mode=fall starts every stroke high and lets it fall (after the West
 * African talking drum; the Wingie personality)
 * mem: 9 modes. s: 0-2 input chain, 3 strike edge, 4 follower, 5 squeeze, 6 armed */
static void run_talkingdrum(node *n, const float *const in[MAX_IN])
{
	wg_mode *m = (wg_mode *)n->mem;
	float *s = n->s;
	int fall = (int)n->opt[0] == 1;
	/* the follower and the stroke, read off last block's level */
	float env = s[4];
	float e = scq_clamp(env * 6.f, 0.f, 1.f);
	int onset = 0;
	if (s[6] == 0.f && e > 0.35f) { onset = 1; s[6] = 1.f; }
	if (e < 0.15f) s[6] = 0.f;
	int struck = 0;
	LOOP if (edge(in[3][i], &s[3])) struck = 1;
	float sq = s[5], held = scq_clamp(in[1][0], 0.f, 1.5f);
	float blk = (float)B * INV_SR;
	float kup = 1.f - scq_exp2(-1.442695f * blk / 0.005f), kdown = 1.f - scq_exp2(-1.442695f * blk / (fall ? 0.3f : 0.15f));
	if (!fall) {
		float target = e + held;
		sq += (target > sq ? kup : kdown) * (target - sq);
	} else {
		if (onset || struck) sq = 1.f + held;
		sq += kdown * (held - sq);
	}
	if (!scq_sane(sq)) sq = 0.f;
	s[5] = sq;
	float bend = scq_clamp(in[4][0], 0.f, 1.f) * scq_clamp(sq, 0.f, 1.5f);   /* depth 1 = an octave */
	float base = 60.f * n->nature * scq_exp2(in[2][0] + bend);
	for (int k = 0; k < WG_N; k++) wg_tune(&m[k], base * WG_MEMBRANE[k]);
	if (struck) wg_strike(m, 0.35f);
	float rho = wg_rho(wg_decay(in[5][0]));
	wg_block(m, rho);
	const float sc = 2.f / rho, lpa = wg_lpa(), rel = scq_exp2(-1.442695f / (0.025f * SR));
	LOOP {
		float x = in[0][i], a = scq_abs(scq_clamp(x, -4.f, 4.f));
		env = a > env ? a : a + rel * (env - a);
		float o, ev;
		wg_tick(m, wg_in(x, &s[0], lpa), &o, &ev);
		n->out[0][i] = wg_out((o + ev) * sc);
	}
	s[4] = env;
	for (int k = 0; k < 5; k++) wg_flush(&s[k]);
}

/* metastaseis — the ruled surface: nine single resonances gliding in straight lines of pitch
 * from chord A to chord B (go turns them round), each starting a little after the one below
 * (stagger), kept sounding by dust; nature is the glide in seconds (after Iannis Xenakis,
 * Metastaseis, 1954)
 * mem: 9 modes. s: 0-2 input chain, 3 go edge, 4 position, 5 direction */
static const float WG_CHORDS[10][WG_N] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 },                    /* unison: the opening G */
	{ -4, -3, -2, -1, 0, 1, 2, 3, 4 },                /* cluster */
	{ -12, -5, 2, 9, 16, 23, 30, 37, 44 },            /* fifths */
	{ -12, -9, -5, 0, 3, 7, 12, 15, 19 },             /* minor */
	{ -12, -8, -5, 0, 4, 7, 12, 16, 19 },             /* major */
	{ -12, -9, -6, -3, 0, 3, 6, 9, 12 },              /* diminished */
	{ -8, -6, -4, -2, 0, 2, 4, 6, 8 },                /* whole tone */
	{ -12, 0, 7.02f, 12, 15.86f, 19.02f, 21.69f, 24, 26.04f },   /* harmonics 1-9 */
	{ -12, -10, -8, -5, -3, 0, 2, 4, 7 },             /* pentatonic */
	{ -12, -7, -2, 3, 8, 13, 18, 23, 28 },            /* fourths */
};
/* chorda=unison|cluster|... and chordb=cluster|unison|...: the switch position to the table */
static const uint8_t WG_CHORD_B[10] = { 1, 0, 2, 3, 4, 5, 6, 7, 8, 9 };
static void run_metastaseis(node *n, const float *const in[MAX_IN])
{
	wg_mode *m = (wg_mode *)n->mem;
	float *s = n->s;
	int ca = (int)n->opt[0], cb = (int)n->opt[1];
	if (ca < 0 || ca > 9) ca = 0;
	if (cb < 0 || cb > 9) cb = 0;
	const float *A = WG_CHORDS[ca], *Bc = WG_CHORDS[WG_CHORD_B[cb]];
	int cycle = (int)n->opt[2] == 1;
	float pos = s[4], dir = s[5];
	int go = 0;
	LOOP if (edge(in[1][i], &s[3])) go = 1;
	if (go) dir = pos >= 0.5f ? -1.f : 1.f;
	if (cycle && dir == 0.f) dir = pos >= 0.5f ? -1.f : 1.f;
	if (dir != 0.f) {
		pos += dir * (float)B * INV_SR / scq_max(n->nature, 0.05f);
		if (pos >= 1.f) { pos = 1.f; dir = cycle ? -1.f : 0.f; }
		if (pos <= 0.f) { pos = 0.f; dir = cycle ? 1.f : 0.f; }
	}
	if (!(pos >= 0.f && pos <= 1.f)) pos = 0.f;
	s[4] = pos; s[5] = dir;
	float stagger = scq_clamp(in[3][0], 0.f, 1.f) * 0.5f;
	float ref = in[2][0];
	for (int v = 0; v < WG_N; v++) {
		float t = scq_clamp((pos - stagger * (float)v * (1.f / 8.f)) / (1.f - stagger), 0.f, 1.f);
		float semis = A[v] + (Bc[v] - A[v]) * t;
		wg_tune(&m[v], 120.f * scq_exp2(ref + semis * (1.f / 12.f)));
	}
	float dens = scq_clamp(in[4][0], 0.f, 1.f);
	float chance = dens > 0.01f ? 0.5f * scq_exp2(8.64f * dens) * INV_SR : 0.f;   /* 0.5 to 200 strikes a second */
	float rho = wg_rho(wg_decay(in[5][0]));
	wg_block(m, rho);
	const float sc = 2.f / rho, lpa = wg_lpa();
	LOOP {
		if (scq_rand01(&n->rng) < chance) {
			int v = (int)(scq_rng(&n->rng) % (uint32_t)WG_N);
			m[v].p += scq_rand01(&n->rng) * 0.35f * m[v].g;
		}
		float o, e;
		wg_tick(m, wg_in(in[0][i], &s[0], lpa), &o, &e);
		n->out[0][i] = wg_out((o + e) * sc);
		n->out[1][i] = wg_out(o * sc);
		n->out[2][i] = wg_out(e * sc);
		n->out[3][i] = pos;
	}
	for (int k = 0; k < 3; k++) wg_flush(&s[k]);
}

/* sittingroom — I am sitting in a room: what comes in is played into a room and recorded again,
 * generation after generation (generations 0..1 is 0..32 passes): with every pass the room's own
 * resonances sharpen (a pass is the room's response once more: after N of them a mode is
 * 1 / sqrt(2^(1/N) - 1) times narrower) and the strongest swallow the rest, until only the room
 * is left singing. The room's modes are a real room's (nature 1 is 5.2 x 4.1 x 2.7 m; 2 is twice
 * that), picked by the place's name (after Alvin Lucier, I Am Sitting in a Room, 1969)
 * mem: 12 modes x (two states, g, k, gain, hz, strength). s: 0 the generations the coefficients
 * were made for (+1), 1 wet, 2 the rate they were made for, 3-4 levels in and out, 5 the level gain */
#define WG_ROOM 12
#define WG_RM   8     /* a mode: 0-1 states, 2-4 coefficients, 5 out gain, 6 Hz, 7 strength */
static uint32_t mem_sittingroom(float nature) { (void)nature; return WG_ROOM * WG_RM * sizeof(float); }
static void init_sittingroom(node *n)
{
	float *r = (float *)n->mem;
	const float L[3] = { 5.2f, 4.1f, 2.7f };
	float size = scq_clamp(n->nature, 0.25f, 8.f);
	uint32_t rng = n->rng ^ 0x51771e5u;
	if (!rng) rng = 1u;
	int got = 0;
	for (int tries = 0; tries < 600 && got < WG_ROOM; tries++) {
		float nx = (float)(scq_rng(&rng) % 9u), ny = (float)(scq_rng(&rng) % 9u), nz = (float)(scq_rng(&rng) % 6u);
		float a = nx / L[0], b = ny / L[1], c = nz / L[2];
		float q = a * a + b * b + c * c;
		if (q <= 0.f) continue;
		float hz = 171.5f * scq_sqrt_from(q, 2.f) / size;   /* c / 2 x the mode's wavenumber */
		if (hz < 90.f || hz > 1800.f) continue;
		int close = 0;
		for (int k = 0; k < got; k++) { float d = r[k * WG_RM + 6] / hz; if (d > 0.97f && d < 1.03f) close = 1; }
		if (close) continue;
		r[got * WG_RM + 6] = hz;
		r[got * WG_RM + 7] = 0.72f + 0.26f * scq_rand01(&rng);   /* how well speaker, room and microphone meet there */
		got++;
	}
	for (; got < WG_ROOM; got++) { r[got * WG_RM + 6] = 150.f * (float)(got + 1) / size; r[got * WG_RM + 7] = 0.7f; }
	r[(int)(scq_rng(&rng) % WG_ROOM) * WG_RM + 7] = 1.f;    /* the one the room loves most */
	n->s[0] = 0.f;
}
static void run_sittingroom(node *n, const float *const in[MAX_IN])
{
	float *r = (float *)n->mem;
	float gen = scq_clamp(in[1][0], 0.f, 1.f);
	if (!(r[6] > 0.f)) init_sittingroom(n);
	if (scq_abs(gen + 1.f - n->s[0]) > 0.002f || n->s[2] != SR) {   /* the room, for this many passes */
		n->s[0] = gen + 1.f;
		n->s[2] = SR;
		float N = 32.f * gen;
		const float q0 = 6.f;
		float q = N > 1.f ? q0 / scq_sqrt_from(scq_exp2(1.f / N) - 1.f, 0.1f) : q0;
		float comp = scq_sqrt_from(q / q0, 1.f);
		float sum = 0.f, a[WG_ROOM];
		for (int k = 0; k < WG_ROOM; k++) { a[k] = scq_exp2(scq_max(N, 1.f) * wg_log2(r[k * WG_RM + 7])); sum += a[k] * a[k]; }
		float norm = comp / scq_sqrt_from(sum, 1.f);
		for (int k = 0; k < WG_ROOM; k++) {
			float *m = &r[k * WG_RM];
			wg_svf(m[6], 1.f / q, &m[2]);
			m[5] = a[k] * norm / q;                  /* band x k: unity at the peak */
		}
		n->s[1] = 1.f - scq_exp2(-N / 1.5f);
	}
	float wet = n->s[1];
	/* each generation is played back as loud as the voice was (Lucier set the level every time):
	 * the room is matched to what comes in, slowly, by up to 40 times */
	float ei = n->s[3], eo = n->s[4], gain = n->s[5];
	if (!(gain >= 1.f)) gain = 1.f;
	float want = scq_clamp(ei / (eo + 1e-4f), 1.f, 40.f);
	float gstep = (want - gain) * scq_min(1.f, (float)B * INV_SR / 0.4f) / (float)B;
	const float ek = scq_min(1.f, INV_SR / 0.3f);
	LOOP {
		float x = scq_clamp(in[0][i], -4.f, 4.f), y = 0.f;
		for (int k = 0; k < WG_ROOM; k++) {
			float *m = &r[k * WG_RM];
			float v3 = x - m[1];
			float v1 = m[2] * m[0] + m[3] * v3;
			float v2 = m[1] + m[3] * m[0] + m[4] * v3;
			m[0] = 2.f * v1 - m[0]; m[1] = 2.f * v2 - m[1];
			y += v1 * m[5];
		}
		ei += ek * (scq_abs(x) - ei);
		eo += ek * (scq_abs(y) - eo);
		gain += gstep;
		y = scq_softclip(y * gain);
		n->out[1][i] = y;
		n->out[0][i] = x * (1.f - wet) + y * wet;
	}
	n->s[3] = ei; n->s[4] = eo; n->s[5] = scq_clamp(gain, 1.f, 40.f);
	wg_flush(&n->s[3]); wg_flush(&n->s[4]);
	for (int k = 0; k < WG_ROOM; k++) { wg_flush(&r[k * WG_RM]); wg_flush(&r[k * WG_RM + 1]); }
}
