/* MIDI to CV: the dream hears MIDI (USB on the SP-1 — from a computer, or from a TRS/DIN
 * keyboard through a USB-MIDI host box — or any MIDI input in the editor).
 *
 * Bytes go in through scq_midi() from the same thread that renders (the SP-1's audio thread,
 * the editor's audio callback under its lock), so nothing here is shared mid-update.
 *
 * Three places, like three modules (a place has four outlets at most):
 *   midi       pitch gate velocity trig     channel=0 (0 = any) voice=0 (0 = mono, last note;
 *                                           1-4 = one voice of four, for chords)
 *   midicc     out                          channel=0 cc=1 (0-127; 128 = pitch bend,
 *                                           129 = aftertouch)
 *   midiclock  clock reset run              div=6 (MIDI clock ticks per pulse: 24 = quarter
 *                                           notes, 6 = sixteenths)
 * Pitch is octaves from middle C (note 60 = 0, 72 = 1), so it goes straight into any pitch
 * inlet; tune= or a quantizer does the rest.
 */

#define MIDI_VOICES 4
#define MIDI_STACK  8

static struct {
	uint8_t  stat, d0, nd;                     /* the message being read (running status) */
	/* per channel; [16] is "any channel" */
	uint8_t  stack[17][MIDI_STACK], ns[17];    /* held notes, oldest first (mono: the last) */
	uint8_t  vel[17];
	uint32_t ons[17];                          /* note-ons so far: a new one retriggers */
	uint8_t  vnote[17][MIDI_VOICES], vvel[17][MIDI_VOICES], vgate[17][MIDI_VOICES];
	uint32_t vons[17][MIDI_VOICES], vage[17][MIDI_VOICES], age[17];
	uint8_t  cc[17][130];                      /* 0-127 CCs, 128 bend (hi 7 bits), 129 aftertouch */
	uint32_t ticks, starts;                    /* clock ticks and starts so far */
	uint8_t  running;
} M;

static void midi_note(int ch, int note, int vel)
{
	for (int pass = 0; pass < 2; pass++) {
		int c = pass ? 16 : ch;
		/* mono: the stack of held notes */
		int n = M.ns[c], k = 0;
		for (int i = 0; i < n; i++) if (M.stack[c][i] != note) M.stack[c][k++] = M.stack[c][i];
		n = k;
		if (vel) {
			if (n == MIDI_STACK) { for (int i = 1; i < n; i++) M.stack[c][i - 1] = M.stack[c][i]; n--; }
			M.stack[c][n++] = (uint8_t)note;
			M.vel[c] = (uint8_t)vel;
			M.ons[c]++;
		}
		M.ns[c] = (uint8_t)n;
		/* poly: four voices, a free one (else the oldest) takes the note */
		if (vel) {
			int v = -1;
			for (int i = 0; i < MIDI_VOICES && v < 0; i++) if (!M.vgate[c][i]) v = i;
			if (v < 0) { v = 0; for (int i = 1; i < MIDI_VOICES; i++) if (M.vage[c][i] < M.vage[c][v]) v = i; }
			M.vnote[c][v] = (uint8_t)note; M.vvel[c][v] = (uint8_t)vel; M.vgate[c][v] = 1;
			M.vons[c][v]++; M.vage[c][v] = ++M.age[c];
		} else {
			for (int i = 0; i < MIDI_VOICES; i++) if (M.vgate[c][i] && M.vnote[c][i] == note) M.vgate[c][i] = 0;
		}
	}
}

static void midi_message(int st, int d0, int d1)
{
	int ch = st & 15;
	switch (st & 0xF0) {
	case 0x90: midi_note(ch, d0, d1); break;
	case 0x80: midi_note(ch, d0, 0); break;
	case 0xB0:
		M.cc[ch][d0] = M.cc[16][d0] = (uint8_t)d1;
		if (d0 == 123 || d0 == 120) {                           /* all notes / sound off */
			for (int c = 0; c < 17; c++) { M.ns[c] = 0; for (int i = 0; i < MIDI_VOICES; i++) M.vgate[c][i] = 0; }
		}
		break;
	case 0xE0: M.cc[ch][128] = M.cc[16][128] = (uint8_t)d1; break;
	case 0xD0: M.cc[ch][129] = M.cc[16][129] = (uint8_t)d0; break;
	case 0xA0: M.cc[ch][129] = M.cc[16][129] = (uint8_t)d1; break;  /* poly pressure: as aftertouch */
	}
}

