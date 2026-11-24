#!/usr/bin/env python3
"""Sleepcousonique on the Wingie: the video manual, in the faltung look (night edition).

  python3 tools/wingie-video/build_video.py [--out ~/Desktop/Sleepcousonique-Wingie-Video-Manual.mp4]
        [--only 0-3] [--work DIR] [--still 4] [--print] [--check]

Modelled on the Cirrostratus video manual (monsoon-fw/tools/video/build_video.py). Every chapter
is one flat deep field: a mono code above a lowercase title, the text and faltung sliders on the
left; in the middle a faltung drawing of the Wingie2's panel, every control where it sits on the
real one (measured from Meng Qi's photo), or a screenshot of the editor; a turning icosahedron,
a stdio tracklist and folded strips on the right. While the narrator names a control, its row
lights up and the same control lights up on the panel (or a box on the screenshot), joined by a line.

Narration: Kokoro, British English "Emma" (bf_emma), run with the Cirrostratus video's venv
(~/monsoon-fw/tools/video/.venv, or tools/wingie-video/.venv if there is one). Words are in
narration.py; the place chapters are read from the manual (docs/wingie-manual), and every
place and hand the narrator names is checked against the engine's catalog first (--check does
only that).

Music: rendered from Sleepcousonique itself. bed.c plays the three scenes of bed.dream (the
Wingie's own resonators rung by wind and dust in a cavern), crossfaded, ducked under the voice.

Needs Pillow, numpy, ffmpeg, a C compiler. Never commit the video.
"""
import argparse, hashlib, json, math, os, re, shutil, subprocess, sys, tempfile
from multiprocessing import Pool

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MANUAL = os.path.join(ROOT, "docs", "wingie-manual")
FIG = os.path.join(MANUAL, "fig")
sys.path.insert(0, HERE)
import narration  # noqa: E402

VENVS = [os.path.join(HERE, ".venv", "bin", "python"),
         os.path.expanduser("~/monsoon-fw/tools/video/.venv/bin/python")]
FONTS = os.path.expanduser("~/faltung-kit/assets/fonts")
W, H, FPS = 1920, 1080, 30
SR = 48000

# ---------------------------------------------------------------------------
# Palette: night grounds (faltung deep, lightness 0.085-0.17, plum to indigo) with the dream
# journal's three signal colours as accents: umber (sound), deep blue (current), gold (pulse).
INK = (246, 244, 238)
GROUNDS = ["#1a1426", "#191a30", "#20172a", "#161c2e", "#1d1530", "#1b1d2b",
           "#221a2e", "#151a2b", "#1e1828", "#181e33"]
ACCENTS = ["#d2a531", "#c98a5a", "#7f9be0", "#e6c76a", "#b49ad8", "#d08a5a", "#8fb4e8", "#e0b35a"]
STRIPS = ["#5a3d2a", "#8a5a36", "#c98a5a", "#d2a531", "#e6c76a", "#7f9be0", "#3c4f8f", "#b49ad8"]


def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


F_SUB = font("Inter-Regular.ttf", 30)
F_BODY = font("Inter-Regular.ttf", 26)
F_MONO = font("IBMPlexMono-Regular.ttf", 22)
F_MONO_S = font("IBMPlexMono-Regular.ttf", 18)
F_MONO_M = font("IBMPlexMono-Medium.ttf", 23)
F_CODE = font("IBMPlexMono-Medium.ttf", 26)
F_PANEL = font("IBMPlexMono-Medium.ttf", 13)
F_PANEL_T = font("Inter-Regular.ttf", 30)


def title_font(text, width=860):
    for size in (96, 88, 80, 72, 64, 56):
        f = font("Inter-Medium.ttf", size)
        if f.getlength(text) <= width:
            return f
    return font("Inter-Medium.ttf", 52)


# ---------------------------------------------------------------------------
# The catalog, and the check that everything spoken exists.

def build_tools(work):
    exe = os.path.join(work, "scqtool")
    bed = os.path.join(work, "bed")
    eng = os.path.join(ROOT, "engine")
    common = [os.path.join(eng, "scq_core.c"), os.path.join(eng, "scq_places.c"), "-lm"]
    if not os.path.exists(exe):
        subprocess.run(["cc", "-std=c99", "-O2", "-DSCQ_HOST", "-I" + eng, os.path.join(ROOT, "tools", "scqtool.c")]
                       + common[:2] + ["-o", exe, "-lm"], check=True)
    if not os.path.exists(bed):
        subprocess.run(["cc", "-std=c99", "-O2", "-DSCQ_HOST", "-I" + eng, os.path.join(HERE, "bed.c")]
                       + common[:2] + ["-o", bed, "-lm"], check=True)
    return exe, bed


def catalog(exe):
    out = subprocess.run([exe, "catalog"], check=True, capture_output=True, text=True).stdout
    places = {}
    for ln in out.splitlines():
        if ln.startswith("P "):
            name = ln.split()[1]
            ins, outs = ln.split("|")[0].split()[5:], ln.split("|")[1]
            ins = ins[0].split(",") if ins else []
            places[name] = set(ins) | set(o.strip() for o in outs.split(",") if o.strip())
        elif ln.startswith("O ") and ln.split()[1] in places:
            for o in ln.split()[2:]:   # switches and params: name=positions
                k, _, v = o.partition("=")
                places[ln.split()[1]] |= {k + "="} | {p for p in v.split("|") if not re.fullmatch(r"[-\d.]+", p)}
    return places


NUMBERED = {"keys": 2, "key": 24, "fader": 3, "mode": 2, "octave": 2}


