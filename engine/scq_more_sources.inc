/*
 * More sources: oscillators, noise, chaos, drums. Included by scq_places.c (shares its helpers).
 * Each one names its lineage — where in the history of modular synthesis it comes from.
 */

/* ---------------------------------------------------------------- shared bits */

/* PolyBLEP: takes the alias out of a hard edge */
static inline float blep(float t, float dt)
{
	if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
	if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
	return 0.f;
}

static inline float saw_blep(float ph, float dt) { return 2.f * ph - 1.f - blep(ph, dt); }

static inline float square_blep(float ph, float dt, float w)
{
	float t2 = ph + 1.f - w;
	t2 -= scq_floor(t2);
	return (ph < w ? 1.f : -1.f) + blep(ph, dt) - blep(t2, dt);
}

/* a topology-preserving state-variable filter: a1..a3 from fc and damping k */
typedef struct { float a1, a2, a3, k; } svf_c;
static inline svf_c svf_make(float fc, float k)
{
	svf_c c;
	float g = scq_tan(SCQ_PI * scq_clamp(fc, 10.f, 18000.f) * INV_SR);
	c.k = k;
	c.a1 = 1.f / (1.f + g * (g + k));
	c.a2 = g * c.a1;
	c.a3 = g * c.a2;
	return c;
}
/* returns band; lp in *lp */
static inline float svf_run(const svf_c *c, float *ic1, float *ic2, float x, float *lp)
{
	float v3 = x - *ic2;
	float v1 = c->a1 * *ic1 + c->a2 * v3;
	float v2 = *ic2 + c->a2 * *ic1 + c->a3 * v3;
	*ic1 = 2.f * v1 - *ic1;
	*ic2 = 2.f * v2 - *ic2;
	if (lp) *lp = v2;
	return v1;
}

/* vowels: three formants (Hz) and their gains, a e i o u (Peterson & Barney, rounded) */
static const float VOWEL[5][6] = {
	{ 730, 1090, 2440, 1.0f, 0.50f, 0.25f },
	{ 530, 1840, 2480, 1.0f, 0.45f, 0.30f },
	{ 270, 2290, 3010, 1.0f, 0.30f, 0.25f },
	{ 570,  840, 2410, 1.0f, 0.45f, 0.20f },
	{ 300,  870, 2240, 1.0f, 0.35f, 0.15f },
};
static void vowel_at(float v, float out[6])
{
	v = scq_clamp(v, 0.f, 1.f) * 4.f;
	int i = (int)v;
	if (i > 3) i = 3;
	float f = v - (float)i;
	for (int k = 0; k < 6; k++) out[k] = VOWEL[i][k] + (VOWEL[i + 1][k] - VOWEL[i][k]) * f;
}

/* ---------------------------------------------------------------- oscillators */

/* vco — the classic analog oscillator: saw, square, triangle, sine (after Moog 921 / Minimoog) */
static void run_vco(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], tri = n->s[1];
	float base = 60.f * n->nature * scq_exp2(in[0][0]) * (n->opt[0] != 0.f ? 0.01f : 1.f);   /* pitch per block; range=audio|lfo */
	LOOP {
		if (edge(in[3][i], &n->s[2])) ph = 0.f;               /* hard sync */
		float f = base * (1.f + in[1][i]);
		float dt = scq_clamp(f * INV_SR, 0.f, 0.45f);
		float w = 0.5f + 0.45f * scq_clamp(in[2][i], -1.f, 1.f);
		float sq = square_blep(ph, dt, w);
		tri += dt * 4.f * sq;
		tri *= 0.999f;
		n->out[0][i] = saw_blep(ph, dt);
		n->out[1][i] = sq;
		n->out[2][i] = scq_clamp(tri, -1.f, 1.f);
		n->out[3][i] = scq_sin2pi(ph);
		ph += dt;
		ph -= scq_floor(ph);
	}
	n->s[0] = ph; n->s[1] = tri;
}

