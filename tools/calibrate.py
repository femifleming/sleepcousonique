#!/usr/bin/env python3
"""Turns bench_places.sh output into the engine's cost table (tooling, not part of the app).
   cost = cycles per sample on the SP-1 (1% of the processor = 13.33 cycles per sample)."""
import re, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
m = {}
for line in open(sys.argv[1]):
    r = re.match(r'(\S+)\.dream\s+engine\s+([\d.]+)%', line.strip())
    if r: m[r.group(1)] = float(r.group(2))
F, bottle, fog4 = m['_nothing'], m['_bottle'], m['_fog4']
node = max(0.5, (bottle - F) / 2)            # a trivial place (bottle, ears) incl. its cord
fog = max(0.2, (fog4 - bottle) / 4)
CYC = 1333 / 100
print(f"fixed {F}%  trivial place {node:.2f}%  mixed cord {fog:.2f}%", file=sys.stderr)
costs = {}
for name, v in m.items():
    if name.startswith('_'): continue
    costs[name] = max(1, round((v - bottle + node) * CYC))
costs['bottle'] = round(node * CYC)
for n in ['lean','door','lift','wake','lucid','left','right','ears','journey']: costs[n] = round(node * CYC)
costs['lamp'] = round(2 * node * CYC)
for f in ['engine/scq_places.c', 'engine/scq_more_table.inc']:
    p = os.path.join(root, f); s = open(p).read()
    def sub(mt):
        n = mt.group(1)
        return mt.group(0) if n not in costs else f'P("{n}",{mt.group(2)}, {mt.group(3)}, {costs[n]},'
    s = re.sub(r'P\("(\w+)",(\s*FAM_\w+), ([^,]+), (\d+),', sub, s)
    open(p, 'w').write(s)
print(f"#define SCQ_COST_FIXED {round(F * CYC)}\n#define SCQ_COST_CORD {round(fog * CYC)}")
for n in sorted(costs, key=lambda k: -costs[k])[:12]: print(f"  {n:12s} {costs[n]:4d} cyc  {costs[n]/CYC:5.1f}%", file=sys.stderr)