def known(token, places):
    """Is `token` (place, hand, or place.port) in the catalog?"""
    base, _, port = token.partition(".")
    m = re.fullmatch(r"([a-z]+?)(\d+)", base)
    if base not in places and m and m.group(1) in NUMBERED and 1 <= int(m.group(2)) <= NUMBERED[m.group(1)]:
        base = m.group(1)
    if base not in places:
        return False
    return not port or port in places[base]


def validate(chapters, places):
    bad = []
    for c in chapters:
        texts = [c["say"], c["desc"]] + [r["spoken"] or "" for r in c["rows"] if not r.get("place")] \
            + [r["text"] for r in c["rows"]]
        for t in texts:
            for tok in re.findall(r"`([^`]+)`", t):
                if not known(tok, places):
                    bad.append("%s: %s" % (c["code"], tok))
        for r in c["rows"]:
            if not r.get("place"):
                continue
            own = places.get(r["place"], set())
            for tok in re.findall(r"`([^`]+)`", r["spoken"]):   # the place's own inlets, outlets, switches
                k = tok.split("=")[0] + "=" if "=" in tok else tok
                if not (known(tok, places) or tok in own or k in own):
                    bad.append("%s: %s has no %s" % (c["code"], r["place"], tok))
        for r in c["rows"]:
            for k in r["hl"]:
                if k not in PANEL:
                    bad.append("%s: panel has no %s" % (c["code"], k))
            if r.get("place") and r["place"] not in places:
                bad.append("%s: no place %s" % (c["code"], r["place"]))
        if c.get("shot") and not os.path.exists(os.path.join(FIG, c["shot"])):
            bad.append("%s: no screenshot %s" % (c["code"], c["shot"]))
    if bad:
        raise SystemExit("the narration names things the engine doesn't have:\n  " + "\n  ".join(bad))
    n = sum(len(re.findall(r"`([^`]+)`", " ".join([c["say"]] + [r["spoken"] or "" for r in c["rows"]])))
            for c in chapters)
    print("narration checked: %d named places and hands, %d place rows, all in the catalog"
          % (n, sum(1 for c in chapters for r in c["rows"] if r.get("place"))))


# ---------------------------------------------------------------------------
# Content.

def clean_md(s):
    s = re.sub(r"\*\*(.+?)\*\*", r"\1", s)
    s = re.sub(r"\*(.+?)\*", r"\1", s)
    s = re.sub(r"`(.+?)`", r"\1", s)
    return s.strip()


SAY = [("Sleepcousonique", "Sleep-coo-so-neek"), ("sleepcousonique", "Sleep-coo-so-neek"),
       ("Wingie2", "Wingie two"), ("Meng Qi", "Mung Chee"), ("cocoquantus", "coco-quantus"),
       ("Cocoquantus", "Coco-quantus"), ("Quantussy", "Kwantussy"), ("paulstretch", "Paul-stretch"),
       ("PaulStretch", "Paul-stretch"), ("psola", "P-sola"), ("PSOLA", "P-sola"), ("eis", "E.I.S."),
       ("specfreeze", "spec-freeze"), ("specshift", "spec-shift"), ("specgate", "spec-gate"),
       ("specfilter", "spec-filter"), ("spectraldelay", "spectral delay"), ("formantshift", "formant shift"),
       ("graindelay", "grain delay"), ("grainfreeze", "grain freeze"), ("harmonicgrains", "harmonic grains"),
       ("beatgrains", "beat grains"), ("granularverb", "granular verb"), ("sittingroom", "sitting room"),
       ("talkingdrum", "talking drum"), ("fourbit", "four bit"), ("bandsplit", "band split"),
       ("autopan", "auto-pan"), ("autowah", "auto-wah"), ("pitchtrack", "pitch track"), ("tapeloop", "tape loop"),
       ("pt2399", "P T twenty-three ninety-nine"), ("PT2399", "P T twenty-three ninety-nine"),
       ("bassman", "bass man"), ("probee", "pro-bee"), ("relabi", "rel-ah-bee"), ("rollz", "rolls"),
       ("Rollz-5", "Rolls five"), ("Rollz", "Rolls"), ("gendy", "Gendy"), ("GENDY", "Gendy"),
       ("benjolin", "Benjolin"), ("midicc", "midi C C"), ("midiclock", "midi clock"),
       ("WSOLA", "W-sola"), ("FOF", "F.O.F."), ("UHJ", "U.H.J."), ("AGC", "A.G.C."), ("YIN", "yin"),
       ("ESP32", "E.S.P. thirty-two"), ("FFT", "F.F.T."), ("dBFS", "decibels"), ("USB", "U.S.B.")]