/* xorsquares — sacred sandwiched multivibrator pendants: two square waves that fight (after Ciat-Lonbarde's Fourses / Plumbutter) */
static void run_xorsquares(node *n, const float *const in[MAX_IN])
{
	float pa = n->s[0], pb = n->s[1], lp = n->s[2];
	float base = 60.f * n->nature, k = scq_exp2(in[0][0]);
	LOOP {
		float tilt = scq_clamp(in[1][i], -1.f, 1.f);
		float a = pa < 0.5f ? 1.f : -1.f, b = pb < 0.5f ? 1.f : -1.f;
		/* each one's speed leans on the other's state */
		pa += base * k * (1.f + 0.3f * tilt * b) * INV_SR;
		pb += base * k * 1.4983f * (1.f - 0.3f * tilt * a) * INV_SR;
		pa -= scq_floor(pa); pb -= scq_floor(pb);
		float x = a * b;                                       /* xor, as sound */
		lp += 0.35f * (x - lp);
		n->out[0][i] = lp * 0.7f;
		n->out[1][i] = a * 0.7f;
		n->out[2][i] = b * 0.7f;
	}
	n->s[0] = pa; n->s[1] = pb; n->s[2] = lp;
}

/* rungler — three hooded goblins: two triangles and a shift register that eats its own tail (after Rob Hordijk's Benjolin / rungler) */
static void run_rungler(node *n, const float *const in[MAX_IN])
{
	float pa = n->s[0], pb = n->s[1], run = n->s[3], lp = n->s[4];
	uint32_t reg = (uint32_t)n->s[2];
	float base = 30.f * n->nature * scq_exp2(in[0][0]) * INV_SR;
	float chaos = scq_clamp(in[1][0], 0.f, 1.f);
	float ia = base * scq_exp2(chaos * 3.f * (run - 0.5f));
	float ib = base * 1.618f * scq_exp2(chaos * 2.f * (0.5f - run));
	LOOP {
		pa += ia; pb += ib;
		int clock = pa >= 1.f;
		pa -= scq_floor(pa); pb -= scq_floor(pb);
		if (clock) {
			uint32_t newbit = in[2][i] > 0.5f ? ((reg >> 7) & 1u) : ((pb < 0.5f) ^ ((reg >> 7) & 1u));
			reg = ((reg << 1) | newbit) & 0xffu;
			run = (float)(reg & 7u) / 7.f;
			ia = base * scq_exp2(chaos * 3.f * (run - 0.5f));
			ib = base * 1.618f * scq_exp2(chaos * 2.f * (0.5f - run));
		}
		float ta = 1.f - 4.f * scq_abs(pa - 0.5f), tb = 1.f - 4.f * scq_abs(pb - 0.5f);
		float cmp = ta > tb ? 1.f : -1.f;
		lp += 0.25f * (cmp - lp);
		n->out[0][i] = lp * 0.7f;
		n->out[1][i] = run;
		n->out[2][i] = ta;
		n->out[3][i] = tb;
	}
	n->s[0] = pa; n->s[1] = pb; n->s[2] = (float)reg; n->s[3] = run; n->s[4] = lp;
}
static void init_rungler(node *n) { n->s[2] = (float)(n->rng & 0xffu); }

