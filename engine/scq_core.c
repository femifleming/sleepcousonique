/*
 * Sleepcousonique — the language and the runner.
 *
 * A .dream file is a journal. A date (or any line) underlined with dashes starts a scene.
 * Lines before the first scene are the prelude, shared by every scene.
 *
 *   the hall is delay 7                 naming: a place with a fixed nature
 *   the route is seq 4: 0 3 7 12        a sequencer and its steps
 *   rain -> frogs ~> hall               passages: -> hall  ~> fog  => water  >| door  ?> maybe
 *   fader1 -60> hall.time               a number inside the arrow sets the amount
 *   value 40 -> bank.band3              a kept value (or just: 40 -> bank.band3)
 *   lucid fader2: peaks.fold 0..90, env.rise 20..5
 *   drift 3                             natures wander 3% from night to night
 *   ; anything after a semicolon is a note to yourself
 */
#include "scq_int.h"
#include "scq_costs.h"
#include "scq_costs_wingie.h"

/* Loops stay loops. GCC likes to turn a clearing or copying loop into a call to memset or memcpy,
 * and the SP-1's C library (built for size) does those a byte at a time: clearing one 32-sample
 * block that way cost 36 cycles a sample, more than most places. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("no-tree-loop-distribute-patterns")
#endif

typedef struct { int start, end, line0, ts, te; } scene;
#define MAX_KONST 32
#define MAX_TUNINGS 32

typedef struct {
	node     n[MAX_NODES];
	int      nn;
	cord     c[MAX_CORDS];
	int      nc;
	uint8_t  order[MAX_NODES];
	uint16_t cstart[MAX_NODES + 1];
	float    konst[MAX_KONST][SCQ_BLOCK];   /* constant blocks: defaults and lone values, filled once */
	float    kval[MAX_KONST];
	int      nk;
#ifdef SCQ_POOL_EXTERN
	uint32_t *pool;              /* handed over by the firmware (scq_pool): the Wingie's heap */
#else
	uint32_t pool[POOL_BYTES / 4];
#endif
	uint32_t used;
	uint32_t wanted;             /* what the scene asked for, whether it fitted or not */

#ifdef SCQ_DEVICE
	const char *text;            /* the SP-1 reads the dream where the firmware keeps it (16 KB of RAM saved) */
#else
	char     text[SCQ_MAX_TEXT];
#endif
	int      len;
	scene    sc[SCQ_MAX_SCENES];
	int      nsc;
	int      pre_end;            /* prelude is text[0..pre_end) */

	int      cur, pending;
	float    gain;
	int      fading;             /* -1 out, +1 in, 0 steady */

	char     msg[2048];
	int      msgn, errors;
#ifdef SCQ_HOST
	char     desc[8192];
#endif

	uint32_t nights;
	int      instrument;         /* SCQ_SP1 / SCQ_WINGIE: from the dream's `instrument` line */
	float    drift;
	float    load;
	float    scene_load[SCQ_MAX_SCENES];
	int      line;               /* line being compiled, for messages */
	int      quiet;              /* don't report (already reported) */
	int      waiting;            /* SCQ_DEVICE: faded out, waiting for scq_service() */
	uint32_t rng;
	uint32_t healed;             /* places started again after running away */
	uint8_t  fever;              /* blocks left in which every place's memory is looked at */
	uint8_t  ward;               /* the place whose memory is looked at this block */
	struct { char name[NAME_LEN]; float *d; } tun[MAX_TUNINGS];   /* tunings, in the pool */
	int      ntun;
} engine;

static engine G0;
#ifdef SCQ_POOL_EXTERN
static uint32_t pool_size;
#define POOL_SIZE pool_size
void scq_pool(void *mem, uint32_t bytes) { G0.pool = (uint32_t *)mem; pool_size = bytes & ~3u; }
#else
#define POOL_SIZE POOL_BYTES
#endif
static engine *Gp = &G0;
#define G (*Gp)

/* ------------------------------------------------------------- little strings */

typedef struct { char *b; int n, cap; } sb;

static void sb_c(sb *s, char c) { if (s->n < s->cap - 1) { s->b[s->n++] = c; s->b[s->n] = 0; } }
static void sb_s(sb *s, const char *t) { while (*t) sb_c(s, *t++); }
static void sb_sn(sb *s, const char *t, int n) { for (int i = 0; i < n; i++) sb_c(s, t[i]); }
static void sb_i(sb *s, int v)
{
	char tmp[12]; int k = 0;
	if (v < 0) { sb_c(s, '-'); v = -v; }
	do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 11);
	while (k) sb_c(s, tmp[--k]);
}
#if defined(__GNUC__)
__attribute__((unused))
#endif
static void sb_f(sb *s, float v)
{
	if (v < 0.f) { sb_c(s, '-'); v = -v; }
	int ip = (int)v;
	int fp = (int)((v - (float)ip) * 1000.f + 0.5f);
	if (fp >= 1000) { ip++; fp -= 1000; }
	sb_i(s, ip);
	if (fp) {
		sb_c(s, '.');
		sb_c(s, (char)('0' + fp / 100)); fp %= 100;
		if (fp) { sb_c(s, (char)('0' + fp / 10)); fp %= 10; if (fp) sb_c(s, (char)('0' + fp)); }
	}
}

static int str_eq(const char *a, int al, const char *b)
{
	int i = 0;
	for (; i < al; i++) if (!b[i] || a[i] != b[i]) return 0;
	return b[i] == 0;
}
static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* within one edit (insert, delete, substitute or swap) */
static int near(const char *a, int al, const char *b, int bl)
{
	if (al < 3 || bl < 3) return 0;
	int d = al - bl;
	if (d > 1 || d < -1) return 0;
	int i = 0, j = 0, edits = 0;
	while (i < al && j < bl) {
		if (lower(a[i]) == lower(b[j])) { i++; j++; continue; }
		if (++edits > 1) return 0;
		if (al == bl) {
			if (i + 1 < al && lower(a[i]) == lower(b[j + 1]) && lower(a[i + 1]) == lower(b[j])) { i += 2; j += 2; continue; }
			i++; j++;
		} else if (al > bl) i++;
		else j++;
	}
	edits += (al - i) + (bl - j);
	return edits <= 1;
}

static float parse_num(const char *s, int n)
{
	float v = 0.f, sign = 1.f, scale = 0.f;
	int i = 0;
	if (i < n && s[i] == '-') { sign = -1.f; i++; }
	for (; i < n; i++) {
		if (s[i] == '.') { if (scale != 0.f) break; scale = 1.f; continue; }
		if (!is_digit(s[i])) break;
		if (scale == 0.f) v = v * 10.f + (float)(s[i] - '0');
		else { scale *= 0.1f; v += (float)(s[i] - '0') * scale; }
	}
	return v * sign;
}

/* A number with its pitch unit, in the units of the place it lands: `per_octave` is what one
 * octave is there (100 for a kept value, where 100 is 1.0; 1 for a pitch knob or tune=; 12 for a
 * seq's steps). 7st is seven semitones, -50ct fifty cents down, 1oct an octave; a bare number is
 * taken as it is. */
static float num_unit(const char *s, int n, float per_octave)
{
	float v = parse_num(s, n);
	if (n >= 2 && s[n - 2] == 's' && s[n - 1] == 't') return v * per_octave / 12.f;
	if (n >= 3 && s[n - 3] == 'o' && s[n - 2] == 'c' && s[n - 1] == 't') return v * per_octave;
	if (n >= 2 && s[n - 2] == 'c' && s[n - 1] == 't') return v * per_octave / 1200.f;
	return v;
}

/* ------------------------------------------------------------- messages */

static void say(char kind, const char *a, const char *w, int wl, const char *b)
{
	if (kind == 'E') G.errors++;
	if (G.quiet) return;
	sb s = { G.msg, G.msgn, (int)sizeof G.msg };
	sb_c(&s, kind); sb_c(&s, ' '); sb_i(&s, G.line); sb_c(&s, ' ');
	sb_s(&s, a);
	if (w) { sb_c(&s, '\''); sb_sn(&s, w, wl); sb_c(&s, '\''); }
	if (b) sb_s(&s, b);
	sb_c(&s, '\n');
	G.msgn = s.n;
}

static void say_renamed(const char *w, int wl, const char *now)
{
	if (G.quiet) return;
	sb s = { G.msg, G.msgn, (int)sizeof G.msg };
	sb_s(&s, "W "); sb_i(&s, G.line); sb_s(&s, " '"); sb_sn(&s, w, wl);
	sb_s(&s, "' is now called '"); sb_s(&s, now); sb_s(&s, "'\n");
	G.msgn = s.n;
}

/* ------------------------------------------------------------- tokens */

enum { T_WORD, T_NUM, T_ARROW, T_COLON, T_COMMA, T_RANGE, T_EQ };
typedef struct { const char *s; int n; uint8_t t; uint8_t kind; float amt; int has_amt; } tok;
#define MAX_TOK 64

static int tokenize(const char *s, int n, tok *t)
{
	int i = 0, k = 0;
	while (i < n && k < MAX_TOK) {
		char c = s[i];
		if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
		tok *o = &t[k];
		o->s = s + i; o->has_amt = 0; o->amt = 0.f; o->kind = 0;
		if (c == ':') { o->t = T_COLON; o->n = 1; i++; k++; continue; }
		if (c == ',') { o->t = T_COMMA; o->n = 1; i++; k++; continue; }
		if (c == '>' && i + 1 < n && s[i + 1] == '|') { o->t = T_ARROW; o->kind = K_DOOR; o->n = 2; i += 2; k++; continue; }
		if (c == '-' || c == '~' || c == '=' || c == '?') {
			int j = i + 1;
			while (j < n && (is_digit(s[j]) || s[j] == '.')) j++;
			if (j < n && s[j] == '>') {
				o->t = T_ARROW;
				o->kind = c == '-' ? K_HALL : (c == '~' ? K_FOG : (c == '?' ? K_MAYBE : K_WATER));
				if (j > i + 1) { o->has_amt = 1; o->amt = parse_num(s + i + 1, j - i - 1); }
				o->n = j + 1 - i; i = j + 1; k++;
				continue;
			}
		}
		if (c == '=') { o->t = T_EQ; o->n = 1; i++; k++; continue; }
		if (is_digit(c) || ((c == '-' || c == '.') && i + 1 < n && (is_digit(s[i + 1]) || s[i + 1] == '.'))) {
			int j = i + 1, dots = 0;
			while (j < n && (is_digit(s[j]) || s[j] == '.'
			                 /* a range may end below zero: 0..-50 */
			                 || (s[j] == '-' && dots && j >= 2 && s[j - 1] == '.' && s[j - 2] == '.'
			                     && j + 1 < n && (is_digit(s[j + 1]) || s[j + 1] == '.')))) {
				if (s[j] == '.' && j + 1 < n && s[j + 1] == '.') dots = 1;
				j++;
			}
			if (j < n && s[j] == '-' && dots == 0) { /* 20-5 is not a thing; stop here */ }
			if (!dots) {   /* a pitch unit stuck on the number: 7st, -50ct, 1oct */
				int u = 0;
				if (j + 2 <= n && s[j] == 's' && s[j + 1] == 't') u = 2;
				else if (j + 2 <= n && s[j] == 'c' && s[j + 1] == 't') u = 2;
				else if (j + 3 <= n && s[j] == 'o' && s[j + 1] == 'c' && s[j + 2] == 't') u = 3;
				if (u && (j + u >= n || !(is_alpha(s[j + u]) || is_digit(s[j + u])))) j += u;
			}
			o->t = dots ? T_RANGE : T_NUM;
			o->n = j - i; i = j; k++;
			continue;
		}
		if (is_alpha(c)) {
			int j = i + 1;
			while (j < n && (is_alpha(s[j]) || is_digit(s[j]) || s[j] == '.' || s[j] == '\'')) j++;
			o->t = T_WORD; o->n = j - i; i = j; k++;
			continue;
		}
		say('E', "I can't read ", s + i, 1, " here");
		return -1;
	}
	return k;
}