def speakable(s):
    s = clean_md(s)
    for a, b in SAY:
        s = re.sub(r"\b%s\b" % re.escape(a), b, s)
    s = re.sub(r"(\w)\.(left|right|both|pitch|go|next|back|decay|strike)\b", r"\1 \2", s)
    s = re.sub(r"(\d)\s*ms\b", r"\1 milliseconds", s)
    s = re.sub(r"(\d)\s*kHz\b", r"\1 kilohertz", s)
    s = re.sub(r"(\d)\s*Hz\b", r"\1 hertz", s)
    s = re.sub(r"(\d)\s*dB\b", r"\1 decibels", s)
    s = re.sub(r"(\d)\s*KB\b", r"\1 kilobytes", s)
    s = re.sub(r"(\d)\s*s\b", r"\1 seconds", s)
    s = re.sub(r"(\d)\s*m\b", r"\1 metres", s)
    s = re.sub(r"(\d)\s*%", r"\1 percent", s)
    s = re.sub(r"(?<![\w])\+(\d)", r"plus \1", s)
    s = re.sub(r"(?<![\w])[−-](\d)", r"minus \1", s)
    s = s.replace(" = ", " is ").replace("~", "about ")
    s = re.sub(r"(\d)\s*×", r"\1 times", s)
    s = re.sub(r"×\s*(\d)", r"times \1", s)
    for a, b in [("→", " to "), ("←", " to "), ("↔", " to "), ("·", ","), ("±", "plus or minus "),
                 ("≈", "about "), ("e.g.", "for example"), ("—", ","), ("–", " to "), ("/", " or "),
                 ("(", ", "), (")", ", "), ("+", " and "), ("=", " ")]:
        s = s.replace(a, b)
    s = re.sub(r"\s+,", ",", s)
    s = re.sub(r",\s*,", ",", s)
    s = re.sub(r",\s*\.", ".", s)
    s = re.sub(r",\s*:", ":", s)
    s = re.sub(r"\.\s*\.", ".", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


HAND_KEYS = [(r"\bmic\.left\b", ["mic_l"]), (r"\bmic\.right\b", ["mic_r"]), (r"\bmic\.both\b", ["mic_l", "mic_r"]),
             (r"\bkeys1\b", ["keys1"]), (r"\bkeys2\b", ["keys2"]), (r"\bkey([1-9]|1[0-2])\b", ["keys1"]),
             (r"\bkey(1[3-9]|2[0-4])\b", ["keys2"]), (r"\bmode1\b", ["mode1"]), (r"\bmode2\b", ["mode2"]),
             (r"\boctave1\b", ["oct1"]), (r"\boctave2\b", ["oct2"]), (r"\bsource\b", ["source"]),
             (r"\bfader1\b", ["f1"]), (r"\bfader2\b", ["f2"]), (r"\bfader3\b", ["f3"]),
             (r"\blights\.left\b", ["led1"]), (r"\blights\.right\b", ["led2"])]


def hands_in(code):
    keys = []
    for pat, ks in HAND_KEYS:
        if re.search(pat, code):
            keys += [k for k in ks if k not in keys]
    return keys


def manual_places(fname):
    """[(name, subtitle, dreamt, first sentence, hands the example uses)] from a manual chapter."""
    text = open(os.path.join(MANUAL, fname), encoding="utf-8").read()
    out = []
    for block in re.split(r"\n(?=#### )", text)[1:]:
        m = re.match(r"#### (\w+) — (.+)", block)
        dreamt = re.search(r"^\*Dreamt: (.+?)\*\s*$", block, re.M)
        body = block.split("\n")
        para = []
        started = False
        for ln in body[1:]:
            if ln.startswith("*Dreamt"):
                started = True
                continue
            if started and ln.strip() and not ln.startswith("<!--") and not ln.startswith("```"):
                para.append(ln.strip())
            elif started and para and not ln.strip():
                break
        first = re.split(r"(?<=[.!?])\s+(?=[A-Z`])", " ".join(para))[0]
        ex = re.search(r"```dream\n(.*?)```", block, re.S)
        out.append(dict(name=m.group(1), sub=m.group(2).strip(), dreamt=dreamt.group(1).strip(),
                        first=first, hands=hands_in(ex.group(1) if ex else "")))
    return out


def place_chapters():
    chs = []
    for code, title, sub, fname, a, b in narration.PLACE_CHAPTERS:
        ps = manual_places(fname)
        names = [p["name"] for p in ps]
        sel = ps[names.index(a):names.index(b) + 1]
        rows = []
        for p in sel:
            dreamt = p["dreamt"].rstrip(".")
            spoken = "`%s`: %s. %s." % (p["name"], p["sub"], dreamt[0].upper() + dreamt[1:])
            if len(p["first"].split()) <= 16:   # a short line of what it does, when there is one
                spoken += " " + p["first"]
            rows.append(dict(label=p["name"], text=p["dreamt"].rstrip(".").lower(), spoken=spoken,
                             hl=p["hands"], box=None, place=p["name"]))
        names_sel = ", ".join(p["name"] for p in sel[:-1]) + " and " + sel[-1]["name"]
        chs.append(dict(code=code, title=title, sub=sub,
                        desc="As each place is named, the panel lights the hands its little dream in the manual uses.",
                        say="The places: %s. %s." % (title.replace(", ii", ", part two"), sub),
                        rows=rows, kind="places", names=names_sel))
    return chs


def build_chapters():
    ch = [dict(c) for c in narration.BEFORE] + place_chapters() + [dict(c) for c in narration.AFTER]
    for i, c in enumerate(ch):
        c["index"] = i
        c["ground"] = hexrgb(GROUNDS[i % len(GROUNDS)])
        c["accent"] = hexrgb(ACCENTS[i % len(ACCENTS)])
    return ch


def segments(c):
    segs = [(c["say"], None)]
    for i, r in enumerate(c["rows"]):
        if r["spoken"]:
            segs.append((r["spoken"], i))
    return segs


# ---------------------------------------------------------------------------
# The Wingie2 panel, faltung style. Coordinates are in units of Meng Qi's photo of the panel
# (the README image, enlarged 4x and cropped to the panel: 670 x 780), so every control sits
# where it does on the real Wingie; PANEL_S scales it into the frame.

PANEL_W, PANEL_H = 670, 780
PANEL_S = 0.83
PANEL_X, PANEL_Y = 1010, 205
WHITE_X = [65, 155, 245, 335, 425, 515, 605]
BLACK_X = [110, 200, 380, 470, 560]
ROW_Y = {"k1b": 465, "k1w": 548, "k2b": 632, "k2w": 715}
KEY_POS = []    # (x, black?) for C..B
_w = iter(WHITE_X)
_b = iter(BLACK_X)
for _k in range(12):
    blk = _k in (1, 3, 6, 8, 10)
    KEY_POS.append((next(_b) if blk else next(_w), blk))
FADER_X = [245, 335, 425]
FADER_TOP, FADER_BOT = 125, 375

PANEL = {
    "mic_l": ("grille", 65, 120, 48), "mic_r": ("grille", 605, 120, 48),
    "source": ("toggle", 150, 115, 18),
    "oct1": ("toggle", 65, 250, 18), "oct2": ("toggle", 155, 250, 18),
    "mode1": ("button", 65, 382, 27), "mode2": ("button", 155, 382, 27),
    "led1": ("led", 513, 382, 9), "led2": ("led", 603, 382, 9),
    "keys1": ("keys", 335, 506, 0), "keys2": ("keys", 335, 673, 0),
}
for _i in range(3):
    PANEL["f%d" % (_i + 1)] = ("fader", FADER_X[_i], (FADER_TOP + FADER_BOT) // 2, 0)


def P(x, y):
    return PANEL_X + x * PANEL_S, PANEL_Y + y * PANEL_S


def circ(d, x, y, r, **kw):
    cx, cy = P(x, y)
    rr = r * PANEL_S
    d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], **kw)


