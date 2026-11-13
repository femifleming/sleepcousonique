/*
 * tinydream.c — a whole patch language in one file.
 *
 * The companion to "Building Sleepcousonique". It has the same bones as the real engine
 * (tokens -> names -> a graph -> a sample loop) with none of the muscle, so you can read it
 * in one sitting and grow it yourself.
 *
 *   cc -O2 -o tinydream tinydream.c -lm
 *   ./tinydream first.dream first.wav 4        (render 4 seconds)
 *
 * The language:
 *   ; a note to yourself
 *   the voice is sine 3          a place with a nature (sine 3 = 3 x 60 Hz)
 *   the slow is sine 0.005       (a slow sine is an LFO)
 *   slow -30> voice.pitch        a passage: 30% of slow into voice's pitch inlet
 *   voice -> fold -> ears        chains; unknown place words make themselves
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RATE      48000
#define MAX_NODES 32
#define MAX_CORDS 64
#define MAX_PORTS 4

/* ------------------------------------------------------------------ the places */

typedef struct Node Node;
typedef void (*RunFn)(Node *n, const float *in);    /* one sample: inputs in, outputs out */

typedef struct {
	const char *name;
	const char *ins[MAX_PORTS];      /* inlet names, NULL-terminated */
	float       defs[MAX_PORTS];     /* what an empty inlet hears    */
	int         nouts;
	RunFn       run;
} Place;

struct Node {
	const Place *place;
	char   name[24];
	float  nature;
	float  out[MAX_PORTS];
	float  state[4];
	int    nin;
};

static void run_sine(Node *n, const float *in)
{
	/* in[0] pitch (octaves), in[1] fm */
	float f = 60.f * n->nature * powf(2.f, in[0]) * (1.f + in[1]);
	n->state[0] += f / RATE;
	n->state[0] -= floorf(n->state[0]);
	n->out[0] = sinf(6.2831853f * n->state[0]);
}

static void run_saw(Node *n, const float *in)
{
	float f = 60.f * n->nature * powf(2.f, in[0]);
	n->state[0] += f / RATE;
	n->state[0] -= floorf(n->state[0]);
	n->out[0] = 2.f * n->state[0] - 1.f;              /* naive: try polyBLEP (exercise 6) */
}

static void run_fold(Node *n, const float *in)
{
	/* a sine wavefolder: in[0] sound, in[1] how much */
	n->out[0] = sinf(1.5707963f * in[0] * (1.f + in[1] * 8.f));
}

static void run_lowpass(Node *n, const float *in)
{
	/* one pole: in[0] sound, in[1] cutoff (octaves above nature x 100 Hz) */
	float fc = 100.f * n->nature * powf(2.f, in[1]);
	float c = 1.f - expf(-6.2831853f * fc / RATE);
	n->state[0] += c * (in[0] - n->state[0]);
	n->out[0] = n->state[0];
}

static void run_ears(Node *n, const float *in) { n->out[0] = in[0]; }

static const Place PLACES[] = {
	{ "sine",    { "pitch", "fm", NULL },  { 0, 0 },    1, run_sine },
	{ "saw",     { "pitch", NULL },        { 0 },       1, run_saw },
	{ "fold",    { "in", "amount", NULL }, { 0, 0 },    1, run_fold },
	{ "lowpass", { "in", "cut", NULL },    { 0, 3 },    1, run_lowpass },
	{ "ears",    { "in", NULL },           { 0 },       0, run_ears },
};
#define NPLACES ((int)(sizeof PLACES / sizeof PLACES[0]))

/* ------------------------------------------------------------------ the dream */

typedef struct { int src, dst, port; float amount; } Cord;

static Node nodes[MAX_NODES];
static int  nnodes;
static Cord cords[MAX_CORDS];
static int  ncords;
static int  line_no, errors;

static void complain(const char *what, const char *word)
{
	fprintf(stderr, "line %d: %s %s\n", line_no, what, word ? word : "");
	errors++;
}

static const Place *find_place(const char *w)
{
	for (int i = 0; i < NPLACES; i++) if (!strcmp(PLACES[i].name, w)) return &PLACES[i];
	return NULL;
}

static int find_node(const char *w)
{
	for (int i = 0; i < nnodes; i++) if (!strcmp(nodes[i].name, w)) return i;
	return -1;
}

static int make_node(const Place *p, const char *name, float nature)
{
	if (nnodes >= MAX_NODES) { complain("too many places", NULL); return -1; }
	Node *n = &nodes[nnodes];
	memset(n, 0, sizeof *n);
	n->place = p;
	snprintf(n->name, sizeof n->name, "%s", name);
	n->nature = nature;
	while (n->nin < MAX_PORTS && p->ins[n->nin]) n->nin++;
	return nnodes++;
}