/* additive — Lakhovsky's multiple wave oscillator: sixteen partials (after Lakhovsky; Harmor / Partials; Buchla 259's harmonic thinking) */
static uint32_t mem_additive(float nature) { (void)nature; return 16 * sizeof(float); }
static void run_additive(node *n, const float *const in[MAX_IN])
{
	float *ph = (float *)n->mem;
	float f0 = 60.f * n->nature * scq_exp2(in[0][0]);
	float tilt = scq_clamp(in[1][0], 0.f, 1.f), odd = scq_clamp(in[2][0], 0.f, 1.f);
	float stretch = scq_clamp(in[3][0], -1.f, 1.f) * 0.02f;
	float amp[16], norm = 0.f;
	int top = 0;
	for (int k = 0; k < 16; k++) {
		float h = (float)(k + 1);
		float f = f0 * h * (1.f + stretch * (h - 1.f));
		float a = 0.f;
		if (f < 0.45f * SR) {
			a = scq_exp2(-tilt * 4.f * scq_log2(h));
			if ((k & 1) == 1) a *= 1.f - odd;          /* even harmonics fall away */
			top = k + 1;
		}
		amp[k] = a;
		norm += a;
	}
	float g = 0.8f / scq_max(norm, 0.5f);
	if (scq_abs(stretch) < 1e-5f) {
		/* true harmonics: one sine and cosine, the rest by recurrence,
		 * sin((k+1)x) = 2cos(x)sin(kx) - sin((k-1)x): a multiply and a subtract each */
		float p = ph[0], inc = f0 * INV_SR;
		LOOP {
			p += inc; p -= scq_floor(p);
			float s1 = scq_sin2pi(p), c2 = 2.f * scq_sin2pi(p + 0.25f);
			float sp = 0.f, sk = s1, sum = amp[0] * s1;
			for (int k = 1; k < top; k++) {
				float sn = c2 * sk - sp;
				sp = sk; sk = sn;
				sum += amp[k] * sn;
			}
			n->out[0][i] = sum * g;
		}
		ph[0] = p;
		return;
	}
	/* stretched (inharmonic) partials: each its own sine, at most eight */
	if (top > 8) top = 8;
	float inc[8];
	for (int k = 0; k < top; k++) inc[k] = f0 * (float)(k + 1) * (1.f + stretch * (float)k) * INV_SR;
	LOOP {
		float sum = 0.f;
		for (int k = 0; k < top; k++) {
			float p = ph[k] + inc[k];
			p -= scq_floor(p);
			ph[k] = p;
			sum += amp[k] * scq_sin2pi(p);
		}
		n->out[0][i] = sum * g;
	}
}

/* pulsar — undulating pulsar wavetables: a burst of formant in each period (after Curtis Roads' pulsar synthesis; Kyma) */
static void run_pulsar(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	const float inc = 60.f * n->nature * scq_exp2(in[0][0]) * INV_SR;
	const float formant = scq_exp2(scq_clamp(in[1][0], 0.f, 4.f) + 0.5f);
	const float duty = 0.05f + 0.95f * scq_clamp(in[2][0], 0.f, 1.f), inv_duty = 1.f / duty;
	LOOP {
		ph += inc;
		ph -= scq_floor(ph);
		float o = 0.f;
		if (ph < duty) {
			float q = ph * inv_duty;
			float w = scq_sin2pi(q * 0.5f);
			o = scq_sin2pi(q * formant * duty * 4.f) * w * w;
		}
		n->out[0][i] = o * 0.8f;
	}
	n->s[0] = ph;
}

/* fm — the smiling torpedo: two-operator FM with feedback (after Chowning / Yamaha DX; Buchla 259's FM) */
static void run_fm(node *n, const float *const in[MAX_IN])
{
	float pc = n->s[0], pm = n->s[1], last = n->s[2];
	const float f = 240.f * scq_exp2(in[0][0]);
	LOOP {
		float ratio = scq_max(n->nature + in[1][i], 0.f);
		float index = scq_clamp(in[2][i], 0.f, 2.f) * 2.f;
		float fb = scq_clamp(in[3][i], 0.f, 1.f) * 0.3f;
		pm += f * ratio * INV_SR; pm -= scq_floor(pm);
		float m = scq_sin2pi(pm + fb * last);
		last = m;
		pc += f * INV_SR; pc -= scq_floor(pc);
		n->out[0][i] = scq_sin2pi(pc + index * m * 0.5f);
		n->out[1][i] = m;
	}
	n->s[0] = pc; n->s[1] = pm; n->s[2] = last;
}

/* phasedist — the inevitably spherical probe beacon: phase distortion (after Casio CZ; Nord Modular's PD oscillator) */
static void run_phasedist(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	const float inc = 60.f * n->nature * scq_exp2(in[0][0]) * INV_SR;
	const float d = 0.5f - 0.48f * scq_clamp(in[1][0], 0.f, 1.f);
	const float ka = 0.5f / d, kb = 0.5f / (1.f - d);
	LOOP {
		ph += inc; ph -= scq_floor(ph);
		float p = ph < d ? ph * ka : 0.5f + (ph - d) * kb;
		float o = -scq_sin2pi(p + 0.25f);                         /* cosine, from silence */
		if (in[2][i] > 0.5f) o *= 1.f - ph;                       /* resonant: a sawtooth window */
		n->out[0][i] = o;
	}
	n->s[0] = ph;
}