/* ------------------------------------------------------------- places & nodes */

static int find_place(const char *w, int n)
{
	for (int p = 0; p < SCQ_NPLACES; p++) if (str_eq(w, n, SCQ_PLACES[p].name)) return p;
	return -1;
}

/* the same, but an old name (before the 2026-11-13 rename) is understood, with a note */
static int find_place_old(const char *w, int n)
{
	int p = find_place(w, n);
	if (p >= 0) return p;
	const char *now = place_renamed(w, n);           /* an old dream: aira is now sine */
	if (now) {
		say_renamed(w, n, now);
		for (int p = 0; p < SCQ_NPLACES; p++) if (str_eq(now, str_len(now), SCQ_PLACES[p].name)) return p;
	}
	return -1;
}

static int find_node(const char *w, int n)
{
	for (int i = 0; i < G.nn; i++) if (str_eq(w, n, G.n[i].name)) return i;
	return -1;
}

static void *pool_take(uint32_t bytes)
{
	bytes = (bytes + 3u) & ~3u;
	G.wanted += bytes;
	if (G.used + bytes > POOL_SIZE) return 0;
	uint32_t *p = &G.pool[G.used / 4];
	for (uint32_t i = 0; i < bytes / 4; i++) p[i] = 0;
	G.used += bytes;
	return p;
}

