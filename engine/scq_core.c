/*
 * Sleepcousonique — the language and the runner.
 *
 * A .dream file is a journal. A date (or any line) underlined with dashes starts a scene.
 * Lines before the first scene are the prelude, shared by every scene.
 *
 *   the hall is hallway 7               naming: a place with a fixed nature
 *   the route is train 4: 0 3 7 12      a train and its stations
 *   rain -> frogs ~> hall               passages: -> hall  ~> fog  => water  >| door
 *   lean1 -60> hall.length              a number inside the arrow sets the amount
 *   bottle 40 -> horns.band3            a kept value (or just: 40 -> horns.band3)
 *   lucid lean2: peaks.fold 0..90, stairs.rise 20..5
 *   drift 3                             natures wander 3% from night to night
 *   ; anything after a semicolon is a note to yourself
 */
#include "scq_int.h"

typedef struct { int start, end, line0, ts, te; } scene;

typedef struct {
	node     n[MAX_NODES];
	int      nn;
	cord     c[MAX_CORDS];
	int      nc;
	uint8_t  order[MAX_NODES];
	uint16_t cstart[MAX_NODES + 1];
	uint32_t pool[POOL_BYTES / 4];
	uint32_t used;

	char     text[SCQ_MAX_TEXT];
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
	float    drift;
	float    load;
	int      line;               /* line being compiled, for messages */
	int      quiet;              /* don't report (already reported) */
	int      waiting;            /* SCQ_DEVICE: faded out, waiting for scq_service() */
	uint32_t rng;
} engine;

static engine G0;
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
static __attribute__((unused)) void sb_f(sb *s, float v)
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

/* ------------------------------------------------------------- tokens */

