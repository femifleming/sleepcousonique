/*
 * More shapes: filters, phasers, chorus, ring and frequency shifting, the vocoder, dynamics,
 * logic and the small tools. Included by scq_places.c after scq_more_sources.c.
 */

/* ladder — the vertical shaft you climb out of: a four-pole transistor ladder (after Moog 904A; Huovilainen) */
static void run_ladder(node *n, const float *const in[MAX_IN])
{
	float *y = &n->s[0];
	float fc = scq_clamp(480.f * n->nature * scq_exp2(in[1][0]), 20.f, 16000.f);
	float g = 1.f - scq_exp2(-1.442695f * 2.f * SCQ_PI * fc * INV_SR);
	float k = 4.f * scq_clamp(in[2][0], 0.f, 1.05f);
	LOOP {
		float x = scq_tanh(in[0][i] - k * y[3]);
		y[0] += g * (x - scq_tanh(y[0]));
		y[1] += g * (scq_tanh(y[0]) - scq_tanh(y[1]));
		y[2] += g * (scq_tanh(y[1]) - scq_tanh(y[2]));
		y[3] += g * (scq_tanh(y[2]) - y[3]);
		n->out[0][i] = y[3] * (1.f + k * 0.35f);
		n->out[1][i] = (y[1] - y[3]) * 2.f;
	}
}

/* svf — the night pond: lowpass, bandpass, highpass and notch at once (after Oberheim SEM; Serge VCFQ) */
static void run_svf(node *n, const float *const in[MAX_IN])
{
	float fc = 480.f * n->nature * scq_exp2(in[1][0]);
	svf_c c = svf_make(fc, 2.f - 1.96f * scq_clamp(in[2][0], 0.f, 1.f));
	LOOP {
		float lp, x = in[0][i];
		float bp = svf_run(&c, &n->s[0], &n->s[1], x, &lp);
		float hp = x - c.k * bp - lp;
		n->out[0][i] = lp;
		n->out[1][i] = bp;
		n->out[2][i] = hp;
		n->out[3][i] = lp + hp;
	}
}

/* vowelfilter — as the speaker preaches: a vowel filter (after the Doepfer A-104 Trautonium formant filter; talk boxes) */
static void run_vowelfilter(node *n, const float *const in[MAX_IN])
{
	float v[6];
	vowel_at(in[1][0], v);
	float shift = scq_exp2(in[2][0]);
	svf_c c[3];
	for (int k = 0; k < 3; k++) c[k] = svf_make(v[k] * shift, 0.15f);
	LOOP {
		float o = 0.f;
		for (int k = 0; k < 3; k++) o += v[3 + k] * svf_run(&c[k], &n->s[k * 2], &n->s[k * 2 + 1], in[0][i], 0);
		n->out[0][i] = o;
	}
}

/* phaser — climbing through glass: a six-stage phaser (after the Mu-Tron Bi-Phase; Electro-Harmonix Small Stone) */
static void run_phaser(node *n, const float *const in[MAX_IN])
{
	float *z = &n->s[0];            /* 6 stage states */
	float ph = n->s[6], fbk = n->s[7];
	float rate = 0.05f * n->nature * scq_exp2(scq_clamp(in[1][0], 0.f, 1.f) * 7.f);
	float depth = scq_clamp(in[2][0], 0.f, 1.f), fb = scq_clamp(in[3][0], 0.f, 0.95f);
	ph += rate * (float)B * INV_SR; ph -= scq_floor(ph);          /* the sweep, per block */
	const float lfo = 0.5f + 0.5f * scq_sin2pi(ph);
	const float t = scq_tan(SCQ_PI * scq_min(200.f * scq_exp2(lfo * depth * 5.f), 16000.f) * INV_SR);
	const float a = (t - 1.f) / (t + 1.f);
	LOOP {
		float x = in[0][i] + fbk * fb;
		for (int k = 0; k < 6; k++) {
			float y = a * x + z[k];
			z[k] = x - a * y;
			x = y;
		}
		fbk = x;
		n->out[0][i] = 0.5f * (in[0][i] + x);
	}
	n->s[6] = ph; n->s[7] = fbk;
}