static uint32_t hash(const char *s, int n, uint32_t h)
{
	for (int i = 0; i < n; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
	return h ? h : 1u;
}

/* ------------------------------------------------------------- switches and params
 * A place's options are a spec like "cycle=off|on range=lfo|audio length=5": a switch lists
 * its positions (the first is the default), a param has a number. Written after the nature:
 *   the env is dusg 2 cycle1 audio curve=exp     (a bare switch name means its second position;
 *                                                 a bare position word picks it) */
typedef struct { const char *name; int nl; const char *vals; int vl; int sw; } optitem;

static int opt_at(const char *spec, int k, optitem *o)
{
	const char *s = spec;
	for (int idx = 0; *s; idx++) {
		while (*s == ' ') s++;
		if (!*s) break;
		const char *w = s;
		while (*s && *s != ' ') s++;
		if (idx != k) continue;
		o->name = w; o->nl = 0;
		while (w + o->nl < s && w[o->nl] != '=') o->nl++;
		o->vals = w + o->nl + 1; o->vl = (int)(s - o->vals);
		o->sw = is_alpha(*o->vals);
		return 1;
	}
	return 0;
}

/* position of word w among a switch's values "a|b|c", or -1 */
static int opt_pos(const optitem *o, const char *w, int wl)
{
	const char *v = o->vals, *end = o->vals + o->vl;
	for (int pos = 0; v < end; pos++) {
		const char *a = v;
		while (v < end && *v != '|') v++;
		if (v - a == wl) { int j = 0; while (j < wl && a[j] == w[j]) j++; if (j == wl) return pos; }
		v++;
	}
	return -1;
}

/* tune: the base pitch knob on every place with a pitch inlet, octaves added to that inlet, so
 * tune=-12 takes an oscillator well below a hertz and tune=8 up past 15 kHz. With att.pitch=10
 * a fader sweeps ten octaves. */
static void pitch_setup(node *n)
{
	const place *pl = &SCQ_PLACES[n->type];
	n->pitchin = (int8_t)place_port(pl, 0, "pitch", 5);
	n->tune = 0.f;
	if (n->pitchin < 0) return;
	const char *spec = place_opts(pl->name);
	optitem o;
	for (int k = 0; k < MAX_OPT && opt_at(spec, k, &o); k++)
		if (o.nl == 4 && o.name[0] == 't' && o.name[1] == 'u' && o.name[2] == 'n' && o.name[3] == 'e') n->tune = n->opt[k];
}

/* samplerate=full|half|quarter|eighth (or the position word alone): any place but the SP-1's
 * own runs at 48, 24, 12 or 6 kHz. Returns 1 if the word was one. */
static int rate_set(node *n, const tok *name, const tok *val)
{
	static const char *const POS[4] = { "full", "half", "quarter", "eighth" };
	const tok *w = val ? val : name;
	if (val && !str_eq(name->s, name->n, "samplerate")) return 0;
	int pos = -1;
	for (int k = 0; k < 4; k++) if (w->t == T_WORD && str_eq(w->s, w->n, POS[k])) pos = k;
	if (pos < 0) {
		if (val) { say('E', "samplerate is full, half, quarter or eighth, not ", w->s, w->n, ""); return 1; }
		return 0;
	}
	if (SCQ_PLACES[n->type].flags & PF_SINGLE) { say('E', "", name->s, name->n, ": the SP-1's own places run at full rate"); return 1; }
	n->rdiv = (uint8_t)(1u << pos);
	return 1;
}

/* resolution=block|fine|audio: a place reads most of its inlets (pitch, cutoff, rates) once a
 * block of 32 samples; fine runs it every 4 samples, audio every sample, so a fast pitch or
 * cutoff sweep stays smooth (audio-rate FM through any pitch inlet). It costs the place's
 * per-block work 8 or 32 times. Returns 1 if the word was one. */
static int res_set(node *n, const tok *name, const tok *val)
{
	static const char *const POS[3] = { "block", "fine", "audio" };
	const tok *w = val ? val : name;
	if (val && !str_eq(name->s, name->n, "resolution")) return 0;
	int pos = -1;
	for (int k = 0; k < 3; k++) if (w->t == T_WORD && str_eq(w->s, w->n, POS[k])) pos = k;
	if (!val && pos != 1) return 0;            /* alone, only "fine" means this ("audio" is also a range) */
	if (pos < 0) { say('E', "resolution is block, fine or audio, not ", w->s, w->n, ""); return 1; }
	if (SCQ_PLACES[n->type].flags & PF_SINGLE) { say('E', "", name->s, name->n, ": the instrument's own places run once a block"); return 1; }
	n->res = (uint8_t)(pos == 0 ? 0 : (pos == 1 ? 4 : 1));
	return 1;
}

/* the default a place gives inlet k (its "defs" string) */
static float place_def(const place *pl, int k)
{
	const char *d = pl->defs;
	for (int i = 0; *d; i++) {
		while (*d == ' ') d++;
		const char *w = d;
		while (*d && *d != ' ') d++;
		if (i == k) return parse_num(w, (int)(d - w));
	}
	return 0.f;
}

/* INLET=value: the inlet's panel knob, as on an analog module. Alone it is the inlet's value;
 * with something patched in, it adds (value - the default), so a knob left alone changes
 * nothing. Only for an inlet name that isn't also one of the place's switches or params.
 * Returns 1 if the word was one. */
static int knob_set(node *n, const tok *name, const tok *val)
{
	if (!val) return 0;
	const place *pl = &SCQ_PLACES[n->type];
	int p = place_port(pl, 0, name->s, name->n);
	if (p < 0) return 0;
	optitem o;
	const char *spec = place_opts(pl->name);
	for (int k = 0; k < MAX_OPT && opt_at(spec, k, &o); k++)
		if (o.nl == name->n) {
			int same = 1;
			for (int j = 0; j < o.nl; j++) if (o.name[j] != name->s[j]) same = 0;
			if (same) return 0;            /* a switch or param of the same name wins */
		}
	if (val->t != T_NUM) { say('E', "", name->s, name->n, " wants a number, like time=0.4"); return 1; }
	float v = num_unit(val->s, val->n, 1.f);
	n->def[p] = v;
	n->koff[p] = v - place_def(pl, p);
	n->knobset |= (uint16_t)(1u << p);
	return 1;
}

#define ATT_MAX 1000.f   /* far past the knob's ends (10 for pitch, 2 for the rest), but a number */
/* att.INLET=amount: the attenuverter on a source's inlet. Returns 1 if the word was one. */
static int att_set(node *n, const tok *name, const tok *val)
{
	if (name->n < 5 || name->s[0] != 'a' || name->s[1] != 't' || name->s[2] != 't' || name->s[3] != '.') return 0;
	const place *pl = &SCQ_PLACES[n->type];
	if (!place_has_att(pl)) { say('E', "", pl->name, str_len(pl->name), " has no attenuverters"); return 1; }
	int p = place_port(pl, 0, name->s + 4, name->n - 4);
	if (p < 0) { say('E', "no inlet called ", name->s + 4, name->n - 4, " to attenuate"); return 1; }
	if (!val || val->t != T_NUM) { say('E', "", name->s, name->n, " wants a number, like att.fm=-0.5"); return 1; }
	n->att[p] = scq_clamp(parse_num(val->s, val->n), -ATT_MAX, ATT_MAX);
	if (n->att[p] != 1.f) n->attset |= (uint16_t)(1u << p);
	else n->attset &= (uint16_t)~(1u << p);
	return 1;
}

static void opt_defaults(node *n, const char *spec)
{
	optitem o;
	for (int k = 0; k < MAX_OPT && opt_at(spec, k, &o); k++)
		n->opt[k] = o.sw ? 0.f : parse_num(o.vals, o.vl);
}

static int find_tuning(const char *w, int n);

static int opt_count(const optitem *o)
{
	int n = 1;
	for (int j = 0; j < o->vl; j++) if (o->vals[j] == '|') n++;
	return n;
}

/* name=value, a bare switch name, or a bare position word. Returns 0 if it was understood. */
static int opt_set(node *n, const tok *name, const tok *val)
{
	const char *spec = place_opts(SCQ_PLACES[n->type].name);
	optitem o;
	int hit = -1, hitpos = 0, hits = 0;
	for (int k = 0; k < MAX_OPT && opt_at(spec, k, &o); k++) {
		int named = o.nl == name->n;
		for (int j = 0; named && j < o.nl; j++) if (o.name[j] != name->s[j]) named = 0;
		if (val) {
			if (!named) continue;
			if (o.sw) {
				int pos = val->t == T_WORD ? opt_pos(&o, val->s, val->n) : -1;
				if (pos < 0 && val->t == T_WORD && str_eq(o.name, o.nl, "scale")) {   /* a tuning: after the built-ins */
					int tn = find_tuning(val->s, val->n);
					if (tn >= 0) pos = opt_count(&o) + tn;
					else { say('E', "no scale called ", val->s, val->n, " (built in, in the scales folder, or a tuning line)"); return 0; }
				}
				if (pos < 0) { say('E', "", val->s, val->n, " isn't one of that switch's positions"); return 0; }
				n->opt[k] = (float)pos;
			} else {
				if (val->t != T_NUM) { say('E', "", name->s, name->n, " wants a number"); return 0; }
				n->opt[k] = num_unit(val->s, val->n, 1.f);   /* tune=7st */
			}
			n->optset |= (uint8_t)(1u << k);
			return 0;
		}
		if (!o.sw) continue;
		if (named) { hit = k; hitpos = 1; hits = 1; break; }
		int pos = opt_pos(&o, name->s, name->n);
		if (pos >= 0) { hit = k; hitpos = pos; hits++; }
	}
	if (hits == 1) { n->opt[hit] = (float)hitpos; n->optset |= (uint8_t)(1u << hit); return 0; }
	if (hits > 1) { say('E', "", name->s, name->n, " could be more than one switch here; write it as switch=position"); return 0; }
	say('E', "", name->s, name->n, val ? " isn't a switch or param of this place" : " isn't a switch of this place");
	return -1;
}

static int natural_nat(float x) { return (int)(x + 0.5f); }

/* where an outlet a place doesn't have points: written, never read */
static float SPARE[B];

static int make_node(int p, const char *name, int nl, float nature)
{
	const place *pl = &SCQ_PLACES[p];
	if (G.nn >= MAX_NODES) { say('E', "the dream is too crowded (", 0, 0, "96 places at most)"); return -1; }
	if ((pl->flags & PF_SP1) && G.instrument == SCQ_WINGIE) {
		say('E', "", name, nl, " is one of the SP-1's hands, and this dream is for the Wingie");
		return -1;
	}
	if ((pl->flags & PF_WINGIE) && G.instrument == SCQ_SP1) {
		say('E', "", name, nl, " is one of the Wingie's hands: write `instrument wingie` at the top of the dream");
		return -1;
	}
	if (G.instrument == SCQ_WINGIE && str_eq(pl->name, str_len(pl->name), "fader") && natural_nat(nature) > 3) {
		say('E', "", name, nl, ": the Wingie has three faders (fader1 Mix, fader2 Decay, fader3 Volume)");
		return -1;
	}
	if (nl >= NAME_LEN) nl = NAME_LEN - 1;
	node *n = &G.n[G.nn];
	uint8_t *raw = (uint8_t *)n;
	for (uint32_t i = 0; i < sizeof *n; i++) raw[i] = 0;
	n->type = (uint8_t)p;
	n->nin = (uint8_t)place_nports(pl->ins);
	n->nout = (uint8_t)place_nports(pl->outs);
	for (int i = 0; i < nl; i++) n->name[i] = name[i];
	n->line = (int16_t)G.line;
	n->given = nature;
	for (int i = 0; i < MAX_IN; i++) n->att[i] = 1.f;
	n->rdiv = 1;
	opt_defaults(n, place_opts(pl->name));
	n->rng = hash(name, nl, 2166136261u + G.nights * 7919u);
	n->nature = nature;
	if ((pl->flags & PF_DRIFT) && G.drift != 0.f) {
		uint32_t r = hash(name, nl, 0x9e3779b9u ^ (G.nights * 2654435761u));
		float u = scq_rand01(&r) * 2.f - 1.f;
		n->nature = nature * (1.f + u * G.drift * 0.01f);
	}
	/* defaults */
	const char *d = pl->defs;
	for (int k = 0; k < n->nin && *d; k++) {
		while (*d == ' ') d++;
		const char *w = d;
		while (*d && *d != ' ') d++;
		n->def[k] = parse_num(w, (int)(d - w));
	}
	for (int o = 0; o < MAX_OUT; o++) n->out[o] = SPARE;
	for (int o = 0; o < n->nout; o++) {          /* a block for each outlet it has */
		float *b = (float *)pool_take(B * sizeof(float));
		if (!b) { say('E', "the house is full: no room for ", name, nl, ""); return -1; }
		n->out[o] = b;
	}
	if (pl->mem) {
		uint32_t bytes = pl->mem(n->nature);
		n->mem = pool_take(bytes);
		if (!n->mem) { say('E', "the house is full: no room for ", name, nl, ""); return -1; }
		n->memlen = bytes;
	}
	return G.nn++;
}

typedef struct { int node, port, is_bottle; float val; } endpoint;

/* Find or dream up the thing a word names. role: 0 = something comes out of it, 1 = goes in */
static int resolve(const tok *t, int role, endpoint *e)
{
	const char *w = t->s;
	int n = t->n, dot = -1;
	for (int i = 0; i < n; i++) if (w[i] == '.') { dot = i; break; }
	int nl = dot < 0 ? n : dot;
	e->is_bottle = 0;

	int id = find_node(w, nl);
	if (id < 0) {
		int p = find_place_old(w, nl);
		float nature = 0.f;
		if (p >= 0) nature = SCQ_PLACES[p].nature;
		else {                                   /* place + number: aira3, lean2 */
			int j = nl;
			while (j > 0 && is_digit(w[j - 1])) j--;
			if (j < nl && j > 0 && (p = find_place_old(w, j)) >= 0) nature = parse_num(w + j, nl - j);
		}
		if (p < 0) {                             /* a misspelling? dreams forgive */
			for (int i = 0; i < G.nn && id < 0; i++)
				if (near(w, nl, G.n[i].name, str_len(G.n[i].name))) id = i;
			if (id >= 0) say('W', "dreamt ", w, nl, " as an earlier name");
			for (int q = 0; q < SCQ_NPLACES && id < 0 && p < 0; q++)
				if (near(w, nl, SCQ_PLACES[q].name, str_len(SCQ_PLACES[q].name))) { p = q; nature = SCQ_PLACES[q].nature; }
			if (id < 0 && p >= 0) say('W', "dreamt ", w, nl, " as a place you know");
		}
		if (id < 0) {
			if (p < 0) { say('E', "nothing called ", w, nl, " lives here"); return -1; }
			id = make_node(p, w, nl, nature);
			if (id < 0) return -1;
		}
	}
	node *nd = &G.n[id];
	const place *pl = &SCQ_PLACES[nd->type];
	e->node = id;
	if (dot < 0) {
		if (role == 1 && nd->nin == 0) { say('E', "", w, nl, " takes nothing in"); return -1; }
		if (role == 0 && nd->nout == 0) { say('E', "", w, nl, " gives nothing out"); return -1; }
		e->port = 0;
		return 0;
	}
	const char *port = w + dot + 1;
	int plen = n - dot - 1;
	e->port = place_port(pl, role == 0, port, plen);
	if (e->port < 0) {
		say('E', "", w, nl, role == 0 ? " has no way out called that" : " has no way in called that");
		return -1;
	}
	return 0;
}

static void add_cord(const endpoint *a, const endpoint *b, int kind, float amt, float amt2, int has_amt)
{
	if (G.nc >= MAX_CORDS) { say('E', "too many passages (", 0, 0, "320 at most)"); return; }
	cord *c = &G.c[G.nc++];
	c->src = (int8_t)(a->is_bottle ? -1 : a->node);
	c->sport = (uint8_t)(a->is_bottle ? 0 : a->port);
	c->dst = (int8_t)b->node;
	c->dport = (uint8_t)b->port;
	c->kind = (uint8_t)kind;
	c->s = 0.f;
	c->direct = 0;
	c->line = (int16_t)G.line;
	switch (kind) {
	case K_HALL:   c->a = has_amt ? amt * 0.01f : 1.f; break;
	case K_FOG:    c->a = has_amt ? amt * 0.01f : 0.5f; break;
	case K_WATER:  c->a = scq_coef_time((has_amt ? amt : 60.f) * 0.001f, SR / (float)B); c->b = has_amt ? amt : 60.f; break;
	case K_DOOR:   c->a = 1.f; break;
	case K_RANGE:  c->a = amt * 0.01f; c->b = amt2 * 0.01f; break;
	case K_BOTTLE: c->a = a->val * 0.01f; break;
	case K_MAYBE:  c->a = has_amt ? amt * 0.01f : 0.5f; c->b = 0.f; break;   /* a = the chance, b = open now */
	}
	if (a->is_bottle) { c->kind = K_BOTTLE; c->a = a->val * 0.01f; }
}

/* ------------------------------------------------------------- statements */

static void st_the(tok *t, int nt)
{
	/* the NAME is|are PLACE [N] [switches and params] [: steps] */
	if (nt < 4 || t[1].t != T_WORD || t[2].t != T_WORD || t[3].t != T_WORD ||
	    !(str_eq(t[2].s, t[2].n, "is") || str_eq(t[2].s, t[2].n, "are"))) {
		say('E', "write it as: the name is place", 0, 0, "");
		return;
	}
	for (int i = 0; i < t[1].n; i++) if (t[1].s[i] == '.') { say('E', "a name can't have a dot: ", t[1].s, t[1].n, ""); return; }
	if (find_node(t[1].s, t[1].n) >= 0) { say('E', "there are already ", t[1].s, t[1].n, " here"); return; }
	int p = find_place_old(t[3].s, t[3].n);
	if (p < 0) {                       /* place + number: the SP-1's own, fader1, button3 */
		int j = t[3].n;
		while (j > 0 && is_digit(t[3].s[j - 1])) j--;
		if (j < t[3].n && j > 0) {
			int q = find_place(t[3].s, j);
			if (q >= 0 && (SCQ_PLACES[q].flags & PF_SINGLE)) p = q;
		}
	}
	if (p >= 0 && (SCQ_PLACES[p].flags & PF_SINGLE)) {
		int same = t[1].n == t[3].n;
		for (int i = 0; same && i < t[1].n; i++) if (t[1].s[i] != t[3].s[i]) same = 0;
		if (same) { say('W', "", t[3].s, t[3].n, " is always there (it's on the SP-1): no need to name it, just use it"); return; }
		say('E', "", t[3].s, t[3].n, " is part of the instrument and always there: use it by its own name (like fader1 -> lfo.rate or keys1.pitch), not a new one");
		return;
	}
	int other = find_place(t[1].s, t[1].n);
	/* naming something after another place is allowed: your name wins in this dream
	 * (so a new place can never break a dream written before it existed) */
	if (other >= 0 && other != p) say('W', "here ", t[1].s, t[1].n, " means yours, not the place of that name");
	if (p < 0) {
		for (int q = 0; q < SCQ_NPLACES && p < 0; q++)
			if (near(t[3].s, t[3].n, SCQ_PLACES[q].name, str_len(SCQ_PLACES[q].name))) p = q;
		if (p < 0) { say('E', "I don't know the place ", t[3].s, t[3].n, ""); return; }
		say('W', "dreamt ", t[3].s, t[3].n, " as a place you know");
	}
	if (SCQ_PLACES[p].flags & PF_SINGLE) { say('E', "", t[3].s, t[3].n, " is part of the SP-1; use it by name"); return; }
	float nature = SCQ_PLACES[p].nature;
	int k = 4;
	if (k < nt && t[k].t == T_NUM) nature = parse_num(t[k].s, t[k].n), k++;
	int id = make_node(p, t[1].s, t[1].n, nature);
	if (id < 0) return;
	while (k < nt && t[k].t == T_WORD) {                       /* switches and params */
		int eq = k + 2 < nt && t[k + 1].t == T_EQ;
		if (att_set(&G.n[id], &t[k], eq ? &t[k + 2] : 0)) { k += eq ? 3 : 1; continue; }
		if (rate_set(&G.n[id], &t[k], eq ? &t[k + 2] : 0)) { k += eq ? 3 : 1; continue; }
		if (res_set(&G.n[id], &t[k], eq ? &t[k + 2] : 0)) { k += eq ? 3 : 1; continue; }
		if (knob_set(&G.n[id], &t[k], eq ? &t[k + 2] : 0)) { k += eq ? 3 : 1; continue; }
		if (opt_set(&G.n[id], &t[k], eq ? &t[k + 2] : 0) < 0) return;
		k += eq ? 3 : 1;
	}
	if (k < nt && t[k].t == T_EQ) { say('E', "write a param as name=value", 0, 0, ""); return; }
	if (k < nt && t[k].t == T_COLON) k++;
	if (k < nt) {
		if (p != find_place("seq", 3)) { say('W', "only a seq has steps; ignoring the rest of ", t[1].s, t[1].n, ""); return; }
		float *m = (float *)G.n[id].mem;
		int c = 0;
		for (; k < nt && c < 32; k++) {
			if (t[k].t == T_COMMA) continue;
			if (t[k].t != T_NUM) { say('E', "a step is a number, not ", t[k].s, t[k].n, ""); return; }
			m[c++] = num_unit(t[k].s, t[k].n, 12.f);
		}
		m[32] = (float)c;
	}
}

static void st_lucid(tok *t, int nt)
{
	/* lucid SRC : DST lo..hi , DST lo..hi */
	if (nt < 5 || t[1].t != T_WORD || t[2].t != T_COLON) { say('E', "write it as: lucid fader1: place.inlet 0..100", 0, 0, ""); return; }
	endpoint src;
	if (resolve(&t[1], 0, &src) < 0) return;
	int k = 3;
	while (k < nt) {
		if (t[k].t == T_COMMA) { k++; continue; }
		if (t[k].t != T_WORD || k + 1 >= nt || t[k + 1].t != T_RANGE) { say('E', "each lucid target needs a range, like 0..100", 0, 0, ""); return; }
		endpoint dst;
		if (resolve(&t[k], 1, &dst) < 0) return;
		const char *r = t[k + 1].s;
		int rn = t[k + 1].n, dd = 0;
		while (dd + 1 < rn && !(r[dd] == '.' && r[dd + 1] == '.')) dd++;
		float lo = parse_num(r, dd), hi = parse_num(r + dd + 2, rn - dd - 2);
		add_cord(&src, &dst, K_RANGE, lo, hi, 1);
		k += 2;
	}
}

static int endpoint_at(tok *t, int nt, int *k, int role, endpoint *e)
{
	if (t[*k].t == T_WORD && (str_eq(t[*k].s, t[*k].n, "value") || str_eq(t[*k].s, t[*k].n, "bottle")) && *k + 1 < nt && t[*k + 1].t == T_NUM) {
		if (role == 1) { say('E', "a value can't take anything in", 0, 0, ""); return -1; }
		e->is_bottle = 1; e->val = parse_num(t[*k + 1].s, t[*k + 1].n); e->node = -1; e->port = 0;
		*k += 2;
		return 0;
	}
	if (t[*k].t == T_NUM) {
		if (role == 1) { say('E', "a number can't take anything in: ", t[*k].s, t[*k].n, ""); return -1; }
		e->is_bottle = 1; e->val = num_unit(t[*k].s, t[*k].n, 100.f); e->node = -1; e->port = 0;
		*k += 1;
		return 0;
	}
	if (t[*k].t != T_WORD) { say('E', "I expected a place, not ", t[*k].s, t[*k].n, ""); return -1; }
	int r = resolve(&t[*k], role, e);
	*k += 1;
	return r;
}

static void st_chain(tok *t, int nt)
{
	/* groups of endpoints joined by arrows: a b -> c ~> d e */
	int gstart[32], gend[32], ng = 0, k = 0;
	tok *arrows[32];
	while (k < nt && ng < 32) {
		gstart[ng] = k;
		while (k < nt && t[k].t != T_ARROW) k++;
		gend[ng] = k;
		if (gend[ng] == gstart[ng]) { say('E', "a passage needs something at both ends", 0, 0, ""); return; }
		if (k < nt) arrows[ng] = &t[k++];
		ng++;
	}
	if (ng < 2) {
		if (nt == 1 && t[0].t == T_WORD) { say('E', "", t[0].s, t[0].n, " goes nowhere; add a passage like -> out"); return; }
		say('E', "I don't understand this line", 0, 0, "");
		return;
	}
	for (int g = 0; g + 1 < ng; g++) {
		/* each source of group g, each destination of group g+1 */
		int ka = gstart[g];
		while (ka < gend[g]) {
			endpoint a;
			if (endpoint_at(t, gend[g], &ka, 0, &a) < 0) return;
			int kb = gstart[g + 1];
			while (kb < gend[g + 1]) {
				endpoint b;
				if (endpoint_at(t, gend[g + 1], &kb, 1, &b) < 0) return;
				add_cord(&a, &b, arrows[g]->kind, arrows[g]->amt, 0.f, arrows[g]->has_amt);
			}
		}
	}
}

static void statement(const char *s, int n)
{
	tok t[MAX_TOK];
	int nt = tokenize(s, n, t);
	if (nt <= 0) return;
	if (t[0].t == T_WORD && (str_eq(t[0].s, t[0].n, "the") || str_eq(t[0].s, t[0].n, "The"))) { st_the(t, nt); return; }
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "tuning") && nt > 2 && t[1].t == T_WORD && t[2].t == T_COLON) return;   /* read by read_tunings() */
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "lucid") && nt > 1 && t[1].t == T_WORD) { st_lucid(t, nt); return; }
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "drift") && nt == 2 && t[1].t == T_NUM) {
		G.drift = parse_num(t[1].s, t[1].n);    /* otherwise "drift" is just a name */
		return;
	}
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "instrument") && nt == 2 && t[1].t == T_WORD) return;   /* read at load */
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "sleepers")) { say('W', "sleepers (voices) are not awake yet", 0, 0, ""); return; }
	st_chain(t, nt);
}

