/*
 * More currents: envelopes, LFOs, clocks, rhythm, random and chaotic control, switching.
 * Included by scq_places.c. Times map 0..1 onto 1 ms..10 s, exponentially.
 */

static inline float time01(float x) { return 0.001f * scq_exp2(scq_clamp(x, 0.f, 1.f) * 13.3f); }

static inline float pulse_out(float *t) { float v = *t > 0.f ? 1.f : 0.f; if (*t > 0.f) *t -= 1.f; return v; }

/* sleep — falling asleep and waking: attack, decay, sustain, release (after ARP / Moog ADSR; Buchla 281) */
static void run_sleep(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], stage = n->s[1], et = n->s[3];
	float a = 1.f / (time01(in[1][0]) * SR), d = scq_coef_time(time01(in[2][0]), SR);
	float s = scq_clamp(in[3][0], 0.f, 1.f), r = scq_coef_time(time01(in[4][0]), SR);
	LOOP {
		int g = in[0][i] > 0.5f;
		if (edge(in[0][i], &n->s[2])) stage = 1.f;
		if (!g && stage > 0.f && stage < 4.f) stage = 4.f;
		if (stage == 1.f) { v += a; if (v >= 1.f) { v = 1.f; stage = 2.f; } }
		else if (stage == 2.f) { v += (s - v) * d; }
		else if (stage == 4.f) { v -= v * r; if (v < 0.0005f) { v = 0.f; stage = 0.f; et = PULSE_SAMPLES; } }
		n->out[0][i] = v;
		n->out[1][i] = pulse_out(&et);
	}
	n->s[0] = v; n->s[1] = stage; n->s[3] = et;
}

/* moon — the moon over the night: an LFO, four shapes, 0 to 1 (after Doepfer A-145; every modular's LFO) */
static void run_moon(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0];
	float f = 0.05f * n->nature * scq_exp2(scq_clamp(in[0][0], 0.f, 1.f) * 10.f);
	LOOP {
		if (edge(in[1][i], &n->s[1])) ph = 0.f;
		ph += f * INV_SR; ph -= scq_floor(ph);
		n->out[0][i] = 0.5f + 0.5f * scq_sin2pi(ph);
		n->out[1][i] = 1.f - 2.f * scq_abs(ph - 0.5f);
		n->out[2][i] = ph;
		n->out[3][i] = ph < 0.5f ? 1.f : 0.f;
	}
	n->s[0] = ph;
}

/* tram — the aboveground tram: a clock, with eighths, halves and bars (after Pamela's / Tempi / every master clock) */
static void run_tram(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], t[4] = { n->s[1], n->s[2], n->s[3], n->s[4] };
	float count = n->s[5];
	float bpm = 40.f * scq_exp2(scq_clamp(in[0][0], 0.f, 1.f) * 3.f);
	int bar = natural(n->nature, 1, 16);
	LOOP {
		if (edge(in[1][i], &n->s[6])) { ph = 0.f; count = 0.f; }
		float o = ph;
		ph += bpm / 60.f * 2.f * INV_SR;                  /* in eighths */
		if (ph >= 1.f) {
			ph -= 1.f;
			int c = (int)count;
			t[1] = PULSE_SAMPLES;                          /* every eighth */
			if ((c & 1) == 0) t[0] = PULSE_SAMPLES;        /* every beat   */
			if ((c & 3) == 0) t[2] = PULSE_SAMPLES;        /* every half   */
			if (c % (bar * 2) == 0) t[3] = PULSE_SAMPLES;  /* every bar    */
			count = (float)((c + 1) % (bar * 4));
		}
		(void)o;
		for (int k = 0; k < 4; k++) n->out[k][i] = pulse_out(&t[k]);
	}
	n->s[0] = ph; n->s[1] = t[0]; n->s[2] = t[1]; n->s[3] = t[2]; n->s[4] = t[3]; n->s[5] = count;
}
static void init_tram(node *n) { n->s[0] = 0.999f; }