def ptext(d, x, y, s, fill, f=F_PANEL):
    cx, cy = P(x, y)
    d.text((cx - f.getlength(s) / 2, cy - f.size * 0.6), s, font=f, fill=fill)


def panel_static(d, c):
    g = c["ground"]
    face = mix(g, INK, 0.11)
    edge = mix(g, INK, 0.32)
    lab = mix(INK, face, 0.25)
    hole = mix(g, (0, 0, 0), 0.55)
    x0, y0 = P(0, 0)
    x1, y1 = P(PANEL_W, PANEL_H)
    d.polygon([(x0 + 18, y0 + 18), (x1 + 18, y0 + 18), (x1 + 18, y1 + 18), (x0 + 18, y1 + 18)],
              fill=mix(g, (0, 0, 0), 0.3))
    d.rounded_rectangle([x0, y0, x1, y1], radius=10, fill=face, outline=edge, width=2)
    # the edge: what plugs in (the jacks sit on the Wingie's top edge)
    ex, ey = P(PANEL_W / 2, -22)
    s = "usb-c · midi in · line in · out"
    d.text((ex - F_PANEL.getlength(s) / 2, ey - 8), s, font=F_PANEL, fill=mix(INK, g, 0.5))
    # microphone grilles: a diamond of holes
    for k in ("mic_l", "mic_r"):
        _, gx, gy, _ = PANEL[k]
        for row in range(-3, 4):
            n = 4 - abs(row)
            for j in range(n):
                x = gx + (j - (n - 1) / 2) * 22
                circ(d, x, gy + row * 17, 4.5, fill=hole)
    ptext(d, 158, 66, "LINE IN", lab)
    ptext(d, 158, 166, "MIC", lab)
    ptext(d, 62, 214, "L.OCT", lab)
    ptext(d, 158, 214, "R.OCT", lab)
    ptext(d, 65, 322, "L.MODE", lab)
    ptext(d, 155, 322, "R.MODE", lab)
    for k in ("source", "oct1", "oct2"):
        _, x, y, r = PANEL[k]
        circ(d, x, y, r + 4, fill=mix(face, (0, 0, 0), 0.25))
    for i, x in enumerate(FADER_X):
        ax, ay = P(x - 7, FADER_TOP)
        bx, by = P(x + 7, FADER_BOT)
        d.rounded_rectangle([ax, ay, bx, by], radius=5, fill=hole)
        ptext(d, x, FADER_BOT + 24, ["MIX", "DECAY", "VOLUME"][i], lab)
    # the two keyboards: black keys dark, white keys pale
    for kb, (yb, yw) in (("1", (ROW_Y["k1b"], ROW_Y["k1w"])), ("2", (ROW_Y["k2b"], ROW_Y["k2w"]))):
        for x, blk in KEY_POS:
            circ(d, x, yb if blk else yw, 27, fill=(18, 16, 22) if blk else mix(face, INK, 0.75))
    ptext(d, PANEL_W - 70, 762, "WINGIE2", lab)


LAMP = [(200, 40, 40), (170, 70, 200), (235, 200, 60), (250, 250, 245)]


def panel_dynamic(d, c, t, hl, lamp, keys_down):
    g = c["ground"]
    acc = c["accent"]
    face = mix(g, INK, 0.11)
    pulse = 0.5 + 0.5 * math.sin(t * 5.0)
    for k in hl:
        kind, x, y, r = PANEL[k]
        if kind == "fader":
            ax, ay = P(x - 26, FADER_TOP - 14)
            bx, by = P(x + 26, FADER_BOT + 14)
            d.rounded_rectangle([ax, ay, bx, by], radius=10, outline=acc, width=3)
        elif kind == "keys":
            ax, ay = P(20, y - 80)
            bx, by = P(PANEL_W - 20, y + 80)
            d.rounded_rectangle([ax, ay, bx, by], radius=14, outline=acc, width=3)
        else:
            circ(d, x, y, r + 14 + 6 * pulse, outline=acc, width=3)
    # faders: caps move when talked about
    for i, x in enumerate(FADER_X):
        on = ("f%d" % (i + 1)) in hl
        v = 0.5 + (0.38 * math.sin(t * 1.4 + i) if on else 0.0)
        cy = FADER_BOT - (FADER_BOT - FADER_TOP) * v
        circ(d, x, cy, 15, fill=acc if on else mix(face, INK, 0.8))
    # toggles: a lever that flicks when talked about
    for k in ("source", "oct1", "oct2"):
        _, x, y, r = PANEL[k]
        on = k in hl
        ang = (math.sin(t * 2.2) * 0.9) if on else 0.0
        cx, cy = P(x, y)
        L = 22 * PANEL_S
        tip = (cx + math.sin(ang) * L, cy - math.cos(ang) * L)
        d.line([(cx, cy), tip], fill=acc if on else mix(face, INK, 0.75), width=5)
        d.ellipse([tip[0] - 4, tip[1] - 4, tip[0] + 4, tip[1] + 4], fill=INK)
    # Mode buttons
    for k in ("mode1", "mode2"):
        _, x, y, r = PANEL[k]
        on = k in hl
        circ(d, x, y, r, fill=mix(acc, g, 0.25) if on else mix(face, INK, 0.45))
    # keys being pressed (pale accent)
    for kb, idx in keys_down:
        x, blk = KEY_POS[idx]
        y = ROW_Y["k%d%s" % (kb, "b" if blk else "w")]
        circ(d, x, y, 27, fill=acc)
    # the two LEDs
    for i, lv in enumerate(lamp):
        col = LAMP[min(3, int(lv * 4))]
        circ(d, [513, 603][i], 382, 9, fill=col)
        circ(d, [513, 603][i], 382, 15, outline=mix(col, g, 0.5), width=2)