/* wavetable — the 7-inch with the purple glitter label: one wave that turns from sine to pulse (after PPG / Waldorf wavetables; Plaits) */
static void run_wavetable(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], tri = n->s[1];
	const float dt = scq_clamp(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.f, 0.45f);
	LOOP {
		float sc = scq_clamp(in[1][i], 0.f, 1.f) * 4.f;
		int a = (int)sc;
		if (a > 3) a = 3;
		float x = sc - (float)a;
		float sq = square_blep(ph, dt, 0.5f);
		tri += dt * 4.f * sq; tri *= 0.999f;
		float w[5];
		w[0] = scq_sin2pi(ph);
		w[1] = scq_clamp(tri, -1.f, 1.f);
		w[2] = saw_blep(ph, dt);
		w[3] = sq;
		w[4] = square_blep(ph, dt, 0.1f) + 0.8f;
		n->out[0][i] = (w[a] + (w[a + 1] - w[a]) * x) * 0.8f;
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph; n->s[1] = tri;
}

/* chord — the velvet throne: a four-note chord in just intonation (after Plaits' chord engine; string machines) */
static const float CHORDS[6][4] = {
	{ 1.f, 1.25f, 1.5f, 2.f },      /* major      */
	{ 1.f, 1.2f, 1.5f, 2.f },       /* minor      */
	{ 1.f, 1.3333f, 1.5f, 2.f },    /* sus        */
	{ 1.f, 1.25f, 1.5f, 1.875f },   /* major 7    */
	{ 1.f, 1.2f, 1.5f, 1.8f },      /* minor 7    */
	{ 1.f, 1.25f, 1.5f, 1.75f },    /* 4:5:6:7    */
};
static void run_chord(node *n, const float *const in[MAX_IN])
{
	int c = (int)(scq_clamp(in[1][0], 0.f, 0.999f) * 6.f);
	float spread = scq_clamp(in[2][0], 0.f, 1.f) * 0.004f;
	float base = 60.f * n->nature * scq_exp2(in[0][0]);
	LOOP {
		float sum = 0.f;
		for (int k = 0; k < 4; k++) {
			float f = base * CHORDS[c][k] * (1.f + spread * (float)(k - 1));
			float dt = scq_clamp(f * INV_SR, 0.f, 0.45f);
			float p = n->s[k] + dt;
			p -= scq_floor(p);
			n->s[k] = p;
			sum += saw_blep(p, dt);
		}
		n->out[0][i] = sum * 0.25f;
	}
}

/* supersaw — the warehouse rave: seven detuned saws (after Roland JP-8000's supersaw) */
static const float RAVE_D[7] = { -0.110f, -0.063f, -0.019f, 0.f, 0.019f, 0.062f, 0.107f };
static void run_supersaw(node *n, const float *const in[MAX_IN])
{
	float base = 60.f * n->nature * scq_exp2(in[0][0]);
	float det = scq_clamp(in[1][0], 0.f, 1.f);
	float mix = scq_clamp(in[2][0], 0.f, 1.f);
	float dts[7];
	for (int k = 0; k < 7; k++) dts[k] = scq_clamp(base * scq_exp2(RAVE_D[k] * det) * INV_SR, 0.f, 0.45f);
	LOOP {
		float side = 0.f, mid = 0.f;
		for (int k = 0; k < 7; k++) {
			float p = n->s[k] + dts[k];
			p -= scq_floor(p);
			n->s[k] = p;
			float v = saw_blep(p, dts[k]);
			if (k == 3) mid = v; else side += v;
		}
		n->out[0][i] = (mid * (1.f - mix * 0.5f) + side * mix * 0.25f) * 0.6f;
	}
}