/* "voice" or "voice.pitch" -> node index and inlet. Unknown place words make themselves. */
static int resolve(const char *word, int *port)
{
	char name[64];
	snprintf(name, sizeof name, "%s", word);
	char *dot = strchr(name, '.');
	if (dot) *dot++ = 0;
	int id = find_node(name);
	if (id < 0) {
		const Place *p = find_place(name);
		if (!p) { complain("nothing called", name); return -1; }
		id = make_node(p, name, 1.f);
		if (id < 0) return -1;
	}
	*port = 0;
	if (dot) {
		const Place *p = nodes[id].place;
		int k = 0;
		while (k < nodes[id].nin && strcmp(p->ins[k], dot)) k++;
		if (k == nodes[id].nin) { complain("no inlet called", dot); return -1; }
		*port = k;
	}
	return id;
}

/* an arrow: "->", or "-40>" for 40% */
static int is_arrow(const char *t, float *amount)
{
	size_t n = strlen(t);
	if (n < 2 || t[0] != '-' || t[n - 1] != '>') return 0;
	*amount = n == 2 ? 1.f : (float)atof(t + 1) / 100.f;
	return 1;
}

static void statement(char *line)
{
	char *tok[32];
	int nt = 0;
	for (char *t = strtok(line, " \t\r\n"); t && nt < 32; t = strtok(NULL, " \t\r\n")) tok[nt++] = t;
	if (nt == 0) return;

	/* the NAME is PLACE [NATURE] */
	if (!strcmp(tok[0], "the")) {
		if (nt < 4 || strcmp(tok[2], "is")) { complain("write it as: the name is place", NULL); return; }
		const Place *p = find_place(tok[3]);
		if (!p) { complain("I don't know the place", tok[3]); return; }
		if (find_node(tok[1]) >= 0) { complain("there is already a", tok[1]); return; }
		make_node(p, tok[1], nt > 4 ? (float)atof(tok[4]) : 1.f);
		return;
	}

	/* a -> b -> c.port   (each word before an arrow feeds the word after it) */
	for (int i = 1; i + 1 < nt; i += 2) {
		float amount;
		if (!is_arrow(tok[i], &amount)) { complain("I expected an arrow, not", tok[i]); return; }
		int sp, dp;
		int src = resolve(tok[i - 1], &sp);
		int dst = resolve(tok[i + 1], &dp);
		if (src < 0 || dst < 0) return;
		if (ncords >= MAX_CORDS) { complain("too many passages", NULL); return; }
		cords[ncords++] = (Cord){ src, dst, dp, amount };
	}
}

/* ------------------------------------------------------------------ running it */

static int order[MAX_NODES], norder;
static int mark[MAX_NODES];

/* depth first: everything feeding a node runs before it. A loop is cut where we find it,
 * and that cord hears the sample before -- which is all feedback is. */
static void visit(int k)
{
	if (mark[k]) return;
	mark[k] = 1;
	for (int c = 0; c < ncords; c++)
		if (cords[c].dst == k) visit(cords[c].src);
	order[norder++] = k;
}

static float tick(void)
{
	float ears = 0.f;
	for (int o = 0; o < norder; o++) {
		Node *n = &nodes[order[o]];
		float in[MAX_PORTS];
		int patched[MAX_PORTS] = { 0 };
		for (int p = 0; p < n->nin; p++) in[p] = 0.f;
		for (int c = 0; c < ncords; c++)          /* stacked cords add up, as on a Serge */
			if (cords[c].dst == order[o]) {
				in[cords[c].port] += nodes[cords[c].src].out[0] * cords[c].amount;
				patched[cords[c].port] = 1;
			}
		for (int p = 0; p < n->nin; p++) if (!patched[p]) in[p] = n->place->defs[p];
		n->place->run(n, in);
		if (n->place->run == run_ears) ears += n->out[0];
	}
	return tanhf(ears);
}

static void put16(FILE *f, int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, int v) { put16(f, v & 0xffff); put16(f, (v >> 16) & 0xffff); }

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: tinydream patch.dream out.wav [seconds]\n"); return 2; }
	FILE *in = fopen(argv[1], "r");
	if (!in) { perror(argv[1]); return 1; }
	char line[512];
	while (fgets(line, sizeof line, in)) {
		line_no++;
		char *semi = strchr(line, ';');
		if (semi) *semi = 0;
		statement(line);
	}
	fclose(in);
	if (errors) return 1;

	for (int k = 0; k < nnodes; k++) visit(k);
	int frames = (int)((argc > 3 ? atof(argv[3]) : 4.0) * RATE);
	FILE *out = fopen(argv[2], "wb");
	fwrite("RIFF", 1, 4, out); put32(out, 36 + frames * 2); fwrite("WAVEfmt ", 1, 8, out);
	put32(out, 16); put16(out, 1); put16(out, 1); put32(out, RATE); put32(out, RATE * 2);
	put16(out, 2); put16(out, 16); fwrite("data", 1, 4, out); put32(out, frames * 2);
	for (int i = 0; i < frames; i++) put16(out, (int)(tick() * 30000.f));
	fclose(out);
	printf("%d places, %d passages, %d samples\n", nnodes, ncords, frames);
	return 0;
}