void scq_midi(const uint8_t *bytes, int len)
{
	for (int i = 0; i < len; i++) {
		uint8_t b = bytes[i];
		if (b >= 0xF8) {                                          /* real time: any moment */
			if (b == 0xF8) M.ticks++;
			else if (b == 0xFA) { M.starts++; M.running = 1; }
			else if (b == 0xFB) M.running = 1;
			else if (b == 0xFC) M.running = 0;
			continue;
		}
		if (b & 0x80) { M.stat = b >= 0xF0 ? 0 : b; M.nd = 0; continue; }   /* sysex etc: ignored */
		if (!M.stat) continue;
		int need = ((M.stat & 0xF0) == 0xC0 || (M.stat & 0xF0) == 0xD0) ? 1 : 2;
		if (M.nd == 0 && need == 2) { M.d0 = b; M.nd = 1; continue; }
		midi_message(M.stat, need == 2 ? M.d0 : b, need == 2 ? b : 0);
		M.nd = 0;
	}
}

static int midi_ch(const node *n) { int c = (int)n->opt[0]; return c >= 1 && c <= 16 ? c - 1 : 16; }

/* midi: pitch gate velocity trig.  s[0] note-ons seen, s[1] trig left, s[2] last pitch, s[3] started */
static void run_midi(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int c = midi_ch(n), v = (int)n->opt[1];
	float pitch = n->s[2], gate, vel;
	uint32_t ons;
	if (v >= 1 && v <= MIDI_VOICES) {
		v--;
		gate = M.vgate[c][v] ? 1.f : 0.f;
		vel = M.vvel[c][v] * (1.f / 127.f);
		ons = M.vons[c][v];
		if (M.vons[c][v]) pitch = ((float)M.vnote[c][v] - 60.f) * (1.f / 12.f);
	} else {
		gate = M.ns[c] ? 1.f : 0.f;
		vel = M.vel[c] * (1.f / 127.f);
		ons = M.ons[c];
		if (M.ns[c]) pitch = ((float)M.stack[c][M.ns[c] - 1] - 60.f) * (1.f / 12.f);
	}
	float trig = n->s[1];
	if (n->s[3] == 0.f) { n->s[3] = 1.f; n->s[0] = (float)(ons & 0xFFFFFF); }   /* first block */
	if ((float)(ons & 0xFFFFFF) != n->s[0]) { n->s[0] = (float)(ons & 0xFFFFFF); trig = PULSE_SAMPLES; }
	n->s[2] = pitch;
	LOOP {
		n->out[0][i] = pitch;
		n->out[1][i] = gate;
		n->out[2][i] = vel;
		n->out[3][i] = trig > 0.f ? 1.f : 0.f;
		if (trig > 0.f) trig -= 1.f;
	}
	n->s[1] = trig;
}

/* midicc: one controller as 0..1 (bend: -1..1).  s[0] the smoothed value */
static void run_midicc(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int c = midi_ch(n), k = (int)n->opt[1];
	if (k < 0) k = 0;
	if (k > 129) k = 129;
	float t = k == 128 ? (M.cc[c][128] - 64.f) * (1.f / 64.f) : M.cc[c][k] * (1.f / 127.f);
	if (k == 128 && M.cc[c][128] == 0 && n->s[1] == 0.f) t = 0.f;   /* bend not seen yet: centre */
	if (k == 128 && M.cc[c][128]) n->s[1] = 1.f;
	float v = n->s[0];
	LOOP { v += (t - v) * 0.004f; n->out[0][i] = v; }                    /* as a fader: no zipper */
	n->s[0] = v;
}

/* midiclock: clock reset run.  s[0] ticks seen, s[1] clock left, s[2] starts seen, s[3] reset left, s[4] started */
static void run_midiclock(node *n, const float *const in[MAX_IN])
{
	(void)in;
	int div = (int)n->opt[0];
	if (div < 1) div = 1;
	float ck = n->s[1], rs = n->s[3];
	const uint32_t now = M.ticks & 0xFFFFFFu, starts = M.starts & 0xFFFFFFu;
	if (n->s[4] == 0.f) {                                 /* first block: join where MIDI is */
		n->s[4] = 1.f; n->s[0] = (float)now; n->s[2] = (float)starts;
	}
	uint32_t seen = (uint32_t)n->s[0];
	if ((float)starts != n->s[2]) {
		n->s[2] = (float)starts;
		rs = PULSE_SAMPLES;
		seen = now;                                       /* count from the start */
		ck = PULSE_SAMPLES;
	}
	uint32_t ahead = (now - seen) & 0xFFFFFFu;
	if (ahead > 96u) { seen = now; ahead = 0; }           /* a long gap: catch up, don't replay */
	while (ahead--) {                                     /* a pulse on every div-th tick */
		seen = (seen + 1u) & 0xFFFFFFu;
		if (seen % (uint32_t)div == 0) ck = PULSE_SAMPLES;
	}
	n->s[0] = (float)seen;
	float run = M.running ? 1.f : 0.f;
	LOOP {
		n->out[0][i] = ck > 0.f ? 1.f : 0.f;
		n->out[1][i] = rs > 0.f ? 1.f : 0.f;
		n->out[2][i] = run;
		if (ck > 0.f) ck -= 1.f;
		if (rs > 0.f) rs -= 1.f;
	}
	n->s[1] = ck; n->s[3] = rs;
}