/* flanger — conduit to conduit, like stargates: a flanger (after the Electric Mistress; bucket-brigade delays) */
#define CONDUIT_LEN 1024
static uint32_t mem_flanger(float nature) { (void)nature; return CONDUIT_LEN * sizeof(float); }
static void run_flanger(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float ph = n->s[1];
	float rate = 0.03f * n->nature * scq_exp2(scq_clamp(in[1][0], 0.f, 1.f) * 7.f);
	float depth = scq_clamp(in[2][0], 0.f, 1.f), fb = scq_clamp(in[3][0], -0.95f, 0.95f);
	LOOP {
		ph += rate * INV_SR; ph -= scq_floor(ph);
		float d = (0.0008f + depth * 0.008f * (0.5f + 0.5f * scq_sin2pi(ph))) * SR;
		float rp = (float)w - d;
		if (rp < 0.f) rp += (float)CONDUIT_LEN;
		uint32_t r0 = (uint32_t)rp;
		float fr = rp - (float)r0;
		float y = buf[r0 & (CONDUIT_LEN - 1)] + (buf[(r0 + 1) & (CONDUIT_LEN - 1)] - buf[r0 & (CONDUIT_LEN - 1)]) * fr;
		buf[w] = scq_tanh(in[0][i] + y * fb);
		w = (w + 1) & (CONDUIT_LEN - 1);
		n->out[0][i] = 0.6f * (in[0][i] + y);
		n->out[1][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = ph;
}

/* chorus — nuns into cats into fairies: a three-voice ensemble chorus (after the Solina / ARP string ensemble) */
#define FAIRY_LEN 2048
static uint32_t mem_chorus(float nature) { (void)nature; return FAIRY_LEN * sizeof(float); }
static void run_chorus(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float ph = n->s[1], vib = n->s[2];
	float depth = scq_clamp(in[1][0], 0.f, 1.f);
	LOOP {
		ph += 0.6f * n->nature * INV_SR; ph -= scq_floor(ph);
		vib += 6.f * INV_SR; vib -= scq_floor(vib);
		buf[w] = in[0][i];
		float o[3];
		for (int k = 0; k < 3; k++) {
			float m = scq_sin2pi(ph + (float)k / 3.f) + 0.15f * scq_sin2pi(vib + (float)k / 3.f);
			float d = (0.01f + depth * 0.004f * m) * SR;
			float rp = (float)w - d;
			if (rp < 0.f) rp += (float)FAIRY_LEN;
			uint32_t r0 = (uint32_t)rp;
			float fr = rp - (float)r0;
			o[k] = buf[r0 & (FAIRY_LEN - 1)] + (buf[(r0 + 1) & (FAIRY_LEN - 1)] - buf[r0 & (FAIRY_LEN - 1)]) * fr;
		}
		w = (w + 1) & (FAIRY_LEN - 1);
		n->out[1][i] = 0.5f * in[0][i] + 0.5f * (o[0] + o[1] * 0.5f);
		n->out[2][i] = 0.5f * in[0][i] + 0.5f * (o[2] + o[1] * 0.5f);
		n->out[0][i] = 0.5f * (n->out[1][i] + n->out[2][i]);
	}
	n->s[0] = (float)w; n->s[1] = ph; n->s[2] = vib;
}

/* ringmod — the pulley system: a ring modulator, with its own carrier if you give it none (after Bode; EMS VCS3; Buchla 285) */
static void run_ringmod(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	int own = !(n->patched & 2);
	const float inc = 120.f * n->nature * scq_exp2(in[2][0]) * INV_SR;
	LOOP {
		float m = in[1][i];
		if (own) {
			ph += inc; ph -= scq_floor(ph);
			m = scq_sin2pi(ph);
		}
		n->out[0][i] = in[0][i] * m;
	}
	n->s[0] = ph;
}

/* freqshift — a portal to other cities: a frequency shifter, up and down (after Harald Bode; Buchla 285) */
static const float HIL_A[4] = { 0.6923878f, 0.9360654f, 0.9882295f, 0.9987488f };
static const float HIL_B[4] = { 0.4021921f, 0.8561711f, 0.9722910f, 0.9952885f };
static uint32_t mem_freqshift(float nature) { (void)nature; return 34 * sizeof(float); }
static void run_freqshift(node *n, const float *const in[MAX_IN])
{
	float *m = (float *)n->mem;          /* 8 stages x (x1 x2 y1 y2), then delay, phase */
	float ph = m[33];
	LOOP {
		float shift = 20.f * n->nature * in[1][i] * 25.f;   /* -1..1 -> -500..500 Hz at nature 1 */
		float xa = in[0][i], xb = in[0][i];
		for (int k = 0; k < 4; k++) {
			float *s = m + k * 4;
			float y = HIL_A[k] * (xa + s[3]) - s[1];
			s[1] = s[0]; s[0] = xa; s[3] = s[2]; s[2] = y;
			xa = y;
			s = m + 16 + k * 4;
			y = HIL_B[k] * (xb + s[3]) - s[1];
			s[1] = s[0]; s[0] = xb; s[3] = s[2]; s[2] = y;
			xb = y;
		}
		float q = m[32];                  /* path B, one sample late */
		m[32] = xb;
		ph += shift * INV_SR; ph -= scq_floor(ph);
		float c = scq_sin2pi(ph + 0.25f), s = scq_sin2pi(ph);
		n->out[0][i] = xa * c - q * s;
		n->out[1][i] = xa * c + q * s;
	}
	m[33] = ph;
}

/* bitcrush — the stolen 516 GB flash drive: fewer bits, fewer samples (after the SP-1200 / early samplers; decimators) */
static void run_bitcrush(node *n, const float *const in[MAX_IN])
{
	float held = n->s[0], cnt = n->s[1];
	LOOP {
		float bits = 1.f + 15.f * (1.f - scq_clamp(in[1][i], 0.f, 1.f));
		float hold = 1.f + 63.f * scq_clamp(in[2][i], 0.f, 1.f);
		if ((cnt += 1.f) >= hold) {
			cnt = 0.f;
			float q = scq_exp2(bits - 1.f);
			held = scq_floor(in[0][i] * q + 0.5f) / q;
		}
		n->out[0][i] = held;
	}
	n->s[0] = held; n->s[1] = cnt;
}

/* tube — tungsten lamplight at night: a warm tube stage (after tube preamps; the Culture Vulture) */
static void run_tube(node *n, const float *const in[MAX_IN])
{
	float lp = n->s[0];
	LOOP {
		float drive = 1.f + 10.f * scq_clamp(in[1][i], 0.f, 1.f) * n->nature;
		float bias = scq_clamp(in[2][i], -1.f, 1.f) * 0.5f;
		float y = scq_tanh(drive * (in[0][i] + bias)) - scq_tanh(drive * bias);
		lp += 0.0013f * (y - lp);
		n->out[0][i] = (y - lp) * (1.2f / scq_tanh(drive * 0.6f + 0.2f));
	}
	n->s[0] = lp;
}

/* compressor — muffled into a pillow: a compressor (after the LA-2A / 1176; Fish's press) */
static void run_compressor(node *n, const float *const in[MAX_IN])
{
	float env = n->s[0];
	float sq = scq_clamp(in[1][0], 0.f, 1.f);
	float thr = 0.5f - 0.45f * sq, ratio = 1.f + 9.f * sq;
	float att = scq_coef_time(0.005f, SR), rel = scq_coef_time(0.15f, SR);
	int side = n->patched & 4;
	LOOP {
		float d = scq_abs(side ? in[2][i] : in[0][i]);
		env += (d - env) * (d > env ? att : rel);
		float g = 1.f;
		if (env > thr) g = scq_exp2((1.f / ratio - 1.f) * scq_log2(env / thr));
		n->out[0][i] = in[0][i] * g * (1.f + sq);
		n->out[1][i] = g;
	}
	n->s[0] = env;
}

/* crossfade — scenes blurring together: a crossfade between two things */
static void run_crossfade(node *n, const float *const in[MAX_IN])
{
	(void)n;
	LOOP {
		float m = scq_clamp(in[2][i], 0.f, 1.f);
		n->out[0][i] = in[0][i] * (1.f - m) + in[1][i] * m;
	}
}

/* vocoder — the DJ radio station in the basement: an eight-band vocoder (after the EMS Vocoder; Mutable Warps) */
static const float VOC_F[8] = { 180.f, 300.f, 500.f, 800.f, 1300.f, 2100.f, 3400.f, 5500.f };
static uint32_t mem_vocoder(float nature) { (void)nature; return (8 * 5 + 2) * sizeof(float); }
static void run_vocoder(node *n, const float *const in[MAX_IN])
{
	float *m = (float *)n->mem;           /* per band: c1 c2 m1 m2 env */
	float ph = m[40];
	int own = !(n->patched & 1);
	svf_c c[8];
	for (int k = 0; k < 8; k++) c[k] = svf_make(VOC_F[k], 0.25f);
	float att = scq_coef_time(0.004f, SR), rel = scq_coef_time(0.04f, SR);
	LOOP {
		float car = in[0][i];
		if (own) {                         /* a buzz of its own to speak through */
			float f = 60.f * n->nature * 2.f;
			car = saw_blep(ph, f * INV_SR);
			ph += f * INV_SR; ph -= scq_floor(ph);
		}
		float o = 0.f;
		for (int k = 0; k < 8; k++) {
			float *b = m + k * 5;
			float bc = svf_run(&c[k], &b[0], &b[1], car, 0);
			float bm = scq_abs(svf_run(&c[k], &b[2], &b[3], in[1][i], 0));
			b[4] += (bm - b[4]) * (bm > b[4] ? att : rel);
			o += bc * b[4];
		}
		n->out[0][i] = o * 4.f;
	}
	m[40] = ph;
}

/* follower — a glowing presence: an envelope follower, with a gate when something is there (after the ARP 2600 envelope follower) */
static void run_follower(node *n, const float *const in[MAX_IN])
{
	float env = n->s[0];
	float att = scq_coef_time(0.001f + 0.2f * scq_clamp(in[1][0], 0.f, 1.f), SR);
	float rel = scq_coef_time(0.01f + 1.f * scq_clamp(in[2][0], 0.f, 1.f), SR);
	LOOP {
		float a = scq_abs(in[0][i]);
		env += (a - env) * (a > env ? att : rel);
		n->out[0][i] = scq_clamp(env * 1.5f, 0.f, 1.f);
		n->out[1][i] = env > 0.08f ? 1.f : 0.f;
	}
	n->s[0] = env;
}

/* samplehold — taking photos of the basement: sample & hold and track & hold (after the ARP 2600 S&H; Buchla 266) */
static void run_samplehold(node *n, const float *const in[MAX_IN])
{
	float held = n->s[0], track = n->s[1];
	int noise = !(n->patched & 1);
	LOOP {
		float x = noise ? scq_rand01(&n->rng) : in[0][i];
		if (edge(in[1][i], &n->s[2])) held = x;
		if (in[1][i] > 0.5f) track = x;
		n->out[0][i] = held;
		n->out[1][i] = track;
	}
	n->s[0] = held; n->s[1] = track;
}

/* logic — the key velcroed to a hood: logic, and / or / xor / not (after Serge and Doepfer logic) */
static void run_logic(node *n, const float *const in[MAX_IN])
{
	(void)n;
	LOOP {
		int a = in[0][i] > 0.5f, b = in[1][i] > 0.5f;
		n->out[0][i] = (float)(a & b);
		n->out[1][i] = (float)(a | b);
		n->out[2][i] = (float)(a ^ b);
		n->out[3][i] = (float)!a;
	}
}

/* comparator — standing at the threshold: a comparator (after Serge and Buchla comparators) */
static void run_comparator(node *n, const float *const in[MAX_IN])
{
	float st = n->s[0];
	LOOP {
		float r = in[1][i];
		if (in[0][i] > r + 0.005f) st = 1.f;
		else if (in[0][i] < r - 0.005f) st = 0.f;      /* a little hysteresis */
		n->out[0][i] = st;
		n->out[1][i] = 1.f - st;
	}
	n->s[0] = st;
}

/* rectify — reflections in a puddle: flip, fold up, cut in half, and the sign (after Serge / Make Noise rectifiers; Fish's reflect) */
static void run_rectify(node *n, const float *const in[MAX_IN])
{
	(void)n;
	LOOP {
		float x = in[0][i];
		n->out[0][i] = -x;
		n->out[1][i] = scq_abs(x);
		n->out[2][i] = x > 0.f ? x : 0.f;
		n->out[3][i] = x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f);
	}
}

/* pan — the van in the driveway: places a sound left or right (after Buchla 227 quad panning; equal-power pans) */
static void run_pan(node *n, const float *const in[MAX_IN])
{
	(void)n;
	LOOP {
		float p = (scq_clamp(in[1][i], -1.f, 1.f) + 1.f) * 0.125f;   /* 0..0.25 turn */
		n->out[0][i] = in[0][i] * scq_sin2pi(p + 0.25f);
		n->out[1][i] = in[0][i] * scq_sin2pi(p);
	}
}

/* suboctave — the ripple subwoofers: one and two octaves under whatever comes in (after Boss OC-2; sub-oscillator dividers) */
static void run_suboctave(node *n, const float *const in[MAX_IN])
{
	float last = n->s[0], env = n->s[1], lp1 = n->s[4], lp2 = n->s[5];
	int ff1 = (int)n->s[2], ff2 = (int)n->s[3];
	LOOP {
		float x = in[0][i];
		if (x > 0.f && last <= 0.f) {
			ff1 ^= 1;
			if (ff1) ff2 ^= 1;
		}
		last = x;
		float a = scq_abs(x);
		env += (a - env) * (a > env ? 0.01f : 0.0005f);
		lp1 += 0.02f * ((ff1 ? 1.f : -1.f) - lp1);
		lp2 += 0.01f * ((ff2 ? 1.f : -1.f) - lp2);
		n->out[0][i] = lp1 * env * 1.5f;
		n->out[1][i] = lp2 * env * 1.5f;
	}
	n->s[0] = last; n->s[1] = env; n->s[2] = (float)ff1; n->s[3] = (float)ff2; n->s[4] = lp1; n->s[5] = lp2;
}
