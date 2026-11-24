/* bed — renders one scene of a .dream for a while, for the video manual's music bed.
 *   cc -std=c99 -O2 -DSCQ_HOST -I../../engine bed.c ../../engine/scq_core.c ../../engine/scq_places.c -o bed -lm
 *   ./bed bed-sp1.dream SCENE SECONDS out.f32 [fader1 fader2 fader3 fader4]
 * Writes interleaved stereo float32 at 48 kHz. Plays SP-1 and Wingie dreams alike: the faders
 * are set, and the input (a Wingie's microphones) hears a soft hiss, the room. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scq.h"

int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: bed file.dream scene seconds out.f32 [f1 f2 f3 f4]\n"); return 2; }
	FILE *f = fopen(argv[1], "rb");
	if (!f) { perror(argv[1]); return 1; }
	static char text[SCQ_MAX_TEXT];
	int len = (int)fread(text, 1, sizeof text - 1, f);
	fclose(f);
	if (scq_load(text, len, 0)) { fprintf(stderr, "%s", scq_messages()); return 1; }
	scq_goto_scene(atoi(argv[2]));
	for (int k = 0; k < 4; k++) scq_control(SCQ_CTL_LEAN1 + k, argc > 5 + k ? (float)atof(argv[5 + k]) : 0.5f);
	long n = (long)(atof(argv[3]) * SCQ_RATE);
	FILE *o = fopen(argv[4], "wb");
	if (!o) { perror(argv[4]); return 1; }
	float l[256], r[256], inl[256], inr[256], buf[512];
	unsigned seed = 12345u;
	for (long i = 0; i < n; i += 256) {
		for (int j = 0; j < 256; j++) {
			seed = seed * 1664525u + 1013904223u; inl[j] = ((seed >> 9) / 4194304.f - 1.f) * 0.004f;
			seed = seed * 1664525u + 1013904223u; inr[j] = ((seed >> 9) / 4194304.f - 1.f) * 0.004f;
		}
		scq_input(inl, inr);
		scq_render(l, r, 256);
		for (int j = 0; j < 256; j++) { buf[2 * j] = l[j]; buf[2 * j + 1] = r[j]; }
		fwrite(buf, sizeof(float), 512, o);
	}
	fclose(o);
	return 0;
}