/* floors — the elevator's floors: divides a clock by 2, 3, 4 and its nature (after Doepfer A-160; Make Noise Tempi) */
static void run_floors(node *n, const float *const in[MAX_IN])
{
	int cnt = (int)n->s[0];
	float t[4] = { n->s[1], n->s[2], n->s[3], n->s[4] };
	const int div[4] = { 2, 3, 4, natural(n->nature, 2, 64) };
	LOOP {
		if (edge(in[1][i], &n->s[6])) cnt = 0;
		if (edge(in[0][i], &n->s[5])) {
			for (int k = 0; k < 4; k++) if (cnt % div[k] == 0) t[k] = PULSE_SAMPLES;
			cnt = (cnt + 1) % (2 * 3 * 4 * 64);
		}
		for (int k = 0; k < 4; k++) n->out[k][i] = pulse_out(&t[k]);
	}
	n->s[0] = (float)cnt; n->s[1] = t[0]; n->s[2] = t[1]; n->s[3] = t[2]; n->s[4] = t[3];
}

/* maze — the far underground maze: Euclidean rhythms, hits spread as evenly as they'll go (after Toussaint; Euclidean modules) */
static void run_maze(node *n, const float *const in[MAX_IN])
{
	int steps = natural(n->nature, 2, 32);
	int step = (int)n->s[0];
	float th = n->s[3], tm = n->s[4];
	LOOP {
		if (edge(in[3][i], &n->s[2])) step = 0;
		if (edge(in[0][i], &n->s[1])) {
			int hits = (int)(scq_clamp(in[1][i], 0.f, 1.f) * (float)steps + 0.5f);
			int rot = (int)(scq_clamp(in[2][i], 0.f, 1.f) * (float)steps);
			int s = (step + rot) % steps;
			int on = hits > 0 && ((s * hits) % steps) < hits;   /* Bjorklund, the short way */
			if (on) th = PULSE_SAMPLES; else tm = PULSE_SAMPLES;
			step = (step + 1) % steps;
		}
		n->out[0][i] = pulse_out(&th);
		n->out[1][i] = pulse_out(&tm);
		n->out[2][i] = (float)step / (float)steps;
	}
	n->s[0] = (float)step; n->s[3] = th; n->s[4] = tm;
}

/* crossroads — tight turns on the estate: each pulse goes one way or the other (after Mutable Branches; Bernoulli gates) */
static void run_crossroads(node *n, const float *const in[MAX_IN])
{
	int way = (int)n->s[1];
	LOOP {
		if (edge(in[0][i], &n->s[0])) way = scq_rand01(&n->rng) < scq_clamp(in[1][i], 0.f, 1.f) ? 1 : 0;
		float g = in[0][i] > 0.5f ? 1.f : 0.f;
		n->out[0][i] = way ? 0.f : g;
		n->out[1][i] = way ? g : 0.f;
	}
	n->s[1] = (float)way;
}

/* notebook — the pink notebook: a melody that writes itself and slowly rewrites (after Music Thing's Turing Machine) */
static void run_notebook(node *n, const float *const in[MAX_IN])
{
	uint32_t reg = (uint32_t)n->s[1];
	int len = natural(n->nature, 2, 16);
	float v = n->s[2], pt = n->s[3];
	LOOP {
		if (edge(in[0][i], &n->s[0])) {
			uint32_t out = (reg >> (len - 1)) & 1u;
			if (scq_rand01(&n->rng) < scq_clamp(in[1][i], 0.f, 1.f)) out ^= 1u;
			reg = ((reg << 1) | out) & 0xffffu;
			v = (float)(reg & 0xffu) / 255.f;
			if (reg & 1u) pt = PULSE_SAMPLES;
		}
		n->out[0][i] = v;
		n->out[1][i] = pulse_out(&pt);
	}
	n->s[1] = (float)reg; n->s[2] = v; n->s[3] = pt;
}
static void init_notebook(node *n) { n->s[1] = (float)(n->rng & 0xffffu); }