# ---------------------------------------------------------------------------
# Screenshot chapters: the editor where the panel would be.

SHOT_X, SHOT_Y, SHOT_W = 990, 190, 900
_SHOTS = {}


def shot_image(name, crop=None):
    """The screenshot, or the part of it given by crop (x, y, w, h), scaled to SHOT_W wide."""
    key = (name, crop)
    if key not in _SHOTS:
        im = Image.open(os.path.join(FIG, name)).convert("RGB")
        if crop:
            im = im.crop((crop[0], crop[1], crop[0] + crop[2], crop[1] + crop[3]))
        _SHOTS[key] = (im.resize((SHOT_W, int(im.height * SHOT_W / im.width)), Image.LANCZOS), SHOT_W / im.width)
    return _SHOTS[key]


def box_rect(box, scale, crop=None):
    x, y, w, h = box
    if crop:
        x, y = x - crop[0], y - crop[1]
    return (SHOT_X + x * scale, SHOT_Y + y * scale, SHOT_X + (x + w) * scale, SHOT_Y + (y + h) * scale)


# ---------------------------------------------------------------------------
# Other drawing (as in the Cirrostratus video).

PHI = (1 + 5 ** 0.5) / 2
ICO_V = np.array([(-1, PHI, 0), (1, PHI, 0), (-1, -PHI, 0), (1, -PHI, 0), (0, -1, PHI), (0, 1, PHI),
                  (0, -1, -PHI), (0, 1, -PHI), (PHI, 0, -1), (PHI, 0, 1), (-PHI, 0, -1), (-PHI, 0, 1)], float)
