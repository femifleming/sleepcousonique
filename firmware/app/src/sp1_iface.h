/*
 * Sleepcousonique -- interface mode: the SP-1 as a 2-channel USB sound card.
 *
 * The USB audio output ("Sleepcousonique out") is always on the device; in dream mode what a
 * computer sends to it is dropped. In interface mode the dream sleeps, its memory pool holds
 * the incoming audio, and that audio is what the SP-1 plays. Several SP-1s on one computer are
 * outputs 1-2, 3-4, ... of one instrument (the Array program).
 *
 *   sp1_iface_start / stop   main thread, entering / leaving the mode
 *   sp1_iface_render         audio thread, one block, while the mode is on
 */
#ifndef SP1_IFACE_H
#define SP1_IFACE_H

#include <stdbool.h>
#include <stdint.h>

void  sp1_iface_start(void *mem, uint32_t bytes);   /* the ring: the engine's lent pool */
void  sp1_iface_stop(void);
bool  sp1_iface_on(void);
bool  sp1_iface_streaming(void);                    /* the computer has the output open */
void  sp1_iface_render(float *left, float *right, uint32_t frames);
float sp1_iface_level(int ch);                      /* peak 0..1, for the LEDs */

struct sp1_iface_stats {
	bool     streaming;
	uint32_t packets, under, over, fill;
	int32_t  ratio_ppm;          /* how far the SP-1 is stepping from 1.0, to follow the host */
};
void  sp1_iface_stats(struct sp1_iface_stats *s);

#endif