/* ------------------------------------------------------------- text & scenes */

static int line_end(int i) { while (i < G.len && G.text[i] != '\n') i++; return i; }

static int is_dashes(int a, int b)
{
	int dashes = 0;
	for (int i = a; i < b; i++) {
		char c = G.text[i];
		if (c == '-') dashes++;
		else if (c != ' ' && c != '\t' && c != '\r') return 0;
	}
	return dashes >= 3;
}

static int is_blank(int a, int b)
{
	for (int i = a; i < b; i++) {
		char c = G.text[i];
		if (c == ';') return 1;
		if (c != ' ' && c != '\t' && c != '\r') return 0;
	}
	return 1;
}

static void find_scenes(void)
{
	G.nsc = 0;
	G.pre_end = G.len;
	int i = 0, line = 1;
	while (i <= G.len) {
		int e = line_end(i);
		int nx = e + 1;
		if (nx <= G.len && !is_blank(i, e)) {
			int e2 = line_end(nx);
			if (is_dashes(nx, e2) && G.nsc < SCQ_MAX_SCENES) {
				if (G.nsc == 0) G.pre_end = i;
				else G.sc[G.nsc - 1].end = i;
				scene *s = &G.sc[G.nsc++];
				s->ts = i; s->te = e;
				s->start = e2 + 1 > G.len ? G.len : e2 + 1;
				s->end = G.len;
				s->line0 = line + 2;
				i = e2 + 1; line += 2;
				continue;
			}
		}
		i = nx; line++;
	}
	if (G.nsc == 0) {          /* no dates: the whole page is one scene */
		G.pre_end = 0;
		G.sc[0].ts = G.sc[0].te = 0;
		G.sc[0].start = 0; G.sc[0].end = G.len; G.sc[0].line0 = 1;
		G.nsc = 1;
	}
}

static void compile_range(int a, int b, int line0)
{
	int i = a;
	G.line = line0;
	while (i < b) {
		int e = line_end(i);
		if (e > b) e = b;
		int c = i;
		while (c < e && G.text[c] != ';') c++;
		int s = i;
		while (s < c && (G.text[s] == ' ' || G.text[s] == '\t')) s++;
		if (s < c) statement(G.text + s, c - s);
		i = e + 1;
		G.line++;
	}
}

/* ------------------------------------------------------------- the load estimate
 *
 * What a scene asks of the SP-1's processor, in cycles per sample (the chip has 1333 at 48 kHz).
 * It is counted the way run_block() spends it: each place's own work (measured on SP-1s, one
 * place at a time), and everything gather() and mix() do to bring it its inlets: zeroing and
 * summing a mixed inlet, each cord by its kind, an attenuverter or a panel knob on a patched
 * inlet, the tune knob, a default that found no constant block, and the decimate / hold of a
 * place at a lower samplerate. Every figure is in scq_costs.h, fitted by tools/calibrate.py
 * from whole patches measured on SP-1s (tools/sp1fleet.py). */
enum { CT_FIXED, CT_MIXED, CT_HALL, CT_FOG, CT_WATER, CT_DOOR, CT_RANGE, CT_BOTTLE, CT_MAYBE,
       CT_SHAPE, CT_FILL, CT_SLOW, CT_SLOW_IN, CT_SLOW_OUT, CT_N };
static const float COST_TERM[CT_N] = {
	SCQ_COST_FIXED, SCQ_COST_MIXED, SCQ_COST_HALL, SCQ_COST_FOG, SCQ_COST_WATER, SCQ_COST_DOOR,
	SCQ_COST_RANGE, SCQ_COST_BOTTLE, SCQ_COST_MAYBE, SCQ_COST_SHAPE, SCQ_COST_FILL,
	SCQ_COST_SLOW_FIXED, SCQ_COST_SLOW_INLET, SCQ_COST_SLOW_OUTLET };

static float place_cycles(int type)
{
	static uint16_t known[256];           /* 0: not looked up yet */
	if (!known[type]) {
		const char *name = SCQ_PLACES[type].name;
		uint16_t c = SCQ_PLACES[type].cost;       /* a place not measured yet: its author's guess */
		for (unsigned i = 0; i < sizeof SCQ_MEASURED / sizeof SCQ_MEASURED[0]; i++)
			if (str_eq(name, str_len(name), SCQ_MEASURED[i].name)) { c = SCQ_MEASURED[i].cycles; break; }
		known[type] = c ? c : 1;
	}
	return (float)known[type];
}

/* the same on the Wingie: its own measured figure, or the SP-1's times the measured median ratio */
static float place_cycles_wingie(int type)
{
	static uint16_t known[256];
	if (!known[type]) {
		const char *name = SCQ_PLACES[type].name;
		float c = place_cycles(type) * SCQ_WINGIE_SCALE_MEASURED;
		for (unsigned i = 0; i < sizeof SCQ_WINGIE_MEASURED / sizeof SCQ_WINGIE_MEASURED[0]; i++)
			if (str_eq(name, str_len(name), SCQ_WINGIE_MEASURED[i].name)) { c = (float)SCQ_WINGIE_MEASURED[i].cycles; break; }
		known[type] = (uint16_t)(c < 1.f ? 1.f : (c > 65000.f ? 65000.f : c));
	}
	return (float)known[type];
}

static inline int steady(const float *p);

/* how much of each kind of work the compiled scene has: t[CT_N], and per place w[type] (how many
 * of it run, a slow one counting 1/divisor); either may be 0. It follows gather(), mix() and
 * run_slow() pass for pass: a pass over a slow place's short block counts 1/divisor. */
static void cost_count(float *t, float *w)
{
	static int frogs = -2;
	if (frogs == -2) frogs = find_place("frogs", 5);
	if (t) { for (int i = 0; i < CT_N; i++) t[i] = 0.f; t[CT_FIXED] = 1.f; }
	for (int k = 0; k < G.nn; k++) {
		const node *n = &G.n[k];
		const place *pl = &SCQ_PLACES[n->type];
		const float inv = 1.f / (float)n->rdiv;
		float runs = inv;
		if (n->res && n->rdiv == 1) runs *= 1.f + (32.f / (float)n->res - 1.f) * 0.25f;   /* per-block work repeated (estimate) */
		if (frogs >= 0 && pl->run == SCQ_PLACES[frogs].run)          /* measured with 3 frogs */
			runs *= scq_clamp(n->nature, 1.f, 8.f) / 3.f;
		if (w) w[n->type] += runs;
		if (!t) continue;
		uint16_t full = 0;                    /* inlets summed on every sample: a cord there remembers */
		if (n->rdiv > 1)
			for (int c = G.cstart[k]; c < G.cstart[k + 1]; c++)
				if (!G.c[c].direct && (G.c[c].kind == K_FOG || G.c[c].kind == K_WATER || G.c[c].kind == K_MAYBE))
					full |= (uint16_t)(1u << G.c[c].dport);
		for (int c = G.cstart[k]; c < G.cstart[k + 1]; c++)
			if (!G.c[c].direct) t[CT_HALL + G.c[c].kind] += (full & (1u << G.c[c].dport)) ? 1.f : inv;
		for (int p = 0; p < n->nin; p++) {
			const uint16_t bit = (uint16_t)(1u << p);
			const int shaped = ((n->attset | n->knobset) & n->patched & bit) || (p == n->pitchin && n->tune != 0.f);
			if (n->mixed & bit) {
				t[CT_MIXED] += (full & bit) ? 1.f : inv;
				if (shaped) t[CT_SHAPE] += inv;
				else if (full & bit) t[CT_SLOW_IN] += inv;           /* every step-th sample of the sum */
			} else if (n->direct[p] && !steady(n->direct[p])) {
				if (shaped) t[CT_SHAPE] += inv;
				else if (n->rdiv > n->dshort[p]) t[CT_SLOW_IN] += inv;   /* every step-th sample of the wire */
			} else if (shaped || (!n->direct[p] && n->def[p] != 0.f)) {
				t[CT_FILL] += inv;                                   /* a constant written out */
			}
		}
		if (n->rdiv > 1) {
			t[CT_SLOW] += 1.f;
			for (int o = 0; o < n->nout; o++) if (n->held & (1u << o)) t[CT_SLOW_OUT] += 1.f;   /* the hold */
		}
	}
}

