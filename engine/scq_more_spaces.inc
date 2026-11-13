/*
 * More spaces: reverbs, echoes, freezing, sympathetic strings, pitch shifting.
 * Included by scq_places.c. Big buffers are 16-bit to fit the SP-1's 80 KB.
 */

/* a fractional read from a 16-bit ring */
static inline float ring16(const int16_t *b, uint32_t len, uint32_t w, float d)
{
	float rp = (float)w - d;
	while (rp < 0.f) rp += (float)len;
	uint32_t r0 = (uint32_t)rp, r1 = r0 + 1 < len ? r0 + 1 : 0;
	float fr = rp - (float)r0;
	return FROM16(b[r0]) + (FROM16(b[r1]) - FROM16(b[r0])) * fr;
}

/* spring — the cabin in the woods: a spring reverb, boing and all (after Accutronics springs in the Buchla, VCS3 and ARP 2600) */
#define CABIN_LEN 2400
static uint32_t mem_spring(float nature) { (void)nature; return CABIN_LEN * sizeof(float) + 12 * sizeof(float); }
static void run_spring(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	float *ap = buf + CABIN_LEN;        /* 12 allpass states: the spring's dispersion */
	uint32_t w = (uint32_t)n->s[0];
	float lp = n->s[1];
	float fb = 0.5f + 0.45f * scq_clamp(in[1][0], 0.f, 1.f);
	float d = (0.025f + 0.01f * scq_clamp(n->nature, 1.f, 4.f)) * SR;
	LOOP {
		float y = buf[(uint32_t)((float)w + (float)CABIN_LEN - d) % CABIN_LEN];
		float x = in[0][i] * 0.5f + y * fb;
		for (int k = 0; k < 12; k++) {           /* a chirp: highs arrive before lows */
			const float a = 0.62f;
			float o = -a * x + ap[k];
			ap[k] = x + a * o;
			x = o;
		}
		lp += 0.4f * (x - lp);
		buf[w] = scq_tanh(lp);
		w = (w + 1) % CABIN_LEN;
		n->out[0][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = lp;
}

/* plate — the empty hotel: a plate reverb, two figure-eight tanks (after Jon Dattorro's plate; Lexicon / EMT 140) */
static const uint16_t HOTEL_LEN[12] = { 80, 60, 214, 156,  379, 2512, 1016, 2099,  512, 2379, 1499, 1784 };
static uint32_t mem_plate(float nature)
{
	(void)nature;
	uint32_t t = 0;
	for (int k = 0; k < 12; k++) t += HOTEL_LEN[k];
	return t * sizeof(int16_t);
}
static void run_plate(node *n, const float *const in[MAX_IN])
{
	int16_t *base = (int16_t *)n->mem;
	int16_t *line[12];
	uint32_t off = 0;
	for (int k = 0; k < 12; k++) { line[k] = base + off; off += HOTEL_LEN[k]; }
	uint32_t w = (uint32_t)n->s[0];
	float bw = n->s[1], dl = n->s[2], dr = n->s[3], feedL = n->s[4], feedR = n->s[5];
	float decay = 0.4f + 0.55f * scq_clamp(in[1][0], 0.f, 1.f);
	float damp = 0.1f + 0.6f * scq_clamp(in[2][0], 0.f, 1.f);
#define TAP(k)   FROM16(line[k][w % HOTEL_LEN[k]])
#define PUT(k,v) line[k][w % HOTEL_LEN[k]] = to16(v)
	LOOP {
		bw += 0.7f * (in[0][i] * 0.5f - bw);
		float x = bw;
		static const float dc[4] = { 0.75f, 0.75f, 0.625f, 0.625f };
		for (int k = 0; k < 4; k++) {             /* input diffusion */
			float z = TAP(k);
			float v = x - dc[k] * z;
			PUT(k, v);
			x = z + dc[k] * v;
		}
		/* left tank: allpass, delay, damp, allpass, delay */
		float a = x + feedR * decay;
		float z = TAP(4); float v = a + 0.7f * z; PUT(4, v); a = z - 0.7f * v;
		float d1 = TAP(5); PUT(5, a);
		dl += (1.f - damp) * (d1 - dl);
		a = dl * decay;
		z = TAP(6); v = a - 0.5f * z; PUT(6, v); a = z + 0.5f * v;
		float d2 = TAP(7); PUT(7, a);
		feedL = d2;
		/* right tank */
		float b = x + feedL * decay;
		z = TAP(8); v = b + 0.7f * z; PUT(8, v); b = z - 0.7f * v;
		float d3 = TAP(9); PUT(9, b);
		dr += (1.f - damp) * (d3 - dr);
		b = dr * decay;
		z = TAP(10); v = b - 0.5f * z; PUT(10, v); b = z + 0.5f * v;
		float d4 = TAP(11); PUT(11, b);
		feedR = d4;
		n->out[1][i] = 0.6f * (d1 + d2 - d3 * 0.5f);
		n->out[2][i] = 0.6f * (d3 + d4 - d1 * 0.5f);
		n->out[0][i] = 0.5f * (n->out[1][i] + n->out[2][i]);
		w++;
		if (w >= 1u << 30) w = 0;
	}
#undef TAP
#undef PUT
	n->s[0] = (float)w; n->s[1] = bw; n->s[2] = dl; n->s[3] = dr; n->s[4] = feedL; n->s[5] = feedR;
}

/* pingpong — a party on the balcony: echoes that cross from ear to ear (after ping-pong delays) */
static uint32_t mem_pingpong(float nature) { return 2u * (uint32_t)natural(nature, 1, 12) * 1200u * sizeof(int16_t); }
static void run_pingpong(node *n, const float *const in[MAX_IN])
{
	uint32_t L = n->memlen / sizeof(int16_t) / 2;
	int16_t *a = (int16_t *)n->mem, *b = a + L;
	uint32_t w = (uint32_t)n->s[0];
	float len = n->s[1];
	LOOP {
		len += (scq_clamp(in[1][i], 0.02f, 1.f) - len) * 0.0003f;
		float d = 1.f + len * (float)(L - 3);
		float ya = ring16(a, L, w, d), yb = ring16(b, L, w, d);
		float fb = scq_clamp(in[2][i], 0.f, 0.95f);
		a[w] = to16(scq_tanh(in[0][i] + yb * fb));
		b[w] = to16(scq_tanh(ya * fb));
		if (++w >= L) w = 0;
		n->out[0][i] = ya;
		n->out[1][i] = yb;
	}
	n->s[0] = (float)w; n->s[1] = len;
}
static void init_pingpong(node *n) { n->s[1] = 1.f; }

/* tapeecho — dub echoes of the steaming train horn: a tape echo, warm and wobbling (after Roland RE-201 Space Echo; Echoplex; Serge Wilson delay) */
static uint32_t mem_tapeecho(float nature) { return (uint32_t)natural(nature, 1, 8) * 4800u * sizeof(int16_t); }
static void run_tapeecho(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t L = n->memlen / sizeof(int16_t);
	uint32_t w = (uint32_t)n->s[0];
	float len = n->s[1], wow = n->s[2], flut = n->s[3], lp = n->s[4], hp = n->s[5];
	LOOP {
		len += (scq_clamp(in[1][i], 0.05f, 1.f) - len) * 0.00008f;   /* tape speeds up slowly */
		wow += 0.5f * INV_SR; wow -= scq_floor(wow);
		flut += 6.3f * INV_SR; flut -= scq_floor(flut);
		float wob = scq_clamp(in[3][i], 0.f, 1.f);
		float d = 2.f + len * (float)(L - 40) * (1.f + wob * (0.004f * scq_sin2pi(wow) + 0.0008f * scq_sin2pi(flut)));
		float y = ring16(buf, L, w, d);
		lp += 0.3f * (y - lp);
		hp += 0.004f * (lp - hp);
		float fb = scq_clamp(in[2][i], 0.f, 1.05f);
		buf[w] = to16(scq_tanh(in[0][i] + (lp - hp) * fb));
		if (++w >= L) w = 0;
		n->out[0][i] = in[0][i] + y;
		n->out[1][i] = y;
	}
	n->s[0] = (float)w; n->s[1] = len; n->s[2] = wow; n->s[3] = flut; n->s[4] = lp; n->s[5] = hp;
}
static void init_tapeecho(node *n) { n->s[1] = 0.6f; }

/* freeze — the rooms are memories: freeze the last moment and live inside it (after Mutable Clouds' freeze; looping delays) */
static uint32_t mem_freeze(float nature) { return (uint32_t)natural(nature, 1, 6) * 12000u * sizeof(int16_t); }
static void run_freeze(node *n, const float *const in[MAX_IN])
{
	int16_t *buf = (int16_t *)n->mem;
	uint32_t L = n->memlen / sizeof(int16_t);
	uint32_t w = (uint32_t)n->s[0];
	int frozen = in[1][0] > 0.5f;
	if (frozen && n->s[2] == 0.f) { n->s[1] = (float)w; n->s[3] = 0.f; }   /* the moment you hold it */
	n->s[2] = frozen ? 1.f : 0.f;
	float span = scq_floor(scq_clamp(in[2][0], 0.02f, 1.f) * (float)(L - 1));
	uint32_t start = ((uint32_t)n->s[1] + L - (uint32_t)span) % L;    /* the last `span` before the freeze */
	LOOP {
		if (!frozen) {
			buf[w] = to16(in[0][i]);
			if (++w >= L) w = 0;
			n->out[0][i] = in[0][i];
			continue;
		}
		float k = n->s[3];
		if (k >= span) k = n->s[3] = 0.f;
		float fade = scq_clamp(scq_min(k, span - k) / 480.f, 0.f, 1.f);   /* no click at the seam */
		n->out[0][i] = FROM16(buf[(start + (uint32_t)k) % L]) * fade;
		n->s[3] = k + 1.f;
	}
	n->s[0] = (float)w;
}

/* sympathetic — copper wiring in the plexiglass sculpture: four sympathetic strings tuned to a chord (after Mutable Rings; Fish's zither; sitar drones) */
#define WIRE_LEN 1024
static uint32_t mem_sympathetic(float nature) { (void)nature; return 4 * WIRE_LEN * sizeof(float); }
static void run_sympathetic(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float f0 = 60.f * n->nature * scq_exp2(in[1][0]);
	float fb = 0.9f + 0.0995f * scq_clamp(in[2][0], 0.f, 1.f);
	int c = (int)(scq_clamp(in[3][0], 0.f, 0.999f) * 6.f);
	float per[4];
	for (int k = 0; k < 4; k++) per[k] = scq_clamp(SR / (f0 * CHORDS[c][k]), 2.f, (float)(WIRE_LEN - 2));
	LOOP {
		float sum = 0.f;
		for (int k = 0; k < 4; k++) {
			float *b = buf + k * WIRE_LEN;
			float rp = (float)w - per[k];
			if (rp < 0.f) rp += (float)WIRE_LEN;
			uint32_t r0 = (uint32_t)rp;
			float fr = rp - (float)r0;
			float y = b[r0 & (WIRE_LEN - 1)] + (b[(r0 + 1) & (WIRE_LEN - 1)] - b[r0 & (WIRE_LEN - 1)]) * fr;
			n->s[1 + k] += 0.5f * (y - n->s[1 + k]);
			b[w] = scq_softclip(in[0][i] * 0.15f + n->s[1 + k] * fb);
			sum += y;
		}
		w = (w + 1) & (WIRE_LEN - 1);
		n->out[0][i] = sum * 0.35f;
	}
	n->s[0] = (float)w;
}

/* pitchshift — a morphing persona: shifts pitch without changing time (after Eventide's H910; granular pitch shifters) */
#define PERSONA_LEN 2048
static uint32_t mem_pitchshift(float nature) { (void)nature; return PERSONA_LEN * sizeof(float); }
static void run_pitchshift(node *n, const float *const in[MAX_IN])
{
	float *buf = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float ph = n->s[1];
	const float win = 1500.f;
	LOOP {
		float ratio = scq_exp2(scq_clamp(in[1][i], -2.f, 2.f));
		buf[w] = in[0][i];
		ph += (1.f - ratio) / win;
		ph -= scq_floor(ph);
		float o = 0.f;
		for (int k = 0; k < 2; k++) {
			float p = ph + 0.5f * (float)k;
			p -= scq_floor(p);
			float d = 4.f + p * win;
			float rp = (float)w - d;
			if (rp < 0.f) rp += (float)PERSONA_LEN;
			uint32_t r0 = (uint32_t)rp;
			float fr = rp - (float)r0;
			float y = buf[r0 & (PERSONA_LEN - 1)] + (buf[(r0 + 1) & (PERSONA_LEN - 1)] - buf[r0 & (PERSONA_LEN - 1)]) * fr;
			float e = scq_sin2pi(p * 0.5f);
			o += y * e * e;
		}
		w = (w + 1) & (PERSONA_LEN - 1);
		n->out[0][i] = o;
	}
	n->s[0] = (float)w; n->s[1] = ph;
}

/* diffuser — steam off the train: smears a sound in time without echo (after allpass diffusers; Schroeder) */
static const uint16_t STEAM_LEN[4] = { 229, 172, 611, 446 };
static uint32_t mem_diffuser(float nature) { (void)nature; return (229 + 172 + 611 + 446) * sizeof(float); }
static void run_diffuser(node *n, const float *const in[MAX_IN])
{
	float *base = (float *)n->mem;
	uint32_t w = (uint32_t)n->s[0];
	float g = 0.3f + 0.45f * scq_clamp(in[1][0], 0.f, 1.f);
	LOOP {
		float x = in[0][i];
		float *b = base;
		for (int k = 0; k < 4; k++) {
			uint32_t p = w % STEAM_LEN[k];
			float z = b[p];
			float v = x + g * z;
			b[p] = v;
			x = z - g * v;
			b += STEAM_LEN[k];
		}
		w++;
		if (w >= 1u << 30) w = 0;
		n->out[0][i] = x;
	}
	n->s[0] = (float)w;
}
