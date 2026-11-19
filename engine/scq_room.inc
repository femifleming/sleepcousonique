/* Radio as weather: the dream hears who is in the room.
 *
 * Phones, watches, earbuds and laptops call out over Bluetooth a few times a second. The
 * computer's radio hears those calls (the editor listens; the SP-1 has no radio of its own to
 * listen with), and the editor hands the engine what it heard: how many devices there were in
 * the last few seconds, how strong the strongest was, how strong they were together, and each
 * arrival, each leaving and each call as it happens. Nothing about a device is kept but a
 * number between 0 and 1 made from its address.
 *
 * The numbers go in through scq_room() and scq_room_event(), from the thread that renders (or
 * under the lock that guards it), like scq_midi().
 *
 * Two places (a place has four outlets at most):
 *   crowd     out count near strength    max=20 rest=0
 *             out       how full the room is, 0 to 1: the count over max, smoothed
 *             count     the plain number of devices: 0, 1, 5, 30 (steps, not smoothed)
 *             near      the strongest signal, 0 to 1: how close the nearest device is
 *             strength  all the signals together (their mean), 0 to 1
 *             max=      the count that makes `out` reach 1
 *             rest=     the count it holds when there is no radio to listen with (the SP-1 on
 *                       its own, a computer whose Bluetooth is off or not allowed)
 *   visitors  arrive leave pulse voice
 *             arrive    a pulse when a device not heard before appears
 *             leave     a pulse when one has gone quiet
 *             pulse     a pulse on every call heard: an uneven rhythm made of whoever is there
 *             voice     the number (0 to 1) of the device that called last, held until the next
 */

static struct {
	uint8_t  live;                     /* a radio is listening */
	float    count, near, strength;    /* as the editor last gave them */
	uint32_t arrived, left, heard;     /* events so far: a new one makes a pulse */
	float    voice;
} R;

void scq_room(int live, int count, float near, float strength)
{
	R.live = live ? 1 : 0;
	R.count = scq_clamp((float)count, 0.f, 10000.f);
	R.near = scq_clamp(near, 0.f, 1.f);
	R.strength = scq_clamp(strength, 0.f, 1.f);
}

void scq_room_event(int kind, float voice)
{
	if (kind == 1) R.arrived++;
	else if (kind == 2) R.left++;
	else if (kind == 0) { R.heard++; R.voice = scq_clamp(voice, 0.f, 1.f); }
}

/* crowd: out count near strength.  s[0..2] the smoothed out, near, strength */
static void run_crowd(node *n, const float *const in[MAX_IN])
{
	(void)in;
	const float max = scq_max(n->opt[0], 1.f);
	float count = R.count, near = R.near, strength = R.strength;
	if (!R.live) {                         /* no radio: the room as the dream says it rests */
		count = scq_clamp(n->opt[1], 0.f, 10000.f);
		near = strength = count > 0.f ? 0.5f : 0.f;
	}
	const float full = scq_clamp(count / max, 0.f, 1.f);
	const float c = 1.f / (0.4f * SR);     /* people don't arrive in a click */
	float a = n->s[0], b = n->s[1], d = n->s[2];
	LOOP {
		a += (full - a) * c; b += (near - b) * c; d += (strength - d) * c;
		n->out[0][i] = a;
		n->out[1][i] = count;
		n->out[2][i] = b;
		n->out[3][i] = d;
	}
	n->s[0] = a; n->s[1] = b; n->s[2] = d;
}

/* visitors: arrive leave pulse voice.  s[0..2] events seen, s[3..5] pulse left, s[6] started */
static void run_visitors(node *n, const float *const in[MAX_IN])
{
	(void)in;
	const uint32_t now[3] = { R.arrived & 0xFFFFFFu, R.left & 0xFFFFFFu, R.heard & 0xFFFFFFu };
	if (n->s[6] == 0.f) {                  /* first block: join where the room is, no burst of old news */
		n->s[6] = 1.f;
		for (int k = 0; k < 3; k++) n->s[k] = (float)now[k];
	}
	float t[3];
	for (int k = 0; k < 3; k++) {
		t[k] = n->s[3 + k];
		if ((float)now[k] != n->s[k]) { n->s[k] = (float)now[k]; t[k] = PULSE_SAMPLES; }
	}
	const float voice = R.voice;
	LOOP {
		for (int k = 0; k < 3; k++) {
			n->out[k][i] = t[k] > 0.f ? 1.f : 0.f;
			if (t[k] > 0.f) t[k] -= 1.f;
		}
		n->out[3][i] = voice;
	}
	for (int k = 0; k < 3; k++) n->s[3 + k] = t[k];
}