enum { T_WORD, T_NUM, T_ARROW, T_COLON, T_COMMA, T_RANGE };
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
		if (c == '-' || c == '~' || c == '=') {
			int j = i + 1;
			while (j < n && (is_digit(s[j]) || s[j] == '.')) j++;
			if (j < n && s[j] == '>') {
				o->t = T_ARROW;
				o->kind = c == '-' ? K_HALL : (c == '~' ? K_FOG : K_WATER);
				if (j > i + 1) { o->has_amt = 1; o->amt = parse_num(s + i + 1, j - i - 1); }
				o->n = j + 1 - i; i = j + 1; k++;
				continue;
			}
		}
		if (is_digit(c) || ((c == '-' || c == '.') && i + 1 < n && (is_digit(s[i + 1]) || s[i + 1] == '.'))) {
			int j = i + 1, dots = 0;
			while (j < n && (is_digit(s[j]) || s[j] == '.')) { if (s[j] == '.' && j + 1 < n && s[j + 1] == '.') dots = 1; j++; }
			if (j < n && s[j] == '-' && dots == 0) { /* 20-5 is not a thing; stop here */ }
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

static int find_node(const char *w, int n)
{
	for (int i = 0; i < G.nn; i++) if (str_eq(w, n, G.n[i].name)) return i;
	return -1;
}

static void *pool_take(uint32_t bytes)
{
	bytes = (bytes + 3u) & ~3u;
	if (G.used + bytes > POOL_BYTES) return 0;
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

static int make_node(int p, const char *name, int nl, float nature)
{
	const place *pl = &SCQ_PLACES[p];
	if (G.nn >= MAX_NODES) { say('E', "the dream is too crowded (", 0, 0, "48 places at most)"); return -1; }
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
		int p = find_place(w, nl);
		float nature = 0.f;
		if (p >= 0) nature = SCQ_PLACES[p].nature;
		else {                                   /* place + number: aira3, lean2 */
			int j = nl;
			while (j > 0 && is_digit(w[j - 1])) j--;
			if (j < nl && j > 0 && (p = find_place(w, j)) >= 0) nature = parse_num(w + j, nl - j);
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
	if (G.nc >= MAX_CORDS) { say('E', "too many passages (", 0, 0, "160 at most)"); return; }
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
	}
	if (a->is_bottle) { c->kind = K_BOTTLE; c->a = a->val * 0.01f; }
}

/* ------------------------------------------------------------- statements */

static void st_the(tok *t, int nt)
{
	/* the NAME is|are PLACE [N] [: stations] */
	if (nt < 4 || t[1].t != T_WORD || t[2].t != T_WORD || t[3].t != T_WORD ||
	    !(str_eq(t[2].s, t[2].n, "is") || str_eq(t[2].s, t[2].n, "are"))) {
		say('E', "write it as: the name is place", 0, 0, "");
		return;
	}
	for (int i = 0; i < t[1].n; i++) if (t[1].s[i] == '.') { say('E', "a name can't have a dot: ", t[1].s, t[1].n, ""); return; }
	if (find_node(t[1].s, t[1].n) >= 0) { say('E', "there are already ", t[1].s, t[1].n, " here"); return; }
	int p = find_place(t[3].s, t[3].n);
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
	if (k < nt && t[k].t == T_COLON) k++;
	int id = make_node(p, t[1].s, t[1].n, nature);
	if (id < 0) return;
	if (k < nt) {
		if (p != find_place("train", 5)) { say('W', "only trains have stations; ignoring the rest of ", t[1].s, t[1].n, ""); return; }
		float *m = (float *)G.n[id].mem;
		int c = 0;
		for (; k < nt && c < 32; k++) {
			if (t[k].t == T_COMMA) continue;
			if (t[k].t != T_NUM) { say('E', "a station is a number, not ", t[k].s, t[k].n, ""); return; }
			m[c++] = parse_num(t[k].s, t[k].n);
		}
		m[32] = (float)c;
	}
}

static void st_lucid(tok *t, int nt)
{
	/* lucid SRC : DST lo..hi , DST lo..hi */
	if (nt < 5 || t[1].t != T_WORD || t[2].t != T_COLON) { say('E', "write it as: lucid lean1: place.inlet 0..100", 0, 0, ""); return; }
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
	if (t[*k].t == T_WORD && str_eq(t[*k].s, t[*k].n, "bottle") && *k + 1 < nt && t[*k + 1].t == T_NUM) {
		if (role == 1) { say('E', "you can't pour into a bottle", 0, 0, ""); return -1; }
		e->is_bottle = 1; e->val = parse_num(t[*k + 1].s, t[*k + 1].n); e->node = -1; e->port = 0;
		*k += 2;
		return 0;
	}
	if (t[*k].t == T_NUM) {
		if (role == 1) { say('E', "a number can't take anything in: ", t[*k].s, t[*k].n, ""); return -1; }
		e->is_bottle = 1; e->val = parse_num(t[*k].s, t[*k].n); e->node = -1; e->port = 0;
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
		if (nt == 1 && t[0].t == T_WORD) { say('E', "", t[0].s, t[0].n, " goes nowhere; add a passage like -> left"); return; }
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
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "lucid") && nt > 1 && t[1].t == T_WORD) { st_lucid(t, nt); return; }
	if (t[0].t == T_WORD && str_eq(t[0].s, t[0].n, "drift") && nt == 2 && t[1].t == T_NUM) {
		G.drift = parse_num(t[1].s, t[1].n);    /* otherwise "drift" is just a name */
		return;
	}
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

static void compile_scene(int idx, int report)
{
	G.nn = 0; G.nc = 0; G.used = 0; G.drift = 0.f;
	G.quiet = !(report & 1);            /* the prelude is reported once */
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
		for (int p = 0; p < n->nin; p++) {
			int count = 0, only = -1;
			for (int c = G.cstart[k]; c < G.cstart[k + 1]; c++)
				if (G.c[c].dport == p) { count++; only = c; }
			n->direct[p] = 0;
			if (count == 0) continue;
			cord *c = &G.c[only];
			if (count == 1 && c->kind == K_HALL && c->a == 1.f && c->src >= 0 && c->src != k) {
				n->direct[p] = G.n[c->src].out[c->sport];
				c->direct = 1;
			} else {
				n->mixed |= (uint16_t)(1u << p);
			}
		}
	}
	uint8_t mark[MAX_NODES] = { 0 };
	int pos = 0;
	for (int k = 0; k < G.nn; k++) visit(k, mark, &pos);

	/* load: rough cycles per sample against what the SP-1 can spare (~1000 of 1333) */
	float cyc = 0.f;
	for (int k = 0; k < G.nn; k++) {
		const place *pl = &SCQ_PLACES[G.n[k].type];
		float cost = (float)pl->cost + 6.f;
		if (pl->run == SCQ_PLACES[find_place("frog", 4)].run) cost = 10.f + 30.f * scq_clamp(G.n[k].nature, 1.f, 8.f);
		cyc += cost;
	}
	cyc += 4.f * (float)G.nc;
	G.load = cyc / 1000.f;
}

/* ------------------------------------------------------------- the public face */

int scq_load(const char *text, int len, uint32_t nights)
{
	if (len > SCQ_MAX_TEXT - 1) len = SCQ_MAX_TEXT - 1;
	for (int i = 0; i < len; i++) G.text[i] = text[i];
	G.text[len] = 0;
	G.len = len;
	G.nights = nights;
	G.msgn = 0; G.msg[0] = 0; G.errors = 0;
	if (!G.rng) G.rng = 0x5eed1234u ^ nights;
	find_scenes();
	for (int s = 0; s < G.nsc; s++) compile_scene(s, s == 0 ? 3 : 2);
	int errors = G.errors;
	if (G.cur >= G.nsc) G.cur = 0;
	compile_scene(G.cur, 0);
	G.errors = errors;
	G.pending = -1;
	G.gain = 1.f; G.fading = 0; G.waiting = 0;
	return errors;
}

const char *scq_messages(void) { return G.msg; }

#ifdef SCQ_HOST
/* Check a text without touching what is playing (the desktop editor only: it costs a
 * second engine's worth of memory, which the SP-1 doesn't have). */
static engine G1;
int scq_check(const char *text, int len, uint32_t nights, char *msgs, int cap)
{
	Gp = &G1;
	G.cur = 0;
	int e = scq_load(text, len, nights);
	int n = 0;
	while (G.msg[n] && n < cap - 1) { msgs[n] = G.msg[n]; n++; }
	if (cap > 0) msgs[n] = 0;
	Gp = &G0;
	return e;
}
#endif

void scq_set_amount(int index, float amount)
{
	if (index < 0 || index >= G.nc) return;
	cord *c = &G.c[index];
	switch (c->kind) {
	case K_HALL: case K_FOG: case K_BOTTLE: c->a = amount * 0.01f; break;
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

int scq_scene(void) { return G.cur; }
int scq_node_count(void) { return G.nn; }

void scq_control(int which, float value)
{
	if (which >= 0 && which < SCQ_CTL_COUNT) W.ctl[which] = value;
}

float scq_lamp(int led) { return (led >= 0 && led < SCQ_LEDS) ? W.lamp[led] : 0.f; }

float scq_probe(int node, int port)
{
	if (node < 0 || node >= G.nn || port < 0 || port >= G.n[node].nout) return 0.f;
	return G.n[node].out[port][B - 1];
}

static const float ZEROS[B];

static void gather(node *n, int k, const float *in[MAX_IN], float scratch[MAX_IN][B])
{
	for (int p = 0; p < n->nin; p++) {
		if (n->direct[p]) { in[p] = n->direct[p]; continue; }
		if (n->mixed & (1u << p)) { LOOP scratch[p][i] = 0.f; in[p] = scratch[p]; continue; }
		float d = n->def[p];
		if (d == 0.f) { in[p] = ZEROS; continue; }
		LOOP scratch[p][i] = d;
		in[p] = scratch[p];
	}
	if (!n->mixed) return;
	for (int ci = G.cstart[k]; ci < G.cstart[k + 1]; ci++) {
		cord *c = &G.c[ci];
		if (c->direct) continue;
		float *dst = scratch[c->dport];
		if (c->kind == K_BOTTLE) { float v = c->a; LOOP dst[i] += v; continue; }
		const float *src = G.n[c->src].out[c->sport];
		switch (c->kind) {
		case K_HALL: { float a = c->a; LOOP dst[i] += src[i] * a; } break;
		case K_FOG: {
			float s = c->s, a = c->a;
			const float f = 0.18f;               /* ~1.5 kHz: a softening */
			LOOP { s += f * (src[i] * a - s); dst[i] += s; }
			c->s = s;
		} break;
		case K_WATER: {                         /* a capacitor: slow to fill, slow to drain */
			float s = c->s, step = (src[B - 1] - s) * c->a, s0 = s;
			s += step;
			LOOP dst[i] += s0 + step * (float)(i + 1) * (1.f / (float)B);
			c->s = s;
		} break;
		case K_DOOR: LOOP dst[i] += src[i] > 0.f ? src[i] : 0.f; break;
		case K_RANGE: { float lo = c->a, d = c->b - c->a; LOOP dst[i] += lo + d * src[i]; } break;
		}
	}
}

static void run_block(void)
{
	static float scratch[MAX_IN][B];        /* static: off the audio thread's small stack */
	const float *in[MAX_IN];
	LOOP { W.outL[i] = 0.f; W.outR[i] = 0.f; }
	for (int o = 0; o < G.nn; o++) {
		int k = G.order[o];
		node *n = &G.n[k];
		gather(n, k, in, scratch);
		SCQ_PLACES[n->type].run(n, in);
	}
	if ((W.want_next || W.want_back) && G.nsc > 1 && G.pending < 0) {
		int to = G.cur + (W.want_next ? 1 : -1);
		if (scq_rand01(&G.rng) < W.miss) {          /* you missed your stop */
			to = G.cur + 1 + (int)(scq_rand01(&G.rng) * (float)(G.nsc - 1));
		}
		G.pending = ((to % G.nsc) + G.nsc) % G.nsc;
	}
	W.want_next = W.want_back = 0;
}

void scq_render(float *left, float *right, int frames)
{
	for (int f = 0; f + B <= frames; f += B) {
		if (G.pending >= 0 && G.fading == 0) G.fading = -1;
		if (G.nn > 0 && !G.waiting) run_block();
		else LOOP { W.outL[i] = W.outR[i] = 0.f; }
		if (G.fading == 0) {
			LOOP { left[f + i] = scq_softclip(W.outL[i]); right[f + i] = scq_softclip(W.outR[i]); }
		} else LOOP {
			if (G.fading < 0) { G.gain -= 1.f / 480.f; if (G.gain < 0.f) G.gain = 0.f; }
			else { G.gain += 1.f / 480.f; if (G.gain >= 1.f) { G.gain = 1.f; G.fading = 0; } }
			left[f + i] = scq_softclip(W.outL[i]) * G.gain;
			right[f + i] = scq_softclip(W.outR[i]) * G.gain;
		}
		if (G.fading < 0 && G.gain <= 0.f) {
#ifdef SCQ_DEVICE
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
		sb_i(&s, n->line); sb_c(&s, '\n');
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
	sb_s(&s, "M "); sb_i(&s, (int)G.used); sb_c(&s, ' '); sb_i(&s, POOL_BYTES); sb_c(&s, ' ');
	sb_f(&s, G.load); sb_c(&s, '\n');
	return G.desc;
}

static char catalog_buf[32768];
const char *scq_catalog(void)
{
	static const char *fam[] = { "source", "shape", "space", "current", "hands", "ears" };
	sb s = { catalog_buf, 0, (int)sizeof catalog_buf };
	if (catalog_buf[0]) return catalog_buf;
	for (int p = 0; p < SCQ_NPLACES; p++) {
		const place *pl = &SCQ_PLACES[p];
		sb_s(&s, "P "); sb_s(&s, pl->name); sb_c(&s, ' ');
		sb_s(&s, fam[pl->family]); sb_c(&s, ' ');
		sb_i(&s, pl->cost); sb_c(&s, ' ');
		sb_f(&s, pl->nature); sb_c(&s, ' ');
		for (const char *q = pl->ins; *q; q++) sb_c(&s, *q == ' ' ? ',' : *q);
		sb_s(&s, " | ");
		for (const char *q = pl->outs; *q; q++) sb_c(&s, *q == ' ' ? ',' : *q);
		sb_s(&s, " | "); sb_s(&s, pl->help); sb_c(&s, '\n');
	}
	return catalog_buf;
}
#endif