ICO_V /= np.linalg.norm(ICO_V[0])
ICO_F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
         (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
         (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]


def draw_ico(d, t, cx, cy, r, col, white_face=0):
    a, b = t * 0.35, t * 0.22 + 0.4
    ry = np.array([[math.cos(a), 0, math.sin(a)], [0, 1, 0], [-math.sin(a), 0, math.cos(a)]])
    rx = np.array([[1, 0, 0], [0, math.cos(b), -math.sin(b)], [0, math.sin(b), math.cos(b)]])
    v = ICO_V @ ry.T @ rx.T
    pts = [(cx + p[0] * r, cy - p[1] * r) for p in v]
    f = ICO_F[white_face]
    n = np.cross(v[f[1]] - v[f[0]], v[f[2]] - v[f[0]])
    if n[2] > 0:
        d.polygon([pts[k] for k in f], fill=INK)
    edges = set()
    for f in ICO_F:
        for i in range(3):
            edges.add(tuple(sorted((f[i], f[(i + 1) % 3]))))
    for e in edges:
        z = (v[e[0]][2] + v[e[1]][2]) / 2
        c = mix(col, INK, 0.35) if z > 0 else mix(col, (0, 0, 0), 0.35)
        d.line([pts[e[0]], pts[e[1]]], fill=c, width=3 if z > 0 else 1)


def skew_rect(d, x, y, w, h, fill, lean=0.55):
    o = h * lean
    d.polygon([(x + o, y), (x + w + o, y), (x + w, y + h), (x, y + h)], fill=fill)


def draw_strips(d, ox, oy, t, code):
    for i in range(6):
        c = hexrgb(STRIPS[(i + code) % len(STRIPS)])
        x = ox + i * 40
        y = oy + 16 * math.sin(t * 0.5 + i * 0.7)
        L = 95 - i * 8
        d.polygon([(x, y), (x + 34, y - 20), (x + 34, y - 20 + L), (x, y + L)], fill=c)
        d.polygon([(x + 34, y - 20), (x + 68, y), (x + 68, y + L), (x + 34, y - 20 + L)],
                  fill=mix(c, (0, 0, 0), 0.28))


def wrap(text, fnt, width):
    words, lines, cur = text.split(), [], ""
    for w_ in words:
        test = (cur + " " + w_).strip()
        if fnt.getlength(test) > width and cur:
            lines.append(cur)
            cur = w_
        else:
            cur = test
    if cur:
        lines.append(cur)
    return lines


TEXT_R = 930
ROW_LX, ROW_SX0, ROW_SX1, ROW_TX = 96, 372, 462, 492


def static_layer(c, chapters):
    im = Image.new("RGB", (W, H), c["ground"])
    d = ImageDraw.Draw(im)
    dim = mix(INK, c["ground"], 0.38)
    rule = mix(INK, c["ground"], 0.75)
    d.text((96, 64), c["code"], font=F_CODE, fill=c["accent"])
    d.text((96 + F_CODE.getlength(c["code"]) + 22, 66), "sleepcousonique · on the wingie", font=F_MONO_S, fill=dim)
    ft = title_font(c["title"], TEXT_R - 96)
    d.text((90, 100), c["title"], font=ft, fill=INK)
    y = 220
    for ln in wrap(c["sub"], F_SUB, TEXT_R - 96)[:2]:
        d.text((96, y), ln, font=F_SUB, fill=dim)
        y += 40
    y += 8
    nrows = len(c["rows"])
    maxlines = 5 if nrows <= 6 else (3 if nrows <= 8 else 2)
    for ln in wrap(clean_md(c["desc"]), F_BODY, TEXT_R - 96)[:maxlines]:
        d.text((96, y), ln, font=F_BODY, fill=INK)
        y += 36
    c["rows_y"] = max(y + 34, 450 if nrows > 8 else 500)
    d.line([(96, c["rows_y"] - 18), (TEXT_R, c["rows_y"] - 18)], fill=rule, width=1)
    if c.get("shot"):
        img, _ = shot_image(c["shot"], c.get("crop"))
        d.rectangle([SHOT_X + 14, SHOT_Y + 14, SHOT_X + img.width + 14, SHOT_Y + img.height + 14],
                    fill=mix(c["ground"], (0, 0, 0), 0.3))
        im.paste(img, (SHOT_X, SHOT_Y))
        d.rectangle([SHOT_X, SHOT_Y, SHOT_X + img.width, SHOT_Y + img.height], outline=mix(INK, c["ground"], 0.6))
        # the tracklist, as one line under the screenshot
        idx = c["index"]
        s = "  ·  ".join(o["code"] for o in chapters[max(0, idx - 3):idx + 4])
        d.text((SHOT_X, SHOT_Y + img.height + 40), "tracklist   " + s, font=F_MONO_S, fill=dim)
        return im
    panel_static(d, c)
    tx, ty = 1625, 470
    d.text((tx, ty - 40), "tracklist", font=F_MONO_S, fill=dim)
    idx = c["index"]
    lo = max(0, min(idx - 3, len(chapters) - 9))
    for k, other in enumerate(chapters[lo:lo + 9]):
        yy = ty + k * 32
        cur = other["index"] == idx
        if cur:
            skew_rect(d, tx - 10, yy - 2, 270, 28, c["accent"], 0.5)
        col = c["ground"] if cur else (INK if other["index"] < idx else dim)
        d.text((tx, yy), "%s  %s" % (other["code"], other["title"][:16]), font=F_MONO_S, fill=col)
    return im


def row_geometry(c):
    n = len(c["rows"])
    y0 = c["rows_y"]
    gap = min(64, (1030 - y0) // max(1, n))
    return y0, gap


def draw_rows(d, c, t, active):
    rows = c["rows"]
    if not rows:
        return
    y0, gap = row_geometry(c)
    dim = mix(INK, c["ground"], 0.45)
    small = gap < 44
    for i, r in enumerate(rows):
        y = y0 + i * gap
        on = i == active
        col = INK if on else dim
        d.text((ROW_LX, y), r["label"].lower()[:20], font=F_MONO_M if on else F_MONO, fill=col)
        d.line([(ROW_SX0, y + 15), (ROW_SX1, y + 15)], fill=mix(INK, c["ground"], 0.65), width=1)
        v = 0.5 + 0.42 * math.sin(t * (0.6 + 0.13 * i) + i * 1.3)
        if on:
            v = 0.5 + 0.48 * math.sin(t * 1.7 + i)
        wv = (ROW_SX1 - ROW_SX0) * v
        skew_rect(d, ROW_SX0, y + 6, wv, 18, c["accent"] if on else mix(c["accent"], c["ground"], 0.45))
        d.line([(ROW_SX0 + wv + 9, y + 4), (ROW_SX0 + wv, y + 25)], fill=INK, width=2)
        txt = r["text"].lower()
        if F_MONO.getlength(txt) <= TEXT_R - ROW_TX:
            d.text((ROW_TX, y + 1), txt, font=F_MONO, fill=col)
            continue
        lines = wrap(txt, F_MONO_S, TEXT_R - ROW_TX)
        if len(lines) == 1 or small:
            s = lines[0] + (" …" if len(lines) > 1 else "")
            d.text((ROW_TX, y + 3), s, font=F_MONO_S, fill=col)
        else:
            d.text((ROW_TX, y - 6), lines[0], font=F_MONO_S, fill=col)
            d.text((ROW_TX, y + 17), lines[1] + (" …" if len(lines) > 2 else ""), font=F_MONO_S, fill=col)


def draw_connector(d, c, active, t):
    r = c["rows"][active]
    y0, gap = row_geometry(c)
    y = y0 + active * gap + 15
    acc = c["accent"]
    if c.get("shot"):
        if not r.get("box"):
            return
        _, scale = shot_image(c["shot"], c.get("crop"))
        x0, y0b, x1, y1 = box_rect(r["box"], scale, c.get("crop"))
        pulse = 0.5 + 0.5 * math.sin(t * 5.0)
        g = 3 + 3 * pulse
        d.rectangle([x0 - g, y0b - g, x1 + g, y1 + g], outline=acc, width=4)
        tx, ty = SHOT_X - 6, (y0b + y1) / 2   # to the screenshot's edge, beside the box
        d.line([(tx, ty), (x0 - g, ty)], fill=acc, width=1)
    else:
        if not r["hl"]:
            return
        kind, x, py, rad = PANEL[r["hl"][0]]
        tx, ty = P(x, py)
        if kind == "fader":
            ty = P(x, FADER_TOP - 14)[1] if y < ty else P(x, FADER_BOT + 14)[1]
            tx -= 26 * PANEL_S
        elif kind == "keys":
            tx = P(20, 0)[0]
        else:
            tx -= (rad + 14) * PANEL_S
    elbow = 965
    d.line([(TEXT_R + 12, y), (elbow, y), (tx, ty)], fill=acc, width=2)
    d.ellipse([TEXT_R + 6, y - 6, TEXT_R + 18, y + 6], fill=acc)


def lamp_and_keys(c, t, hl):
    """What the panel's lights and keys do while a chapter plays."""
    lamp = [0.1, 0.1]
    keys = []
    if c["kind"] == "slots":
        ph = int(t / 1.4) % 12
        keys = [(1, ph)]
        if int(t * 8) % 2 == 0 and (t % 1.4) < 0.4:
            lamp = [0.6, 0.6]
    elif c["kind"] == "heavy":
        if int(t / 3) % 2 == 0 and int(t * 8) % 2 == 0:
            lamp = [0.3, 0.3]
    elif "keys1" in hl or "keys2" in hl:
        kb = 1 if "keys1" in hl else 2
        keys = [(kb, [0, 4, 7, 11, 7, 4][int(t / 0.6) % 6])]
        lamp = [0.55 + 0.4 * math.sin(t * 3) ** 2, lamp[1]]
    if "led1" in hl or "led2" in hl:
        v = (t * 0.25) % 1.0
        lamp = [v, (v + 0.5) % 1.0]
    return lamp, keys


def render_chapter(args):
    c, chapters, dur, timeline, path = args
    if os.path.exists(path) and os.path.getsize(path) > 0 and os.path.exists(path + ".done"):
        return path
    base = static_layer(c, chapters)
    nframes = int(round(dur * FPS))
    ff = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", "%dx%d" % (W, H), "-r", str(FPS), "-i", "-", "-c:v", "libx264",
                           "-preset", "medium", "-crf", "19", "-pix_fmt", "yuv420p", path],
                          stdin=subprocess.PIPE)
    nrows = len(c["rows"])
    for f in range(nframes):
        t = f / FPS
        im = base.copy()
        d = ImageDraw.Draw(im)
        if not c.get("shot"):
            draw_ico(d, t + c["index"] * 3.1, 1755, 225, 120, c["accent"], white_face=c["index"] % 20)
        draw_strips(d, 1640 if not c.get("shot") else 1660, 940, t, c["index"])
        active = None
        for start, ri in timeline:
            if t >= start and ri is not None:
                active = ri
        if active is None and nrows and not any(ri is not None for _, ri in timeline):
            active = int(t / max(dur, 0.1) * nrows) % nrows
        draw_rows(d, c, t, active)
        hl = c["rows"][active]["hl"] if active is not None else []
        if not c.get("shot"):
            lamp, keys = lamp_and_keys(c, t, hl)
            panel_dynamic(d, c, t, hl, lamp, keys)
        if active is not None:
            draw_connector(d, c, active, t)
        prog = (c["index"] + t / dur) / len(chapters)
        skew_rect(d, 0, H - 8, W * prog, 8, c["accent"], 0.5)
        a = min(1.0, t / 0.45, (dur - t) / 0.45)
        if a < 1.0:
            im = Image.blend(Image.new("RGB", (W, H), (10, 8, 16)), im, max(0.0, a))
        ff.stdin.write(im.tobytes())
    ff.stdin.close()
    ff.wait()
    open(path + ".done", "w").write("%.3f" % dur)
    return path


