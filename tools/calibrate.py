#!/usr/bin/env python3
"""Fits the load model to what SP-1s measured, and writes engine/scq_costs.h (tooling, not part of the app).

     calibrate.py SCQTOOL DIR[,DIR...] RESULTS.csv[,MORE.csv] [--write engine/scq_costs.h] [--hold PREFIX,...]

   RESULTS.csv comes from tools/sp1fleet.py: one line per patch measured on an SP-1,
   "file,unit,avg %,max %,..." (firmware before 1.4) or "file,unit,cyc,AVG CYCLES,MAX CYCLES,BUDGET,..." (1.4).
   Every patch is described by what the engine's load estimate counts in it (scqtool terms), and the
   cost of each kind of work and of each place is fitted by least squares, none below zero.
   Patches whose names start with a --hold prefix are kept out of the fit and only predicted."""
import sys, os, re, subprocess
import numpy as np
tool, dirs, csvs = sys.argv[1], sys.argv[2].split(','), sys.argv[3].split(',')
write = sys.argv[sys.argv.index('--write') + 1] if '--write' in sys.argv else None
hold = tuple(sys.argv[sys.argv.index('--hold') + 1].split(',')) if '--hold' in sys.argv else ()
CYC = 1333.0

def nnls(A, b, iters=3000):
    try:
        from scipy.optimize import nnls as f
        return f(A, b, maxiter=20 * A.shape[1])[0]
    except Exception:
        x = np.zeros(A.shape[1]); L = np.linalg.norm(A, 2) ** 2          # projected gradient
        for _ in range(iters * 10): x = np.maximum(0, x - A.T @ (A @ x - b) / L)
        return x

meas = {}
for c in csvs:
    for l in open(c):
        t = l.strip().split(',')
        if len(t) < 4 or t[2] == 'ERR': continue
        if t[2] == 'cyc': cyc, mx = float(t[3]) / float(t[5]) * CYC, float(t[4]) / float(t[5]) * CYC
        else: cyc, mx = float(t[2]) / 100 * CYC, float(t[3]) / 100 * CYC
        meas.setdefault(t[0], []).append((cyc, mx, t[1]))

rows = []
for name, ms in sorted(meas.items()):
    path = next((os.path.join(d, name) for d in dirs if os.path.exists(os.path.join(d, name))), None)
    if not path: continue
    r = subprocess.run([tool, 'terms', path], capture_output=True, text=True)
    m = re.match(r'scene 0 load ([\d.]+) \| (.*?) \|(.*)', r.stdout.strip().split('\n')[0]) if r.returncode == 0 and r.stdout else None
    if not m: continue
    terms = {k: float(v) for k, v in (kv.split('=') for kv in m.group(2).split())}
    pl = {k: float(v) for k, v in (kv.split('=') for kv in m.group(3).split())}
    rows.append(dict(name=name, old=float(m.group(1)) * CYC, terms=terms, places=pl,
                     cyc=float(np.mean([x[0] for x in ms])), max=float(np.max([x[1] for x in ms])), units=ms))

tnames = list(rows[0]['terms'].keys())
pnames = sorted({p for r in rows for p in r['places']})
cols = tnames + pnames
def vec(r): return [r['terms'].get(t, 0) for t in tnames] + [r['places'].get(p, 0) for p in pnames]
fit = [r for r in rows if not r['name'].startswith(hold)] if hold else rows
A = np.array([vec(r) for r in fit]); b = np.array([r['cyc'] for r in fit])
used = A.sum(axis=0) > 0
x = np.zeros(len(cols)); x[used] = nnls(A[:, used], b)
coef = dict(zip(cols, x))

def report(title, rs):
    if not rs: return
    P = np.array([vec(r) for r in rs]) @ x; M = np.array([r['cyc'] for r in rs]); O = np.array([r['old'] for r in rs])
    e, eo = (P - M) / CYC * 100, (O - M) / CYC * 100
    print(f"{title:28s} n={len(rs):3d}   new model: mean |err| {np.abs(e).mean():4.1f}  worst {np.abs(e).max():5.1f}  bias {e.mean():+5.1f}"
          f"     model in the tree: mean |err| {np.abs(eo).mean():4.1f}  worst {np.abs(eo).max():5.1f}  bias {eo.mean():+5.1f}   (points of the SP-1's processor)")
