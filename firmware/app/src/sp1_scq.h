/*
 * Sleepcousonique on the SP-1: the engine, its hands, its lamp, its USB serial link and
 * the dream slots in flash.
 *
 *   sp1_scq_init()            main thread, once, after sp1_audio_init()
 *   sp1_scq_render()          AUDIO THREAD ONLY (sp1_audio.c's fill_block)
 *   everything else           main thread
 *
 * The engine is guarded by a mutex the audio thread only ever TRIES: while the main
 * thread is loading a dream (a few ms), the audio thread renders silence for that block
 * rather than waiting, so it never blocks and never sees a half-built dream.
 */
#ifndef SP1_SCQ_H
#define SP1_SCQ_H

#include <stdint.h>
#include <stdbool.h>

void sp1_scq_init(void);

/* AUDIO THREAD: one block of stereo floats. */
void sp1_scq_render(float *left, float *right, uint32_t frames);

/* Main loop, every control tick, after sp1_controls_scan(). `fnc` is "••". */
void sp1_scq_tick(uint32_t elapsed_ms, bool fnc);

/* The lamp, 0-255 per LED: track row (above T1-T4) and play row. While plugged in, a play
 * row the dream isn't using shows the charge bar, exactly as STANDBY draws it: breathing
 * while charging, solid when full. */
void sp1_scq_track_leds(uint8_t out[4]);
void sp1_scq_play_leds(uint32_t elapsed_ms);

#endif
