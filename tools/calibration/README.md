# load-meter calibration

What the SP-1s measured, and the patches they measured, so the load meter's figures
(`engine/scq_costs.h`) can be refitted or checked.

- `patches/`: the patches, written by `tools/calib_patches.py` (seed 1): `m_*` one kind of work at a
  time, `p_*` each place alone, `d_*` each place driven by a clock and a sine, `r_*` random patches.
- `sp1-1.4.0-beta1.csv`: all of them on an SP-1 running 1.4.0-beta1 (`tools/sp1fleet.py bench`):
  cycles per 2 ms block, average and worst, of 127968, over 1500 blocks. Also the example dreams
  and a user's dreams, one line per scene (the dreams themselves aren't here).
- `sp1-1.2.0.csv`: the same on 1.2.0 (whole percent, from its 5 s report), shared over three SP-1s.
- `sp1-1.2.0-two-units.csv`: the same dreams on two SP-1s, to see how much units differ
  (0.4 points on average, 2 at most).

Refit after an engine change: measure again, then

    tools/sp1fleet.py bench new.csv tools/calibration/patches/*.dream
    tools/calibrate.py tools/scqtool tools/calibration/patches new.csv --write engine/scq_costs.h
