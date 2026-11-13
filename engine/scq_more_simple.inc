/* The bare places: one job each, one outlet, nothing computed that isn't heard. For big
 * patches on the SP-1, where every cycle counts. Pitch is set once a block (the tune knob and
 * att.pitch reach them like every other oscillator). */

/* saw — a sawtooth, nothing else */
static void run_saw(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	const float dt = scq_min(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.45f);
	LOOP {
		n->out[0][i] = saw_blep(ph, dt);
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* tri — a triangle, nothing else */
static void run_tri(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	const float dt = scq_min(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.45f);
	LOOP {
		n->out[0][i] = 4.f * scq_abs(ph - 0.5f) - 1.f;
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* pulse — a pulse wave; width -1..1 (0 is square), read once a block */
static void run_pulse(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	const float dt = scq_min(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.45f);
	const float w = 0.5f + 0.45f * scq_clamp(in[1][0], -1.f, 1.f);
	LOOP {
		n->out[0][i] = square_blep(ph, dt, w);
		ph += dt; ph -= scq_floor(ph);
	}
	n->s[0] = ph;
}

/* white — white noise, nothing else */
static void run_white(node *n, const float *const in[MAX_IN])
{
	(void)in;
	LOOP n->out[0][i] = 0.5f * scq_noise(&n->rng);
}

/* follow — an envelope follower, nothing else: fast attack, release nature x 50 ms */
static void run_follow(node *n, const float *const in[MAX_IN])
{
	float s = n->s[0];
	const float up = scq_coef_time(0.002f, SR), down = scq_coef_time(0.05f * scq_max(n->nature, 0.1f), SR);
	LOOP {
		float a = scq_abs(in[0][i]);
		s += (a > s ? up : down) * (a - s);
		n->out[0][i] = s;
	}
	n->s[0] = s;
}

/* onepole — the gentlest lowpass: one pole at nature x 120 Hz, cutoff in octaves */
static void run_onepole(node *n, const float *const in[MAX_IN])
{
	float s = n->s[0];
	const float k = scq_coef_hz(120.f * n->nature * scq_exp2(in[1][0]), SR);
	LOOP {
		s += k * (in[0][i] - s);
		n->out[0][i] = s;
	}
	n->s[0] = s;
}

/* ---------------------------------------------------- lite: the heavy places, cut to the bone */

/* reverblite — two cross-fed lines instead of four, mono: a small cheap room */
static const uint16_t RL_LEN[2] = { 1601, 2273 };
static uint32_t mem_reverblite(float nature)
{
	float sc = 0.6f + 0.2f * scq_clamp(nature, 1.f, 4.f);
	return ((uint32_t)(RL_LEN[0] * sc) + (uint32_t)(RL_LEN[1] * sc)) * sizeof(int16_t);
}
static void run_reverblite(node *n, const float *const in[MAX_IN])
{
	float sc = 0.6f + 0.2f * scq_clamp(n->nature, 1.f, 4.f);
	uint32_t l0 = (uint32_t)(RL_LEN[0] * sc), l1 = (uint32_t)(RL_LEN[1] * sc);
	int16_t *a = (int16_t *)n->mem, *b = a + l0;
	uint32_t p0 = (uint32_t)n->s[0], p1 = (uint32_t)n->s[1];
	float d0 = n->s[2], d1 = n->s[3];
	const float g = 0.7f * (0.55f + 0.44f * scq_clamp(in[1][0], 0.f, 1.f));
	LOOP {
		d0 += 0.45f * (FROM16(a[p0]) - d0);
		d1 += 0.45f * (FROM16(b[p1]) - d1);
		float x = in[0][i] * 0.5f;
		a[p0] = to16(x + g * (d0 + d1) * 0.7071f);
		b[p1] = to16(g * (d0 - d1) * 0.7071f - x);
		if (++p0 >= l0) p0 = 0;
		if (++p1 >= l1) p1 = 0;
		n->out[0][i] = d0 + d1;
	}
	n->s[0] = (float)p0; n->s[1] = (float)p1; n->s[2] = d0; n->s[3] = d1;
}

/* choruslite — one voice, a triangle sweep, 16-bit memory */
#define CL_LEN 1024
static uint32_t mem_choruslite(float nature) { (void)nature; return CL_LEN * sizeof(int16_t); }
static void run_choruslite(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float ph = n->s[1];
	const float dph = 0.6f * n->nature * INV_SR;
	const float depth = 0.004f * SR * scq_clamp(in[1][0], 0.f, 1.f), mid = 0.01f * SR;
	LOOP {
		buf[w] = to16(in[0][i]);
		ph += dph; ph -= scq_floor(ph);
		float rp = (float)w - (mid + depth * (4.f * scq_abs(ph - 0.5f) - 1.f));
		if (rp < 0.f) rp += (float)CL_LEN;
		uint32_t r0 = (uint32_t)rp;
		float fr = rp - (float)r0;
		float y0 = FROM16(buf[r0 & (CL_LEN - 1)]), y1 = FROM16(buf[(r0 + 1) & (CL_LEN - 1)]);
		w = (w + 1) & (CL_LEN - 1);
		n->out[0][i] = 0.5f * (in[0][i] + y0 + (y1 - y0) * fr);
	}
	n->s[0] = (float)w; n->s[1] = ph;
}

/* delaylite — the delay with its time read once a block and no interpolation or saturation */
static void run_delaylite(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t L = n->memlen / sizeof(int16_t);
	uint32_t w = (uint32_t)n->s[0];
	uint32_t d = 1u + (uint32_t)(scq_clamp(in[1][0], 0.01f, 1.f) * (float)(L - 2));
	uint32_t r = w >= d ? w - d : w + L - d;
	const float fb = scq_clamp(in[2][0], 0.f, 0.95f);
	LOOP {
		float y = FROM16(buf[r]);
		buf[w] = to16(scq_clamp(in[0][i] + y * fb, -1.f, 1.f));
		if (++w >= L) w = 0;
		if (++r >= L) r = 0;
		n->out[0][i] = y;
	}
	n->s[0] = (float)w;
}

/* fmlite — two-operator FM, ratio and index read once a block, no feedback */
static void run_fmlite(node *n, const float *const in[MAX_IN])
{
	float pc = n->s[0], pm = n->s[1];
	const float dc = 240.f * scq_exp2(in[0][0]) * INV_SR;
	const float dm = dc * scq_max(n->nature + in[1][0], 0.f);
	const float index = scq_clamp(in[2][0], 0.f, 2.f);
	LOOP {
		pm += dm; pm -= scq_floor(pm);
		pc += dc; pc -= scq_floor(pc);
		n->out[0][i] = scq_sin2pi(pc + index * scq_sin2pi(pm));
	}
	n->s[0] = pc; n->s[1] = pm;
}

/* supersawlite — three detuned plain saws instead of seven band-limited ones */
static void run_supersawlite(node *n, const float *const in[MAX_IN])
{
	float p0 = n->s[0], p1 = n->s[1], p2 = n->s[2];
	const float dt = scq_min(60.f * n->nature * scq_exp2(in[0][0]) * INV_SR, 0.3f);
	const float det = 0.02f * scq_clamp(in[1][0], 0.f, 1.f);
	const float d1 = dt * (1.f + det), d2 = dt * (1.f - det);
	LOOP {
		p0 += dt; p0 -= scq_floor(p0);
		p1 += d1; p1 -= scq_floor(p1);
		p2 += d2; p2 -= scq_floor(p2);
		n->out[0][i] = (p0 + p1 + p2) * (2.f / 3.f) - 1.f;
	}
	n->s[0] = p0; n->s[1] = p1; n->s[2] = p2;
}

/* froglet — one frog: a falling chirp on its own timer or on a pulse; silent costs nothing */
static void run_froglet(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], fr = n->s[1], env = n->s[2], timer = n->s[3];
	int woke = 0;
	if (n->patched & 1u) { LOOP if (edge(in[0][i], &n->s[4])) woke = 1; }
	timer -= (float)B;
	if (timer <= 0.f || woke) {
		timer = (0.3f + 2.2f * scq_rand01(&n->rng)) * SR;
		fr = 60.f * n->nature * scq_exp2(in[1][0]) * INV_SR;
		env = 0.5f;
	}
	if (env < 0.0005f) { LOOP n->out[0][i] = 0.f; }
	else LOOP {
		ph += fr; ph -= scq_floor(ph);
		n->out[0][i] = env * scq_sin2pi(ph);
		env *= 0.9993f;
		fr *= 0.99998f;
	}
	n->s[0] = ph; n->s[1] = fr; n->s[2] = env; n->s[3] = timer;
}

/* difftone — two sines whose difference tones you hear (after Alex Chechile, "Practical
 * Applications of Difference Tones in Electronic Music Composition and Synthesis", 2020).
 * f1 = nature x 60 Hz (the tune knob and pitch move both), f2 = f1 x ratio (the ratio knob, plus
 * the ratio inlet). Equal levels, nothing nonlinear here: the ear makes f2-f1 (the quadratic
 * difference tone) and 2f1-f2 (the cubic, strongest near ratio 1.2) itself. Primaries around
 * 1-2 kHz and a healthy level bring them out. */
static void run_difftone(node *n, const float *const in[MAX_IN])
{
	float p1 = n->s[0], p2 = n->s[1];
	const float f1 = 60.f * n->nature * scq_exp2(in[0][0]);
	const float ratio = scq_clamp(n->opt[0] + in[1][0], 1.f, 3.f);
	const float d1 = scq_min(f1 * INV_SR, 0.45f), d2 = scq_min(f1 * ratio * INV_SR, 0.45f);
	LOOP {
		float a = scq_sin2pi(p1), b = scq_sin2pi(p2);
		n->out[1][i] = a;
		n->out[2][i] = b;
		n->out[0][i] = 0.5f * (a + b);
		p1 += d1; p1 -= scq_floor(p1);
		p2 += d2; p2 -= scq_floor(p2);
	}
	n->s[0] = p1; n->s[1] = p2;
}
