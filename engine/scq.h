/*
 * Sleepcousonique — a dream-journal patch language for the SP-1.
 *
 * This engine is freestanding C99: no libc, no malloc, no floating-point library.
 * The same file builds for the browser (WebAssembly) and for the SP-1 (nRF52840,
 * Cortex-M4F), so what you hear in the simulator is what the device computes.
 */
#ifndef SCQ_H
#define SCQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCQ_RATE        48000
#define SCQ_BLOCK       32          /* samples per engine block (the SP-1 renders 96: three blocks) */
#define SCQ_MAX_TEXT    (16 * 1024) /* largest patch file */
#define SCQ_SLOT_TEXT   (SCQ_MAX_TEXT - 8) /* largest dream an SP-1 slot keeps (a slot is 16 KB with its 8-byte header) */
#define SCQ_MAX_SCENES  16
#define SCQ_LEDS        8           /* 0-3 track row (above T1-T4), 4-7 play row */

/* ---- controls (the SP-1's hands) ---- */
enum {
	SCQ_CTL_LEAN1 = 0, SCQ_CTL_LEAN2, SCQ_CTL_LEAN3, SCQ_CTL_LEAN4, /* faders 0..1        */
	SCQ_CTL_DOOR1, SCQ_CTL_DOOR2, SCQ_CTL_DOOR3, SCQ_CTL_DOOR4,     /* T1-T4  0/1         */
	SCQ_CTL_LIFT,                                                   /* rocker -1 / 0 / +1 */
	SCQ_CTL_WAKE,                                                   /* PLAY   0/1         */
	SCQ_CTL_LUCID,                                                  /* ••     0/1         */
	SCQ_CTL_COUNT
};

/* Load a whole .dream file. Every scene is compiled once to find mistakes, then
 * the first scene starts. Returns the number of errors (warnings don't count).
 * `nights` is how many times this dream has been loaded before; `drift` uses it. */
int  scq_load(const char *text, int len, uint32_t nights);
/* On the SP-1 (SCQ_DEVICE) the text is NOT copied: it is read again at every scene change, so it
 * must stay where it is, unchanged, until the next scq_load() (or scq_sleep()). */

/* Errors and warnings from the last load / scene change, one per line:
 *   E <line> <message>     or     W <line> <message>                       */
const char *scq_messages(void);

/* A description of the scene that is playing, for the editor, one item per line:
 *   S <index> <count> <title>
 *   N <id> <place> <nature> <name>
 *   C <src> <srcport> <dst> <dstport> <kind> <amount> <amount2>   (src -1 = bottle)
 *   R <places> <most> <cords> <most> <text bytes> <most an SP-1 slot keeps>
 *   M <pool bytes used, or asked for if they didn't fit> <pool bytes total> <load estimate 0..1+>  */
const char *scq_describe(void);

/* The place catalogue, one place per line:
 *   P <name> <family> <cost> <ins, comma separated> | <outs, comma separated> | <help> */
const char *scq_catalog(void);

void  scq_control(int which, float value);
/* MIDI bytes in (any mix of messages, running status, real time), for the midi, midicc and
 * midiclock places. Call it from the thread that calls scq_render. */
void  scq_midi(const uint8_t *bytes, int len);
/* The room, for the crowd and visitors places: what a radio hears of the Bluetooth devices
 * nearby (the editor listens with the computer's; the SP-1 has none to listen with and never
 * calls these). live: a radio is listening (0: crowd holds its rest= count); count: devices
 * heard in the last few seconds; near: the strongest of them, 0..1; strength: their mean, 0..1.
 * An event is one thing heard: kind 0 a signal from a device (voice: that device's number,
 * 0..1), 1 a device arrived, 2 a device left. Call them from the thread that calls scq_render. */
void  scq_room(int live, int count, float near, float strength);
void  scq_room_event(int kind, float voice);
void  scq_render(float *left, float *right, int frames); /* frames: multiple of SCQ_BLOCK */
void  scq_goto_scene(int index);
void  scq_memory(uint32_t *used, uint32_t *total);   /* the pool: bytes the playing scene uses (or asked for), and its size */
int   scq_scene(void);
int   scq_node_count(void);             /* places in the scene that's playing */
int   scq_scene_count(void);
float scq_scene_load(int scene);        /* the SP-1's processor, 1.0 = all of it (measured costs) */
float scq_lamp(int led);                /* 0..1 */
float scq_probe(int node, int port);    /* last value of any outlet, for scopes */
/* How many times a place has been started again because it ran away (an outlet or a memory
 * that was not a number, or beyond anything a dream holds). It costs a dropout in that place,
 * never the rest of the dream; a dream that behaves leaves this at 0. */
uint32_t scq_healed(void);

/* Change one passage's amount while it plays (index as listed by scq_describe).
 * Same units as the text: percent, or milliseconds for water. */
void  scq_set_amount(int cord, float amount);

/* Turn a place's tune knob (base pitch, octaves) while it plays (index as listed by scq_describe). */
void  scq_set_tune(int node, float octaves);
/* Turn an inlet's attenuverter (scales what's patched in) while it plays. */
void  scq_set_att(int node, int inlet, float amount);
/* What's sounding now, in the text's units (the editor's snapshots read these). */
float scq_get_amount(int cord);        /* percent (milliseconds for water); 0 for a door */
float scq_get_tune(int node);
float scq_get_att(int node, int inlet);
float scq_get_param(int node, int k);
float scq_get_control(int which);
float scq_get_knob(int node, int inlet);  /* an inlet's panel knob (its own value) */

/* Turn a numeric param (k = its place in the place's switches and params) while it plays. */
void  scq_set_param(int node, int k, float value);
/* Turn an inlet's panel knob: its value when nothing is patched in, added to what is. */
void  scq_set_knob(int node, int inlet, float value);

/* Desktop editor only (built with SCQ_HOST): check text without disturbing the sound.
 * Fills msgs like scq_messages(); returns the number of errors. */
int   scq_check(const char *text, int len, uint32_t nights, char *msgs, int cap);
int   scq_checked_scenes(void);         /* after scq_check: each scene's load on the SP-1 */
float scq_checked_load(int scene);
const char *scq_cost_terms(int scene);  /* what the load estimate counted, for tools/calibrate.py */

/* On the SP-1 (built with SCQ_DEVICE) a scene change is compiled outside the audio thread:
 * render fades out and waits in silence; call this from the main loop to finish the change.
 * Returns 1 if it changed scene. Elsewhere scenes change inside scq_render and this does nothing. */
int   scq_service(void);

/* Interface mode (the SP-1 as a USB sound card): stop the dream and lend its memory pool;
 * returns the pool and its size. scq_load() wakes the engine and takes the pool back. */
void *scq_sleep(uint32_t *len);

#ifdef __cplusplus
}
#endif
#endif