/* formant — nuns into cats into fairies, as the speaker preaches: a singing voice (after FOF / IRCAM CHANT; Doepfer A-104 formants) */
static uint32_t mem_formant(float nature) { (void)nature; return 8 * sizeof(float); }
static void run_formant(node *n, const float *const in[MAX_IN])
{
	float *st = (float *)n->mem;
	float ph = n->s[0];
	float base = 60.f * n->nature;
	float v[6];
	vowel_at(in[1][0], v);
	svf_c c[3];
	for (int k = 0; k < 3; k++) c[k] = svf_make(v[k], 0.12f);
	const float dt = scq_clamp(base * scq_exp2(in[0][0]) * INV_SR, 0.f, 0.45f);
	LOOP {
		float buzz = saw_blep(ph, dt);
		ph += dt; ph -= scq_floor(ph);
		float o = 0.f;
		for (int k = 0; k < 3; k++) o += v[3 + k] * svf_run(&c[k], &st[k * 2], &st[k * 2 + 1], buzz, 0);
		n->out[0][i] = o * 0.6f;
	}
	n->s[0] = ph;
}

/* hum — the 60 Hz hum the journal tunes to: mains buzz and its harmonics (after the 60 Hz just-intonation control writing) */
static void run_hum(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	float inc = 60.f * n->nature * INV_SR;
	float buzz = scq_clamp(in[0][0], 0.f, 1.f);
	float amp[8], a = 1.f;
	for (int h = 1; h <= 8; h++) { amp[h - 1] = a; a *= (h & 1) ? 0.35f + 0.5f * buzz : 0.25f + 0.3f * buzz; }
	float drive = 0.6f + buzz * 2.f;
	LOOP {
		ph += inc; ph -= scq_floor(ph);
		float s1 = scq_sin2pi(ph), c2 = 2.f * scq_sin2pi(ph + 0.25f);
		float sp = 0.f, sk = s1, o = amp[0] * s1;
		for (int h = 1; h < 8; h++) { float sn = c2 * sk - sp; sp = sk; sk = sn; o += amp[h] * sn; }
		n->out[0][i] = scq_tanh(o * drive) * 0.5f * scq_clamp(in[1][i], 0.f, 1.f);
	}
	n->s[0] = ph;
}

/* ---------------------------------------------------------------- noise and chaos */

/* noise — the generative freshwater faucet: white, pink, brown and blue water */
static void run_noise(node *n, const float *const in[MAX_IN])
{
	float b0 = n->s[0], b1 = n->s[1], b2 = n->s[2], br = n->s[3], last = n->s[4];
	LOOP {
		float w = scq_noise(&n->rng), g = in[0][i];
		b0 = 0.99765f * b0 + w * 0.0990460f;
		b1 = 0.96300f * b1 + w * 0.2965164f;
		b2 = 0.57000f * b2 + w * 1.0526913f;
		br = (br + w * 0.02f) * 0.998f;
		n->out[0][i] = w * 0.5f * g;
		n->out[1][i] = (b0 + b1 + b2 + w * 0.1848f) * 0.12f * g;
		n->out[2][i] = br * 2.5f * g;
		n->out[3][i] = (w - last) * 0.3f * g;
		last = w;
	}
	n->s[0] = b0; n->s[1] = b1; n->s[2] = b2; n->s[3] = br; n->s[4] = last;
}

/* crackle — golden glitter: crackle and dust (after SuperCollider's Crackle and Dust) */
static void run_crackle(node *n, const float *const in[MAX_IN])
{
	float y1 = n->s[0], y2 = n->s[1];
	if (y1 == 0.f && y2 == 0.f) y1 = 0.3f;
	LOOP {
		float p = 1.5f + 0.49f * scq_clamp(in[0][i], 0.f, 1.f);
		float y0 = scq_abs(y1 * p - y2 - 0.05f);
		y2 = y1; y1 = y0;
		n->out[0][i] = scq_clamp(y0 - 0.5f, -1.f, 1.f);
		float rate = 4.f * n->nature * scq_exp2(scq_clamp(in[1][i], 0.f, 1.f) * 8.f);
		n->out[1][i] = scq_rand01(&n->rng) < rate * INV_SR ? scq_noise(&n->rng) : 0.f;
	}
	n->s[0] = y1; n->s[1] = y2;
}