static float cost_estimate(void)
{
	static float w[256];
	float t[CT_N], cyc = 0.f;
	for (int i = 0; i < SCQ_NPLACES && i < 256; i++) w[i] = 0.f;
	cost_count(t, w);
	if (G.instrument == SCQ_WINGIE) {     /* measured on a Wingie (scq_costs_wingie.h) */
		cyc = SCQ_WINGIE_FIXED;
		for (int i = CT_FIXED + 1; i < CT_N; i++) cyc += t[i] * COST_TERM[i] * SCQ_WINGIE_SCALE_MEASURED;
		for (int i = 0; i < SCQ_NPLACES && i < 256; i++) if (w[i] != 0.f) cyc += w[i] * place_cycles_wingie(i);
		return cyc / (float)SCQ_WINGIE_CYCLES_PER_SAMPLE;
	}
	for (int i = 0; i < CT_N; i++) cyc += t[i] * COST_TERM[i];
	for (int i = 0; i < SCQ_NPLACES && i < 256; i++) if (w[i] != 0.f) cyc += w[i] * place_cycles(i);
	return cyc / (float)SCQ_CYCLES_PER_SAMPLE;
}

float scq_cycles_per_sample(int instrument)
{
	return instrument == SCQ_WINGIE ? (float)SCQ_WINGIE_CYCLES_PER_SAMPLE : (float)SCQ_CYCLES_PER_SAMPLE;
}

/* a block that holds v forever, shared by every inlet that wants it (0 if the table is full) */
static const float *konst(float v)
{
	for (int j = 0; j < G.nk; j++) if (G.kval[j] == v) return G.konst[j];
	if (G.nk >= MAX_KONST) return 0;
	G.kval[G.nk] = v;
	LOOP G.konst[G.nk][i] = v;
	return G.konst[G.nk++];
}

static int visit(int k, uint8_t *mark, int *pos)
{
	if (mark[k] == 2) return 0;
	if (mark[k] == 1) return 0;           /* a loop: it hears last block */
	mark[k] = 1;
	for (int c = G.cstart[k]; c < G.cstart[k + 1]; c++)
		if (G.c[c].src >= 0) visit(G.c[c].src, mark, pos);
	mark[k] = 2;
	G.order[(*pos)++] = (uint8_t)k;
	return 0;
}

const float *scq_tuning(int i) { return i >= 0 && i < G.ntun ? G.tun[i].d : 0; }

static int find_tuning(const char *w, int n)
{
	for (int i = 0; i < G.ntun; i++) if (str_eq(w, n, G.tun[i].name)) return i;
	return -1;
}

/* Tunings belong to the whole dream, wherever they are written (the editor adds the ones your
 * dream names from the scales folder, at the end):
 *   tuning young_wtp: 203.91 ... 1200        cents of each degree after the root; the last is the period
 * A long tuning continues on the next line with the same name. Kept in the pool as
 * period, n, then n degrees (0 first). */
static void read_tunings(void)
{
	G.ntun = 0;
	int i = 0, line = 1, open = -1;
	while (i <= G.len) {
		int e = line_end(i), c = i;
		while (c < e && G.text[c] != ';') c++;
		int s = i;
		while (s < c && (G.text[s] == ' ' || G.text[s] == '\t')) s++;
		int is_tuning = c - s > 7 && str_eq(G.text + s, 6, "tuning") && (G.text[s + 6] == ' ' || G.text[s + 6] == '\t');
		if (!is_tuning) {
			if (s < c) open = -1;          /* anything else ends a tuning that's being continued */
		} else {
			tok t[MAX_TOK];
			G.line = line;
			int nt = tokenize(G.text + s, c - s, t);
			if (nt < 4 || t[1].t != T_WORD || t[2].t != T_COLON) { say('E', "write a tuning as: tuning name: cents ... period", 0, 0, ""); goto next; }
			int k = find_tuning(t[1].s, t[1].n);
			if (k >= 0 && k != open) { say('E', "there's already a tuning called ", t[1].s, t[1].n, ""); goto next; }
			if (k < 0) {
				if (G.ntun >= MAX_TUNINGS) { say('E', "too many tunings (32)", 0, 0, ""); goto next; }
				k = G.ntun;
				float *d = (float *)pool_take(2 * sizeof(float));
				if (!d) { say('E', "no room left for the tuning ", t[1].s, t[1].n, ""); goto next; }
				int nl = t[1].n < NAME_LEN - 1 ? t[1].n : NAME_LEN - 1;
				for (int j = 0; j < nl; j++) G.tun[k].name[j] = t[1].s[j];
				G.tun[k].name[nl] = 0;
				d[0] = 0.f; d[1] = 0.f;
				G.tun[k].d = d;
				G.ntun++;
				open = k;
			}
			for (int j = 3; j < nt; j++) {
				if (t[j].t != T_NUM) { say('E', "a tuning is cents: ", t[j].s, t[j].n, " isn't a number"); break; }
				float *v = (float *)pool_take(sizeof(float));     /* right after the others: this tuning was the last thing taken */
				if (!v) { say('E', "no room left for the tuning ", t[1].s, t[1].n, ""); break; }
				*v = parse_num(t[j].s, t[j].n);
				G.tun[k].d[1] += 1.f;
			}
		}
	next:
		i = e + 1; line++;
	}
	/* raw cents (the last is the period) -> period, n, 0, the rest */
	for (int k = 0; k < G.ntun; k++) {
		float *d = G.tun[k].d;
		int n = (int)d[1];
		if (n < 1 || d[2 + n - 1] <= 0.f) { d[0] = 1200.f; d[1] = -12.f; continue; }   /* empty or no period: chromatic */
		d[0] = d[2 + n - 1];
		for (int j = n - 1; j >= 1; j--) d[2 + j] = d[2 + j - 1];
		d[2] = 0.f;
	}
}

/* `instrument wingie` / `instrument sp1` anywhere in the dream (usually its first line); without
 * one, a dream is for the instrument the engine was built for (the SP-1 on the desktop) */
#ifndef SCQ_INSTRUMENT
#define SCQ_INSTRUMENT SCQ_SP1
#endif
static int read_instrument(void)
{
	int i = 0;
	while (i < G.len) {
		int e = line_end(i), a = i;
		while (a < e && (G.text[a] == ' ' || G.text[a] == '\t')) a++;
		if (e - a > 11 && str_eq(G.text + a, 10, "instrument") && (G.text[a + 10] == ' ' || G.text[a + 10] == '\t')) {
			int w = a + 10;
			while (w < e && (G.text[w] == ' ' || G.text[w] == '\t')) w++;
			int we = w;
			while (we < e && G.text[we] > ' ' && G.text[we] != ';') we++;
			if (str_eq(G.text + w, we - w, "wingie")) return SCQ_WINGIE;
			if (str_eq(G.text + w, we - w, "sp1") || str_eq(G.text + w, we - w, "sp-1")) return SCQ_SP1;
		}
		i = e + 1;
	}
	return SCQ_INSTRUMENT;
}

static void compile_scene(int idx, int report)
{
	G.nn = 0; G.nc = 0; G.used = 0; G.wanted = 0; G.drift = 0.f; G.nk = 0;
	G.quiet = !(report & 1);            /* the prelude is reported once */
	read_tunings();
	compile_range(0, G.pre_end, 1);
	G.quiet = !(report & 2);
	scene *s = &G.sc[idx];
	compile_range(s->start, s->end, s->line0);
	G.quiet = 0;

	for (int i = 0; i < G.nn; i++) {
		const place *pl = &SCQ_PLACES[G.n[i].type];
		if (pl->init) pl->init(&G.n[i]);
	}
	/* sort cords by destination, stable */
	for (int i = 1; i < G.nc; i++) {
		cord c = G.c[i];
		int j = i - 1;
		while (j >= 0 && G.c[j].dst > c.dst) { G.c[j + 1] = G.c[j]; j--; }
		G.c[j + 1] = c;
	}
	int c = 0;
	for (int k = 0; k <= G.nn; k++) {
		while (c < G.nc && G.c[c].dst < k) c++;
		G.cstart[k] = (uint16_t)c;
	}
	for (int k = 0; k < G.nc; k++) G.n[G.c[k].dst].patched |= (uint16_t)(1u << G.c[k].dport);
	/* how each inlet gets its block: a lone plain wire from another place is read where it
	 * lies (no copy); anything else is summed into scratch each block */
	for (int k = 0; k < G.nn; k++) {
		node *n = &G.n[k];
		pitch_setup(n);
		for (int p = 0; p < n->nin; p++) {
			int count = 0, only = -1;
			for (int c = G.cstart[k]; c < G.cstart[k + 1]; c++)
				if (G.c[c].dport == p) { count++; only = c; }
			n->direct[p] = 0;
			if (count == 0) {                     /* unpatched: its default, as a constant block */
				if (n->def[p] != 0.f) n->direct[p] = konst(n->def[p]);
				continue;
			}
			cord *c = &G.c[only];
			if (count == 1 && c->kind == K_HALL && c->a == 1.f && c->src >= 0 && c->src != k) {
				n->direct[p] = G.n[c->src].out[c->sport];
				c->direct = 1;
			} else if (count == 1 && c->kind == K_BOTTLE && (n->direct[p] = konst(c->a)) != 0) {
				c->direct = 1;                    /* a lone value: a constant block too */
			} else {
				n->mixed |= (uint16_t)(1u << p);
			}
		}
	}
	/* A place at a lower samplerate makes a short block (B / its divisor). An outlet is held out
	 * to the full block only if someone needs it so: a place running faster than it, or a cord
	 * that remembers (fog, water, maybe: those listen to every sample), or any cord into an
	 * inlet that has one. Everyone else reads the short block as it is: a slow place feeding a
	 * place at its own rate costs nothing between them. */
	{
		uint16_t remembers[MAX_NODES] = { 0 };
		for (int k = 0; k < G.nc; k++)
			if (G.c[k].kind == K_FOG || G.c[k].kind == K_WATER || G.c[k].kind == K_MAYBE)
				remembers[G.c[k].dst] |= (uint16_t)(1u << G.c[k].dport);
		for (int k = 0; k < G.nn; k++) { G.n[k].held = 0; for (int p = 0; p < MAX_IN; p++) G.n[k].dshort[p] = 1; }
		for (int k = 0; k < G.nc; k++) {
			const cord *c = &G.c[k];
			if (c->src < 0 || G.n[c->src].rdiv <= 1) continue;
			if (G.n[c->dst].rdiv < G.n[c->src].rdiv || (remembers[c->dst] & (1u << c->dport)))
				G.n[c->src].held |= (uint8_t)(1u << c->sport);
		}
		for (int k = 0; k < G.nc; k++) {
			const cord *c = &G.c[k];
			if (c->direct && c->src >= 0 && G.n[c->src].rdiv > 1 && !(G.n[c->src].held & (1u << c->sport)))
				G.n[c->dst].dshort[c->dport] = G.n[c->src].rdiv;
		}
	}
	uint8_t mark[MAX_NODES] = { 0 };
	int pos = 0;
	for (int k = 0; k < G.nn; k++) visit(k, mark, &pos);

	G.load = cost_estimate();
}

