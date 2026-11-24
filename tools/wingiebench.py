#!/usr/bin/env python3
"""wingiebench — talks to a Wingie running Sleepcousonique (tooling, not part of the app).

    wingiebench.py hello                 firmware, pool, a few status lines
    wingiebench.py watch [secs]          print the Wingie's status lines (CPU, memory, levels)
    wingiebench.py places OUT.csv        measure every place alone (SCQ TRY + SCQ MEASURE): its
                                         Wingie cycles per sample against the SP-1's, and the
                                         SCQ_WINGIE_SCALE that fits them best
    wingiebench.py dreams OUT.csv FILE... measure whole dreams (estimate vs measured)
    wingiebench.py stress                ceiling (how many of a heavy place before blocks are late),
                                         50 quick sends, scene and slot changes, a soak
    ... --port /dev/cu.usbserial-XXXX    which Wingie (default: the first usbserial port)

The editor holds the port while it is open: quit it first."""
import os, sys, termios, tty, time, select, re, glob, subprocess, statistics, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCQTOOL = os.path.join(HERE, "scqtool")
BLOCK = 64

class Port:
    def __init__(self, path):
        self.path = path
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        a = termios.tcgetattr(self.fd)
        a[2] |= termios.CLOCAL | termios.CREAD
        a[4] = a[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.buf = b''
        self.log = []
    def line(self, deadline):
        while True:
            i = self.buf.find(b'\n')
            if i >= 0:
                l, self.buf = self.buf[:i], self.buf[i + 1:]
                l = l.decode('utf-8', 'replace').strip('\r')
                self.log.append(l)
                return l
            left = deadline - time.time()
            if left <= 0: return None
            r, _, _ = select.select([self.fd], [], [], min(left, 0.2))
            if r:
                try: self.buf += os.read(self.fd, 4096)
                except OSError: pass
    def write(self, data):
        if isinstance(data, str): data = data.encode()
        while data:
            try: n = os.write(self.fd, data[:256]); data = data[n:]
            except BlockingIOError: time.sleep(0.002)
    def drain(self, secs=0.05):
        while self.line(time.time() + secs) is not None: pass
    def ask(self, cmd, want, secs=5):
        self.write(cmd + '\n'); end = time.time() + secs; got = []
        while True:
            l = self.line(end)
            if l is None: return got
            if l.startswith('SCQ WHANDS'): continue
            got.append(l)
            if re.search(want, l): return got
    def hello(self):
        # opening the port can reset the ESP32: give it time to boot, then ask
        for _ in range(8):
            got = self.ask('SCQ HELLO', r'SLEEPCOUSONIQUE', 1.0)
            for l in got:
                if l.startswith('SLEEPCOUSONIQUE'): return l
        return None
    def send(self, text, slot=None):
        b = text.encode()
        self.write((f'SCQ LOAD {len(b)} {slot}\n' if slot is not None else f'SCQ TRY {len(b)}\n').encode() + b)
        got = self.ask('', r'^(OK|ERR)', 12)
        return got[-1] if got else ''
    def measure(self, blocks=600):
        got = self.ask(f'SCQ MEASURE {blocks}', r'^SCQ MEASURED', blocks * BLOCK / 48000 + 5)
        for l in got:
            if l.startswith('SCQ MEASURED'):
                v = [int(x) for x in l.split()[2:]]
                return dict(avg=v[0], max=v[1], budget=v[2], blocks=v[3], dropped=v[4], mem=v[5], memtotal=v[6])
        return None
    def close(self): os.close(self.fd)

def find_port(args):
    if '--port' in args: return args[args.index('--port') + 1]
    p = sorted(glob.glob('/dev/cu.usbserial*') + glob.glob('/dev/cu.wchusbserial*'))
    if not p: sys.exit('no Wingie on USB (cu.usbserial*)')
    return p[0]

def catalog():
    out = subprocess.run([SCQTOOL, 'catalog'], capture_output=True, text=True).stdout
    places = []
    sp1 = set()
    for l in out.splitlines():
        if l.startswith('I ') and l.endswith(' sp1'): sp1.add(l.split()[1])
    for l in out.splitlines():
        if not l.startswith('P '): continue
        f = l.split(' | ')
        head = f[0].split()
        name, fam, cost = head[1], head[2], int(head[3])
        ins = head[5].split(',') if len(head) > 5 else []
        outs = f[1].split(',') if len(f) > 1 and f[1] else []
        places.append(dict(name=name, fam=fam, cost=cost, ins=[i for i in ins if i], outs=outs, sp1=name in sp1))
    return places

PULSE = ('strike', 'pluck', 'trig', 'trig1', 'trig2', 'clock', 'gate', 'next', 'record', 'burst', 'sync',
         'hold', 'capture', 'throw', 'go', 'punch', 'freeze', 'listen', 'reset', 'skip', 'flip', 'overdub', 'fast')
AUDIO = ('in', 'a', 'b', 'carrier', 'modulator', 'in1', 'in2', 'in3', 'in4', 'in5', 'in6', 'left', 'right')

def place_dream(p):
    """the place alone, fed a clock and a sound, as tools/test_places.sh does, for the Wingie"""
    L = ['instrument wingie', f'the x is {p["name"]}', 'the clk is clock', 'the src is vco 3']
    for i in p['ins']:
        if i in AUDIO: L.append(f'src -> x.{i}')
        elif i in PULSE: L.append(f'clk.eighth -> x.{i}')
    for k, o in enumerate(p['outs']):
        L.append('x -> out' if k == 0 else f'x.{o} -60> out')
    return '\n'.join(L) + '\n'

def estimate(text):
    with tempfile.NamedTemporaryFile('w', suffix='.dream', delete=False) as f:
        f.write(text); name = f.name
    out = subprocess.run([SCQTOOL, 'check', name], capture_output=True, text=True).stdout
    os.unlink(name)
    m = re.search(r'^M \d+ \d+ ([\d.]+)', out, re.M)
    return float(m.group(1)) if m else None

def copies_dream(p, n):
    """n copies of the place, fed one shared clock and sound, each heard quietly"""
    L = ['instrument wingie', 'the clk is clock', 'the src is vco 3', 'src -1> out']
    for k in range(n):
        L.append(f'the x{k} is {p["name"]}')
        for i in p['ins']:
            if i in AUDIO: L.append(f'src -> x{k}.{i}')
            elif i in PULSE: L.append(f'clk.eighth -> x{k}.{i}')
        L.append(f'x{k} -5> out')
    return '\n'.join(L) + '\n'

def cmd_places(P, out):
    """each place's cost: four copies of it against the same dream without them, so the place
    stands well above the measurement's noise (one copy alone hides in it)"""
    base = None
    if P.send(copies_dream(dict(name='', ins=[]), 0)).startswith('OK'):
        base = P.measure(800)['avg']
    print(f'harness alone (clock, vco, out): {base} cycles a block')
    rows, ratios = [], []
    with open(out, 'w') as f:
        f.write('place,sp1_cycles,wingie_cycles,ratio,copies,avg,max,budget,dropped,mem\n')
        for p in catalog():
            if p['fam'] in ('hands', 'ears') or p['sp1'] or p['name'] in ('value', 'crowd', 'visitors', 'slot'): continue
            got = None
            for n in (4, 2, 1):
                r = P.send(copies_dream(p, n))
                if r.startswith('OK'):
                    m = P.measure(800)
                    if m: got = (n, m); break
            if not got:
                print(f'{p["name"]:14s} would not fit even once: {r}'); continue
            n, m = got
            wcyc = max(1.0, (m['avg'] - base) / (n * BLOCK))
            ratio = wcyc / max(1, p['cost'])
            ratios.append(ratio)
            f.write(f'{p["name"]},{p["cost"]},{wcyc:.0f},{ratio:.2f},{n},{m["avg"]},{m["max"]},{m["budget"]},{m["dropped"]},{m["mem"]}\n')
            print(f'{p["name"]:14s} SP-1 {p["cost"]:4d}  Wingie {wcyc:6.0f} cycles/sample (x{n})  ratio {ratio:.2f}  dropped {m["dropped"]}')
    if ratios:
        print(f'\nWingie cycles / SP-1 cycles: median {statistics.median(ratios):.2f}')
        print(f'empty dream: {round(base / BLOCK)} cycles/sample')

def cmd_dreams(P, out, files):
    with open(out, 'w') as f:
        f.write('file,estimate,avg,max,budget,dropped,mem\n')
        for fn in files:
            t = open(fn).read()
            if 'instrument wingie' not in t: t = 'instrument wingie\n' + t
            est = estimate(t)
            r = P.send(t)
            if not r.startswith('OK'): print(f'{fn}: {r}'); continue
            m = P.measure(1500)
            a = 100 * m['avg'] / m['budget']; mx = 100 * m['max'] / m['budget']
            print(f'{os.path.basename(fn):36s} estimate {100*(est or 0):5.1f}%  measured {a:5.1f}% (peak {mx:5.1f}%)  dropped {m["dropped"]}  mem {m["mem"]}/{m["memtotal"]}')
            f.write(f'{fn},{est},{m["avg"]},{m["max"]},{m["budget"]},{m["dropped"]},{m["mem"]}\n')

def cmd_stress(P):
    print('-- ceiling: copies of a heavy place until blocks are late')
    for place in ('reverb', 'sympathetic', 'granular'):
        for n in (1, 2, 4, 6, 8, 10, 12, 16, 20, 24):
            L = ['instrument wingie', 'the src is vco 3'] + [f'the r{i} is {place}' for i in range(n)]
            L += [f'src -> r{i}' for i in range(n)] + [f'r{i} -10> out' for i in range(n)]
            r = P.send('\n'.join(L) + '\n')
            if not r.startswith('OK'): print(f'  {place} x{n}: {r}'); break
            m = P.measure(800)
            a = 100 * m['avg'] / m['budget']; mx = 100 * m['max'] / m['budget']
            print(f'  {place:12s} x{n:2d}: avg {a:5.1f}%  peak {mx:5.1f}%  dropped {m["dropped"]}  mem {m["mem"]}/{m["memtotal"]}')
            if m['dropped'] or mx > 100: break
    print('-- 50 quick sends (TRY), alternating two dreams')
    a = 'instrument wingie\nthe s is sine 3\ns -> out\n'
    b = 'instrument wingie\nthe s is vco 5\nthe f is lowpass\ns -> f -> out\n'
    t0 = time.time(); bad = 0
    for i in range(50):
        r = P.send(a if i % 2 else b)
        if not r.startswith('OK'): bad += 1
    print(f'  {50 - bad}/50 OK in {time.time() - t0:.1f} s')
    print('-- a dream with mistakes is refused and the old one keeps playing')
    print('  ', P.send('instrument wingie\nthe s is nothing\ns -> out\n'))
    print('-- an SP-1 dream is refused')
    print('  ', P.send('button1 -> out\n'))
    print('-- slots: keep a dream in slot 12, play slot 12, back to slot 1')
    print('  ', P.send('instrument wingie\n; bench\nthe s is sine 2\ns -> out\n', slot=11))
    print('  ', P.ask('SCQ SLOT 12', r'^OK', 5)[-1:])
    print('  ', P.ask('SCQ SLOT 1', r'^OK', 5)[-1:])
    print('-- scenes: a dream that changes scene by itself every 0.25 s for 10 s')
    sc = 'instrument wingie\nthe c is clock\n\none\n---\nthe s is sine 3\ns -> out\nc.bar -> scene.next\n\ntwo\n---\nthe s is vco 4\ns -> out\nc.bar -> scene.next\n'
    print('  ', P.send(sc))
    end = time.time() + 10; seen = set()
    P.write('SCQ WATCH 1\n')
    while time.time() < end:
        l = P.line(end)
        if l and l.startswith('SCQ WHANDS'): seen.add(l.split()[11])
    P.write('SCQ WATCH 0\n')
    print(f'   scenes seen: {sorted(seen)}')
    print('-- soak: the first light for 60 s, status every second')
    P.ask('SCQ SLOT 1', r'^OK', 5)
    end = time.time() + 60; worst = 0; drops = 0; lines = 0
    while time.time() < end:
        l = P.line(end)
        if l and 'SCQ engine avg=' in l:
            lines += 1
            mx = int(re.search(r'max=(\d+)%', l).group(1)); worst = max(worst, mx)
            drops += int(re.search(r'dropped=(\d+)', l).group(1))
    print(f'   {lines} reports, worst peak {worst}%, late blocks {drops}')

def main():
    a = sys.argv[1:]
    if not a: sys.exit(__doc__)
    P = Port(find_port(a))
    h = P.hello()
    if not h: sys.exit('no answer to SCQ HELLO (is the Sleepcousonique Wingie firmware on it?)')
    print(h)
    if a[0] == 'hello':
        for l in P.log:
            if 'pool' in l or 'slot' in l: print(l)
        end = time.time() + 3
        while time.time() < end:
            l = P.line(end)
            if l and 'SCQ engine' in l: print(l)
    elif a[0] == 'watch':
        secs = float(a[1]) if len(a) > 1 and a[1][0].isdigit() else 30
        end = time.time() + secs
        while time.time() < end:
            l = P.line(end)
            if l and not l.startswith('SCQ WHANDS'): print(l)
    elif a[0] in ('places', 'dreams'):
        P.ask('SCQ FALLBACK 0', r'^OK', 3)    # measure heavy dreams as they are, not what they fall back to
        if a[0] == 'places': cmd_places(P, a[1])
        else: cmd_dreams(P, a[1], [x for x in a[2:] if not x.startswith('--') and not x.startswith('/dev/')])
        P.ask('SCQ FALLBACK 1', r'^OK', 3)
    elif False: cmd_dreams(P, a[1], [x for x in a[2:] if not x.startswith('--') and not x.startswith('/dev/')])
    elif a[0] == 'stress': cmd_stress(P)
    P.close()

if __name__ == '__main__':
    main()