/* lorenz — the icy spillway: a Lorenz attractor, three ways out (after Lorenz; chaos modules) */
static void run_lorenz(node *n, const float *const in[MAX_IN])
{
	float x = n->s[0], y = n->s[1], z = n->s[2];
	if (x == 0.f && y == 0.f && z == 0.f) { x = 0.1f; y = 0.f; z = 0.f; }
	LOOP {
		float dt = 0.00002f * n->nature * scq_exp2(scq_clamp(in[0][i], 0.f, 1.f) * 10.f);
		float rho = 20.f + 40.f * scq_clamp(in[1][i], 0.f, 1.f);
		float dx = 10.f * (y - x), dy = x * (rho - z) - y, dz = x * y - 2.6667f * z;
		x += dx * dt; y += dy * dt; z += dz * dt;
		n->out[0][i] = x * 0.05f;
		n->out[1][i] = y * 0.04f;
		n->out[2][i] = z * 0.02f - 0.5f;
	}
	n->s[0] = x; n->s[1] = y; n->s[2] = z;
}
static void init_lorenz(node *n) { n->s[0] = 0.1f; n->s[1] = 0.f; n->s[2] = 1.f; }

/* ---------------------------------------------------------------- drums and struck things */

/* marimba — the marimba in the basement: a struck bar (after Mutable Rings / Elements' modal voices) */
static const float BAR_R[3] = { 1.f, 3.99f, 10.65f };
static void run_marimba(node *n, const float *const in[MAX_IN])
{
	float f0 = 60.f * n->nature * scq_exp2(in[1][0]);
	float T = 0.15f + 2.f * scq_clamp(in[2][0], 0.f, 1.f);
	float bright = scq_clamp(in[3][0], 0.f, 1.f);
	float c1[3], c2[3], g[3];
	for (int k = 0; k < 3; k++) {
		float f = scq_min(f0 * BAR_R[k], 18000.f);
		float r = scq_exp2(-1.442695f / (T / (1.f + (float)k * 2.f) * SR));
		c1[k] = 2.f * r * scq_sin2pi(f * INV_SR + 0.25f);
		c2[k] = -r * r;
		g[k] = scq_sin2pi(f * INV_SR) * (k == 0 ? 0.5f : 0.3f * bright);
	}
	LOOP {
		float x = in[0][i] - n->s[6];
		n->s[6] = in[0][i];
		float sum = 0.f;
		for (int k = 0; k < 3; k++) {
			float y = g[k] * x + c1[k] * n->s[k] + c2[k] * n->s[3 + k];
			n->s[3 + k] = n->s[k];
			n->s[k] = y;
			sum += y;
		}
		n->out[0][i] = sum;
	}
}

