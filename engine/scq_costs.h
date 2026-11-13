/* measured on an SP-1 (2026-11-13): tools/bench_places.sh, then tools/calibrate.py.
 * cycles per sample; the chip has 1333 at 48 kHz */
#define SCQ_COST_FIXED 93
#define SCQ_COST_CORD 30
/* 2026-11-13: the engine block went from 16 to 32 samples, which halves each place's per-block
 * overhead (about 20 cycles per sample at 16, measured on the trivial places). Until the places
 * are re-benched at 32 (tools/bench_places.sh), the load takes off this ESTIMATE per place. */
#define SCQ_COST_BLOCK_SAVING 8
