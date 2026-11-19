/* Engine internals shared by the language (scq_core.c) and the places (scq_places.c). */
#ifndef SCQ_INT_H
#define SCQ_INT_H

#include "scq.h"
#include "scq_math.h"

#define MAX_IN     10
#define MAX_OUT    4
#define MAX_NODES  64
#define MAX_CORDS  224
#define MAX_STATE  24
#define POOL_BYTES (80 * 1024)
#define NAME_LEN   20
#define MAX_OPT    6     /* switches and params, set when the place is named */

#define B      SCQ_BLOCK
#define SR     ((float)SCQ_RATE)
#define INV_SR (1.f / (float)SCQ_RATE)
#define LOOP   for (int i = 0; i < B; i++)

typedef struct node node;
typedef void (*place_run)(node *n, const float *const in[MAX_IN]);
typedef void (*place_init)(node *n);
typedef uint32_t (*place_mem)(float nature);

enum { FAM_SOURCE, FAM_SHAPE, FAM_SPACE, FAM_CURRENT, FAM_HANDS, FAM_EARS };

enum {            /* place flags */
	PF_SINGLE = 1,  /* one per scene, named after itself (controls, outputs) */
	PF_DRIFT  = 2,  /* its nature drifts from night to night */
	PF_COUNTED = 4, /* name+digit picks the nature: lean1, door3 */
	PF_KEEPMEM = 8  /* its memory is written in the dream (a seq's steps): never cleared */
};

typedef struct {
	const char *name;
	uint8_t     family;
	uint8_t     flags;
	uint16_t    cost;        /* rough Cortex-M4F cycles per sample */
	float       nature;      /* default nature when none is given */
	const char *ins;         /* space separated */
	const char *defs;        /* default inlet values, space separated */
	const char *outs;
	place_run   run;
	place_init  init;
	place_mem   mem;
	const char *help;
} place;

struct node {
	uint8_t  type;
	uint8_t  nin, nout;
	uint16_t patched;            /* bit per inlet that has cords */
	float    nature;             /* after drift */
	float    given;              /* as written */
	char     name[NAME_LEN];
	int16_t  line;
	float    def[MAX_IN];
	const float *direct[MAX_IN];  /* a lone plain wire: read the source's buffer itself */
	uint16_t mixed;               /* bit per inlet that has to be summed each block      */
	float    out[MAX_OUT][B];
	float    s[MAX_STATE];
	float    opt[MAX_OPT];         /* switch positions (0, 1, ...) or param values */
	uint8_t  optset;              /* bit per option written in the dream */
	uint8_t  rdiv;                /* samplerate=: runs at SCQ_RATE / rdiv (1, 2, 4, 8) */
	int8_t   pitchin;             /* the pitch inlet, or -1 */
	float    tune;                /* base pitch knob, octaves added to the pitch inlet */
	uint16_t attset;              /* bit per inlet whose attenuverter isn't 1 */
	float    att[MAX_IN];         /* attenuverters: scale what's patched in (not the default) */
	uint16_t knobset;             /* bit per inlet whose panel knob was turned */
	float    koff[MAX_IN];        /* knob - the place's default: added to what's patched in */
	uint32_t rng;
	void    *mem;
	uint32_t memlen;
};

/* passage kinds */
enum { K_HALL, K_FOG, K_WATER, K_DOOR, K_RANGE, K_BOTTLE, K_MAYBE };   /* ?> maybe: each wave/pulse passes by chance */

typedef struct {
	int8_t  src, dst;
	uint8_t sport, dport;
	uint8_t kind;
	float   a, b;     /* amount; range high */
	float   s;        /* state (fog / water) */
	int16_t line;
	uint8_t direct;   /* this cord is read in place: nothing to do per block */
} cord;

extern const place SCQ_PLACES[];
extern const int   SCQ_NPLACES;

/* the engine's shared world, read by places */
typedef struct {
	float    ctl[SCQ_CTL_COUNT];
	float    outL[B], outR[B];
	float    lamp[SCQ_LEDS];
	int      want_next, want_back;
	float    miss;
	int      nb;          /* the block the running place sees: SCQ_BLOCK, or less at a lower samplerate */
	float    sr, isr;     /* the sample rate it sees, and 1/that */
} world;

extern world W;

int place_port(const place *p, int out, const char *port, int len);
int place_nports(const char *list);
const char *place_renamed(const char *old, int len);
/* "name=a|b|c" switches (default a) and "name=5" params, space separated; "" if none */
const char *place_opts(const char *place);   /* new name of an old place, or 0 */
int place_has_att(const place *p);
/* tunings written in the dream ("tuning NAME: cents ... period"): period, n, then n degrees in
 * cents (0 first), as the built-in scales; 0 past the last */
const float *scq_tuning(int index);
#define RATE_OPT "samplerate=full|half|quarter|eighth"   /* every place but the SP-1's own */           /* sources and pitched places: an attenuverter per inlet */

#endif
