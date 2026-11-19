/*
 * Plain modular modules: Doepfer A-100 and Serge style, and the Buchla 100 series.
 * Built to be cheap so big patches fit: controls are read once per block, no buffers,
 * no per-sample exp/tan. Included by scq_places.c after the other scq_more_*.inc.
 */

#define OPT(k) ((int)n->opt[k])

/* time for a 0..1 control: 1 ms .. 10 s, or 100x faster in the audio range */
static inline float seg_seconds(float x, int audio)
{
	float t = 0.001f * scq_exp2(scq_clamp(x, 0.f, 1.f) * 13.3f);
	return audio ? t * 0.01f : t;
}

/* curve switch lin|exp|log, applied to a 0..1 level (cheap: no exp per sample) */
static inline float shape_curve(float v, int curve)
{
	return curve == 1 ? v * v : (curve == 2 ? v * (2.f - v) : v);
}

/* ------------------------------------------------------------------ sources */

/* sinesaw — dual sine-sawtooth oscillator (after Buchla 158). range=audio|lfo */
static void run_sinesaw(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	float base = 60.f * n->nature * scq_exp2(in[0][0]) * (OPT(0) ? 0.01f : 1.f);
	LOOP {
		float dt = scq_clamp(base * (1.f + in[1][i]) * INV_SR, 0.f, 0.45f);
		n->out[0][i] = scq_sin2pi(ph);
		n->out[1][i] = saw_blep(ph, dt);
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* square — square / pulse oscillator (after Buchla 144; Doepfer A-110) */
static void run_square(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	float base = 60.f * n->nature * scq_exp2(in[0][0]) * (OPT(0) ? 0.01f : 1.f);
	float w = 0.5f + 0.45f * scq_clamp(in[2][0], -1.f, 1.f);
	LOOP {
		float dt = scq_clamp(base * (1.f + in[1][i]) * INV_SR, 0.f, 0.45f);
		n->out[0][i] = square_blep(ph, dt, w);
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* harmonics — harmonic generator: harmonics 1-4 of one cosine, by Chebyshev polynomials
 * (after Buchla 148) */
static void run_harmonics(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	float dt = scq_clamp(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.f, 0.11f);
	LOOP {
		float x = scq_sin2pi(ph + 0.25f);
		float t2 = 2.f * x * x - 1.f, t3 = 2.f * x * t2 - x;
		n->out[0][i] = x; n->out[1][i] = t2; n->out[2][i] = t3; n->out[3][i] = 2.f * x * t3 - t2;
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* pulser — timing pulse generator: a pulse every period, trig fires it now (after Buchla 140) */
static void run_pulser(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], pt = n->s[1];
	float inc = 0.5f * n->nature * scq_exp2(scq_clamp(in[0][0], -1.f, 1.f) * 8.f) * INV_SR;
	LOOP {
		ph += inc;
		if (ph >= 1.f || edge(in[1][i], &n->s[2])) { ph -= scq_floor(ph); if (ph >= 1.f) ph = 0.f; pt = PULSE_SAMPLES; }
		n->out[0][i] = pt > 0.f ? 1.f : 0.f;
		n->out[1][i] = ph;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = ph; n->s[1] = pt;
}

/* ------------------------------------------------------------------ shapes */

/* mixer — six inputs summed (after Buchla 106; Doepfer A-138). Unpatched inputs cost nothing */
static void run_mixer(node *n, const float *const in[MAX_IN])
{
	LOOP n->out[0][i] = 0.f;
	for (int k = 0; k < 6; k++)
		if (n->patched & (1u << k)) LOOP n->out[0][i] += in[k][i];
}

/* vcmix — four inputs, each with its own level (after Buchla 107; Doepfer A-135) */
static void run_vcmix(node *n, const float *const in[MAX_IN])
{
	LOOP n->out[0][i] = 0.f;
	for (int k = 0; k < 4; k++)
		if (n->patched & (1u << k)) LOOP n->out[0][i] += in[k][i] * in[4 + k][i];
}

/* vcgate — dual voltage-controlled gate (after Buchla 110) */
static void run_vcgate(node *n, const float *const in[MAX_IN])
{
	LOOP { n->out[0][i] = in[0][i] * in[2][i]; n->out[1][i] = in[1][i] * in[3][i]; }
}

/* quadvca — four VCAs; response=lin|exp (after Doepfer A-130 / A-131, A-132) */
static void run_quadvca(node *n, const float *const in[MAX_IN])
{
	int ex = OPT(0);
	for (int k = 0; k < 4; k++) {
		if (!(n->patched & (1u << k))) { LOOP n->out[k][i] = 0.f; continue; }
		const float *x = in[k], *cv = in[4 + k];
		float *y = n->out[k];
		if (ex) LOOP { float g = scq_max(cv[i], 0.f); y[i] = x[i] * (g * g * g); }
		else LOOP { float g = scq_max(cv[i], 0.f); y[i] = x[i] * g; }
	}
}

/* highpass — resonant 2-pole highpass at nature x 120 Hz (after the Doepfer A-100 filters) */
static void run_highpass(node *n, const float *const in[MAX_IN])
{
	svf_c c = svf_make(120.f * n->nature * scq_exp2(in[1][0]), 2.f - 1.96f * scq_clamp(in[2][0], 0.f, 1.f));
	LOOP {
		float lp, x = in[0][i];
		float bp = svf_run(&c, &n->s[0], &n->s[1], x, &lp);
		n->out[0][i] = x - c.k * bp - lp;
	}
}

/* attenuverter — in x amount (-1..1) + offset (after Doepfer A-183-2) */
static void run_attenuverter(node *n, const float *const in[MAX_IN])
{
	LOOP n->out[0][i] = in[0][i] * in[1][i] + in[2][i];
}

/* sineshaper — triangle / anything into sine, more drive folds it (after Serge waveshapers) */
static void run_sineshaper(node *n, const float *const in[MAX_IN])
{
	float k = 0.25f * (1.f + scq_max(in[1][0], 0.f) * 7.f);
	LOOP n->out[0][i] = scq_sin2pi(in[0][i] * k);
}

/* minmax — the higher and the lower of a and b (after Serge Peak & Trough) */
static void run_minmax(node *n, const float *const in[MAX_IN])
{
	LOOP {
		float a = in[0][i], b = in[1][i];
		n->out[0][i] = a > b ? a : b;
		n->out[1][i] = a > b ? b : a;
	}
}

/* cvprocessor — sums a, b and an offset, then lags it by time (after Buchla 156) */
static void run_cvprocessor(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0];
	float c = 1.f - 1.f / (1.f + seg_seconds(in[3][0], 0) * SR * 0.25f);
	if (in[3][0] <= 0.f) c = 0.f;
	LOOP {
		float x = in[0][i] + in[1][i] + in[2][i];
		v = x + (v - x) * c;
		n->out[0][i] = v;
	}
	n->s[0] = v;
}

/* slew — slew limiter: rise and fall times; shape=lin|exp (after Doepfer A-170; Serge slew) */
static void run_slew(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0];
	float up = 1.f / (seg_seconds(in[1][0], 0) * SR), dn = 1.f / (seg_seconds(in[2][0], 0) * SR);
	if (OPT(0)) {                             /* exponential: a one-pole each way */
		up = scq_min(up * 4.f, 1.f); dn = scq_min(dn * 4.f, 1.f);
		LOOP { float x = in[0][i]; v += (x - v) * (x > v ? up : dn); n->out[0][i] = v; }
	} else {
		LOOP { float x = in[0][i]; v = x > v ? scq_min(x, v + up) : scq_max(x, v - dn); n->out[0][i] = v; }
	}
	n->s[0] = v;
}

/* the vactrol under a low pass gate: fast on, slow off; per block it sets the cutoff */
static inline void lpg_run(float *st, const float *x, const float *cv, float *out, int mode, float base)
{
	float v = st[0], l1 = st[1], l2 = st[2];
	const float att = 1.f / (0.002f * SR), rel = 1.f / (0.25f * SR);
	float c = mode == 2 ? 1.f : scq_coef_hz(base * scq_exp2(v * 10.f), SR);
	LOOP {
		float t = scq_clamp(cv[i], 0.f, 1.f);
		v += (t - v) * (t > v ? att : rel * (0.3f + v));
		l1 += c * (x[i] - l1);
		l2 += c * (l1 - l2);
		out[i] = mode == 1 ? l2 : l2 * v;
	}
	st[0] = v; st[1] = l1; st[2] = l2;
}

/* duallpg — two low pass gates; mode1 / mode2 = both|lowpass|vca (after Buchla 292) */
static void run_duallpg(node *n, const float *const in[MAX_IN])
{
	lpg_run(&n->s[0], in[0], in[1], n->out[0], OPT(0), 20.f);
	lpg_run(&n->s[3], in[2], in[3], n->out[1], OPT(1), 20.f);
}

/* dualprocessor — two voltage-controlled lag / slew processors: smooth control voltages, or
 * run audio through them as a gentle lowpass (rate 0..1 is 20 Hz .. 20 kHz).
 * mode1 / mode2 = lag|slew (after the Serge Dual Processor) */
static void run_dualprocessor(node *n, const float *const in[MAX_IN])
{
	for (int ch = 0; ch < 2; ch++) {
		const float *x = in[ch * 2];
		float hz = 20.f * scq_exp2(scq_clamp(in[ch * 2 + 1][0], 0.f, 1.f) * 10.f);
		float v = n->s[ch];
		if (OPT(ch)) {
			float d = hz * 2.f * INV_SR;
			LOOP { float t = x[i]; v = t > v ? scq_min(t, v + d) : scq_max(t, v - d); n->out[ch][i] = v; }
		} else {
			float c = scq_min(hz * 6.2831853f * INV_SR, 1.f);
			LOOP { v += c * (x[i] - v); n->out[ch][i] = v; }
		}
		n->s[ch] = v;
	}
}

/* lpg — low pass gate, vactrol-style; pulse level for a pluck. mode=both|lowpass|vca (after Buchla 292) */
static void run_lpg(node *n, const float *const in[MAX_IN])
{
	lpg_run(&n->s[0], in[0], in[1], n->out[0], OPT(0), 20.f);
}

/* ------------------------------------------------------------------ currents */

/* one slope: rises and falls when triggered, follows `in` otherwise (Serge's universal slope).
 * st: value, mode (0 follow, 1 rise, 2 fall), trigger memory, end-pulse timer */
static inline void slope_run(float *st, const float *x, const float *trig, float rise, float fall,
                             int cycle, int audio, int curve, float *out, float *end)
{
	float v = st[0], mode = st[1], et = st[3];
	const float up = 1.f / (seg_seconds(rise, audio) * SR), dn = 1.f / (seg_seconds(fall, audio) * SR);
	LOOP {
		float tg = trig[i];
		if (edge(tg, &st[2])) mode = 1.f;
		if (mode == 1.f) {
			v += up;
			if (v >= 1.f) { v = 1.f; mode = 2.f; }
		} else if (mode == 2.f) {
			v -= dn;
			if (v <= 0.f) {
				v = 0.f; mode = 0.f; et = PULSE_SAMPLES;
				if (tg > 0.5f || cycle) mode = 1.f;
			}
		} else {
			float t = x[i];
			v = t > v ? scq_min(t, v + up) : scq_max(t, v - dn);
		}
		out[i] = curve ? shape_curve(v, curve) : v;
		end[i] = et > 0.f ? 1.f : 0.f;
		if (et > 0.f) et -= 1.f;
	}
	st[0] = v; st[1] = mode; st[3] = et;
}

/* dusg — dual universal slope generator: two slopes, each a function generator, envelope,
 * LFO (cycle) or audio oscillator (range=audio), slew or lowpass on what comes in.
 * cycle1 / cycle2 = off|on, range = lfo|audio, curve = lin|exp|log (after the Serge DUSG) */
static void run_dusg(node *n, const float *const in[MAX_IN])
{
	slope_run(&n->s[0], in[0], in[1], in[2][0], in[3][0], OPT(0), OPT(2), OPT(3), n->out[0], n->out[1]);
	slope_run(&n->s[4], in[4], in[5], in[6][0], in[7][0], OPT(1), OPT(2), OPT(3), n->out[2], n->out[3]);
}
/* slope — one universal slope; cycle=off|on range=lfo|audio curve=lin|exp|log */
static void run_slope(node *n, const float *const in[MAX_IN])
{
	slope_run(&n->s[0], in[0], in[1], in[2][0], in[3][0], OPT(0), OPT(1), OPT(2), n->out[0], n->out[1]);
}
static void init_dusg(node *n) { n->s[3] = PULSE_SAMPLES; n->s[7] = PULSE_SAMPLES; }

/* ad — attack / decay envelope; loop=off|on makes it an LFO, range=lfo|audio, curve=exp|lin
 * (after Buchla 180 Dual Attack Generator; Doepfer A-142) */
static void run_ad(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], stage = n->s[1], et = n->s[3];
	int audio = OPT(1), lin = OPT(2);
	float a = 1.f / (seg_seconds(in[1][0], audio) * SR);
	float d = 1.f / (seg_seconds(in[2][0], audio) * SR);
	float dk = scq_min(d * 5.f, 1.f);
	LOOP {
		if (edge(in[0][i], &n->s[2])) stage = 1.f;
		if (stage == 1.f) { v += a; if (v >= 1.f) { v = 1.f; stage = 2.f; } }
		else if (stage == 2.f) {
			if (lin) v -= d; else v -= v * dk;
			if (v <= 0.001f) { v = 0.f; stage = OPT(0) ? 1.f : 0.f; et = PULSE_SAMPLES; }
		}
		n->out[0][i] = v;
		n->out[1][i] = et > 0.f ? 1.f : 0.f;
		if (et > 0.f) et -= 1.f;
	}
	n->s[0] = v; n->s[1] = stage; n->s[3] = et;
}

/* ar — attack / release while a gate is held; curve=exp|lin (after Doepfer A-141 / A-142) */
static void run_ar(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0];
	int lin = OPT(0);
	float a = 1.f / (seg_seconds(in[1][0], 0) * SR);
	float r = 1.f / (seg_seconds(in[2][0], 0) * SR), rk = scq_min(r * 5.f, 1.f);
	LOOP {
		if (in[0][i] > 0.5f) v = scq_min(v + a, 1.f);
		else if (lin) v = scq_max(v - r, 0.f);
		else v -= v * rk;
		n->out[0][i] = v;
	}
	n->s[0] = v;
}

/* decay — a percussive envelope: jumps to 1 on a pulse and dies away; curve=exp|lin */
static void run_decay(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0];
	float d = 1.f / (seg_seconds(in[1][0], 0) * SR), dk = scq_min(d * 5.f, 1.f);
	int lin = OPT(0);
	LOOP {
		if (edge(in[0][i], &n->s[1])) v = 1.f;
		if (lin) v = scq_max(v - d, 0.f); else v -= v * dk;
		n->out[0][i] = v;
	}
	n->s[0] = v;
}

/* stepseq — eight-stage sequencer: s1..s8 are the steps (patch values or faders into them),
 * nature is the length; dir=forward|backward|pingpong|random (after Buchla 123; Doepfer A-155) */
static void run_stepseq(node *n, const float *const in[MAX_IN])
{
	int len = natural(n->nature, 1, 8), at = (int)n->s[0], dir = OPT(0);
	float pt = n->s[3], way = n->s[4] == 0.f ? 1.f : n->s[4];
	if (at >= len) at = 0;
	LOOP {
		if (edge(in[1][i], &n->s[2])) { at = 0; way = 1.f; pt = PULSE_SAMPLES; }
		if (edge(in[0][i], &n->s[1])) {
			if (dir == 0) at = (at + 1) % len;
			else if (dir == 1) at = (at + len - 1) % len;
			else if (dir == 2) {
				if (len > 1) { if (at + (int)way < 0 || at + (int)way >= len) way = -way; at += (int)way; }
			} else at = (int)(scq_rand01(&n->rng) * (float)len) % len;
			pt = PULSE_SAMPLES;
		}
		n->out[0][i] = in[2 + at][i];
		n->out[1][i] = pt > 0.f ? 1.f : 0.f;
		n->out[2][i] = len > 1 ? (float)at / (float)(len - 1) : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = (float)at; n->s[3] = pt; n->s[4] = way;
}

/* flipflop — toggles on each clock pulse; reset sends it low (after Doepfer / Serge logic) */
static void run_flipflop(node *n, const float *const in[MAX_IN])
{
	float q = n->s[0];
	LOOP {
		if (edge(in[0][i], &n->s[1])) q = 1.f - q;
		if (edge(in[1][i], &n->s[2])) q = 0.f;
		n->out[0][i] = q;
		n->out[1][i] = 1.f - q;
	}
	n->s[0] = q;
}

/* trigger — a gate's rising edge becomes a pulse; length=2 (ms) is a param
 * (after Doepfer A-162; Serge gate-to-trigger) */
static void run_trigger(node *n, const float *const in[MAX_IN])
{
	float pt = n->s[0], len = scq_max(n->opt[0], 0.05f) * 0.001f * SR;
	LOOP {
		if (edge(in[0][i], &n->s[1])) pt = len;
		n->out[0][i] = pt > 0.f ? 1.f : 0.f;
		if (pt > 0.f) pt -= 1.f;
	}
	n->s[0] = pt;
}