/* party — the house party you missed: kick, snare and hat patterns on a map (after Mutable Grids) */
static const uint8_t PARTY[4][3][16] = {
	/* four on the floor */
	{ { 255,0,0,0, 255,0,0,0, 255,0,0,0, 255,0,0,40 },
	  { 0,0,0,0, 255,0,0,0, 0,0,0,0, 255,0,0,0 },
	  { 0,0,200,0, 0,0,200,0, 0,0,200,0, 0,0,200,90 } },
	/* broken beat */
	{ { 255,0,0,120, 0,0,200,0, 0,160,255,0, 0,0,90,0 },
	  { 0,0,0,0, 255,0,0,80, 0,0,0,0, 255,0,60,0 },
	  { 200,60,150,60, 200,60,150,60, 200,60,150,60, 200,60,150,110 } },
	/* dub, sparse */
	{ { 255,0,0,0, 0,0,0,0, 0,0,180,0, 0,0,0,0 },
	  { 0,0,0,0, 0,0,0,0, 255,0,0,0, 0,0,0,0 },
	  { 0,0,120,0, 0,0,120,0, 0,0,120,0, 0,0,120,0 } },
	/* jungle, busy */
	{ { 255,0,0,0, 0,0,0,0, 0,0,255,0, 0,0,0,0 },
	  { 0,0,0,0, 255,0,0,130, 0,110,0,0, 255,0,0,90 },
	  { 230,120,230,120, 230,120,230,190, 230,120,230,120, 230,170,230,210 } },
};
static void run_party(node *n, const float *const in[MAX_IN])
{
	int step = (int)n->s[0];
	float t[4] = { n->s[2], n->s[3], n->s[4], n->s[5] };
	LOOP {
		if (edge(in[4][i], &n->s[6])) step = 0;
		if (edge(in[0][i], &n->s[1])) {
			float x = scq_clamp(in[1][i], 0.f, 1.f), y = scq_clamp(in[2][i], 0.f, 1.f);
			float dens = scq_clamp(in[3][i], 0.f, 1.f);
			int accent = 0;
			for (int lane = 0; lane < 3; lane++) {
				float a = PARTY[0][lane][step], b = PARTY[1][lane][step], c = PARTY[2][lane][step], d = PARTY[3][lane][step];
				float lvl = (a * (1.f - x) + b * x) * (1.f - y) + (c * (1.f - x) + d * x) * y;
				if (lvl > (1.f - dens) * 255.f) { t[lane] = PULSE_SAMPLES; if (lvl > 200.f) accent = 1; }
			}
			if (accent) t[3] = PULSE_SAMPLES;
			step = (step + 1) & 15;
		}
		for (int k = 0; k < 4; k++) n->out[k][i] = pulse_out(&t[k]);
	}
	n->s[0] = (float)step; n->s[2] = t[0]; n->s[3] = t[1]; n->s[4] = t[2]; n->s[5] = t[3];
}

/* fire — the hall on fire: a burst of pulses from one (after burst generators; Nord Modular ratchets) */
static void run_fire(node *n, const float *const in[MAX_IN])
{
	float left = n->s[1], gap = n->s[2], t = n->s[3];
	LOOP {
		if (edge(in[0][i], &n->s[0])) { left = 1.f + scq_floor(scq_clamp(in[1][i], 0.f, 1.f) * 15.f); gap = 0.f; }
		if (left > 0.f) {
			gap -= 1.f;
			if (gap <= 0.f) {
				t = PULSE_SAMPLES;
				left -= 1.f;
				gap = (0.01f + 0.2f * scq_clamp(in[2][i], 0.f, 1.f)) * SR;
			}
		}
		n->out[0][i] = pulse_out(&t);
	}
	n->s[1] = left; n->s[2] = gap; n->s[3] = t;
}

/* insomniac — lying awake in a continuous stasis: the logistic map, from order to chaos (after May's logistic map) */
static void run_insomniac(node *n, const float *const in[MAX_IN])
{
	float x = n->s[0], ph = n->s[1], pt = n->s[3];
	int clocked = n->patched & 1;
	if (x <= 0.f || x >= 1.f) x = 0.37f;
	LOOP {
		int step;
		if (clocked) step = edge(in[0][i], &n->s[2]);
		else { ph += n->nature * 2.f * INV_SR; step = ph >= 1.f; if (step) ph -= 1.f; }
		if (step) {
			float r = 3.4f + 0.6f * scq_clamp(in[1][i], 0.f, 1.f);
			x = r * x * (1.f - x);
			if (x > 0.5f) pt = PULSE_SAMPLES;
		}
		n->out[0][i] = x;
		n->out[1][i] = pulse_out(&pt);
	}
	n->s[0] = x; n->s[1] = ph; n->s[3] = pt;
}

