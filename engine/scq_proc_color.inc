/*
 * Colour, dynamics, modulation, space and listening: the pedalboard, the tape machine, the
 * record from the archive, the shortwave set, the mixing desk's tools, the Leslie, and two ears
 * that turn what comes in into pulses and pitch (the Wingie's microphones, a field recording
 * under the highway). Included by scq_places.c after the other scq_*.inc.
 *
 * Every helper here is called pc_ (the other collections are included alongside). Controls are
 * read once per block unless they have to move at audio rate; sound coming in is clamped to
 * +-8 first, so whatever runs away upstream (or isn't a number) can't get into a filter here.
 */

#define PC_OPT(k) ((int)n->opt[k])

static inline float pc_in(float x) { return scq_clamp(x, -8.f, 8.f); }
static inline float pc_01(float x) { return scq_clamp(x, 0.f, 1.f); }
static inline float pc_pulse(float *t) { float v = *t > 0.f ? 1.f : 0.f; if (*t > 0.f) *t -= 1.f; return v; }
/* peak follower: attack and release coefficients */
static inline float pc_env(float *e, float x, float att, float rel)
{
	float a = scq_abs(x);
	*e += (a - *e) * (a > *e ? att : rel);
	return *e;
}
/* the states a filter leaves decaying towards zero are set to zero before they turn denormal */
static inline void pc_flush(float *s, int k)
{
	for (int i = 0; i < k; i++) if (scq_abs(s[i]) < 1e-15f) s[i] = 0.f;
}
/* a block-rate ramp: from the last block's value to this one's */
#define PC_RAMP(from, to) const float from##_d = ((to) - (from)) / (float)B

/* ------------------------------------------------------------------ colour */

/* the transistor of a fuzz: soft to +1, sooner to -0.6 (even harmonics) */
static inline float pc_asym(float u) { return u > 0.f ? scq_tanh(u) : 0.6f * scq_tanh(1.6f * u); }

/* fuzz — a germanium fuzz face in a puddle of light: two hot transistors and a bias. Bias 0 is
 * the sweet spot; below, the battery dies (the sound splutters and gates, velcro); above, it
 * fattens one side (after the Arbiter Fuzz Face, the Fuzz Factory's starve) */
static void run_fuzz(node *n, const float *const in[MAX_IN])
{
	/* s: 0 coupling cap, 1 env, 2 tone, 3 dc, 4 sputter value, 5 sputter timer */
	float hp = n->s[0], env = n->s[1], tone = n->s[2], dc = n->s[3], spv = n->s[4], spt = n->s[5];
	float drive = pc_01(in[1][0]);
	float bias = scq_clamp(in[2][0], -1.f, 1.f);
	float starve = scq_max(-bias, 0.f), fat = scq_max(bias, 0.f);
	float g = (2.f + 70.f * drive * drive) * scq_clamp(n->nature, 0.1f, 4.f);
	float off = 0.35f * fat - 0.45f * starve;
	float rest = pc_asym(off);
	float dz = 0.55f * starve * starve, dzn = 1.f / (1.f - dz);
	float th = 0.05f * starve, ith = 1.f / (th + 1e-4f);
	const float hpa = scq_coef_hz(90.f, SR), ta = scq_coef_hz(5200.f - 2500.f * drive, SR), da = scq_coef_hz(15.f, SR);
	const float ea = scq_coef_time(0.001f, SR), er = scq_coef_time(0.04f, SR);
	const float sph = 0.003f * SR;
	LOOP {
		float x = pc_in(in[0][i]);
		hp += hpa * (x - hp);
		x -= hp;
		float e = pc_env(&env, x, ea, er);
		float y = pc_asym(g * x + off) - rest;
		if (dz > 0.f) {                               /* the starved transistor doesn't conduct below dz */
			float a = scq_abs(y) - dz;
			y = a > 0.f ? (y > 0.f ? a : -a) * dzn : 0.f;
		}
		if (th > 0.f) {                               /* and a quiet input doesn't get through at all */
			if (--spt <= 0.f) { spv = scq_rand01(&n->rng); spt = sph * (0.3f + scq_rand01(&n->rng)); }
			float open = scq_clamp((e - th) * ith, 0.f, 1.f);
			y *= scq_clamp(open * 1.5f - 0.5f * spv, 0.f, 1.f);
		}
		tone += ta * (y - tone);
		dc += da * (tone - dc);
		n->out[0][i] = (tone - dc) * 0.8f;
	}
	n->s[0] = hp; n->s[1] = env; n->s[2] = tone; n->s[3] = dc; n->s[4] = spv; n->s[5] = spt;
	pc_flush(n->s, 4);
}

/* chloe — after the Chloe module: a vacuum tube driven into its grid, a multimode filter that
 * sings when pushed, and a low-pass gate struck by gate (a vactrol: quick to light, slow to go
 * dark). mode=low|band|high. With nothing in gate the gate stands open */
static void run_chloe(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 filter, 2 dc, 3 vactrol, 4 5 gate lowpass, 6 gate edge */
	float dc = n->s[2], v = n->s[3], l1 = n->s[4], l2 = n->s[5];
	float drive = pc_01(in[1][0]);
	float d = 1.f + 24.f * drive * drive;
	float tb = scq_tanh(d * 0.15f);
	float mk = 1.f / scq_tanh(scq_max(d * 0.7f, 0.3f));
	float fc = 30.f * scq_exp2(pc_01(in[2][0]) * 9.4f) * scq_clamp(n->nature, 0.1f, 8.f);
	float k = 2.f - 1.97f * pc_01(in[3][0]);
	svf_c c = svf_make(fc, k);
	int mode = PC_OPT(0), gated = (n->patched & 16u) != 0;
	const float rise = scq_coef_time(0.0006f, SR), fall = scq_coef_time(0.18f, SR), da = scq_coef_hz(12.f, SR);
	float bn = k < 1.f ? 0.5f + 0.5f * k : 1.f;   /* the band mode: its peak kept near the others' */
	LOOP {
		float x = pc_in(in[0][i]);
		float t = (scq_tanh(d * (x + 0.15f)) - tb) * mk;
		dc += da * (t - dc);
		t -= dc;
		float lp, bp = svf_run(&c, &n->s[0], &n->s[1], t, &lp);
		n->s[0] = 3.f * scq_softclip(n->s[0] * 0.3333f);   /* the integrator saturates: it sings, it doesn't run away */
		float y = mode == 0 ? lp : (mode == 1 ? bp * bn : t - k * bp - lp);
		if (gated) {
			float gv = in[4][i];
			int strike = edge(gv, &n->s[6]);
			if (strike || gv > 0.5f) v += (1.f - v) * rise * (strike ? 4.f : 1.f);
			else v -= v * fall * (0.3f + 1.4f * v);
			float a = 0.004f + 0.9f * v * v;
			l1 += a * (y - l1);
			l2 += a * (l1 - l2);
			y = l2 * (0.15f + 0.85f * v) * v;
		}
		n->out[0][i] = scq_tanh(y);
	}
	n->s[2] = dc; n->s[3] = v; n->s[4] = l1; n->s[5] = l2;
	pc_flush(n->s, 6);
}

/* tape — the cassette that lived in a car: wow and flutter, the head bump, a soft saturated
 * top, hiss, and now and then a dropout where the oxide wore off. age wears it, wow wobbles it;
 * nature is the speed (1 is 7.5 ips, 2 is 15: less wow, the bump higher) */
