#!/usr/bin/env python3
"""Writes the patches the load model is calibrated from (tooling, not part of the app).
     calib_patches.py SCQTOOL OUTDIR [how many random patches] [seed]
   m_*  one kind of work at a time (cords by kind, attenuverters, knobs, tune, samplerate...)
   p_*  each place, several of it, alone;   d_*  the same, driven by a clock and a sine
   r_*  random patches, as a player might build them"""
import sys, os, subprocess, random, re
tool, out = sys.argv[1], sys.argv[2]
NR = int(sys.argv[3]) if len(sys.argv) > 3 else 120
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 1)
os.makedirs(out, exist_ok=True)

places = {}
last = None
for l in subprocess.run([tool, 'catalog'], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if l.startswith('P '):
        parts = l.split('|')
        head = parts[0].split()
        last = dict(name=head[1], fam=head[2], cost=int(head[3]), nature=float(head[4]),
                    ins=head[5].split(',') if len(head) > 5 else [], outs=[o for o in parts[1].strip().split(',') if o], opts=[], defs=[])
        places[last['name']] = last
    elif l.startswith('O ') and last and t[1] == last['name']:
        for o in t[2:]:
            k, v = o.split('=', 1)
            last['opts'].append((k, v.split('|') if v[0].isalpha() else v))
    elif l.startswith('D ') and last and t[1] == last['name']:
        last['defs'] = [float(x) for x in t[2:]]

SOUND = {'in', 'in1', 'in2', 'in3', 'in4', 'a', 'b', 'mod', 'x', 'y', 'carrier', 'modulator', 'left', 'right'}
PULSE = {'trig', 'trig1', 'trig2', 'strike', 'pluck', 'gate', 'clock', 'reset', 'sync', 'next', 'back', 'learn', 'follow', 'hold', 'freeze'}
usable = [p for p in places.values() if p['fam'] not in ('hands', 'ears') and p['name'] not in ('value', 'scene') and not p['name'].startswith('midi')]
sources = [p for p in usable if p['outs']]

def check(text):
    f = os.path.join(out, '_try.dream'); open(f, 'w').write(text)
    r = subprocess.run([tool, 'terms', f], capture_output=True, text=True)
    os.remove(f)
    if r.returncode != 0: return None
    m = re.search(r'load ([\d.]+)', r.stdout)
    return float(m.group(1)) if m else None

def write(name, text, lo=0.0, hi=9.9):
    est = check(text)
    if est is None or not (lo <= est <= hi): return False
    open(os.path.join(out, name + '.dream'), 'w').write(text)
    return True

# ---- one kind of work at a time -------------------------------------------------------------
def many(n, place, opts=''): return ''.join('the %s%d is %s %s\n' % (place[:1], i, place, opts) for i in range(1, n + 1))
write('m_nothing', '; nothing\n')
write('m_sine8', many(8, 'sine'))
for arrow, tag in (('->', 'wire'), ('-50>', 'hall'), ('~>', 'fog'), ('=>', 'water'), ('>|', 'door'), ('?>', 'maybe')):
    write('m_%s8' % tag, many(8, 'sine') + many(8, 'onepole') + ''.join('s%d %s o%d\n' % (i, arrow, i) for i in range(1, 9)))
write('m_range8', many(8, 'sine') + many(8, 'onepole') + ''.join('lucid s%d: o%d.cutoff -100..100\n' % (i, i) for i in range(1, 9)))
write('m_bottle8', many(8, 'sine') + many(8, 'onepole') + ''.join('s%d -50> o%d\nvalue 20 -> o%d\n' % (i, i, i) for i in range(1, 9)))
write('m_sum2', many(8, 'sine') + many(4, 'onepole') + ''.join('s%d -> o%d\n' % (i, (i + 1) // 2) for i in range(1, 9)))
write('m_sum8', many(8, 'sine') + many(1, 'onepole') + ''.join('s%d -> o1\n' % i for i in range(1, 9)))
write('m_out8', many(8, 'sine') + ''.join('s%d -> out\n' % i for i in range(1, 9)))
write('m_att8', many(8, 'sine') + many(8, 'sine', 'att.fm=0.5').replace('the s', 'the t') + ''.join('s%d -> t%d.fm\n' % (i, i) for i in range(1, 9)))
write('m_att0', many(8, 'sine') + many(8, 'sine').replace('the s', 'the t') + ''.join('s%d -> t%d.fm\n' % (i, i) for i in range(1, 9)))
write('m_knob8', many(8, 'sine') + many(8, 'sine', 'fm=0.25').replace('the s', 'the t') + ''.join('s%d -> t%d.fm\n' % (i, i) for i in range(1, 9)))
write('m_tune8', many(8, 'sine', 'tune=1.5'))
write('m_tune8p', many(8, 'sine') + many(8, 'sine', 'tune=1.5').replace('the s', 'the t') + ''.join('s%d -> t%d.pitch\n' % (i, i) for i in range(1, 9)))
for r in ('half', 'quarter', 'eighth'):
    write('m_sine8_' + r, many(8, 'sine', 'samplerate=' + r))
    write('m_lfo8_' + r, many(8, 'lfo', 'samplerate=' + r))
    write('m_vca8_' + r, many(8, 'sine') + many(8, 'vca', 'samplerate=' + r) + ''.join('s%d -> v%d\ns%d -> v%d.gain\n' % (i, i, i, i) for i in range(1, 9)))
write('m_lfo8', many(8, 'lfo'))
write('m_vca8', many(8, 'sine') + many(8, 'vca') + ''.join('s%d -> v%d\ns%d -> v%d.gain\n' % (i, i, i, i) for i in range(1, 9)))
# defaults that find no constant block: more than 32 different ones
write('m_fill', ''.join('the v%d is vca gain=%.3f\n' % (i, 0.1 + i * 0.013) for i in range(1, 49)))
write('m_nofill', ''.join('the v%d is vca gain=0.7\n' % i for i in range(1, 49)))
write('m_faders', many(4, 'onepole') + ''.join('fader%d -> o%d.cutoff\n' % (i, i) for i in range(1, 5)))
write('m_hands', many(8, 'onepole') + 'button1 -> o1.cutoff\nbutton2 -> o2.cutoff\nrocker -> o3.cutoff\nplay -> o4.cutoff\ndots -> o5.cutoff\nrocker.up -> o6.cutoff\n')
write('m_ears', many(3, 'sine') + 's1 -> left\ns2 -> right\ns3 -> leds\n')

# ---- every place: several of it, alone, then driven ------------------------------------------
for p in usable:
    n = max(1, min(10, int(0.55 * 1333 / max(p['cost'], 1))))
    while n >= 1:
        body = ''.join('the x%d is %s\n' % (i, p['name']) for i in range(1, n + 1))
        tail = ''.join('x%d -> out\n' % i for i in range(1, n + 1)) if p['outs'] else ''
        if write('p_' + p['name'], body + tail, 0, 0.92): break
        n -= 1
    if n < 1: print('skipped', p['name'], file=sys.stderr); continue
    drive = ''
    for i in range(1, n + 1):
        for inl in p['ins']:
            if inl in SOUND: drive += 'sn -> x%d.%s\n' % (i, inl)
            elif inl in PULSE: drive += 'ck.eighth -> x%d.%s\n' % (i, inl)
    if drive:
        m = n
        while m >= 1:
            body = ''.join('the x%d is %s\n' % (i, p['name']) for i in range(1, m + 1))
            tail = ''.join('x%d -> out\n' % i for i in range(1, m + 1)) if p['outs'] else ''
            d = '\n'.join(l for l in drive.splitlines() if int(re.search(r'x(\d+)\.', l).group(1)) <= m) + '\n'
            if write('d_' + p['name'], 'the ck is clock\nthe sn is sine 2\n' + body + tail + d, 0, 0.92): break
            m -= 1

# ---- random patches ---------------------------------------------------------------------------
def rnd_patch():
    k = random.randint(4, 14)
    chosen = [random.choice(usable) for _ in range(k)]
    names, lines, cords = [], [], []
    for i, p in enumerate(chosen):
        name = 'n%d' % i
        opts = ''
        if p['fam'] != 'hands':
            if abs(p['nature']) > 0 and random.random() < 0.2:
                nat = max(1, round(p['nature'] * random.uniform(0.5, 1.5))) if p['nature'] >= 1 else p['nature']
                opts += ' %g' % nat
            for k_, v in p['opts']:
                if k_ == 'samplerate':
                    if random.random() < 0.3: opts += ' samplerate=' + random.choice(['half', 'quarter', 'eighth'])
                elif k_ == 'tune':
                    if random.random() < 0.35: opts += ' tune=%g' % round(random.uniform(-2, 3), 2)
                elif isinstance(v, list):
                    if random.random() < 0.3: opts += ' %s=%s' % (k_, random.choice(v))
        names.append((name, p)); lines.append('the %s is %s%s' % (name, p['name'], opts))
    def src():
        if random.random() < 0.15:
            return random.choice(['fader1', 'fader2', 'fader3', 'fader4', 'button1', 'rocker', 'play'])
        cands = [(n, p) for n, p in names if p['outs']]
        n, p = random.choice(cands)
        o = random.randrange(len(p['outs']))
        return n if o == 0 else n + '.' + p['outs'][o]
    extra = {}
    for n, p in names:
        for i, inl in enumerate(p['ins']):
            for rep in range(2):
                if random.random() > (0.5 if rep == 0 else 0.12): continue
                dst = n if i == 0 else n + '.' + inl
                r = random.random()
                if r < 0.62: cords.append('%s -> %s' % (src(), dst))
                elif r < 0.72: cords.append('%s -%d> %s' % (src(), random.choice([20, 35, 50, 75, 150]), dst))
                elif r < 0.80: cords.append('%s ~> %s' % (src(), dst))
                elif r < 0.87: cords.append('%s => %s' % (src(), dst))
                elif r < 0.91: cords.append('%s >| %s' % (src(), dst))
                elif r < 0.95: cords.append('%s ?> %s' % (src(), dst))
                else: cords.append('value %d -> %s' % (random.choice([10, 20, 40, 60]), dst))
                if p['fam'] == 'source' or 'pitch' in p['ins']:
                    if random.random() < 0.2: extra.setdefault(n, []).append('att.%s=%g' % (inl, round(random.uniform(-2, 2), 2)))
            if inl not in SOUND and inl not in PULSE and inl != 'pitch' and random.random() < 0.15:
                d = p['defs'][i] if i < len(p['defs']) else 0
                extra.setdefault(n, []).append('%s=%g' % (inl, round(random.uniform(min(0, d), max(1, 2 * d)), 3)))
    for j, (n, p) in enumerate(names):
        if n in extra: lines[j] += ' ' + ' '.join(dict((e.split('=')[0], e) for e in extra[n]).values())
    outs = [n for n, p in names if p['outs']]
    for n in random.sample(outs, min(len(outs), random.randint(2, 4))):
        cords.append('%s -> %s' % (n, random.choice(['out', 'left', 'right'])))
    if random.random() < 0.3 and outs: cords.append('%s -> leds' % random.choice(outs))
    return '\n'.join(lines + cords) + '\n'

made = tries = 0
while made < NR and tries < NR * 60:
    tries += 1
    if write('r_%03d' % made, rnd_patch(), 0.30, 0.84): made += 1
print(len([f for f in os.listdir(out) if f.endswith('.dream')]), 'patches,', made, 'random', file=sys.stderr)