# ---------------------------------------------------------------------------
# Audio.

def venv_python():
    for p in VENVS:
        if os.path.exists(p):
            return p
    raise SystemExit("no Kokoro venv: python3.12 -m venv tools/wingie-video/.venv && "
                     "tools/wingie-video/.venv/bin/pip install kokoro soundfile")


def speak_all(texts, work):
    paths, jobs = [], []
    for s in texts:
        h = hashlib.sha1(("bf_emma|" + s).encode()).hexdigest()[:16]
        p = os.path.join(work, "tts_%s.wav" % h)
        paths.append(p)
        jobs.append(dict(text=s, out=p))
    jf = os.path.join(work, "tts_jobs.json")
    json.dump(jobs, open(jf, "w"))
    subprocess.run([venv_python(), os.path.join(HERE, "tts_kokoro.py"), jf], check=True)
    out = []
    for p in paths:
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", p, "-ar", str(SR), "-ac", "1", "-f", "f32le", "-"],
                             check=True, stdout=subprocess.PIPE).stdout
        out.append(np.frombuffer(raw, np.float32).copy())
    return out


def make_bed(bed_exe, work, seconds):
    """The three scenes of bed.dream, 100 s each, crossfaded over 10 s, round and round."""
    part, xf = 100.0, 10.0
    scenes = []
    for s in range(3):
        p = os.path.join(work, "bed%d.f32" % s)
        if not os.path.exists(p):
            subprocess.run([bed_exe, os.path.join(HERE, "bed.dream"), str(s), str(part), p, "0.5", "0.5", "0.5"],
                           check=True)
        x = np.fromfile(p, np.float32).reshape(-1, 2)
        scenes.append(x[int(8 * SR):])   # past the first swell
    n = int(seconds * SR) + SR
    out = np.zeros((n, 2), np.float32)
    pos, k = 0, 0
    fadein = np.linspace(0, 1, int(xf * SR))[:, None]
    while pos < n:
        x = scenes[k % 3].copy()
        x[:len(fadein)] *= fadein
        x[-len(fadein):] *= fadein[::-1]
        m = min(len(x), n - pos)
        out[pos:pos + m] += x[:m]
        pos += len(x) - len(fadein)
        k += 1
    return out[:n]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.expanduser("~/Desktop/Sleepcousonique-Wingie-Video-Manual.mp4"))
    ap.add_argument("--only", help="chapter range a-b (for previews)")
    ap.add_argument("--work", default=None)
    ap.add_argument("--jobs", type=int, default=max(1, os.cpu_count() - 2))
    ap.add_argument("--still", help="write one frame per chapter as PNG at this time (s) and stop")
    ap.add_argument("--print", action="store_true", help="print the spoken text and stop")
    ap.add_argument("--check", action="store_true", help="only check the narration against the catalog")
    a = ap.parse_args()

    work = os.path.abspath(a.work or tempfile.mkdtemp(prefix="scq_wingie_video_"))
    os.makedirs(work, exist_ok=True)
    scqtool, bed_exe = build_tools(work)
    chapters = build_chapters()
    validate(chapters, catalog(scqtool))
    if a.check:
        return
    sel = chapters
    if a.only:
        lo, hi = (int(x) for x in a.only.split("-"))
        sel = chapters[lo:hi + 1]
    if a.print:
        for c in sel:
            for s, ri in segments(c):
                print("[%s/%s] %s" % (c["code"], ri, speakable(s)))
        return
    print("work dir", work, "-", len(sel), "chapters")

    lead, gap, tail = 0.7, 0.45, 1.5
    all_segs = [(ci, ri, speakable(s)) for ci, c in enumerate(sel) for s, ri in segments(c)]
    voices = speak_all([s for _, _, s in all_segs], work)
    durs, timelines, placed = [], [], []
    pos = 0.0
    for ci, c in enumerate(sel):
        t = lead
        tl = []
        for (cj, ri, _), v in zip(all_segs, voices):
            if cj != ci:
                continue
            tl.append((t, ri))
            placed.append((pos + t, v))
            t += len(v) / SR + gap
        d = max(t - gap + tail, 7.0)
        if not any(ri is not None for _, ri in tl):
            d = max(d, len(c["rows"]) * 2.2)
        durs.append(d)
        timelines.append(tl)
        pos += d
    total = pos
    print("total %.1f s (%.1f min)" % (total, total / 60))

    if a.still:
        ts = float(a.still)
        for c, d, tl in zip(sel, durs, timelines):
            render_still(c, chapters, d, tl, min(ts, d - 0.6), os.path.join(work, "still_%03d.png" % c["index"]))
        return

    free = shutil.disk_usage(os.path.dirname(os.path.expanduser(a.out))).free
    if free < 6e9:
        raise SystemExit("only %.1f GB free: the encode needs room" % (free / 1e9))
    jobs = [(c, chapters, d, tl, os.path.join(work, "v%03d.mp4" % c["index"]))
            for c, d, tl in zip(sel, durs, timelines)]
    with Pool(a.jobs) as pool:
        parts = pool.map(render_chapter, jobs)
    listing = os.path.join(work, "parts.txt")
    open(listing, "w").write("".join("file '%s'\n" % p for p in parts))
    video = os.path.join(work, "video.mp4")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", listing, "-c", "copy",
                    video], check=True)

    n = int(total * SR) + SR
    voice = np.zeros(n, np.float32)
    for start, v in placed:
        s = int(start * SR)
        voice[s:s + len(v)] += v[:max(0, n - s)]
    music = make_bed(bed_exe, work, n / SR)[:n]
    music /= max(1e-6, np.abs(music).max())
    env = np.abs(voice)
    att, rel = 1 - math.exp(-1 / (0.04 * SR)), 1 - math.exp(-1 / (0.6 * SR))
    e = np.zeros_like(env)
    acc = 0.0
    step = 64
    for i in range(0, n, step):
        x = env[i:i + step].max()
        acc = acc + ((att * step) if x > acc else (rel * step)) * (x - acc)
        acc = max(0.0, min(1.0, acc))
        e[i:i + step] = acc
    speaking = np.clip(e * 12, 0, 1)
    gain = 0.5 - 0.38 * speaking
    fade = np.ones(n, np.float32)
    fi, fo = int(2.5 * SR), int(5 * SR)
    fade[:fi] = np.linspace(0, 1, fi)
    fade[-fo:] = np.linspace(1, 0, fo)
    music *= (gain * fade)[:, None]
    vpk = np.abs(voice).max()
    if vpk > 0:
        voice *= 0.8 / vpk
    mixd = music + voice[:, None]
    peak = np.abs(mixd).max()
    if peak > 0.98:
        mixd *= 0.98 / peak
    audio = os.path.join(work, "audio.f32")
    mixd.astype(np.float32).tofile(audio)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", video, "-f", "f32le", "-ar", str(SR), "-ac", "2",
                    "-i", audio, "-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest",
                    "-movflags", "+faststart", os.path.expanduser(a.out)], check=True)
    print(a.out)


def render_still(c, chapters, dur, timeline, t, png):
    """One frame, for checking the layout."""
    import types
    frames = []

    class Cap:
        def __init__(self):
            self.stdin = types.SimpleNamespace(write=lambda b: frames.append(b), close=lambda: None)

        def wait(self):
            pass
    real = subprocess.Popen
    subprocess.Popen = lambda *a, **k: Cap()
    try:
        tmp = png + ".mp4"
        global FPS
        fps0 = FPS
        FPS = 1.0 / max(t, 0.01)
        render_chapter((c, chapters, max(dur, t + 0.6), timeline, tmp))
        FPS = fps0
        if os.path.exists(tmp + ".done"):
            os.remove(tmp + ".done")
    finally:
        subprocess.Popen = real
    Image.frombytes("RGB", (W, H), frames[min(1, len(frames) - 1)]).save(png)
    print(png)


if __name__ == "__main__":
    main()
