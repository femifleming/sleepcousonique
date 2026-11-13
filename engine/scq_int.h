/* Engine internals shared by the language (scq_core.c) and the places (scq_places.c). */
#ifndef SCQ_INT_H
#define SCQ_INT_H

#include "scq.h"
#include "scq_math.h"

#define MAX_IN     10
#define MAX_OUT    4
#define MAX_NODES  48
#define MAX_CORDS  160
#define MAX_STATE  24
#define POOL_BYTES (80 * 1024)
#define NAME_LEN   20

#define B      SCQ_BLOCK
#define SR     ((float)SCQ_RATE)
#define INV_SR (1.f / (float)SCQ_RATE)
#define LOOP   for (int i = 0; i < B; i++)

typedef struct node node;
typedef void (*place_run)(node *n, float in[MAX_IN][B]);
typedef void (*place_init)(node *n);
typedef uint32_t (*place_mem)(float nature);

enum { FAM_SOURCE, FAM_SHAPE, FAM_SPACE, FAM_CURRENT, FAM_HANDS, FAM_EARS };

enum {            /* place flags */
	PF_SINGLE = 1,  /* one per scene, named after itself (controls, outputs) */
	PF_DRIFT  = 2,  /* its nature drifts from night to night */
	PF_COUNTED = 4  /* name+digit picks the nature: lean1, door3, stem2 */
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
	float    out[MAX_OUT][B];
	float    s[MAX_STATE];
	uint32_t rng;
	void    *mem;
	uint32_t memlen;
};

/* passage kinds */
enum { K_HALL, K_FOG, K_WATER, K_DOOR, K_RANGE, K_BOTTLE };

typedef struct {
	int8_t  src, dst;
	uint8_t sport, dport;
	uint8_t kind;
	float   a, b;     /* amount; range high */
	float   s;        /* state (fog / water) */
	int16_t line;
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
} world;

extern world W;

int place_port(const place *p, int out, const char *port, int len);
int place_nports(const char *list);

#endif