print(f"{len(rows)} patches measured, {len(fit)} in the fit, {int(used.sum())} figures fitted")
for pre, title in (('m_', 'one kind of work'), ('p_', 'each place alone'), ('d_', 'each place driven'), ('r_', 'random patches')):
    report(title, [r for r in rows if r['name'].startswith(pre)])
report('real dreams', [r for r in rows if not r['name'].startswith(('m_', 'p_', 'd_', 'r_'))])
if hold: report('held out ' + ','.join(hold), [r for r in rows if r['name'].startswith(hold)])
report('everything', rows)
# unit against unit: the same patch on different SP-1s
if '--units' in sys.argv:
    by = {}
    for r in rows:
        for cyc, mx, u in r['units']: by.setdefault(u, []).append(cyc - r['cyc'])
    print('units:', {u: (len(v), round(float(np.mean(v)) / CYC * 100, 2)) for u, v in by.items()})
print('terms:', {t: round(coef[t], 1) for t in tnames})
if '--worst' in sys.argv:
    P = np.array([vec(r) for r in rows]) @ x
    for i in np.argsort(-np.abs(P - np.array([r['cyc'] for r in rows])))[:14]:
        r = rows[i]; print(f"   {r['name']:28s} measured {r['cyc']/CYC*100:6.1f}%  model {P[i]/CYC*100:6.1f}%  old {r['old']/CYC*100:6.1f}%")
if '--places' in sys.argv:
    for p in sorted(pnames, key=lambda p: -coef[p]): print(f"   {p:16s} {coef[p]:6.0f}")

if write:
    T = {t: int(round(coef[t])) for t in tnames}
    h = f'''/* What things cost on the SP-1, in processor cycles per sample (written by tools/calibrate.py from
 * {len(fit)} patches measured on SP-1s with tools/sp1fleet.py; don't edit by hand, re-measure).
 * The chip runs 64 MHz and the codec clocks 48 kHz, so a sample has 1333 cycles. */
#define SCQ_CYCLES_PER_SAMPLE 1333

#define SCQ_COST_FIXED       {T['fixed']:<4d} /* an empty dream: the output stage and the block loop */
#define SCQ_COST_MIXED       {T['mixed']:<4d} /* an inlet that is summed (the first cord writes it) */
#define SCQ_COST_HALL        {T['hall']:<4d} /* cords into a summed inlet, by kind (a lone plain wire is free) */
#define SCQ_COST_FOG         {T['fog']:<4d}
#define SCQ_COST_WATER       {T['water']:<4d}
#define SCQ_COST_DOOR        {T['door']:<4d}
#define SCQ_COST_RANGE       {T['range']:<4d}
#define SCQ_COST_BOTTLE      {T['bottle']:<4d}
#define SCQ_COST_MAYBE       {T['maybe']:<4d}
#define SCQ_COST_SHAPE       {T['shape']:<4d} /* an attenuverter, a turned knob or the tune knob on an inlet: one pass for all three */
#define SCQ_COST_FILL        {T['fill']:<4d} /* an unpatched inlet whose default found no constant block */
#define SCQ_COST_SLOW_FIXED  {T['slow']:<4d} /* samplerate=half/quarter/eighth: the place costs cost / divisor, plus */
#define SCQ_COST_SLOW_INLET  {T['slow_in']:<4d} /*   the decimate, per live inlet / divisor */
#define SCQ_COST_SLOW_OUTLET {T['slow_out']:<4d} /*   and the hold, per outlet */

/* each place, at full rate (frogs and froglet: three of them) */
static const struct {{ const char *name; unsigned short cycles; }} SCQ_MEASURED[] = {{
''' + ''.join(f'\t{{ "{p}", {max(1, int(round(coef[p])))} }},\n' for p in pnames if used[cols.index(p)]) + '};\n'
    open(write, 'w').write(h)
    print('wrote', write)