/* ------------------------------------------------------------- the public face */

#ifdef SCQ_HOST
/* Carrying the sound over an edit (the desktop editor reloads the dream on every change).
 * Without it every place starts again from silence: delays and reverbs lose their tails,
 * oscillators restart, frogs wait up to 2.5 s for their first chirp. Each place that is still
 * there after the edit (the same name, place, nature and samplerate) keeps its state, its
 * memory and its last block. */
typedef struct {
	char     name[NAME_LEN];
	uint8_t  type, rdiv;
	float    given;
	uint32_t memlen, memoff, rng;
	float    s[MAX_STATE];
	float    out[MAX_OUT][B];
} carry_node;
static carry_node carry[MAX_NODES];
static uint32_t   carry_mem[POOL_BYTES / 4];
static int        ncarry, carry_scene = -1;

static void carry_save(void)
{
	uint32_t used = 0;
	ncarry = 0;
	carry_scene = G.cur;
	for (int k = 0; k < G.nn; k++) {
		const node *n = &G.n[k];
		carry_node *c = &carry[ncarry++];
		for (int i = 0; i < NAME_LEN; i++) c->name[i] = n->name[i];
		c->type = n->type; c->rdiv = n->rdiv; c->given = n->given; c->rng = n->rng;
		for (int i = 0; i < MAX_STATE; i++) c->s[i] = n->s[i];
		for (int o = 0; o < MAX_OUT; o++) for (int i = 0; i < B; i++) c->out[o][i] = n->out[o][i];
		c->memlen = 0;
		uint32_t words = (n->memlen + 3u) / 4u;
		if (n->mem && n->memlen && used + words <= POOL_BYTES / 4) {
			const uint32_t *m = (const uint32_t *)n->mem;
			for (uint32_t i = 0; i < words; i++) carry_mem[used + i] = m[i];
			c->memoff = used; c->memlen = n->memlen;
			used += words;
		}
	}
}

static void carry_restore(void)
{
	if (G.cur != carry_scene) return;                 /* another scene: it starts fresh */
	for (int k = 0; k < G.nn; k++) {
		node *n = &G.n[k];
		for (int j = 0; j < ncarry; j++) {
			const carry_node *c = &carry[j];
			if (c->type != n->type || c->rdiv != n->rdiv || c->given != n->given) continue;
			int same = 1;
			for (int i = 0; i < NAME_LEN && same; i++) { if (c->name[i] != n->name[i]) same = 0; if (!c->name[i]) break; }
			if (!same) continue;
			for (int i = 0; i < MAX_STATE; i++) n->s[i] = c->s[i];
			for (int o = 0; o < MAX_OUT; o++) for (int i = 0; i < B; i++) n->out[o][i] = c->out[o][i];
			n->rng = c->rng;
			if (c->memlen && c->memlen == n->memlen && n->mem) {
				uint32_t *m = (uint32_t *)n->mem;
				for (uint32_t i = 0; i < (c->memlen + 3u) / 4u; i++) m[i] = carry_mem[c->memoff + i];
			}
			break;
		}
	}
}
#endif

int scq_load(const char *text, int len, uint32_t nights)
{
#ifdef SCQ_HOST
	const int carrying = Gp == &G0 && G.nn > 0;      /* the engine that plays, not the checker */
	if (carrying) carry_save();
#endif
	if (len > SCQ_MAX_TEXT - 1) len = SCQ_MAX_TEXT - 1;
#ifdef SCQ_DEVICE
	G.text = text;               /* not copied: it must stay as it is while this dream plays (scq.h) */
#else
	for (int i = 0; i < len; i++) G.text[i] = text[i];
	G.text[len] = 0;
#endif
	G.len = len;
	G.nights = nights;
	G.instrument = read_instrument();
	if (Gp == &G0) W.instrument = G.instrument;
	G.msgn = 0; G.msg[0] = 0; G.errors = 0;
	if (!G.rng) G.rng = 0x5eed1234u ^ nights;
	find_scenes();
#if defined(SCQ_DEVICE)
	if (G.instrument != SCQ_INSTRUMENT) {
		say('E', SCQ_INSTRUMENT == SCQ_WINGIE ? "this dream is for the SP-1, and this is a Wingie"
		                                      : "this dream is for the Wingie, and this is an SP-1", 0, 0, "");
		G.nn = 0; G.nc = 0; G.nsc = 0;
		return G.errors;
	}
#endif
	for (int s = 0; s < G.nsc; s++) { compile_scene(s, s == 0 ? 3 : 2); G.scene_load[s] = G.load; }
	int errors = G.errors;
	if (G.cur >= G.nsc) G.cur = 0;
	compile_scene(G.cur, 0);
#ifdef SCQ_HOST
	if (carrying) carry_restore();
#endif
	G.errors = errors;
	G.pending = -1;
	G.gain = 1.f; G.fading = 0; G.waiting = 0;
	return errors;
}

const char *scq_messages(void) { return G.msg; }

#ifdef SCQ_HOST
/* For tools/calibrate.py: what the load estimate counted in a scene, one line:
 *   "fixed=1 mixed=3 hall=2 ... | sine=1 saw=0.25 ..."   (the dream must be loaded) */
const char *scq_cost_terms(int scene)
{
	static const char *const NAMES[CT_N] = { "fixed", "mixed", "hall", "fog", "water", "door", "range", "bottle",
		"maybe", "shape", "fill", "slow", "slow_in", "slow_out" };
	static char out[4096];
	static float w[256];
	float t[CT_N];
	sb s = { out, 0, (int)sizeof out };
	out[0] = 0;
	if (scene < 0 || scene >= G.nsc) return out;
	const int was = G.cur;
	compile_scene(scene, 0);
	for (int i = 0; i < SCQ_NPLACES && i < 256; i++) w[i] = 0.f;
	cost_count(t, w);
	for (int i = 0; i < CT_N; i++) { sb_s(&s, NAMES[i]); sb_c(&s, '='); sb_f(&s, t[i]); sb_c(&s, ' '); }
	sb_c(&s, '|');
	for (int i = 0; i < SCQ_NPLACES && i < 256; i++)
		if (w[i] != 0.f) { sb_c(&s, ' '); sb_s(&s, SCQ_PLACES[i].name); sb_c(&s, '='); sb_f(&s, w[i]); }
	compile_scene(was, 0);
	return out;
}
#endif

#ifdef SCQ_HOST
/* Check a text without touching what is playing (the desktop editor only: it costs a
 * second engine's worth of memory, which the SP-1 doesn't have). */
static engine G1;
static float checked_load[SCQ_MAX_SCENES];
static int checked_scenes;
static int checked_instrument;
int scq_checked_scenes(void) { return checked_scenes; }
int scq_checked_instrument(void) { return checked_instrument; }
float scq_checked_load(int s) { return (s >= 0 && s < checked_scenes) ? checked_load[s] : 0.f; }

int scq_check(const char *text, int len, uint32_t nights, char *msgs, int cap)
{
	Gp = &G1;
	G.cur = 0;
	int e = scq_load(text, len, nights);
	int n = 0;
	while (G.msg[n] && n < cap - 1) { msgs[n] = G.msg[n]; n++; }
	if (cap > 0) msgs[n] = 0;
	checked_scenes = G.nsc;
	checked_instrument = G.instrument;
	for (int k = 0; k < G.nsc; k++) checked_load[k] = G.scene_load[k];
	Gp = &G0;
	return e;
}
#endif

void scq_set_tune(int index, float octaves)
{
	if (index < 0 || index >= G.nn) return;
	G.n[index].tune = octaves;
}

float scq_get_amount(int index)
{
	if (index < 0 || index >= G.nc) return 0.f;
	const cord *c = &G.c[index];
	switch (c->kind) {
	case K_WATER: return c->b;
	case K_DOOR:  return 0.f;
	default:      return c->a * 100.f;
	}
}

float scq_get_tune(int index) { return index >= 0 && index < G.nn ? G.n[index].tune : 0.f; }
float scq_get_att(int index, int inlet)
{
	return index >= 0 && index < G.nn && inlet >= 0 && inlet < G.n[index].nin ? G.n[index].att[inlet] : 1.f;
}
float scq_get_param(int index, int k) { return index >= 0 && index < G.nn && k >= 0 && k < MAX_OPT ? G.n[index].opt[k] : 0.f; }
float scq_get_control(int which) { return which >= 0 && which < SCQ_CTL_COUNT ? W.ctl[which] : 0.f; }

float scq_get_knob(int index, int inlet)
{
	if (index < 0 || index >= G.nn || inlet < 0 || inlet >= G.n[index].nin) return 0.f;
	return place_def(&SCQ_PLACES[G.n[index].type], inlet) + G.n[index].koff[inlet];
}

void scq_set_knob(int index, int inlet, float value)
{
	if (index < 0 || index >= G.nn || inlet < 0 || inlet >= G.n[index].nin) return;
	node *n = &G.n[index];
	n->koff[inlet] = value - place_def(&SCQ_PLACES[n->type], inlet);
	n->def[inlet] = value;
	n->knobset |= (uint16_t)(1u << inlet);
	if (!(n->patched & (1u << inlet))) n->direct[inlet] = 0;   /* read def from now on, not a shared constant */
}

void scq_set_param(int index, int k, float value)
{
	if (index < 0 || index >= G.nn || k < 0 || k >= MAX_OPT) return;
	G.n[index].opt[k] = value;
}

void scq_set_att(int index, int inlet, float amount)
{
	if (index < 0 || index >= G.nn || inlet < 0 || inlet >= G.n[index].nin) return;
	node *n = &G.n[index];
	amount = scq_clamp(amount, -ATT_MAX, ATT_MAX);
	n->att[inlet] = amount;
	if (amount != 1.f) n->attset |= (uint16_t)(1u << inlet);
	else n->attset &= (uint16_t)~(1u << inlet);
}

void scq_set_amount(int index, float amount)
{
	if (index < 0 || index >= G.nc) return;
	cord *c = &G.c[index];
	if (c->direct) {                 /* read in place until now: from here on it is summed */
		node *n = &G.n[c->dst];
		c->direct = 0;
		n->direct[c->dport] = 0;
		n->mixed |= (uint16_t)(1u << c->dport);
	}
	switch (c->kind) {
	case K_HALL: case K_FOG: case K_BOTTLE: case K_MAYBE: c->a = amount * 0.01f; break;
	case K_WATER: c->b = amount; c->a = scq_coef_time(amount * 0.001f, SR / (float)B); break;
	case K_RANGE: c->a = amount * 0.01f; break;
	default: break;
	}
}

void scq_goto_scene(int index)
{
	if (G.nsc <= 0) return;
	G.pending = ((index % G.nsc) + G.nsc) % G.nsc;
}

void scq_memory(uint32_t *used, uint32_t *total)
{
	if (used) *used = G.wanted > G.used ? G.wanted : G.used;   /* more than there is, if it didn't fit */
	if (total) *total = (uint32_t)POOL_SIZE;
}

int scq_scene(void) { return G.cur; }
int scq_node_count(void) { return G.nn; }
int scq_scene_count(void) { return G.nsc; }
float scq_scene_load(int s) { return (s >= 0 && s < G.nsc) ? G.scene_load[s] : 0.f; }