/* string — swinging on rebar: a plucked string (after Karplus-Strong; Fish's string; Rings) */
#define REBAR_LEN 2048
static uint32_t mem_string(float nature) { (void)nature; return REBAR_LEN * sizeof(float); }
static void run_string(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float lp = n->s[1];
	float f = 60.f * n->nature * scq_exp2(in[1][0]);
	float period = scq_clamp(SR / scq_max(f, 1.f), 2.f, (float)(REBAR_LEN - 2));
	float damp = 0.95f + 0.0499f * (1.f - scq_clamp(in[2][0], 0.f, 1.f));
	float bright = 0.2f + 0.8f * scq_clamp(in[3][0], 0.f, 1.f);
	LOOP {
		if (edge(in[0][i], &n->s[2])) {                    /* pluck: fill one period with noise */
			float nl = 0.f;
			int len = (int)period;
			for (int k = 0; k < len; k++) {
				nl += bright * (scq_noise(&n->rng) - nl);
				buf[(w + (uint32_t)REBAR_LEN - (uint32_t)len + (uint32_t)k) & (REBAR_LEN - 1)] = nl;
			}
		}
		float rp = (float)w - period;
		if (rp < 0.f) rp += (float)REBAR_LEN;
		uint32_t r0 = (uint32_t)rp;
		float fr = rp - (float)r0;
		float y = buf[r0 & (REBAR_LEN - 1)] + (buf[(r0 + 1) & (REBAR_LEN - 1)] - buf[r0 & (REBAR_LEN - 1)]) * fr;
		lp += bright * (y - lp);
		buf[w] = lp * damp;
		w = (w + 1) & (REBAR_LEN - 1);
		n->out[0][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = lp;
}

/* kick — the swinging wooden trebuchet: a kick drum (after the TR-808 bass drum; Mutable Peaks) */
static void run_kick(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], env = n->s[1], penv = n->s[2];
	float f0 = 60.f * n->nature * scq_exp2(in[1][0]);
	float punch = scq_clamp(in[2][0], 0.f, 1.f) * 6.f;
	float decay = scq_exp2(-1.442695f / ((0.08f + 1.2f * scq_clamp(in[3][0], 0.f, 1.f)) * SR));
	float pdecay = scq_exp2(-1.442695f / (0.025f * SR));
	LOOP {
		if (edge(in[0][i], &n->s[3])) { env = 1.f; penv = 1.f; ph = 0.f; }
		ph += f0 * (1.f + punch * penv) * INV_SR;
		ph -= scq_floor(ph);
		n->out[0][i] = scq_softclip(scq_sin2pi(ph) * env * 1.4f);
		env *= decay; penv *= pdecay;
	}
	n->s[0] = ph; n->s[1] = env; n->s[2] = penv;
}

/* snare — the balloon pop, the speculative gunshot: a snare (after the TR-808 / 909 snare) */
static void run_snare(node *n, const float *const in[MAX_IN])
{
	float p1 = n->s[0], p2 = n->s[1], te = n->s[2], ne = n->s[3], hp = n->s[4], bp1 = n->s[5], bp2 = n->s[6];
	float f = 180.f * n->nature * scq_exp2(in[1][0]);
	float snap = scq_clamp(in[2][0], 0.f, 1.f);
	float td = scq_exp2(-1.442695f / (0.08f * SR)), nd = scq_exp2(-1.442695f / ((0.05f + 0.2f * snap) * SR));
	svf_c c = svf_make(3200.f, 0.8f);
	LOOP {
		if (edge(in[0][i], &n->s[7])) { te = 1.f; ne = 1.f; }
		p1 += f * INV_SR; p1 -= scq_floor(p1);
		p2 += f * 1.83f * INV_SR; p2 -= scq_floor(p2);
		float tone = (scq_sin2pi(p1) + 0.6f * scq_sin2pi(p2)) * te * 0.5f;
		float w = scq_noise(&n->rng);
		hp += 0.2f * (w - hp);
		float bp = svf_run(&c, &bp1, &bp2, w - hp, 0);
		n->out[0][i] = scq_softclip(tone * (1.f - snap * 0.5f) + bp * ne * (0.6f + snap));
		te *= td; ne *= nd;
	}
	n->s[0] = p1; n->s[1] = p2; n->s[2] = te; n->s[3] = ne; n->s[4] = hp; n->s[5] = bp1; n->s[6] = bp2;
}

/* hihat — the sizzle of the grill: a metallic hi-hat, six squares (after the TR-808 cymbal circuit) */
static const float HAT_F[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f };
static uint32_t mem_hihat(float nature) { (void)nature; return 6 * sizeof(uint32_t); }
static void run_hihat(node *n, const float *const in[MAX_IN])
{
	/* six squares on whole-number phases: the top bit is the square, an add moves it */
	uint32_t *ph = (uint32_t *)n->mem;
	float env = n->s[6], h1 = n->s[7], h2 = n->s[8];
	float k = n->nature * scq_exp2(in[2][0]);
	uint32_t inc[6];
	for (int j = 0; j < 6; j++) inc[j] = (uint32_t)(scq_min(HAT_F[j] * k * INV_SR, 0.45f) * 4294967296.f);
	float decay = scq_exp2(-1.442695f / ((0.02f + 0.5f * scq_clamp(in[1][0], 0.f, 1.f)) * SR));
	svf_c c = svf_make(9000.f, 0.6f);
	LOOP {
		if (edge(in[0][i], &n->s[9])) env = 1.f;
		int sum = 0;
		for (int j = 0; j < 6; j++) { ph[j] += inc[j]; sum += (int)(ph[j] >> 31); }
		float x = ((float)sum * 2.f - 6.f) * 0.17f;
		float lp;
		float bp = svf_run(&c, &h1, &h2, x, &lp);
		n->out[0][i] = (x - lp - bp * 0.3f) * env;
		env *= decay;
	}
	n->s[6] = env; n->s[7] = h1; n->s[8] = h2;
}
