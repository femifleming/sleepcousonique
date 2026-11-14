/* measured on an SP-1 (2026-11-14): tools/bench_places.sh, then tools/calibrate.py; every place
 * re-benched at the 32-sample block, and the model checked against 9 whole patches measured on
 * the SP-1. Cycles per sample; the chip has 1333 at 48 kHz. */
#define SCQ_COST_FIXED 93
#define SCQ_COST_CORD 30
#define SCQ_COST_BLOCK_SAVING 0   /* measured at the current block size: nothing to take off */

/* samplerate=half/quarter/eighth, measured (difftone, sine, lfo, frogs, follower, quantizer at each
 * rate): a place costs cost / divisor, plus the decimate and the hold, about 16 cycles, 12 more per
 * outlet and 3 per patched inlet. So a slow place saves less than its divisor, and a cheap one with
 * several outlets almost nothing. */
#define SCQ_COST_SLOW_FIXED  16
#define SCQ_COST_SLOW_OUTLET 12
#define SCQ_COST_SLOW_INLET  3

/* each different kind of place in a scene (the instruction cache): tried 0-38 against the 9
 * measured patches, no help, so off */
#define SCQ_COST_PER_KIND    0

/* frogs (and froglet) are costed busy, all voices chirping, as when an LFO or PLAY keeps waking
 * them (measured: about 35 % busy against 10 % idle), so patches with frogs are estimated near
 * their peaks, the safe side. */