void scq_control(int which, float value)
{
	if (which >= 0 && which < SCQ_CTL_COUNT) W.ctl[which] = value;
}

float scq_lamp(int led) { return (led >= 0 && led < SCQ_LEDS) ? W.lamp[led] : 0.f; }
uint32_t scq_healed(void) { return G.healed; }

float scq_probe(int node, int port)
{
	if (node < 0 || node >= G.nn || port < 0 || port >= G.n[node].nout) return 0.f;
	return G.n[node].out[port][B - 1];
}

static const float ZEROS[B];

/* Bringing a place its inlets is most of what a big dream costs the SP-1, so each inlet's block
 * is made in as few passes as it can be, and only as long as the place will read it: a place at
 * a lower samplerate (step > 1) gets cnt = B / step samples, taken from every step-th one.
 *   - a lone plain wire, or a constant: the block is read where it lies, nothing is done;
 *   - several cords, or one with an amount: the first cord writes the sum, the others add to it
 *     (no clearing first), and at a lower samplerate only the samples that will be read;
 *   - an attenuverter, a turned knob, the tune knob: one pass for all three.
 * Fog, water and maybe-cords remember, so they always run on every sample, and a slow place
 * takes every step-th one of their sum. */

/* a samplerate divisor (1, 2, 4, 8) as a shift */
static inline int rshift(int d) { return d >= 8 ? 3 : (d >= 4 ? 2 : (d >= 2 ? 1 : 0)); }

/* a block that never changes: the shared zeros, or a constant block (a default, a lone value) */
static inline int steady(const float *p)
{
	return p == ZEROS || (uintptr_t)p - (uintptr_t)&G.konst[0][0] < (uintptr_t)sizeof G.konst;
}

/* dst[j] = or += what the cord gives at sample j of m, reading every st-th sample of src */
#define MIXLOOP(expr) do { \
		if (first) { for (int j = 0; j < m; j++) { const float x = src[j * st]; dst[j] = (expr); } } \
		else       { for (int j = 0; j < m; j++) { const float x = src[j * st]; dst[j] += (expr); } } \
	} while (0)

/* sums every mixed inlet of place k into scratch; returns the inlets already at the place's own
 * rate (cnt samples), the others hold all B and gather() takes every step-th */
static uint16_t mix(int k, float scratch[MAX_IN][B], int step, int cnt)
{
	uint16_t seen = 0, full = 0;
	if (step > 1)                            /* inlets with a cord that remembers: summed on every sample */
		for (int ci = G.cstart[k]; ci < G.cstart[k + 1]; ci++) {
			const cord *c = &G.c[ci];
			if (!c->direct && (c->kind == K_FOG || c->kind == K_WATER || c->kind == K_MAYBE)) full |= (uint16_t)(1u << c->dport);
		}
	for (int ci = G.cstart[k]; ci < G.cstart[k + 1]; ci++) {
		cord *c = &G.c[ci];
		if (c->direct) continue;
		const uint16_t bit = (uint16_t)(1u << c->dport);
		const int first = !(seen & bit);
		int st = (full & bit) ? 1 : step;
		const int m = (full & bit) ? B : cnt;
		float *dst = scratch[c->dport];
		seen |= bit;
		if (c->src >= 0 && G.n[c->src].rdiv > 1 && !(G.n[c->src].held & (1u << c->sport)))
			st >>= rshift(G.n[c->src].rdiv);     /* a short block: already every rdiv-th sample */
		if (c->kind == K_BOTTLE) {
			const float v = c->a;
			if (first) { for (int j = 0; j < m; j++) dst[j] = v; }
			else { for (int j = 0; j < m; j++) dst[j] += v; }
			continue;
		}
		const float *src = G.n[c->src].out[c->sport];
		switch (c->kind) {
		case K_HALL: { const float a = c->a; MIXLOOP(x * a); } break;
		case K_FOG: {
			float s = c->s;
			const float a = c->a, f = 0.18f;     /* ~1.5 kHz: a softening */
			MIXLOOP((s += f * (x * a - s)));
			c->s = scq_sane(s) ? s : 0.f;         /* what ran away upstream doesn't stay in the fog */
		} break;
		case K_WATER: {                         /* a capacitor: slow to fill, slow to drain */
			float s = c->s;
			const float stp = (src[B - 1] - s) * c->a, s0 = s;
			s += stp;
			MIXLOOP(((void)x, s0 + stp * (float)(j + 1) * (1.f / (float)B)));
			c->s = scq_sane(s) ? s : 0.f;
		} break;
		case K_DOOR: MIXLOOP(x > 0.f ? x : 0.f); break;
		case K_RANGE: { const float lo = c->a, d = c->b - c->a; MIXLOOP(lo + d * x); } break;
		case K_MAYBE: {                         /* each rise through zero rolls the dice again */
			float prev = c->s, open = c->b;
			const float chance = c->a;
			if (first) for (int j = 0; j < m; j++) dst[j] = 0.f;
			for (int j = 0; j < m; j++) {
				const float v = src[j];
				if (prev <= 0.f && v > 0.f) open = scq_rand01(&G.rng) < chance ? 1.f : 0.f;
				prev = v;
				dst[j] += v * open;
			}
			c->s = prev; c->b = open;
		} break;
		}
	}
	return (uint16_t)(seen & ~full);
}

static void gather(node *n, int k, const float *in[MAX_IN], float scratch[MAX_IN][B])
{
	const int step = n->rdiv, cnt = B >> rshift(step);
	const uint16_t att = n->attset & n->patched, knob = n->knobset & n->patched;
	const uint16_t atrate = n->mixed ? mix(k, scratch, step, cnt) : 0;
	for (int p = 0; p < n->nin; p++) {
		const uint16_t bit = (uint16_t)(1u << p);
		const float tune = (p == n->pitchin) ? n->tune : 0.f;
		const int shaped = (att & bit) || (knob & bit) || tune != 0.f;   /* an attenuverter, a knob, the tune knob */
		const float g = (att & bit) ? n->att[p] : 1.f, o = (knob & bit) ? n->koff[p] : 0.f;
		const float *src;
		float *dst = scratch[p];
		int st = step;
		if (n->mixed & bit) { src = dst; if (atrate & bit) st = 1; }
		else if (n->direct[p]) { src = n->direct[p]; st = step >> rshift(n->dshort[p]); }
		else if (n->def[p] == 0.f) src = ZEROS;
		else {                                    /* a default that found no constant block */
			const float v = n->def[p] + tune;
			for (int j = 0; j < cnt; j++) dst[j] = v;
			in[p] = dst;
			continue;
		}
		if (!(n->mixed & bit) && steady(src)) {   /* a constant: shaped once, not sample by sample */
			if (!shaped) { in[p] = src; continue; }
			const float v = (g * src[0] + o) + tune;
			for (int j = 0; j < cnt; j++) dst[j] = v;
			in[p] = dst;
			continue;
		}
		if (!shaped) {
			if (st == 1) { in[p] = src; continue; }
			for (int j = 0; j < cnt; j++) dst[j] = src[j * st];           /* every step-th sample */
		} else if (tune == 0.f && !(knob & bit)) {
			for (int j = 0; j < cnt; j++) dst[j] = g * src[j * st];
		} else if (!(att & bit) && tune == 0.f) {
			for (int j = 0; j < cnt; j++) dst[j] = src[j * st] + o;
		} else if (!(att & bit) && !(knob & bit)) {
			for (int j = 0; j < cnt; j++) dst[j] = tune + src[j * st];
		} else {
			for (int j = 0; j < cnt; j++) dst[j] = tune + (g * src[j * st] + o);
		}
		in[p] = dst;
	}
}

/* a place at a lower samplerate: gather() gave it every rdiv-th sample of its inlets; it runs on
 * the shorter block, then each outlet sample is held for rdiv samples (the zero-order hold of a
 * sample-rate reducer) */
static void run_slow(node *n, const float *in[MAX_IN])
{
	const int d = n->rdiv, nb = B >> rshift(d);
	W.nb = nb; W.sr = SR / (float)d; W.isr = (float)d * INV_SR;
	SCQ_PLACES[n->type].run(n, in);
	W.nb = B; W.sr = SR; W.isr = INV_SR;
	for (int o = 0; o < n->nout; o++) {
		float *b = n->out[o];
		if (!(n->held & (1u << o))) { b[B - 1] = b[nb - 1]; continue; }   /* kept short; its last sample where the nurse and the editor look */
		float *q = b + B;                          /* from the end, so nothing is overwritten before it is read */
		if (d == 2) for (int j = nb - 1; j >= 0; j--) { const float v = b[j]; q -= 2; q[0] = v; q[1] = v; }
		else if (d == 4) for (int j = nb - 1; j >= 0; j--) { const float v = b[j]; q -= 4; q[0] = v; q[1] = v; q[2] = v; q[3] = v; }
		else for (int j = nb - 1; j >= 0; j--) {
			const float v = b[j];
			q -= 8;
			q[0] = v; q[1] = v; q[2] = v; q[3] = v; q[4] = v; q[5] = v; q[6] = v; q[7] = v;
		}
	}
}

/* A place that ran away is started again: its state, its memory and its block are cleared, as
 * when the scene was built. Nothing a place can be sent (a fader at the top through an
 * attenuverter at the top, its own sound fed back, a number too large to count) may leave it
 * broken until the program is restarted: whatever goes wrong inside lasts one block. */
static void heal(node *n)
{
	const place *pl = &SCQ_PLACES[n->type];
	for (int i = 0; i < MAX_STATE; i++) n->s[i] = 0.f;
	for (int o = 0; o < MAX_OUT; o++) LOOP n->out[o][i] = 0.f;
	if (n->mem && !(pl->flags & PF_KEEPMEM)) {
		uint32_t *m = (uint32_t *)n->mem;
		for (uint32_t i = 0; i < (n->memlen + 3u) / 4u; i++) m[i] = 0;
	}
	if (pl->init) pl->init(n);
	G.healed++;
	G.fever = 3;                             /* and look at its neighbours for a few blocks */
}

/* Is this place well? Every block: the last sample of each outlet (one compare each). Its
 * memory is looked at when its turn comes round (one place a block), and in every place for
 * a few blocks after any of them was healed, since what ran away has been heard downstream. */
static void nurse(node *n, int k)
{
	int ill = 0;
	for (int o = 0; o < n->nout; o++) ill |= !scq_sane(n->out[o][B - 1]);
	if (!ill && (G.fever || k == G.ward))
		for (int i = 0; i < MAX_STATE; i++) ill |= !(scq_abs(n->s[i]) < 2147483648.f);   /* a count or a phase past counting */
	if (ill) heal(n);
}

/* resolution=fine|audio: the place is run on short stretches of its block, each seeing its
 * inlets and writing its outlets from there on (every inlet and outlet block is a full block) */
static void run_fine(node *n, const float *in[MAX_IN])
{
	const float *sub[MAX_IN];
	float *outs[MAX_OUT];
	const int step = n->res, full = W.nb;
	for (int o = 0; o < MAX_OUT; o++) outs[o] = n->out[o];
	W.nb = step;
	for (int off = 0; off < full; off += step) {
		for (int k = 0; k < MAX_IN; k++) sub[k] = in[k] ? in[k] + off : in[k];
		for (int o = 0; o < n->nout; o++) n->out[o] = outs[o] + off;
		SCQ_PLACES[n->type].run(n, sub);
	}
	for (int o = 0; o < MAX_OUT; o++) n->out[o] = outs[o];
	W.nb = full;
}

