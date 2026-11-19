#!/usr/bin/env python3
"""sp1fleet — measures dreams on every SP-1 that is plugged in, at once (tooling, not part of the app).

     sp1fleet.py who                        each SP-1: port, firmware, serial, unit, playing slot
     sp1fleet.py slots                      which slots hold a dream (each SP-1 is left on its slot)
     sp1fleet.py bench OUT.csv FILE...      measure every dream once, shared out over the SP-1s
     sp1fleet.py bench OUT.csv --each FILE...   measure every dream on every SP-1 (unit against unit)
     ... --port /dev/cu.usbmodemX               only that SP-1 (repeat for several)

   Firmware 1.4 and later: each dream is played without being kept ("SCQ TRY": no slot is written)
   and measured over exactly 1500 blocks ("SCQ MEASURE"): a line "file,unit,cyc,AVG,MAX,BUDGET,
   blocks,dropped,memory used,memory" (cycles per 2 ms block). About 4 s a dream.
   Firmware before 1.4: each dream is SENT TO A SLOT (--slot N, default the first empty one; nothing is
   measured if there is none) and the SP-1's own 5 s report is read: "file,unit,avg %,max %,
   places,over,audio avg,audio max". About 12 s a dream.
   The editor holds the port while it is open: quit it first. tools/calibrate.py reads the csv."""
import os, sys, termios, tty, time, select, threading, queue, re, glob

class Port:
    def __init__(self, path):
        self.path = path
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        a = termios.tcgetattr(self.fd); a[2] |= termios.CLOCAL | termios.CREAD
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.buf = b''; self.version = ''; self.serial = ''; self.unit = ''; self.slot = None
    def line(self, deadline):
        while True:
            i = self.buf.find(b'\n')
            if i >= 0:
                l, self.buf = self.buf[:i], self.buf[i + 1:]
                return l.decode('utf-8', 'replace').strip('\r')
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
    def drain(self):
        while self.line(time.time() + 0.05) is not None: pass
    def settle(self):                 # opening the port replays the stored log: let it finish
        end = time.time() + 15
        while True:
            l = self.line(min(end, time.time() + 2.0))
            if l is None or 'STORED LOG: end' in l: break
        time.sleep(0.3); self.drain()
    def ask(self, cmd, want, secs=5):
        self.write(cmd + '\n'); end = time.time() + secs; got = []
        while True:
            l = self.line(end)
            if l is None: return got
            if 'SCQ HANDS' in l: continue
            got.append(l)
            if re.search(want, l): return got
    def hello(self):
        self.settle()
        for l in self.ask('SCQ HELLO', r'^SLEEPCOUSONIQUE'):
            if l.startswith('SLEEPCOUSONIQUE'): self.version = l.split()[1]
        for l in self.ask('SCQ WHO', r'^SCQ ME'):
            if l.startswith('SCQ ME'): t = l.split(); self.serial, self.unit = t[2], t[3]
        end = time.time() + 7
        while self.slot is None:
            l = self.line(end)
            if l is None: break
            m = re.search(r'SCQ engine .* slot=(\d+)', l)
            if m: self.slot = int(m.group(1))
        return self
    def modern(self):                 # 1.4 and later: SCQ TRY and SCQ MEASURE
        m = re.match(r'(\d+)\.(\d+)', self.version)
        return bool(m) and (int(m.group(1)), int(m.group(2))) >= (1, 4)
    def slots(self):
        full = {}
        for s in range(1, 6):
            for l in self.ask('SCQ SLOT %d' % s, r'SCQ slot %d' % s):
                m = re.search(r'SCQ slot %d: (\d+) bytes' % s, l)
                if m: full[s] = int(m.group(1))
        if self.slot: self.ask('SCQ SLOT %d' % self.slot, r'SCQ slot %d' % self.slot)
        return full
    def close(self): os.close(self.fd)

