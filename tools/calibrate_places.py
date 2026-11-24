#!/usr/bin/env python3
"""Refits only each place's cost from SP-1 measurements, keeping the kinds-of-work figures already
in engine/scq_costs.h (use it when new places arrive and the full fit, calibrate.py, lets its
'fixed' term soak up what belongs to places).

    calibrate_places.py SCQTOOL PATCHDIR RESULTS.csv [--write engine/scq_costs.h]

Each p_<place>.dream is k copies of the place into out; its cost is
(measured cycles a sample - what the kinds of work and the other places in it cost) / k."""
import sys, os, re, csv, subprocess
tool, pdir, res = sys.argv[1], sys.argv[2], sys.argv[3]
write = sys.argv[sys.argv.index('--write') + 1] if '--write' in sys.argv else None
here = os.path.dirname(os.path.abspath(__file__)); hpath = os.path.join(here, '..', 'engine', 'scq_costs.h')
H = open(hpath).read()
term = {k.lower(): int(v) for k, v in re.findall(r'#define SCQ_COST_(\w+)\s+(\d+)', H)}
TERMNAME = {'fixed': 'fixed', 'mixed': 'mixed', 'hall': 'hall', 'fog': 'fog', 'water': 'water', 'door': 'door', 'range': 'range',
            'bottle': 'bottle', 'maybe': 'maybe', 'shape': 'shape', 'fill': 'fill', 'slow': 'slow_fixed', 'slow_in': 'slow_inlet', 'slow_out': 'slow_outlet'}
place = {n: int(c) for n, c in re.findall(r'\{ "([^"]+)", (\d+) \}', H)}
for l in subprocess.run([tool, 'catalog'], capture_output=True, text=True).stdout.splitlines():
    if l.startswith('P '):
        t = l.split(); place.setdefault(t[1], int(t[3]))
FRAMES = 96
meas = {}
for r in csv.reader(open(res)):
    if len(r) > 5 and r[2] == 'cyc': meas[r[0]] = int(r[3]) / FRAMES
def terms(f):
    out = subprocess.run([tool, 'terms', f], capture_output=True, text=True).stdout
    m = re.search(r'\| (.*) \| (.*)$', out.splitlines()[0]) if out else None
    if not m: return None, None
    T = {k: float(v) for k, v in (x.split('=') for x in m.group(1).split())}
    P = {k: float(v) for k, v in (x.split('=') for x in m.group(2).split())}
    return T, P
def predict(T, P, cost):
    c = sum(T[k] * term[TERMNAME[k]] for k in T)
    return c + sum(P[p] * cost.get(p, 0) for p in P)
new = dict(place)
for f in sorted(os.listdir(pdir)):
    if not f.startswith('p_') or f not in meas: continue
    name = f[2:-6]
    T, P = terms(os.path.join(pdir, f))
    if not T or name not in P: continue
    k = P[name]; others = dict(P); del others[name]
    c = (meas[f] - predict(T, others, new)) / k
    new[name] = max(1, round(c))
# how good is it, over every patch measured
def score(cost):
    errs = []
    for f, m in meas.items():
        p = os.path.join(pdir, f)
        if not os.path.exists(p): continue
        T, P = terms(p)
        if T is None: continue
        errs.append((predict(T, P, cost) - m) / 1333 * 100)
    n = len(errs); return n, sum(abs(e) for e in errs) / n, max(abs(e) for e in errs), sum(errs) / n
for label, cost in (('in the tree', place), ('places refitted', new)):
    n, mae, worst, bias = score(cost)
    print(f'{label:16s} n={n}  mean |err| {mae:5.1f}  worst {worst:6.1f}  bias {bias:+5.1f}  (points of the SP-1)')
if write:
    body = '\n'.join(f'\t{{ "{n}", {c} }},' for n, c in sorted(new.items()))
    H2 = re.sub(r'(static const struct \{ const char \*name; unsigned short cycles; \} SCQ_MEASURED\[\] = \{\n)(.*?)(\n\};)', lambda m: m.group(1) + body + m.group(3), H, flags=re.S)
    open(write, 'w').write(H2); print('wrote', write)