static void run_block(void)
{
	static float scratch[MAX_IN][B];        /* static: off the audio thread's small stack */
	const float *in[MAX_IN];
	LOOP { W.outL[i] = 0.f; W.outR[i] = 0.f; }
	const int fever = G.fever;
	for (int o = 0; o < G.nn; o++) {
		int k = G.order[o];
		node *n = &G.n[k];
		gather(n, k, in, scratch);
		if (n->rdiv > 1) run_slow(n, in);
		else if (n->res) run_fine(n, in);
		else SCQ_PLACES[n->type].run(n, in);
		nurse(n, k);
	}
	if (fever && G.fever == fever) G.fever--;
	if (++G.ward >= G.nn) G.ward = 0;
	if ((W.want_next || W.want_back) && G.nsc > 1 && G.pending < 0) {
		int to = G.cur + (W.want_next ? 1 : -1);
		if (scq_rand01(&G.rng) < W.miss) {          /* you missed your stop */
			to = G.cur + 1 + (int)(scq_rand01(&G.rng) * (float)(G.nsc - 1));
		}
		G.pending = ((to % G.nsc) + G.nsc) % G.nsc;
	}
	W.want_next = W.want_back = 0;
}

/* the output stage: the soft clip, and silence for a sample that isn't a number (a place that
 * ran away is healed at the end of its block; until then nothing of it reaches the converter) */
static inline float out_clip(float x)
{
	if (scq_abs(x) <= 1.5f) return x - 0.148148f * x * x * x;    /* nearly always: one test, then scq_softclip's cubic */
	return x == x ? scq_softclip(x) : 0.f;
}

static const float *in_l, *in_r;
static const float SILENT_IN[SCQ_BLOCK];
void scq_input(const float *left, const float *right) { in_l = left; in_r = right; }
int scq_instrument(void) { return G.instrument; }

int scq_wanted_slot(int *slot, int *step)
{
	if (W.want_slot < 0 && W.want_step == 0) return 0;
	if (slot) *slot = W.want_slot;
	if (step) *step = W.want_step;
	W.want_slot = -1; W.want_step = 0;
	return 1;
}

void scq_render(float *left, float *right, int frames)
{
	for (int f = 0; f + B <= frames; f += B) {
		W.inL = in_l ? in_l + f : SILENT_IN;
		W.inR = in_r ? in_r + f : (in_l ? in_l + f : SILENT_IN);
		if (G.pending >= 0 && G.fading == 0) G.fading = -1;
		if (G.nn > 0 && !G.waiting) run_block();
		else LOOP { W.outL[i] = W.outR[i] = 0.f; }
		if (G.fading == 0) {
			LOOP { left[f + i] = out_clip(W.outL[i]); right[f + i] = out_clip(W.outR[i]); }
		} else LOOP {
			if (G.fading < 0) { G.gain -= 1.f / 480.f; if (G.gain < 0.f) G.gain = 0.f; }
			else { G.gain += 1.f / 480.f; if (G.gain >= 1.f) { G.gain = 1.f; G.fading = 0; } }
			left[f + i] = out_clip(W.outL[i]) * G.gain;
			right[f + i] = out_clip(W.outR[i]) * G.gain;
		}
		if (G.fading < 0 && G.gain <= 0.f) {
#if defined(SCQ_DEVICE)
			G.waiting = 1;                      /* the main loop compiles it */
#else
			G.cur = G.pending;
			G.pending = -1;
			compile_scene(G.cur, 0);            /* already reported at load */
			G.fading = 1;
#endif
		}
	}
}

/* Interface mode (the SP-1 as a USB sound card): the dream stops and lends its memory pool.
 * Nothing runs until scq_load() builds a dream again, which takes the pool back. */
void *scq_sleep(uint32_t *len)
{
	G.nn = 0; G.nc = 0; G.used = 0;
	G.pending = -1; G.waiting = 0; G.fading = 0; G.gain = 1.f;
	if (len) *len = (uint32_t)POOL_SIZE;
	return (void *)G.pool;
}

int scq_service(void)
{
	if (!G.waiting) return 0;
	G.cur = G.pending;
	G.pending = -1;
	compile_scene(G.cur, 0);
	G.waiting = 0;
	G.fading = 1;
	return 1;
}

#ifndef SCQ_HOST
const char *scq_describe(void) { return ""; }
const char *scq_catalog(void) { return ""; }
#else
const char *scq_describe(void)
{
	sb s = { G.desc, 0, (int)sizeof G.desc };
	G.desc[0] = 0;
	for (int k = 0; k < G.nsc; k++) {
		sb_s(&s, "S "); sb_i(&s, k); sb_c(&s, ' '); sb_i(&s, G.nsc); sb_c(&s, ' ');
		sb_i(&s, k == G.cur ? 1 : 0); sb_c(&s, ' ');
		if (G.sc[k].te > G.sc[k].ts) sb_sn(&s, G.text + G.sc[k].ts, G.sc[k].te - G.sc[k].ts);
		else sb_s(&s, "untitled");
		sb_c(&s, '\n');
	}
	for (int k = 0; k < G.nn; k++) {
		node *n = &G.n[k];
		sb_s(&s, "N "); sb_i(&s, k); sb_c(&s, ' ');
		sb_s(&s, SCQ_PLACES[n->type].name); sb_c(&s, ' ');
		sb_f(&s, n->nature); sb_c(&s, ' ');
		sb_s(&s, n->name); sb_c(&s, ' ');
		sb_i(&s, n->line);
		{   /* the switches and params written in the dream: name=value,name=value */
			optitem o;
			const char *spec = place_opts(SCQ_PLACES[n->type].name);
			int first = 1;
			for (int q = 0; q < MAX_OPT && opt_at(spec, q, &o); q++) {
				if (!(n->optset & (1u << q))) continue;
				sb_c(&s, first ? ' ' : ','); first = 0;
				sb_sn(&s, o.name, o.nl); sb_c(&s, '=');
				if (o.sw && (int)n->opt[q] >= opt_count(&o)) {          /* a tuning */
					const float *d = 0; int tn = (int)n->opt[q] - opt_count(&o);
					d = scq_tuning(tn);
					if (d) sb_s(&s, G.tun[tn].name);
				} else if (o.sw) {
					const char *v = o.vals, *end = o.vals + o.vl;
					for (int pos = 0; v < end; pos++) {
						const char *a = v;
						while (v < end && *v != '|') v++;
						if (pos == (int)n->opt[q]) sb_sn(&s, a, (int)(v - a));
						v++;
					}
				} else sb_f(&s, n->opt[q]);
			}
			for (int p = 0; p < n->nin; p++) {
				if (!(n->attset & (1u << p))) continue;
				sb_c(&s, first ? ' ' : ','); first = 0;
				sb_s(&s, "att.");
				const char *w = SCQ_PLACES[n->type].ins;
				for (int idx = 0; *w; idx++) {      /* the inlet's name */
					while (*w == ' ') w++;
					const char *a = w;
					while (*w && *w != ' ') w++;
					if (idx == p) { sb_sn(&s, a, (int)(w - a)); break; }
				}
				sb_c(&s, '='); sb_f(&s, n->att[p]);
			}
			for (int p = 0; p < n->nin; p++) {             /* panel knobs: INLET=value */
				if (!(n->knobset & (1u << p))) continue;
				sb_c(&s, first ? ' ' : ','); first = 0;
				const char *w = SCQ_PLACES[n->type].ins;
				for (int idx = 0; *w; idx++) {
					while (*w == ' ') w++;
					const char *a = w;
					while (*w && *w != ' ') w++;
					if (idx == p) { sb_sn(&s, a, (int)(w - a)); break; }
				}
				sb_c(&s, '='); sb_f(&s, place_def(&SCQ_PLACES[n->type], p) + n->koff[p]);
			}
			if (n->rdiv > 1) {
				sb_c(&s, first ? ' ' : ','); first = 0;
				sb_s(&s, n->rdiv == 2 ? "samplerate=half" : (n->rdiv == 4 ? "samplerate=quarter" : "samplerate=eighth"));
			}
			if (n->res) {
				sb_c(&s, first ? ' ' : ','); first = 0;
				sb_s(&s, n->res == 4 ? "resolution=fine" : "resolution=audio");
			}
		}
		sb_c(&s, '\n');
	}
	for (int k = 0; k < G.nc; k++) {
		cord *c = &G.c[k];
		sb_s(&s, "C "); sb_i(&s, c->src); sb_c(&s, ' '); sb_i(&s, c->sport); sb_c(&s, ' ');
		sb_i(&s, c->dst); sb_c(&s, ' '); sb_i(&s, c->dport); sb_c(&s, ' ');
		sb_i(&s, c->kind); sb_c(&s, ' ');
		sb_f(&s, c->kind == K_WATER ? c->b : c->a * 100.f); sb_c(&s, ' ');
		sb_f(&s, c->kind == K_RANGE ? c->b * 100.f : 0.f); sb_c(&s, ' ');
		sb_i(&s, c->line); sb_c(&s, '\n');
	}
	sb_s(&s, "R "); sb_i(&s, G.nn); sb_c(&s, ' '); sb_i(&s, MAX_NODES); sb_c(&s, ' ');
	sb_i(&s, G.nc); sb_c(&s, ' '); sb_i(&s, MAX_CORDS); sb_c(&s, ' ');
	sb_i(&s, G.len); sb_c(&s, ' '); sb_i(&s, SCQ_SLOT_TEXT); sb_c(&s, '\n');
	sb_s(&s, "M "); sb_i(&s, (int)(G.wanted > G.used ? G.wanted : G.used)); sb_c(&s, ' '); sb_i(&s, POOL_BYTES); sb_c(&s, ' ');
	sb_f(&s, G.load); sb_c(&s, '\n');
	return G.desc;
}

static char catalog_buf[98304];
const char *scq_catalog(void)
{
	static const char *fam[] = { "source", "shape", "space", "current", "hands", "ears" };
	sb s = { catalog_buf, 0, (int)sizeof catalog_buf };
	if (catalog_buf[0]) return catalog_buf;
	for (int p = 0; p < SCQ_NPLACES; p++) {
		const place *pl = &SCQ_PLACES[p];
		sb_s(&s, "P "); sb_s(&s, pl->name); sb_c(&s, ' ');
		sb_s(&s, fam[pl->family]); sb_c(&s, ' ');
		sb_i(&s, (int)place_cycles((int)(pl - SCQ_PLACES))); sb_c(&s, ' ');
		sb_f(&s, pl->nature); sb_c(&s, ' ');
		for (const char *q = pl->ins; *q; q++) sb_c(&s, *q == ' ' ? ',' : *q);
		sb_s(&s, " | ");
		for (const char *q = pl->outs; *q; q++) sb_c(&s, *q == ' ' ? ',' : *q);
		sb_s(&s, " | "); sb_s(&s, pl->help); sb_c(&s, '\n');
		if (!(pl->flags & PF_SINGLE) || *place_opts(pl->name)) {
			sb_s(&s, "O "); sb_s(&s, pl->name);
			if (*place_opts(pl->name)) { sb_c(&s, ' '); sb_s(&s, place_opts(pl->name)); }
			if (!(pl->flags & PF_SINGLE)) { sb_c(&s, ' '); sb_s(&s, RATE_OPT); }
			sb_c(&s, '\n');
		}
		if (*pl->defs) { sb_s(&s, "D "); sb_s(&s, pl->name); sb_c(&s, ' '); sb_s(&s, pl->defs); sb_c(&s, '\n'); }   /* the inlets' defaults */
		if (pl->flags & (PF_SP1 | PF_WINGIE)) {   /* hands that only one instrument has */
			sb_s(&s, "I "); sb_s(&s, pl->name); sb_s(&s, (pl->flags & PF_WINGIE) ? " wingie\n" : " sp1\n");
		}
	}
	return catalog_buf;
}
#endif