/* hourglass — sand running through: a ramp from 0 to 1, at any speed up to audio (after Max/MSP and Pd's phasor~) */
static void run_hourglass(node *n, const float *const in[MAX_IN])
{
	float ph = n->s[0], pt = n->s[2];
	const float inc = 0.01f * n->nature * scq_exp2(scq_clamp(in[0][0], 0.f, 1.f) * 17.f) * INV_SR;
	LOOP {
		if (edge(in[1][i], &n->s[1])) ph = 0.f;
		ph += inc;
		if (ph >= 1.f) { ph -= scq_floor(ph); pt = PULSE_SAMPLES; }
		n->out[0][i] = ph;
		n->out[1][i] = pulse_out(&pt);
	}
	n->s[0] = ph; n->s[2] = pt;
}

/* waitingroom — waiting to be called: each pulse, again, later (after Doepfer A-162 trigger delays) */
static void run_waitingroom(node *n, const float *const in[MAX_IN])
{
	float due = n->s[1], t = n->s[2];
	LOOP {
		if (edge(in[0][i], &n->s[0])) due = time01(in[1][i]) * SR;
		if (due > 0.f) { due -= 1.f; if (due <= 0.f) t = PULSE_SAMPLES; }
		n->out[0][i] = pulse_out(&t);
	}
	n->s[1] = due; n->s[2] = t;
}

/* concierge — the hotel concierge: four things in, one out, stepping on each pulse (after Doepfer A-151; Buchla sequential switches) */
static void run_concierge(node *n, const float *const in[MAX_IN])
{
	int at = (int)n->s[0];
	int count = natural(n->nature, 2, 4);
	LOOP {
		if (edge(in[5][i], &n->s[2])) at = 0;
		if (edge(in[4][i], &n->s[1])) at = (at + 1) % count;
		n->out[0][i] = in[at][i];
		n->out[1][i] = (float)at / (float)(count - 1);
	}
	n->s[0] = (float)at;
}

/* lilypads — lily pads on a flooded street, a grid that grows by rule: a cellular automaton (after Wolfram's rules; CA sequencers) */
static void run_lilypads(node *n, const float *const in[MAX_IN])
{
	uint32_t cells = (uint32_t)n->s[1];
	int rule = natural(n->nature, 0, 255);
	float pt = n->s[3];
	LOOP {
		if (edge(in[1][i], &n->s[2])) cells = scq_rng(&n->rng) & 0xffffu;
		if (edge(in[0][i], &n->s[0])) {
			uint32_t next = 0;
			for (int c = 0; c < 16; c++) {
				uint32_t l = (cells >> ((c + 1) & 15)) & 1u, m = (cells >> c) & 1u, r = (cells >> ((c + 15) & 15)) & 1u;
				next |= (uint32_t)((rule >> ((l << 2) | (m << 1) | r)) & 1) << c;
			}
			if (next == 0) next = 1u << 8;                  /* never let the pond die */
			cells = next;
			if (cells & (1u << 8)) pt = PULSE_SAMPLES;    /* the middle pad */
		}
		int alive = 0;
		for (int c = 0; c < 16; c++) alive += (int)((cells >> c) & 1u);
		n->out[0][i] = (float)alive / 16.f;
		n->out[1][i] = pulse_out(&pt);
		n->out[2][i] = (float)(cells & 0xffu) / 255.f;
	}
	n->s[1] = (float)cells; n->s[3] = pt;
}
static void init_lilypads(node *n) { n->s[1] = (float)(1u << 8); }

/* wander — wandering the empty school: a random walk that turns back at the walls (after the Wogglebug; Buchla 266's stored random) */
static void run_wander(node *n, const float *const in[MAX_IN])
{
	float v = n->s[0], ph = n->s[1], target = n->s[3];
	int clocked = n->patched & 2;
	LOOP {
		int step;
		if (clocked) step = edge(in[1][i], &n->s[2]);
		else { ph += n->nature * 8.f * INV_SR; step = ph >= 1.f; if (step) ph -= 1.f; }
		if (step) {
			target += scq_noise(&n->rng) * scq_clamp(in[0][i], 0.f, 1.f) * 0.5f;
			if (target > 1.f) target = 2.f - target;
			if (target < 0.f) target = -target;
		}
		v += (target - v) * 0.002f;
		n->out[0][i] = v;
	}
	n->s[0] = v; n->s[1] = ph; n->s[3] = target;
}
static void init_wander(node *n) { n->s[0] = n->s[3] = 0.5f; }