def ports():
    return sorted(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/ttyACM*'))

def measure(P, path, slot, blocks=1500):
    body = open(path, 'rb').read(); name = os.path.basename(path); P.drain()
    if P.modern():
        P.write('SCQ TRY %d\n' % len(body)); P.write(body)
        end = time.time() + 6
        while True:
            l = P.line(end)
            if l is None: return '%s,%s,ERR,no answer' % (name, P.serial)
            if l.startswith('ERR'): return '%s,%s,ERR,%s' % (name, P.serial, l)
            if l.startswith('OK'): break
        time.sleep(0.4)                           # past the first blocks of a new dream
        for l in P.ask('SCQ MEASURE %d' % blocks, r'^SCQ MEASURED', 4 + blocks * 0.004):
            if l.startswith('SCQ MEASURED'):
                t = l.split()
                return '%s,%s,cyc,%s' % (name, P.serial, ','.join(t[2:]))
        return '%s,%s,ERR,no measure' % (name, P.serial)
    P.write('SCQ LOAD %d %d\n' % (len(body), slot - 1)); P.write(body)
    end = time.time() + 16; ok = False; n = 0; eng = None
    while True:
        l = P.line(end)
        if l is None: return '%s,%s,ERR,no report' % (name, P.serial)
        if l.startswith('OK'): ok = True
        if l.startswith('ERR'): return '%s,%s,ERR,%s' % (name, P.serial, l)
        m = re.search(r'SCQ engine avg=(\d+)% max=(\d+)%\s+places=(\d+)', l)
        if ok and m:
            n += 1                                # the first report straddles the load: keep the second
            if n >= 2: eng = (m.group(1), m.group(2), m.group(3), '1' if 'OVER' in l else '0')
        m = re.search(r'AUD .*cyc avg=([\d.]+)% max=([\d.]+)%', l)
        if eng and m: return '%s,%s,%s,%s,%s' % (name, P.serial, ','.join(eng), m.group(1), m.group(2))

def worker(path, q, out, lock, slot):
    P = Port(path).hello()
    if not P.modern() and not slot:
        full = P.slots(); empty = [s for s in range(1, 6) if s not in full]
        if not empty:
            sys.stderr.write('%s: every slot holds a dream; give --slot N to measure over one\n' % path); P.close(); return
        slot = empty[0]
        sys.stderr.write('%s (%s, firmware %s): measuring through slot %d\n' % (path, P.serial, P.version, slot))
    while True:
        try: f = q.get_nowait()
        except queue.Empty: break
        line = measure(P, f, slot)
        with lock: out.write(line + '\n'); out.flush()
    if P.slot: P.ask('SCQ SLOT %d' % P.slot, r'SCQ slot %d' % P.slot)     # back to what it was playing
    P.close()

def main():
    a = sys.argv[1:]
    if not a or a[0] not in ('who', 'slots', 'bench'): sys.exit(__doc__)
    if a[0] in ('who', 'slots'):
        for p in ports():
            P = Port(p).hello()
            print('%s  firmware %s  serial %s  unit %s  playing slot %s' % (p, P.version, P.serial, P.unit, P.slot))
            if a[0] == 'slots':
                full = P.slots()
                for s in range(1, 6): print('    slot %d: %s' % (s, '%d bytes' % full[s] if s in full else 'empty'))
            P.close()
        return
    out = open(a[1], 'a'); rest = a[2:]; slot = 0; each = False; only = []
    while '--port' in rest: i = rest.index('--port'); only.append(rest[i + 1]); del rest[i:i + 2]
    if '--slot' in rest: i = rest.index('--slot'); slot = int(rest[i + 1]); del rest[i:i + 2]
    if '--each' in rest: rest.remove('--each'); each = True
    lock = threading.Lock(); ts = []; shared = queue.Queue()
    for f in rest: shared.put(f)
    for p in (only or ports()):
        q = shared
        if each:
            q = queue.Queue()
            for f in rest: q.put(f)
        t = threading.Thread(target=worker, args=(p, q, out, lock, slot)); t.start(); ts.append(t)
    for t in ts: t.join()

main()