#define PC_TAPE_LEN 1024
static uint32_t mem_tape(float nature) { (void)nature; return PC_TAPE_LEN * sizeof(int16_t); }
static void run_tape(node *n, const float *const in[MAX_IN])
{
	/* s: 0 write, 1 wow phase, 2 flutter phase, 3 drift, 4 drift target, 5 6 bump, 7 8 top,
	 *    9 hiss lp, 10 dropout gain, 11 dropout target, 12 dropout timer, 13 delay at block end, 14 hysteresis */
	int16_t *buf = (int16_t *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float age = pc_01(in[1][0]), wow = pc_01(in[2][0]);
	float sp = scq_clamp(n->nature, 0.25f, 4.f), isp = 1.f / sp;
	float bt = (float)B * INV_SR;
	/* the slow things, once a block */
	float ph = n->s[1] + 0.55f * sp * bt, fp = n->s[2] + 6.3f * sp * bt;
	ph -= scq_floor(ph); fp -= scq_floor(fp);
	if (scq_rand01(&n->rng) < bt * 0.7f) n->s[4] = scq_noise(&n->rng);
	n->s[3] += (n->s[4] - n->s[3]) * scq_coef_time(0.6f, 1.f / bt);
	float wd = (wow * 0.0035f + age * 0.0008f) * SR * isp;          /* wow, in samples */
	float fd = (0.00004f + 0.00022f * age + 0.0001f * wow) * SR * isp; /* flutter */
	float d1 = 4.f + wd + fd + wd * (0.7f * scq_sin2pi(ph) + 0.3f * n->s[3]) + fd * scq_sin2pi(fp);
	float d0 = n->s[13] > 0.f ? n->s[13] : d1;
	PC_RAMP(d0, d1);
	svf_c bump = svf_make(70.f * sp, 1.3f);
	const float top = scq_coef_hz((17000.f - 12500.f * age) * scq_min(sp, 1.5f), SR);
	const float hl = scq_coef_hz(3000.f, SR), hiss = 0.0015f + 0.018f * age * age;
	const float drv = 1.f + 2.5f * age, dmk = 1.f / (0.65f * drv + 0.35f), hy = 0.12f * age;
	const float dropp = age * age * 0.4f * INV_SR, dsm = scq_coef_time(0.006f, SR);
	float g = n->s[10], gt = n->s[11], dt = n->s[12], prev = n->s[14];
	float dl = d0;
	LOOP {
		float x = pc_in(in[0][i]);
		float sx = scq_tanh(drv * x + hy * prev) * dmk;   /* the record head: soft, with a little memory */
		buf[w & (PC_TAPE_LEN - 1)] = to16(sx * 0.5f);
		dl += d0_d;
		float rp = (float)w - dl;
		float fl = scq_floor(rp), fr = rp - fl;
		uint32_t r0 = (uint32_t)(int32_t)fl;
		float a = FROM16(buf[r0 & (PC_TAPE_LEN - 1)]), b = FROM16(buf[(r0 + 1u) & (PC_TAPE_LEN - 1)]);
		float y = 2.f * (a + (b - a) * fr);
		w++;
		float bp = svf_run(&bump, &n->s[5], &n->s[6], y, 0);
		y += 0.45f * bp;                               /* the head bump */
		n->s[7] += top * (y - n->s[7]);
		n->s[8] += top * (n->s[7] - n->s[8]);
		y = n->s[8];
		prev = y;
		if (dt > 0.f) { dt -= 1.f; if (dt <= 0.f) gt = 1.f; }
		else if (scq_rand01(&n->rng) < dropp) { gt = 0.1f + 0.5f * scq_rand01(&n->rng); dt = (0.02f + 0.1f * scq_rand01(&n->rng)) * SR; }
		g += dsm * (gt - g);
		float nz = scq_noise(&n->rng);
		n->s[9] += hl * (nz - n->s[9]);
		n->out[0][i] = y * g + (nz - n->s[9]) * hiss;
	}
	if (gt <= 0.f) gt = 1.f;
	n->s[0] = (float)(w & (PC_TAPE_LEN - 1)); n->s[1] = ph; n->s[2] = fp; n->s[13] = d1;
	n->s[10] = g; n->s[11] = gt; n->s[12] = dt; n->s[14] = prev;
	pc_flush(n->s + 5, 5);
}

/* vinyl — a record from the archive, found in a basement: crackle and pops, a scratch that
 * comes round every turn, rumble, the warp's slow wow, a worn top end, groove noise. age is how
 * many times it was played; nature the speed (1 is 33, 1.35 is 45) */
#define PC_VINYL_LEN 1024
static uint32_t mem_vinyl(float nature) { (void)nature; return PC_VINYL_LEN * sizeof(int16_t); }
static void run_vinyl(node *n, const float *const in[MAX_IN])
{
	/* s: 0 write, 1 turn, 2 crackle, 3 pop lp1, 4 pop lp2, 5 6 rumble, 7 8 top, 9 groove hp, 10 groove lp,
	 *    11 where the scratch is, 12 delay at block end, 13 the scratch's own lp, 14 inner groove */
	int16_t *buf = (int16_t *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float age = pc_01(in[1][0]);
	float sp = scq_clamp(n->nature, 0.25f, 4.f);
	float bt = (float)B * INV_SR;
	if (n->s[11] == 0.f) n->s[11] = 0.05f + 0.9f * scq_rand01(&n->rng);
	float t0 = n->s[1], t1 = t0 + 0.5556f * sp * bt;
	int scratch = age > 0.25f && t0 < n->s[11] && t1 >= n->s[11];
	t1 -= scq_floor(t1);
	float wd = (0.0006f + 0.0016f * age) * SR / sp;
	float d1 = 3.f + wd * (1.f + scq_sin2pi(t1));
	float d0 = n->s[12] > 0.f ? n->s[12] : d1;
	PC_RAMP(d0, d1);
	const float crk = (4.f + 260.f * age * age) * INV_SR, popp = (0.15f + 2.5f * age) * INV_SR;
	const float cl = 0.12f + 0.5f * age;
	const float pa = scq_coef_hz(260.f, SR), ra = scq_coef_hz(28.f, SR);
	const float top = scq_coef_hz(16500.f - 12500.f * age, SR);
	const float gh = scq_coef_hz(2500.f, SR), gl = scq_coef_hz(7000.f, SR);
	const float rum = 0.06f + 0.25f * age, groove = (0.004f + 0.02f * age) * (1.f + 0.6f * scq_sin2pi(t1));
	const float inner = 0.25f * age;
	float c = n->s[2], dl = d0;
	LOOP {
		float x = pc_in(in[0][i]);
		buf[w & (PC_VINYL_LEN - 1)] = to16(x * 0.5f);
		dl += d0_d;
		float rp = (float)w - dl;
		float fl = scq_floor(rp), fr = rp - fl;
		uint32_t r0 = (uint32_t)(int32_t)fl;
		float a = FROM16(buf[r0 & (PC_VINYL_LEN - 1)]), b = FROM16(buf[(r0 + 1u) & (PC_VINYL_LEN - 1)]);
		float y = 2.f * (a + (b - a) * fr);
		w++;
		y += inner * y * scq_abs(y) * 0.5f;           /* the inner groove's even distortion */
		n->s[7] += top * (y - n->s[7]);
		n->s[8] += top * (n->s[7] - n->s[8]);
		y = n->s[8];
		/* crackle: sharp ticks, most small, a few loud */
		float u = scq_rand01(&n->rng), imp = 0.f;
		if (u < crk) { float r = scq_rand01(&n->rng); r *= r; r *= r; imp = (scq_rng(&n->rng) & 1u ? r : -r) * cl; }
		c = c * -0.55f + imp;
		/* pops and the scratch: thumps */
		float pimp = 0.f;
		if (scq_rand01(&n->rng) < popp) pimp = (0.3f + 0.7f * scq_rand01(&n->rng)) * (0.5f + age);
		if (scratch && i == 0) pimp += 0.6f + 0.6f * age;
		n->s[3] += pa * (pimp * 30.f - n->s[3]);
		n->s[4] += pa * (n->s[3] - n->s[4]);
		/* rumble and groove noise */
		float nz = scq_noise(&n->rng);
		n->s[5] += ra * (nz - n->s[5]);
		n->s[6] += ra * (n->s[5] - n->s[6]);
		n->s[9] += gh * (nz - n->s[9]);
		float gn = nz - n->s[9];
		n->s[10] += gl * (gn - n->s[10]);
		n->out[0][i] = y + c + n->s[4] + n->s[6] * rum + n->s[10] * groove * 4.f;
	}
	n->s[0] = (float)(w & (PC_VINYL_LEN - 1)); n->s[1] = t1; n->s[2] = c; n->s[12] = d1;
	pc_flush(n->s + 2, 9);
}

/* radio — shortwave at night: a narrow band, fading in and out (and a notch wandering through
 * it), whistles where carriers beat, static, an AGC that lifts the noise when the station sinks,
 * and a dial that drifts. tune 0.5 is on the station; drift is how much it wanders */
static void run_radio(node *n, const float *const in[MAX_IN])
{
	/* s: 0 hp, 1 2 band lowpass, 3 4 selective fade notch, 5 fade, 6 fade target, 7 notch pos,
	 *    8 notch target, 9 whistle phase, 10 drift, 11 drift target, 12 agc env, 13 static, 14 agc gain */
	float bt = (float)B * INV_SR;
	float dr = pc_01(in[2][0]);
	if (scq_rand01(&n->rng) < bt * (0.3f + dr)) n->s[11] = scq_noise(&n->rng) * dr * 0.12f;
	n->s[10] += (n->s[11] - n->s[10]) * scq_coef_time(1.5f, 1.f / bt);
	if (scq_rand01(&n->rng) < bt * 0.5f) n->s[6] = 0.08f + 0.92f * scq_rand01(&n->rng);
	if (n->s[6] <= 0.f) n->s[6] = 1.f;
	n->s[5] += (n->s[6] - n->s[5]) * scq_coef_time(0.5f, 1.f / bt);
	if (scq_rand01(&n->rng) < bt * 0.4f) n->s[8] = scq_rand01(&n->rng);
	n->s[7] += (n->s[8] - n->s[7]) * scq_coef_time(1.2f, 1.f / bt);
	float t = scq_clamp(in[1][0], -1.f, 2.f) - 0.5f + n->s[10];
	float m = scq_abs(t) * 24.f;                      /* how far off the station, in channel widths */
	float sel = 1.f / (1.f + m * m);
	float wh = 0.25f * 2.f * m / (1.f + m * m);
	float whf = (40.f + 3200.f * scq_min(m, 2.f)) * (1.f + 0.02f * scq_noise(&n->rng)) * INV_SR;
	float stat = 0.012f + 0.2f * (m * m / (1.f + m * m)) + 0.05f * (1.f - n->s[5]);
	float fade = n->s[5] * sel;
	const float sc = scq_clamp(n->nature, 0.25f, 4.f);
	const float hpa = scq_coef_hz(320.f * sc, SR);
	svf_c band = svf_make(2700.f * sc, 1.1f);
	svf_c notch = svf_make((400.f + 2200.f * n->s[7]) * sc, 0.9f);
	const float ea = scq_coef_time(0.01f, SR), er = scq_coef_time(0.4f, SR), ga = scq_coef_time(0.05f, SR);
	const float crk = (40.f + 900.f * stat) * INV_SR;
	float wp = n->s[9], env = n->s[12], st = n->s[13], g = n->s[14] > 0.f ? n->s[14] : 1.f;
	LOOP {
		float x = pc_in(in[0][i]);
		n->s[0] += hpa * (x - n->s[0]);
		x -= n->s[0];
		float lp;
		svf_run(&band, &n->s[1], &n->s[2], x, &lp);
		float nb = svf_run(&notch, &n->s[3], &n->s[4], lp, 0);
		float sig = (lp - 0.9f * nb * 0.9f) * fade;   /* a notch, not a full one: the selective fade */
		wp += whf; wp -= scq_floor(wp);
		float nz = scq_noise(&n->rng);
		st = st * 0.3f + nz * 0.7f;
		float s = st * stat;
		if (scq_rand01(&n->rng) < crk) s += scq_noise(&n->rng) * (0.2f + stat);
		float y = sig + s + wh * scq_sin2pi(wp) * n->s[5];
		pc_env(&env, y, ea, er);
		float gt = scq_clamp(0.35f / (env + 0.02f), 0.25f, 6.f);
		g += ga * (gt - g);
		n->out[0][i] = scq_tanh(y * g) * 0.8f;
	}
	n->s[9] = wp; n->s[12] = env; n->s[13] = st; n->s[14] = g;
	pc_flush(n->s, 5);
}

/* ------------------------------------------------------------------ the desk */

/* bandsplit — the sound system's crossover: low, mid and high out, the three summing back flat
 * (fourth-order, after Linkwitz-Riley). low and high are the crossover points, 20 Hz to 20 kHz */
static void run_bandsplit(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 split at the low point, 2 3 its lowpass, 4 5 its highpass, 6 7 split at the high point,
	 *    8 9 its lowpass, 10 11 its highpass, 12 13 the low band's allpass */
	float fl = 20.f * scq_exp2(pc_01(in[1][0]) * 9.966f);
	float fh = 20.f * scq_exp2(pc_01(in[2][0]) * 9.966f);
	if (fh < fl * 1.25f) fh = fl * 1.25f;
	const float k = 1.41421356f;
	svf_c a = svf_make(fl, k), b = svf_make(fh, k);
	float *s = n->s;
	LOOP {
		float x = pc_in(in[0][i]);
		float lp, bp = svf_run(&a, &s[0], &s[1], x, &lp);
		float hp = x - k * bp - lp;
		float lo, b2 = svf_run(&a, &s[2], &s[3], lp, &lo);
		float l3, b3 = svf_run(&a, &s[4], &s[5], hp, &l3);
		float hi = hp - k * b3 - l3;                  /* the rest, above the low point */
		float l4, b4 = svf_run(&b, &s[6], &s[7], hi, &l4);
		float h4 = hi - k * b4 - l4;
		float mid, b5 = svf_run(&b, &s[8], &s[9], l4, &mid);
		float l6, b6 = svf_run(&b, &s[10], &s[11], h4, &l6);
		float top = h4 - k * b6 - l6;
		float b7 = svf_run(&b, &s[12], &s[13], lo, 0);
		(void)b2; (void)b5;
		n->out[0][i] = lo - 2.f * k * b7;           /* the low band through the high point's allpass */
		n->out[1][i] = mid;
		n->out[2][i] = top;
	}
	pc_flush(n->s, 14);
}

/* the desk's gain law: 0.5 flat, 1 is +12 dB, 0 kills the band (a DJ isolator's) */
static inline float pc_band_gain(float x)
{
	x = pc_01(x);
	return x < 0.5f ? 4.f * x * x : scq_exp2((x - 0.5f) * 4.f);
}

/* eq — the mixer's three knobs: a low shelf, a sweepable mid and a high shelf. 0.5 is flat, up to
 * +12 dB, and down to nothing (kill, as on a sound-system mixer's isolator). freq sweeps the mid
 * 80 Hz to 10 kHz; nature moves the shelves (after the Allen & Heath and Formula Sound mixers) */
static void run_eq(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 low, 2 3 mid, 4 5 high, 6 7 8 the gains, smoothed by block */
	float sc = scq_clamp(n->nature, 0.25f, 4.f);
	n->s[6] += 0.3f * (pc_band_gain(in[1][0]) - n->s[6]);
	n->s[7] += 0.3f * (pc_band_gain(in[2][0]) - n->s[7]);
	n->s[8] += 0.3f * (pc_band_gain(in[4][0]) - n->s[8]);
	float gl = n->s[6] - 1.f, gm = n->s[7] - 1.f, gh = n->s[8] - 1.f;
	const float km = 0.9f;
	svf_c lo = svf_make(160.f * sc, 1.f), mi = svf_make(80.f * scq_exp2(pc_01(in[3][0]) * 7.f), km), hi = svf_make(5000.f * sc, 1.f);
	LOOP {
		float x = pc_in(in[0][i]);
		float l; svf_run(&lo, &n->s[0], &n->s[1], x, &l);
		float m = svf_run(&mi, &n->s[2], &n->s[3], x, 0);
		float hl, hb = svf_run(&hi, &n->s[4], &n->s[5], x, &hl);
		float h = x - hb - hl;
		n->out[0][i] = x + gl * l + gm * km * m + gh * h;
	}
	pc_flush(n->s, 6);
}

/* exciter — the air above it: the top is split off, made to ring with new harmonics and laid back
 * under the sound, brighter without getting louder (after the Aphex Aural Exciter). nature moves
 * where the top starts (1 is 2.5 kHz) */
static void run_exciter(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 the split, 2 after the harmonics */
	float amt = pc_01(in[1][0]);
	float f = 2500.f * scq_clamp(n->nature, 0.25f, 4.f);
	const float a = scq_coef_hz(f, SR), b = scq_coef_hz(f * 1.4f, SR);
	float mix = amt * 0.9f, drv = 2.f + 10.f * amt;
	LOOP {
		float x = pc_in(in[0][i]);
		n->s[0] += a * (x - n->s[0]);
		float h1 = x - n->s[0];
		n->s[1] += a * (h1 - n->s[1]);
		float h = (h1 - n->s[1]) * drv;
		float e = scq_tanh(h + 0.35f * h * scq_abs(h));   /* odd and even, the even from the square */
		n->s[2] += b * (e - n->s[2]);
		n->out[0][i] = x + mix * (e - n->s[2]) * 0.5f;
	}
	pc_flush(n->s, 3);
}

/* ------------------------------------------------------------------ dynamics */

/* limiter — the wall at the end of the night: a block of lookahead, so the gain is already down
 * when a peak arrives; nothing comes out above ceiling, ever. release 5 ms to 1 s; reduction is
 * how far it is pulling down (0 to 1) */
static uint32_t mem_limiter(float nature) { (void)nature; return SCQ_BLOCK * sizeof(float); }
static void run_limiter(node *n, const float *const in[MAX_IN])
{
	/* s: 0 gain at the end of the last block, 1 the held block's peak, 2 started */
	float *held = (float *)n->mem;
	float ceil = scq_clamp(in[1][0], 0.001f, 4.f);
	float rt = 0.005f * scq_exp2(pc_01(in[2][0]) * 7.6f);
	float rc = scq_coef_time(rt, SR / (float)B);
	float p = 0.f;
	LOOP { float x = scq_abs(pc_in(in[0][i])); if (x > p) p = x; }
	if (n->s[2] == 0.f) { n->s[0] = 1.f; n->s[2] = 1.f; }
	float g0 = n->s[0], pold = n->s[1];
	float pk = scq_max(p, pold);
	float tgt = pk > ceil ? ceil / pk : 1.f;
	float g1 = tgt < g0 ? tgt : g0 + (tgt - g0) * rc;
	PC_RAMP(g0, g1);
	float g = g0;
	LOOP {
		g += g0_d;
		float y = held[i] * g;
		n->out[0][i] = scq_clamp(y, -ceil, ceil);
		n->out[1][i] = 1.f - g;
		held[i] = pc_in(in[0][i]);
	}
	n->s[0] = g1; n->s[1] = p;
}

/* gate — the door that shuts behind the sound: opens above threshold, closes at half of it
 * (hysteresis), holds 50 ms, then falls. env is how open it is, open pulses each time it opens.
 * nature stretches the hold and the release */
static void run_gate(node *n, const float *const in[MAX_IN])
{
	/* s: 0 env, 1 open, 2 hold, 3 gain, 4 pulse */
	float env = n->s[0], open = n->s[1], hold = n->s[2], g = n->s[3], pt = n->s[4];
	float th = pc_01(in[1][0]);
	float sc = scq_clamp(n->nature, 0.1f, 10.f);
	const float ea = scq_coef_time(0.0002f, SR), er = scq_coef_time(0.02f, SR);
	const float ga = scq_coef_time(0.0005f, SR), gr = scq_coef_time(0.08f * sc, SR);
	const float hs = 0.05f * sc * SR;
	LOOP {
		float x = pc_in(in[0][i]);
		pc_env(&env, x, ea, er);
		if (env > th) { if (open == 0.f) pt = PULSE_SAMPLES; open = 1.f; hold = hs; }
		else if (open != 0.f && env < th * 0.5f) { if (hold > 0.f) hold -= 1.f; else open = 0.f; }
		g += (open - g) * (open > g ? ga : gr);
		n->out[0][i] = x * g;
		n->out[1][i] = g;
		n->out[2][i] = pc_pulse(&pt);
	}
	n->s[0] = env; n->s[1] = open; n->s[2] = hold; n->s[3] = g; n->s[4] = pt;
	pc_flush(n->s, 1);
	if (g < 1e-6f) n->s[3] = 0.f;
}

/* transient — the hit and the ring: attack above 0.5 sharpens every strike, below softens it;
 * sustain above 0.5 lets the tails bloom, below dries them, whatever the level (after the SPL
 * Transient Designer: the difference of two envelopes) */
static void run_transient(node *n, const float *const in[MAX_IN])
{
	/* s: 0 fast, 1 slow attack, 2 slow release, 3 gain */
	float e1 = n->s[0], e2 = n->s[1], e3 = n->s[2], g = n->s[3] > 0.f ? n->s[3] : 1.f;
	float at = (pc_01(in[1][0]) - 0.5f) * 4.f, su = (pc_01(in[2][0]) - 0.5f) * 4.f;
	const float fa = scq_coef_time(0.0004f, SR), fr = scq_coef_time(0.04f, SR);
	const float sa = scq_coef_time(0.02f, SR), lr = scq_coef_time(0.4f, SR), gs = scq_coef_time(0.0005f, SR);
	const float eps = 1e-4f;
	LOOP {
		float x = pc_in(in[0][i]);
		pc_env(&e1, x, fa, fr);
		pc_env(&e2, x, sa, fr);
		pc_env(&e3, x, fa, lr);
		float l1 = scq_log2(e1 + eps), l2 = scq_log2(e2 + eps), l3 = scq_log2(e3 + eps);
		float ex = scq_clamp(at * (l1 - l2) + su * (l3 - l1), -3.f, 3.f);
		g += gs * (scq_exp2(ex) - g);
		n->out[0][i] = x * g;
	}
	n->s[0] = e1; n->s[1] = e2; n->s[2] = e3; n->s[3] = g;
	pc_flush(n->s, 3);
}

/* ------------------------------------------------------------------ space and movement */

/* widener — mid and side: width 0 is mono, 0.5 as it was, 1 the sides doubled; the bass stays in
 * the middle where a sound system wants it. With only in patched it makes its own stereo from
 * one sound, mono-compatible (after Lauridsen: the sides are a delayed copy, opposite in each
 * ear). nature is that delay (1 is 13 ms) */
#define PC_WIDE_LEN 1024
static uint32_t mem_widener(float nature) { (void)nature; return PC_WIDE_LEN * sizeof(int16_t); }
static void run_widener(node *n, const float *const in[MAX_IN])
{
	/* s: 0 write, 1 side lowpass (the bass), 2 the delayed copy's highpass */
	int16_t *buf = (int16_t *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float sw = 2.f * pc_01(in[2][0]), swl = scq_min(sw, 1.f);
	int stereo = (n->patched & 2u) != 0;
	uint32_t D = (uint32_t)scq_clamp(0.013f * scq_clamp(n->nature, 0.1f, 1.6f) * SR, 1.f, (float)(PC_WIDE_LEN - 1));
	const float la = scq_coef_hz(150.f, SR), ha = scq_coef_hz(300.f, SR);
	LOOP {
		float l = pc_in(in[0][i]), r = stereo ? pc_in(in[1][i]) : l;
		float m = 0.5f * (l + r), sd = 0.5f * (l - r);
		if (!stereo) {
			buf[w & (PC_WIDE_LEN - 1)] = to16(m * 0.25f);
			float dl = 4.f * FROM16(buf[(w - D) & (PC_WIDE_LEN - 1)]);
			n->s[2] += ha * (dl - n->s[2]);
			sd = 0.6f * (dl - n->s[2]);
			w++;
		}
		n->s[1] += la * (sd - n->s[1]);
		float s2 = n->s[1] * swl + (sd - n->s[1]) * sw;
		n->out[0][i] = m + s2;
		n->out[1][i] = m - s2;
	}
	n->s[0] = (float)(w & (PC_WIDE_LEN - 1));
	pc_flush(n->s + 1, 2);
}

/* equal-power pan gains for p in -1..1 */
static inline void pc_pan(float p, float *gl, float *gr)
{
	float q = (scq_clamp(p, -1.f, 1.f) + 1.f) * 0.125f;
	*gl = scq_sin2pi(q + 0.25f);
	*gr = scq_sin2pi(q);
}

/* autopan — the sound walking between the stacks. mode=lfo|random|follow: a sine, a wander to a
 * new place at each tick, or the input's own loudness (quiet left, loud right). rate 0.05 to 25 Hz */
static void run_autopan(node *n, const float *const in[MAX_IN])
{
	/* s: 0 phase, 1 2 the gains last block, 3 random target, 4 random now, 5 env, 6 started */
	float bt = (float)B * INV_SR;
	float hz = 0.05f * scq_exp2(pc_01(in[1][0]) * 9.f);
	float depth = pc_01(in[2][0]);
	float ph = n->s[0] + hz * bt;
	int wrap = ph >= 1.f;
	ph -= scq_floor(ph);
	float p;
	switch (PC_OPT(0)) {
	case 1:
		if (wrap || n->s[6] == 0.f) n->s[3] = scq_noise(&n->rng);
		n->s[4] += (n->s[3] - n->s[4]) * scq_clamp(hz * bt * 4.f, 0.f, 1.f);
		p = n->s[4] * depth;
		break;
	case 2: {
		float pk = 0.f;
		LOOP { float a = scq_abs(pc_in(in[0][i])); if (a > pk) pk = a; }
		n->s[5] += (pk - n->s[5]) * (pk > n->s[5] ? 0.3f : scq_clamp(hz * bt * 2.f, 0.001f, 0.3f));
		p = depth * (2.f * scq_clamp(n->s[5] * 2.f, 0.f, 1.f) - 1.f);
		break;
	}
	default: p = depth * scq_sin2pi(ph);
	}
	float gl1, gr1;
	pc_pan(p, &gl1, &gr1);
	float gl0 = n->s[6] != 0.f ? n->s[1] : gl1, gr0 = n->s[6] != 0.f ? n->s[2] : gr1;
	PC_RAMP(gl0, gl1);
	PC_RAMP(gr0, gr1);
	float gl = gl0, gr = gr0;
	LOOP {
		float x = pc_in(in[0][i]);
		gl += gl0_d; gr += gr0_d;
		n->out[0][i] = x * gl;
		n->out[1][i] = x * gr;
	}
	n->s[0] = ph; n->s[1] = gl1; n->s[2] = gr1; n->s[6] = 1.f;
}

/* the tremolo's wave: sine, rounding off into a square as shape goes to 1 */
static inline float pc_trem_wave(float ph, float sh)
{
	return scq_clamp(scq_sin2pi(ph) * (1.f + 30.f * sh * sh), -1.f, 1.f);
}

/* tremolo — the amp in the corner of the blues dance, its bulb blinking: type=amp|harmonic
 * (harmonic: the lows and highs throb against each other, after the brown Fender amps), shape=0
 * sine to 1 square, stereo=off|on (the right throbs opposite). rate 0.25 to 30 Hz */
static void run_tremolo(node *n, const float *const in[MAX_IN])
{
	/* s: 0 phase, 1 2 the waves last block, 3 harmonic split, 4 started */
	float hz = 0.25f * scq_exp2(pc_01(in[1][0]) * 7.f);
	float depth = pc_01(in[2][0]);
	int harm = PC_OPT(0) == 1, st = PC_OPT(2) == 1;
	float sh = pc_01(n->opt[1]);
	float ph = n->s[0] + hz * (float)B * INV_SR;
	ph -= scq_floor(ph);
	float m1 = pc_trem_wave(ph, sh), r1 = st ? pc_trem_wave(ph + 0.5f, sh) : m1;
	float m0 = n->s[4] != 0.f ? n->s[1] : m1, r0 = n->s[4] != 0.f ? n->s[2] : r1;
	PC_RAMP(m0, m1);
	PC_RAMP(r0, r1);
	float m = m0, r = r0, hd = 0.5f * depth;
	const float xa = scq_coef_hz(700.f, SR);
	LOOP {
		float x = pc_in(in[0][i]);
		m += m0_d; r += r0_d;
		if (harm) {
			n->s[3] += xa * (x - n->s[3]);
			float lo = n->s[3], hi = x - lo;
			n->out[0][i] = lo * (1.f - hd * (1.f - m)) + hi * (1.f - hd * (1.f + m));
			n->out[1][i] = lo * (1.f - hd * (1.f - r)) + hi * (1.f - hd * (1.f + r));
		} else {
			n->out[0][i] = x * (1.f - hd * (1.f - m));
			n->out[1][i] = x * (1.f - hd * (1.f - r));
		}
	}
	n->s[0] = ph; n->s[1] = m1; n->s[2] = r1; n->s[4] = 1.f;
	pc_flush(n->s + 3, 1);
}

/* rotary — the Leslie cabinet behind the organ: a horn and a drum turning at their own speeds,
 * the horn's doppler and both their throb, two microphones. fast above 0.5 spins them up; they
 * take their time (the drum is heavy) to get there and to slow down (after the Leslie 122) */
#define PC_ROT_LEN 256
static uint32_t mem_rotary(float nature) { (void)nature; return PC_ROT_LEN * sizeof(float); }
static void run_rotary(node *n, const float *const in[MAX_IN])
{
	/* s: 0 write, 1 horn phase, 2 drum phase, 3 horn speed, 4 drum speed, 5 6 crossover */
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float bt = (float)B * INV_SR;
	int fast = in[1][0] > 0.5f;
	float sc = scq_clamp(n->nature, 0.25f, 4.f);
	if (n->s[3] == 0.f) { n->s[3] = 0.8f * sc; n->s[4] = 0.66f * sc; }
	float ht = (fast ? 6.8f : 0.8f) * sc, dt = (fast ? 5.9f : 0.66f) * sc;
	n->s[3] += (ht - n->s[3]) * scq_coef_time(ht > n->s[3] ? 0.7f : 1.0f, 1.f / bt);
	n->s[4] += (dt - n->s[4]) * scq_coef_time(dt > n->s[4] ? 3.5f : 5.f, 1.f / bt);
	float hph = n->s[1], dph = n->s[2];
	float hi = n->s[3] * INV_SR, di = n->s[4] * INV_SR;
	/* each turning as a phasor, rotated a sample at a time and set exact again every block */
	float hc = scq_sin2pi(hph + 0.25f), hs = scq_sin2pi(hph);
	float dc = scq_sin2pi(dph + 0.25f), ds = scq_sin2pi(dph);
	float hrc = scq_sin2pi(hi + 0.25f), hrs = scq_sin2pi(hi);
	float drc = scq_sin2pi(di + 0.25f), drs = scq_sin2pi(di);
	hph += hi * (float)B; hph -= scq_floor(hph);
	dph += di * (float)B; dph -= scq_floor(dph);
	const float xa = scq_coef_hz(800.f, SR);
	const float dm = 0.00045f * SR, d0 = dm + 3.f;
	LOOP {
		float x = scq_tanh(1.2f * pc_in(in[0][i])) * 0.85f;   /* the 122's valve amp, gently */
		n->s[5] += xa * (x - n->s[5]);
		n->s[6] += xa * (n->s[5] - n->s[6]);
		float lo = n->s[6], hiq = x - lo;
		buf[w & (PC_ROT_LEN - 1)] = hiq;
		/* left mic: the horn at angle h; right mic a quarter turn round */
		float rl = (float)w - (d0 + dm * hs), rr = (float)w - (d0 + dm * hc);
		float fl = scq_floor(rl), fr = rl - fl;
		uint32_t a = (uint32_t)(int32_t)fl;
		float hl = buf[a & (PC_ROT_LEN - 1)] + (buf[(a + 1u) & (PC_ROT_LEN - 1)] - buf[a & (PC_ROT_LEN - 1)]) * fr;
		fl = scq_floor(rr); fr = rr - fl;
		a = (uint32_t)(int32_t)fl;
		float hr = buf[a & (PC_ROT_LEN - 1)] + (buf[(a + 1u) & (PC_ROT_LEN - 1)] - buf[a & (PC_ROT_LEN - 1)]) * fr;
		w++;
		float yl = hl * (0.62f + 0.38f * hc) + lo * (0.8f + 0.2f * dc);
		float yr = hr * (0.62f - 0.38f * hs) + lo * (0.8f - 0.2f * ds);
		n->out[0][i] = yl;
		n->out[1][i] = yr;
		float t = hc * hrc - hs * hrs; hs = hs * hrc + hc * hrs; hc = t;
		t = dc * drc - ds * drs; ds = ds * drc + dc * drs; dc = t;
	}
	n->s[0] = (float)(w & (PC_ROT_LEN - 1)); n->s[1] = hph; n->s[2] = dph;
	pc_flush(n->s + 5, 2);
}

/* autowah — the envelope filter: play harder and it opens (dir=up) or closes (dir=down);
 * mode=band|low|high. sensitivity is how far a touch moves it, resonance how it quacks (after
 * the Musitronics Mu-Tron III). nature is where it rests (1 is 220 Hz) */
static void run_autowah(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 filter, 2 env */
	float env = n->s[2];
	float sens = pc_01(in[1][0]);
	float k = 2.f - 1.9f * pc_01(in[2][0]);
	float kk = 0.5f + 20.f * sens * sens, e = env * kk / (1.f + env * kk);   /* loudness, gently: 0 to 1 */
	float o = PC_OPT(1) == 1 ? 3.6f - e * 3.6f : e * 3.6f;
	svf_c c = svf_make(220.f * scq_clamp(n->nature, 0.1f, 8.f) * scq_exp2(o), k);
	int mode = PC_OPT(0);
	const float ea = scq_coef_time(0.003f, SR), er = scq_coef_time(0.08f, SR);
	float bn = 1.4f * k;
	LOOP {
		float x = pc_in(in[0][i]);
		pc_env(&env, x, ea, er);
		float lp, bp = svf_run(&c, &n->s[0], &n->s[1], x, &lp);
		float y = mode == 0 ? bp * bn : (mode == 1 ? lp : x - k * bp - lp);
		n->out[0][i] = scq_tanh(y);
	}
	n->s[2] = env;
	pc_flush(n->s, 3);
}

/* the slicer's patterns, sixteen steps */
static const char *const PC_SLICES[] = {
	"xx.xx.xxx.xx.x.x",   /* trance */
	"x.x.x.x.x.x.x.x.",   /* eighths */
	"..x...x...x...x.",   /* offbeat: the skank */
	"........x.......",   /* onedrop: only the third beat */
	"x.xxx.xxx.xxx.xx",   /* gallop */
	"x..x..x.x..x..x.",   /* tresillo, 3-3-2 */
	"x..x..x..x..x...",   /* euclid5 */
	"xxxxxxxx.x.x.xxx",   /* stutter */
};

/* slicer — the trance gate, the dub mixer's mute button played in time: each clock pulse is a
 * step, pattern=trance|eighths|offbeat|onedrop|gallop|tresillo|euclid5|stutter|random (random:
 * a new bar each time round), len= how much of a step it stays open, smooth edges. depth is how
 * far it closes. With nothing in clock it runs at nature x 8 steps a second */
static void run_slicer(node *n, const float *const in[MAX_IN])
{
	/* s: 0 step, 1 clock edge, 2 samples since the last pulse, 3 step length, 4 gain, 5 own clock,
	 *    6 the random bar, 7 into the step */
	int step = (int)n->s[0];
	float cnt = n->s[2], per = n->s[3] > 0.f ? n->s[3] : 0.125f * SR, g = n->s[4], own = n->s[5];
	float pos = n->s[7];
	uint32_t rb = (uint32_t)n->s[6];
	int pat = PC_OPT(0);
	float len = scq_clamp(n->opt[1], 0.02f, 1.f);
	float depth = pc_01(in[2][0]);
	int clocked = (n->patched & 2u) != 0;
	float oi = 8.f * scq_clamp(n->nature, 0.05f, 16.f) * INV_SR;
	const float sm = scq_coef_time(0.0015f, SR);
	if (pat >= 8 && rb == 0u) rb = (scq_rng(&n->rng) & 0xffffu) | 1u;
	LOOP {
		int tick;
		if (clocked) tick = edge(in[1][i], &n->s[1]);
		else { own += oi; tick = own >= 1.f; if (tick) own -= 1.f; }
		cnt += 1.f;
		if (tick) {
			if (clocked) per = scq_clamp(cnt, 0.002f * SR, 4.f * SR);
			else per = 1.f / oi;
			cnt = 0.f; pos = 0.f;
			step = (step + 1) & 15;
			if (step == 0 && pat >= 8) rb = (scq_rng(&n->rng) & 0xffffu) | 1u;
		}
		int on = pat >= 8 ? (int)((rb >> step) & 1u) : PC_SLICES[pat][step] == 'x';
		float tgt = (on && pos < len * per) ? 1.f : 1.f - depth;
		pos += 1.f;
		g += sm * (tgt - g);
		n->out[0][i] = pc_in(in[0][i]) * g;
	}
	n->s[0] = (float)step; n->s[2] = scq_min(cnt, 8.f * SR); n->s[3] = per; n->s[4] = g; n->s[5] = own;
	n->s[6] = (float)rb; n->s[7] = scq_min(pos, 8.f * SR);
}

/* ------------------------------------------------------------------ listening */

/* onset — an ear for attacks, like the Wingie's threshold sequencer: each strike in what comes in
 * becomes a pulse. Three bands, each one's quick envelope against its slow one (spectral flux,
 * lite), over a threshold that rises with how busy the sound has been. strength is how hard the
 * last one was (0 to 1, held), env the loudness. sensitivity 1 hears a feather */
static void run_onset(node *n, const float *const in[MAX_IN])
{
	/* s: 0 1 band splits, 2-4 quick envelopes, 5-7 slow, 8 spent (0: armed), 9 refractory, 10 pulse,
	 *    11 strength, 12 env, 13 decimation, 14 busy (mean of the flux), 15 last flux */
	float *s = n->s;
	float sens = pc_01(in[1][0]);
	float base = 1.8f - 1.5f * sens;
	float floor_ = 0.0004f + 0.03f * (1.f - sens) * (1.f - sens);
	const float la = scq_coef_hz(250.f, SR), ha = scq_coef_hz(2500.f, SR);
	const float fa = scq_coef_time(0.0005f, SR), fr = scq_coef_time(0.012f, SR);
	const float sl = scq_coef_time(0.06f, SR * 0.25f), mb = scq_coef_time(0.6f, SR * 0.25f);
	const float ea = scq_coef_time(0.001f, SR), er = scq_coef_time(0.1f, SR);
	const float refr = 0.045f * SR;
	float pt = s[10];
	LOOP {
		float x = pc_in(in[0][i]);
		s[0] += la * (x - s[0]);
		s[1] += ha * (x - s[1]);
		float lo = s[0], mid = s[1] - s[0], hi = x - s[1];
		pc_env(&s[2], lo, fa, fr);
		pc_env(&s[3], mid, fa, fr);
		pc_env(&s[4], hi, fa, fr);
		pc_env(&s[12], x, ea, er);
		if (s[9] > 0.f) s[9] -= 1.f;
		s[13] += 1.f;
		if (s[13] >= 4.f) {                            /* the flux, at a quarter of the rate */
			s[13] = 0.f;
			float d = 0.f;
			for (int b = 0; b < 3; b++) {
				s[5 + b] += sl * (s[2 + b] - s[5 + b]);
				float r = scq_log2(s[2 + b] + floor_) - scq_log2(s[5 + b] + floor_);
				if (r > 0.f) d += r * (b == 0 ? 0.8f : (b == 1 ? 1.f : 1.2f));
			}
			s[14] += mb * (d - s[14]);
			float thr = base + 1.5f * s[14];
			if (d > thr && s[8] == 0.f && s[9] <= 0.f) {
				pt = PULSE_SAMPLES; s[8] = 1.f; s[9] = refr;
				s[11] = pc_01(0.25f + (d - thr) * 0.25f);
			} else if (pt > 0.f && s[15] < d) s[11] = scq_max(s[11], pc_01(0.25f + (d - thr) * 0.25f));
			if (d < thr * 0.5f) s[8] = 0.f;
			s[15] = d;
		}
		n->out[0][i] = pc_pulse(&pt);
		n->out[1][i] = s[11];
		n->out[2][i] = pc_01(s[12]);
	}
	s[10] = pt;
	pc_flush(s, 8);
	pc_flush(s + 12, 1);
	if (s[15] < 1e-6f) s[15] = 0.f;
	if (s[14] < 1e-6f) s[14] = 0.f;
}

/* pitchtrack — an ear for pitch: one voice, a whistle, a string, the Wingie's resonance in the
 * room, followed (YIN, lite: the input slowed to about 8 kHz, the search spread over many blocks,
 * about 40 times a second). pitch is in octaves from nature x 60 Hz, so it plays any pitch inlet;
 * it holds the last note when nothing is sung. confidence 0 to 1, gate while it hears a note.
 * 60 Hz to 1.5 kHz */
/* log2 to a few millionths of an octave (scq_log2 is cheap but a few cents out, and one high):
 * the exponent, then the series for ln((1 + t) / (1 - t)) on the mantissa */
static float pc_log2(float x)
{
	union { float f; int32_t i; } u;
	if (!(x > 1e-30f)) return -100.f;
	u.f = x;
	float e = (float)(((u.i >> 23) & 255) - 127);
	u.i = (u.i & ~(255 << 23)) | (127 << 23);
	float m = u.f, t = (m - 1.f) / (m + 1.f), t2 = t * t;
	return e + 2.885390082f * t * (1.f + t2 * (0.3333333f + t2 * (0.2f + t2 * (0.1428571f + t2 * 0.1111111f))));
}

#define PT_W    128
#define PT_TMAX 136
#define PT_RING 512
static uint32_t mem_pitchtrack(float nature)
{
	(void)nature;
	return (PT_W + PT_TMAX + 2 + 2 * (PT_TMAX + 2)) * sizeof(float) + PT_RING * sizeof(int16_t);
}
static void run_pitchtrack(node *n, const float *const in[MAX_IN])
{
	/* s: 0 ring write, 1 decimation count, 2 3 the anti-alias filter, 4 stage (0 wait, else the
	 *    next lag), 5 since the last look, 6 pitch now, 7 pitch target, 8 confidence, 9 gate,
	 *    10 running sum of d, 11 loudness of the window, 12 started */
	float *snap = (float *)n->mem;
	float *d = snap + PT_W + PT_TMAX + 2;           /* the difference at each lag */
	float *dn = d + PT_TMAX + 2;                     /* and divided by its running mean */
	int16_t *ring = (int16_t *)(dn + PT_TMAX + 2);
	float *s = n->s;
	int D = (int)(SR / 8000.f + 0.5f);
	if (D < 1) D = 1;
	float fa = SR / (float)D;
	int tmax = (int)(fa / 60.f);
	if (tmax > PT_TMAX) tmax = PT_TMAX;
	int tmin = (int)(fa / 1500.f);
	if (tmin < 2) tmin = 2;
	uint32_t w = (uint32_t)s[0];
	svf_c aa = svf_make(scq_min(1800.f, fa * 0.25f), 1.2f);
	float base = 60.f * scq_clamp(n->nature, 0.01f, 64.f);
	if (s[12] == 0.f) { s[12] = 1.f; s[6] = s[7] = 0.f; }
	/* slowed and written */
	int dc = (int)s[1];
	LOOP {
		float lp;
		svf_run(&aa, &s[2], &s[3], pc_in(in[0][i]), &lp);
		if (++dc >= D) { dc = 0; ring[w & (PT_RING - 1)] = to16(lp); w++; }
	}
	s[1] = (float)dc;
	s[5] += (float)B;
	int stage = (int)s[4];
	if (stage == 0) {
		if (s[5] >= 0.025f * SR) {                    /* a look: the last window and its lags, held still */
			float e = 0.f;
			int L = PT_W + tmax + 1;
			for (int j = 0; j < L; j++) {
				float v = FROM16(ring[(w - (uint32_t)L + (uint32_t)j) & (PT_RING - 1)]);
				snap[j] = v;
				if (j < PT_W) e += v * v;
			}
			s[11] = e * (1.f / (float)PT_W);
			s[5] = 0.f; s[10] = 0.f;
			d[0] = 1.f;
			stage = 1;
		}
	} else {
		int lpb = (tmax + 31) / 32;                    /* lags a block: the whole search in about 32 blocks */
		if (lpb < 2) lpb = 2;
		for (int k = 0; k < lpb && stage <= tmax; k++, stage++) {
			float sum = 0.f;
			for (int j = 0; j < PT_W; j++) { float q = snap[j] - snap[j + stage]; sum += q * q; }
			s[10] += sum;
			d[stage] = sum;
			dn[stage] = s[10] > 1e-12f ? sum * (float)stage / s[10] : 1.f;   /* cumulative mean normalised */
		}
		if (stage > tmax) {                            /* the choice */
			int best = -1;
			for (int t = tmin; t < tmax; t++)
				if (dn[t] < 0.15f) { while (t + 1 < tmax && dn[t + 1] < dn[t]) t++; best = t; break; }
			if (best < 0) {
				best = tmin;
				for (int t = tmin; t < tmax; t++) if (dn[t] < dn[best]) best = t;
			}
			float tau = (float)best;
			if (best > 1 && best < tmax) {
				float a = d[best - 1], b = d[best], c = d[best + 1];
				float den = a - 2.f * b + c;
				if (den > 1e-9f) tau += scq_clamp(0.5f * (a - c) / den, -0.5f, 0.5f);
			}
			float conf = pc_01(1.f - dn[best]);
			int voiced = conf > 0.8f && s[11] > 2e-5f;
			s[8] = voiced ? conf : conf * 0.5f;
			s[9] = voiced ? 1.f : 0.f;
			if (voiced) s[7] = pc_log2(fa / (tau * base));
			stage = 0;
		}
	}
	s[4] = (float)stage;
	s[0] = (float)(w & (PT_RING - 1));
	float p = s[6];
	const float pg = scq_coef_time(0.008f, SR);
	LOOP {
		p += pg * (s[7] - p);
		n->out[0][i] = p;
		n->out[1][i] = s[8];
		n->out[2][i] = s[9];
	}
	s[6] = p;
	pc_flush(s + 2, 2);
}

/* ------------------------------------------------------------------ the rough end */

/* decimate — the sample rate dropped through the floor, and the clock that drops it is shaky:
 * rate 100 Hz to 48 kHz, jitter wobbles each sample's length; hold=step|linear|zero (zero: back
 * to nothing between samples, all mirrors and bells) (after the Disting's sample-rate reducer) */
static void run_decimate(node *n, const float *const in[MAX_IN])
{
	/* s: 0 phase, 1 held, 2 held before, 3 this sample's stretch */
	float ph = n->s[0], h = n->s[1], hp = n->s[2], jf = n->s[3] > 0.f ? n->s[3] : 1.f;
	float jit = pc_01(in[2][0]) * 0.9f;
	int mode = PC_OPT(0), moving = (n->patched & 2u) != 0;
	float inc = 100.f * scq_exp2(pc_01(in[1][0]) * 8.9f) * INV_SR;
	LOOP {
		if (moving) inc = 100.f * scq_exp2(pc_01(in[1][i]) * 8.9f) * INV_SR;
		ph += scq_min(inc * jf, 1.f);
		if (ph >= 1.f) {
			ph -= scq_floor(ph);
			hp = h; h = pc_in(in[0][i]);
			jf = 1.f / (1.f + jit * scq_noise(&n->rng));
		}
		n->out[0][i] = mode == 1 ? hp + (h - hp) * ph : (mode == 2 ? (ph < 0.5f ? 1.4f * h : 0.f) : h);
	}
	n->s[0] = ph; n->s[1] = h; n->s[2] = hp; n->s[3] = jf;
}

/* one diode: nothing below 0.2, a soft knee to 0.4, then a straight line (after Julian Parker's
 * diode ring model) */
static inline float pc_diode(float v)
{
	if (v <= 0.2f) return 0.f;
	if (v <= 0.4f) { v -= 0.2f; return 2.5f * v * v; }
	return v - 0.3f;
}

/* diode — the ring of four diodes on the old bench: in rides carrier, with the dead zone and the
 * soft knees real diodes have; leak unbalances the ring, so the carrier and a little of in bleed
 * through. With nothing in carrier it has its own sine at nature x 60 Hz (after the Bode ring
 * modulator, Parker's model) */
static void run_diode(node *n, const float *const in[MAX_IN])
{
	/* s: 0 carrier phase, 1 dc */
	float ph = n->s[0], dc = n->s[1];
	float leak = pc_01(in[2][0]);
	float as = 1.f + 0.03f + 0.35f * leak, bl = 0.25f * leak;
	int own = !(n->patched & 2u);
	float inc = scq_clamp(60.f * scq_clamp(n->nature, 0.01f, 100.f) * INV_SR, 0.f, 0.45f);
	const float da = scq_coef_hz(10.f, SR);
	LOOP {
		float m = pc_in(in[0][i]), c;
		if (own) { c = scq_sin2pi(ph); ph += inc; ph -= scq_floor(ph); }
		else c = pc_in(in[1][i]);
		c *= 2.f;
		float y = pc_diode(c + m) + pc_diode(-c - m) * as - pc_diode(c - m) * as - pc_diode(-c + m);
		y = 0.5f * y + bl * c * 0.5f + bl * 0.3f * m;
		dc += da * (y - dc);
		n->out[0][i] = y - dc;
	}
	n->s[0] = ph; n->s[1] = dc;
	pc_flush(n->s + 1, 1);
}

/* drive — the pedalboard on the floor: type=od|dist|rat|crunch. od keeps its lows clean and
 * clips the mids softly (after the Ibanez Tube Screamer); dist clips hard into a scooped tone
 * stack (after the Boss DS-1 / Big Muff); rat is the op-amp pushed until its own bandwidth gives
 * out, diodes to ground (after the ProCo RAT); crunch is two valve stages (an amp on the edge).
 * tone 0 dark, 1 bright */
static void run_drive(node *n, const float *const in[MAX_IN])
{
	/* s: 0 pre, 1 2 tone, 3 the op-amp's own lowpass, 4 dc, 5 interstage */
	float *s = n->s;
	float drive = pc_01(in[1][0]), tone = pc_01(in[2][0]);
	const float da = scq_coef_hz(12.f, SR);
	switch (PC_OPT(0)) {
	default: {                                         /* od */
		float g = 1.f + 80.f * drive * drive;
		const float pa = scq_coef_hz(720.f, SR), ta = scq_coef_hz(700.f * scq_exp2(tone * 3.f), SR);
		LOOP {
			float x = pc_in(in[0][i]);
			s[0] += pa * (x - s[0]);
			float y = 0.55f * x + 0.6f * scq_tanh(g * (x - s[0]));
			s[1] += ta * (y - s[1]);
			n->out[0][i] = s[1];
		}
		break;
	}
	case 1: {                                          /* dist */
		float g = 2.f + 300.f * drive * drive;
		const float pa = scq_coef_hz(100.f, SR), la = scq_coef_hz(420.f, SR), ha = scq_coef_hz(1300.f, SR);
		LOOP {
			float x = pc_in(in[0][i]);
			s[0] += pa * (x - s[0]);
			float y = scq_softclip(g * (x - s[0])) * 0.8f;
			s[1] += la * (y - s[1]);
			s[2] += ha * (y - s[2]);
			n->out[0][i] = 1.5f * (s[1] * (1.f - tone) + (y - s[2]) * tone);
		}
		break;
	}
	case 2: {                                          /* rat */
		float g = 1.f + 1500.f * drive * drive;
		const float pa = scq_coef_hz(60.f, SR), ga = scq_coef_hz(scq_min(1e6f / g, 20000.f), SR);
		const float ta = scq_coef_hz(500.f * scq_exp2(tone * 5.2f), SR);
		LOOP {
			float x = pc_in(in[0][i]);
			s[0] += pa * (x - s[0]);
			s[3] += ga * (g * (x - s[0]) - s[3]);
			s[3] = scq_clamp(s[3], -50.f, 50.f);
			float y = scq_softclip(s[3]) * 0.75f;
			s[1] += ta * (y - s[1]);
			n->out[0][i] = s[1];
		}
		break;
	}
	case 3: {                                          /* crunch */
		float g1 = 1.f + 14.f * drive, g2 = 1.f + 6.f * drive;
		float o1 = scq_tanh(0.25f), o2 = scq_tanh(-0.1f);
		const float ia = scq_coef_hz(140.f, SR), ta = scq_coef_hz(800.f, SR);
		float lw = 2.f - 2.f * tone, hw = 2.f * tone;
		LOOP {
			float x = pc_in(in[0][i]);
			float a = scq_tanh(g1 * x + 0.25f) - o1;
			s[5] += ia * (a - s[5]);
			a -= s[5];
			float b = (scq_tanh(g2 * a * 1.5f - 0.1f) - o2) * 0.75f;
			s[4] += da * (b - s[4]);
			b -= s[4];
			s[1] += ta * (b - s[1]);
			n->out[0][i] = 0.8f * (s[1] * lw + (b - s[1]) * hw) * 0.6f;
		}
		break;
	}
	}
	pc_flush(s, 6);
}
