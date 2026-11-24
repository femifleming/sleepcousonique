#!/usr/bin/env python3
"""Writes engine/scq_costs_wingie.h from tools/calibration/wingie-places.csv (tools/wingiebench.py places)."""
import csv, os, statistics, re
here = os.path.dirname(os.path.abspath(__file__))
rows = list(csv.DictReader(open(os.path.join(here, 'calibration', 'wingie-places.csv'))))
ratios = [float(r['ratio']) for r in rows]
log = open('/tmp/wingie-places.log').read() if os.path.exists('/tmp/wingie-places.log') else ''
m = re.search(r'empty dream: (\d+) cycles a block', log)
fixed = round(int(m.group(1)) / 64) if m else 140
out = ['/* What things cost on the Wingie (ESP32, 240 MHz, 48 kHz: 5000 cycles a sample), in cycles per',
       ' * sample, measured on a Wingie2 with tools/wingiebench.py places and written by',
       ' * tools/calibrate_wingie.py: do not edit by hand, measure again. A place not in the table costs',
       ' * its SP-1 figure times SCQ_WINGIE_SCALE (the median ratio of the measured ones). */',
       f'#define SCQ_WINGIE_FIXED {fixed}   /* an empty dream: the block loop, the output stage */',
       f'#define SCQ_WINGIE_SCALE_MEASURED {statistics.median(ratios):.2f}f',
       'static const struct { const char *name; unsigned short cycles; } SCQ_WINGIE_MEASURED[] = {']
for r in sorted(rows, key=lambda r: r['place']):
    out.append(f'\t{{ "{r["place"]}", {int(float(r["wingie_cycles"]))} }},')
out.append('};')
open(os.path.join(here, '..', 'engine', 'scq_costs_wingie.h'), 'w').write('\n'.join(out) + '\n')
print(f'{len(rows)} places, fixed {fixed}, median ratio {statistics.median(ratios):.2f}')
