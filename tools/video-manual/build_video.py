#!/usr/bin/env python3
"""Sleepcousonique, the video manual: the SP-1 and the Wingie in one film, in the faltung look
(rainbow edition).

  python3 tools/video-manual/build_video.py [--out ~/Desktop/Sleepcousonique-Video-Manual.mp4]
        [--chapters ~/Desktop/Sleepcousonique-Video-Manual-chapters.txt]
        [--only 0-3] [--work DIR] [--still 4] [--print] [--check] [--shots] [--app PATH]

One builder for both instruments (it replaces tools/sp1-video and tools/wingie-video). Every
chapter is one flat deep field: a mono code above a lowercase title, the text and faltung sliders
on the left; in the middle a faltung drawing of the SP-1 (every control where it sits on the real
one, measured from the photo in ref/), of the Wingie2 (measured from Meng Qi's photo: the
microphones, the Line In / Mic switch, the octave switches, the Mode buttons, Mix, Decay and
Volume, the lights, and the two keyboards, keys2 on top), of both side by side, or a screenshot
of the editor; a turning icosahedron, a stdio tracklist and folded strips on the right. While the
narrator names a control, its row lights up and the same control lights up on the instrument (or
a box on the screenshot), joined by a line.

The rainbow: every part of the film has its own hue, walking round the colour wheel (red, orange,
yellow, green, cyan, blue, violet, magenta), and each chapter steps a little further within its
part. The hue tints the deep ground (lightness about 0.2, barely coloured, so the text stays
high-contrast) and gives the accent: the code, the sliders, the highlight rings and lines. The
icosahedron's edges, the folded strips and the progress bar along the bottom carry the whole
spectrum; the title card's title is a rainbow gradient over seven thin folded bands.

Screenshots: made here, by the editor's own --shot, with --no-link --no-radio (the serial port and
Bluetooth are never touched, and --shot never opens an input). The editor's settings file is
copied first and put back byte for byte afterwards. The file path in the journal look's status bar
is covered in the bar's own colour (the repository is public).

Narration: Kokoro, British English "Emma" (bf_emma), run with the Cirrostratus video's venv
(~/monsoon-fw/tools/video/.venv, or tools/video-manual/.venv if there is one). Words are in
narration.py; every place and hand the narrator names is checked against the engine's catalog
first, for the instrument the chapter is about (--check does only that).

Music: rendered from Sleepcousonique itself. bed.c plays the three scenes of bed-sp1.dream (a
string, a marimba and a bell rung by dust, rain and wind in a cavern) and the three of
bed-wingie.dream (the Wingie's own resonators rung by wind and dust), taking turns, crossfaded,
ducked under the voice.

Chapters: the YouTube chapter list is written from the chapters' rendered lengths (every
chapter is a whole number of frames, so the starts are exact).

Needs Pillow, numpy, ffmpeg, a C compiler, and a built editor (build/Sleepcousonique_artefacts).
Never commit the video.
"""
import argparse, colorsys, hashlib, json, math, os, re, shutil, subprocess, sys, tempfile
from multiprocessing import Pool

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import narration  # noqa: E402

VENVS = [os.path.join(HERE, ".venv", "bin", "python"),
         os.path.expanduser("~/monsoon-fw/tools/video/.venv/bin/python")]
APP = os.path.join(ROOT, "build", "Sleepcousonique_artefacts", "Release", "Sleepcousonique.app",
                   "Contents", "MacOS", "Sleepcousonique")
SETTINGS = os.path.expanduser("~/Library/Application Support/Sleepcousonique.settings")
FONTS = os.path.expanduser("~/faltung-kit/assets/fonts")
W, H, FPS = 1920, 1080, 30
SR = 48000

# ---------------------------------------------------------------------------
# Colour: OKLCH, so every hue of the rainbow sits at the same lightness.

INK = (244, 243, 236)


def oklch(L, C, h):
    """OKLCH (h in degrees) to sRGB 0-255, chroma pulled in until it fits."""
    for _ in range(40):
        a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
        l_ = L + 0.3963377774 * a + 0.2158037573 * b
        m_ = L - 0.1055613458 * a - 0.0638541728 * b
        s_ = L - 0.0894841775 * a - 1.2914855480 * b
        l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
        rgb = (4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
               -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
               -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)
        if all(-0.001 <= v <= 1.001 for v in rgb):
            break
        C *= 0.93
    out = []
    for v in rgb:
        v = min(1.0, max(0.0, v))
        v = 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
        out.append(int(round(v * 255)))
    return tuple(out)


# the parts of the film, each with its hue (OKLCH degrees): red, orange, yellow, green, cyan,
# blue, violet, magenta; a chapter's hue walks from its part's hue towards the next one's
PART_HUES = [25, 55, 95, 145, 195, 255, 300, 340, 385]
PARTS = ["i · the dream", "ii · the editor", "iii · writing a dream", "iv · the sp-1", "v · the wingie",
         "vi · processing", "vii · limits", "viii · sleep well"]


def spectral(h, L=0.74, C=0.14):
    return oklch(L, C, h % 360)


def ground_of(h):
    return oklch(0.215, 0.032, h % 360)


def accent_of(h):
    return oklch(0.80, 0.135, h % 360)


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
F_GLYPH = font("Inter-Medium.ttf", 18)
_PF = {}


def panel_font(size):
    size = max(8, int(round(size)))
    if size not in _PF:
        _PF[size] = font("IBMPlexMono-Medium.ttf", size)
    return _PF[size]


def title_font(text, width=860, sizes=(96, 88, 80, 72, 64, 56)):
    for size in sizes:
        f = font("Inter-Medium.ttf", size)
        if f.getlength(text) <= width:
            return f
    return font("Inter-Medium.ttf", 52)


# ---------------------------------------------------------------------------
# The catalog, and the check that everything spoken exists on the instrument it's said of.

def build_tools(work):
    exe = os.path.join(work, "scqtool")
    bed = os.path.join(work, "bed")
    eng = os.path.join(ROOT, "engine")
    srcs = [os.path.join(eng, "scq_core.c"), os.path.join(eng, "scq_places.c")]
    if not os.path.exists(exe):
        subprocess.run(["cc", "-std=c99", "-O2", "-DSCQ_HOST", "-I" + eng, os.path.join(ROOT, "tools", "scqtool.c")]
                       + srcs + ["-o", exe, "-lm"], check=True)
    if not os.path.exists(bed):
        subprocess.run(["cc", "-std=c99", "-O2", "-DSCQ_HOST", "-I" + eng, os.path.join(HERE, "bed.c")]
                       + srcs + ["-o", bed, "-lm"], check=True)
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


# the numbered hands, and whose they are
NUMBERED = {"sp1": {"fader": 4, "button": 4}, "wingie": {"keys": 2, "key": 24, "fader": 3, "mode": 2, "octave": 2}}
NUMBERED["both"] = {"fader": 4, "button": 4, "keys": 2, "key": 24, "mode": 2, "octave": 2}
SP1_ONLY = {"button", "rocker", "play", "dots", "leds"}
WINGIE_ONLY = {"mic", "line", "keys", "key", "mode", "octave", "source", "lights"}


def known(token, places, inst):
    """Is `token` (place, hand, or place.port) in the catalog, and on this instrument?"""
    base, _, port = token.partition(".")
    m = re.fullmatch(r"([a-z]+?)(\d+)", base)
    if base not in places and m and m.group(1) in NUMBERED[inst] and 1 <= int(m.group(2)) <= NUMBERED[inst][m.group(1)]:
        base = m.group(1)
    if base not in places:
        return False
    if (inst == "sp1" and base in WINGIE_ONLY) or (inst == "wingie" and (base in SP1_ONLY or token == "fader4")):
        return False
    return not port or port in places[base]


def inst_of(c):
    return c.get("inst") or {"sp1": "sp1", "wingie": "wingie"}.get(c.get("panel"), "both")


def validate(chapters, places):
    bad = []
    for c in chapters:
        inst = inst_of(c)
        texts = [c["say"], c["desc"]] + [r["spoken"] or "" for r in c["rows"]] + [r["text"] for r in c["rows"]]
        for t in texts:
            for tok in re.findall(r"`([^`]+)`", t):
                if not known(tok, places, inst):
                    bad.append("%s: %s (%s)" % (c["code"], tok, inst))
        for r in c["rows"]:
            for k in r["hl"]:
                if not panel_key(c, k):
                    bad.append("%s: the panel has no %s" % (c["code"], k))
            if r.get("place") and r["place"] not in places:
                bad.append("%s: no place %s" % (c["code"], r["place"]))
            if r.get("box") and not c.get("shot"):
                bad.append("%s: a box but no screenshot" % c["code"])
        if c.get("shot") and c["shot"] not in narration.SHOTS:
            bad.append("%s: no screenshot %s" % (c["code"], c["shot"]))
        if not c.get("shot") and c.get("panel") not in ("sp1", "wingie", "both"):
            bad.append("%s: neither a panel nor a screenshot" % c["code"])
    codes = [c["code"] for c in chapters]
    if len(set(codes)) != len(codes):
        bad.append("two chapters share a code")
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


SAY = [("Sleepcousonique", "Sleep-coo-so-neek"), ("sleepcousonique", "sleep-coo-so-neek"),
       ("SP-1s", "S P ones"), ("SP-1's", "S P one's"), ("SP-1", "S P one"), ("Wingie2", "Wingie two"),
       ("Meng Qi's", "Mung Chee's"), ("Meng Qi", "Mung Chee"),
       ("T1", "T one"), ("T2", "T two"), ("T3", "T three"), ("T4", "T four"),
       ("USB-C", "U.S.B. C"), ("USB-MIDI", "U.S.B. midi"), ("USB", "U.S.B."), ("MIDI", "midi"),
       ("TRS", "T.R.S."), ("DIN", "din"), ("LEDs", "L.E.D.s"), ("LED", "L.E.D."), ("LFOs", "L.F.O.s"),
       ("LFO", "L.F.O."), ("CPU", "C.P.U."), ("ESP32", "E.S.P. thirty-two"), ("FFT", "F.F.T."),
       ("GPLv3", "G P L version three"), ("MIT", "M.I.T."),
       ("Hordijk", "Hordike"), ("Mycenae", "My-see-nee"), ("Oliveros", "Oliveros"),
       ("crossfm", "cross F M"), ("steprandom", "step random"), ("lpg", "L P G"), ("svf", "S V F"),
       ("eq", "E Q"), ("dusg", "D U S G"), ("vco", "V C O"), ("lfo", "L F O"), ("midicc", "midi C C"),
       ("midiclock", "midi clock"), ("reverblite", "reverb lite"), ("choruslite", "chorus lite"),
       ("delaylite", "delay lite"), ("fmlite", "F M lite"), ("supersawlite", "supersaw lite"),
       ("onepole", "one pole"), ("tapeecho", "tape echo"), ("paulstretch", "Paul-stretch"),
       ("cocoquantus", "coco-quantus"), ("psola", "P-sola"), ("eis", "E.I.S."), ("EIS", "E.I.S."),
       ("specfreeze", "spec-freeze"), ("specshift", "spec-shift"), ("specgate", "spec-gate"),
       ("specfilter", "spec-filter"), ("spectraldelay", "spectral delay"), ("formantshift", "formant shift"),
       ("pitchshift", "pitch shift"), ("graindelay", "grain delay"), ("grainfreeze", "grain freeze"),
       ("harmonicgrains", "harmonic grains"), ("beatgrains", "beat grains"), ("granularverb", "granular verb"),
       ("sittingroom", "sitting room"), ("talkingdrum", "talking drum"), ("fourbit", "four bit"),
       ("tapeloop", "tape loop"), ("pitchtrack", "pitch track"), ("autowah", "auto-wah"), ("autopan", "auto-pan"),
       ("pt2399", "P T twenty-three ninety-nine"), ("PT2399", "P T twenty-three ninety-nine"),
       ("pingpong", "ping-pong"), ("samplehold", "sample and hold"), ("flipflop", "flip-flop"),
       ("gendy", "Gendy"), ("esptool", "E S P tool"), ("Esptool", "E S P tool"), ("benjolin", "Benjolin"), ("WSOLA", "W-sola"), ("tri", "try"),
       ("dB", "decibels"), ("PLAY", "play"), ("leds", "L.E.D.s"), ("wingie", "Wingie")]


def speakable(s):
    s = clean_md(s)
    s = s.replace("e.g.", "for example")
    for a, b in SAY:
        s = re.sub(r"\b%s\b" % re.escape(a), b, s)
    s = re.sub(r"\b(fader|button|keys|key|mode|octave|oct)(\d+)\b", r"\1 \2", s)
    s = re.sub(r"(?<=[a-z0-9])\.(?=[a-z])", " ", s)       # rocker.up: rocker up; keys1.pitch: keys 1 pitch
    s = re.sub(r"(\d)\s*ms\b", r"\1 milliseconds", s)
    s = re.sub(r"(\d)\s*kHz\b", r"\1 kilohertz", s)
    s = re.sub(r"(\d)\s*Hz\b", r"\1 hertz", s)
    s = re.sub(r"(\d)\s*KB\b", r"\1 kilobytes", s)
    s = re.sub(r"(\d)\s*%", r"\1 percent", s)
    s = re.sub(r"(?<![\w])\+(\d)", r"plus \1", s)
    s = re.sub(r"(?<![\w])[−-](\d)", r"minus \1", s)
    s = re.sub(r"(\d)\s*×", r"\1 times", s)
    for a, b in [("→", " to "), ("←", " to "), ("·", ","), ("—", ","), ("–", " to "), ("/", " or "),
                 ("|", " or "), ("(", ", "), (")", ", "), ("+", " and "), ("≈", "about "), ("~", "about ")]:
        s = s.replace(a, b)
    s = re.sub(r"\s+,", ",", s)
    s = re.sub(r",\s*,", ",", s)
    s = re.sub(r",\s*\.", ".", s)
    s = re.sub(r",\s*:", ":", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def build_chapters():
    ch = [dict(c) for c in narration.CHAPTERS]
    by_part = {}
    for c in ch:
        by_part.setdefault(c["part"], []).append(c)
    for i, c in enumerate(ch):
        c["index"] = i
        group = by_part[c["part"]]
        j = group.index(c)
        h0, h1 = PART_HUES[c["part"]], PART_HUES[c["part"] + 1]
        c["hue"] = (h0 + (h1 - h0) * 0.8 * j / max(1, len(group))) % 360
        c["ground"] = ground_of(c["hue"])
        c["accent"] = accent_of(c["hue"])
    return ch


def segments(c):
    segs = [(c["say"], None)]
    for i, r in enumerate(c["rows"]):
        if r["spoken"]:
            segs.append((r["spoken"], i))
    return segs


# ---------------------------------------------------------------------------
# A drawing's place in the frame: x, y of its (0, 0), and its scale.

class Geo:
    def __init__(self, x, y, s):
        self.x, self.y, self.s = x, y, s

    def P(self, x, y):
        return self.x + x * self.s, self.y + y * self.s

    def rect(self, d, x0, y0, x1, y1, r=0, **kw):
        a, b = self.P(x0, y0), self.P(x1, y1)
        if r:
            d.rounded_rectangle([a[0], a[1], b[0], b[1]], radius=max(1, r * self.s), **kw)
        else:
            d.rectangle([a[0], a[1], b[0], b[1]], **kw)

    def circ(self, d, x, y, r, **kw):
        cx, cy = self.P(x, y)
        rr = r * self.s
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], **kw)

    def text(self, d, x, y, s, fill, size=14):
        f = panel_font(size * self.s / 0.9)
        cx, cy = self.P(x, y)
        d.text((cx - f.getlength(s) / 2, cy - f.size * 0.6), s, font=f, fill=fill)


SP1_ALONE = Geo(1066, 238, 0.9)
WG_ALONE = Geo(1010, 205, 0.83)
SP1_BOTH = Geo(992, 330, 0.5)
WG_BOTH = Geo(1278, 300, 0.5)


# ---------------------------------------------------------------------------
# The SP-1, faltung style. Coordinates are in units of the photo in ref/ (enlarged 4x), with
# (0, 0) at the face's top-left corner: the aluminium face is 417 x 690, the cream strip 82 wide
# on its right; the four faders, the track-row LEDs and T1-T4 sit where they do on the real one.

FACE_W, FACE_H = 417, 690
STRIP_X1 = 499
FADER_X = [60, 157, 255, 352]
SLOT_TOP, SLOT_BOT = 278, 415
LED_Y = 483
BTN_Y0, BTN_Y1 = 530, 603
PLAY_XY = (462, 128)
DOTS_X, DOTS_Y = 459, (557, 575)
PLAYROW_Y = [470, 400, 330, 260]   # leds 5..8: LED 5 at the •• end, LED 8 by PLAY

ALU = (190, 193, 187)
ALU_EDGE = (150, 153, 148)
CREAM = (214, 206, 172)
CREAM_EDGE = (168, 160, 128)
RED = (214, 96, 86)
LIT = (255, 248, 226)

SP1 = {
    "rocker": ("tab", -24, 38, 2, 152), "play": ("tab", 436, 66, 529, 190), "dots": ("tab", 436, 506, 529, 630),
    "volm": ("tab", 52, -12, 106, 3), "volp": ("tab", 132, -12, 196, 3),
    "trackrow": ("row", 38, 470, 374, 496), "playrow": ("row", 444, 244, 474, 486),
}
for _i, _x in enumerate(FADER_X):
    SP1["f%d" % (_i + 1)] = ("fader", _x - 17, SLOT_TOP - 8, _x + 17, SLOT_BOT + 6)
    SP1["t%d" % (_i + 1)] = ("button", _x - 19, BTN_Y0 - 3, _x + 19, BTN_Y1 + 3)


def sp1_static(d, c, g):
    gr = c["ground"]
    lab = mix(INK, gr, 0.45)
    shadow = mix(gr, (0, 0, 0), 0.35)
    x0, y0 = g.P(-24, -12)
    x1, y1 = g.P(529, FACE_H)
    o = 20 * g.s / 0.9
    d.polygon([(x0 + o, y0 + o), (x1 + o, y0 + o), (x1 + o, y1 + o), (x0 + o, y1 + o)], fill=shadow)
    # what sticks out of the edges: the rocker (left), VOL - / VOL + (top), PLAY's and ••'s tabs (right)
    g.rect(d, -20, 40, 6, 150, 5, fill=CREAM, outline=CREAM_EDGE, width=2)
    g.rect(d, 57, -8, 102, 6, 3, fill=ALU, outline=ALU_EDGE, width=2)
    g.rect(d, 137, -8, 192, 6, 3, fill=ALU, outline=ALU_EDGE, width=2)
    g.rect(d, 490, 70, 527, 185, 6, fill=CREAM, outline=CREAM_EDGE, width=2)
    g.rect(d, 490, 510, 527, 625, 6, fill=CREAM, outline=CREAM_EDGE, width=2)
    # the face and the strip
    g.rect(d, 0, 0, STRIP_X1, FACE_H, 16, fill=CREAM, outline=CREAM_EDGE, width=2)
    g.rect(d, 0, 0, 417, FACE_H, 16, fill=ALU, outline=ALU_EDGE, width=2)
    g.rect(d, 400, 2, 417, FACE_H - 2, 0, fill=ALU)
    d.line([g.P(417, 1), g.P(417, FACE_H - 1)], fill=CREAM_EDGE, width=2)
    # PLAY: the pale red triangle; ••: the two dots
    cx, cy = g.P(*PLAY_XY)
    s = 9 * g.s
    d.polygon([(cx, cy - s), (cx + s, cy + s * 0.75), (cx - s, cy + s * 0.75)], fill=RED)
    for y in DOTS_Y:
        g.circ(d, DOTS_X, y, 3.6, fill=(40, 40, 30))
    for x in FADER_X:
        g.rect(d, x - 14, SLOT_TOP, x + 14, SLOT_BOT, 14, fill=(92, 95, 92))
        g.rect(d, x - 7, SLOT_TOP + 10, x + 7, SLOT_BOT - 8, 7, fill=(14, 15, 12))
    for x in FADER_X:
        g.rect(d, x - 16, BTN_Y0, x + 16, BTN_Y1, 4, fill=(72, 74, 72))
        g.rect(d, x - 13, BTN_Y0 + 3, x + 13, BTN_Y1 - 3, 3, fill=(200, 202, 198))
    # labels: not printed on the SP-1, so they sit outside it, small and dim
    g.text(d, 80, -40, "vol −", lab)
    g.text(d, 165, -40, "vol +", lab)
    for i, x in enumerate(FADER_X):
        g.text(d, x, FACE_H + 24, "t%d" % (i + 1), lab)
    g.text(d, 462, FACE_H + 24, "sp-1", lab)


def ring(d, g, x0, y0, x1, y1, acc, pulse):
    e = 8 + 6 * pulse
    a, b = g.P(x0 - e, y0 - e), g.P(x1 + e, y1 + e)
    box = [a[0], a[1], b[0], b[1]]
    d.rounded_rectangle(box, radius=12 * g.s / 0.9, outline=(10, 12, 14), width=7)   # dark under, so it shows on aluminium
    d.rounded_rectangle(box, radius=12 * g.s / 0.9, outline=acc, width=3)


def sp1_dynamic(d, c, t, hl, leds, g):
    acc = c["accent"]
    pulse = 0.5 + 0.5 * math.sin(t * 5.0)
    press = 0.5 + 0.5 * math.sin(t * 3.2) > 0.45
    for i, x in enumerate(FADER_X):
        on = ("f%d" % (i + 1)) in hl
        v = 0.5 + (0.42 * math.sin(t * 1.4 + i) if on else 0.0)
        cy = (SLOT_BOT - 18) - (SLOT_BOT - SLOT_TOP - 36) * v
        g.circ(d, x, cy, 12.5, fill=acc if on else (226, 226, 220), outline=(60, 62, 58), width=2)
    for i, x in enumerate(FADER_X):
        if ("t%d" % (i + 1)) in hl and press:
            g.rect(d, x - 13, BTN_Y0 + 3, x + 13, BTN_Y1 - 3, 3, fill=acc)
    if "rocker" in hl:
        up = math.sin(t * 2.4) > 0
        g.rect(d, -20, 40 if up else 95, 6, 95 if up else 150, 5, fill=acc)
    for k, box in (("play", (499, 72, 527, 183)), ("dots", (499, 512, 527, 623)),
                   ("volm", (57, -8, 102, 6)), ("volp", (137, -8, 192, 6))):
        if k in hl and press:
            g.rect(d, *box, 4, fill=acc)
    if g.s > 0.7:
        f = panel_font(18 * g.s / 0.9)
        for y, s in ((66, "^"), (126, "v")):
            cx, cy = g.P(-7, y)
            d.text((cx - f.getlength(s) / 2, cy - f.size * 0.6), s, font=f, fill=(70, 64, 44))
    pos = [(x, LED_Y) for x in FADER_X] + [(DOTS_X, y) for y in PLAYROW_Y]
    for k, (x, y) in enumerate(pos):
        v = max(0.0, min(1.0, leds[k]))
        if v > 0.03:
            g.circ(d, x, y, 9 + 5 * v, fill=mix(ALU if k < 4 else CREAM, LIT, 0.45 * v))
            g.circ(d, x, y, 4.2, fill=mix((70, 70, 60), LIT, v))
        else:
            g.circ(d, x, y, 3.2, fill=(46, 47, 42) if k < 4 else (150, 144, 118))
    for k in hl:
        _, x0, y0, x1, y1 = SP1[k]
        ring(d, g, x0, y0, x1, y1, acc, pulse)


def sp1_leds(c, t, row):
    """What the eight LEDs do while a row is spoken (or the chapter plays)."""
    anim = row.get("anim") if row else None
    hl = [k[2:] if k.startswith("s:") else k for k in (row["hl"] if row else [])]
    z = [0.0] * 8
    if anim == "boot":
        z[0] = 1.0
    elif anim == "standby":
        b = 0.35 + 0.12 * math.sin(t * 2 * math.pi / 2.2)
        z[4:7] = [b, b, b]
    elif anim == "poweron":
        ph = t % 2.4
        for i in range(4):
            z[i] = 1.0 if 0.15 + i * 0.09 < ph < 1.3 else 0.0
    elif anim == "poweroff":
        ph = t % 2.4
        for i in range(4):
            z[i] = max(0.0, min(1.0, 1.0 - (ph - 0.7 - i * 0.2) / 0.15)) if ph < 1.6 else 0.0
    elif anim == "battery":
        if t % 2.6 < 1.5:
            z[4:7] = [0.35, 0.35, 0.35]
    elif anim in ("scene", "slot"):
        s = int(t / 1.3) % (4 if anim == "scene" else 5)
        if t % 1.3 < 0.6:
            for i in range(min(4, s + 1)):
                z[i] = 1.0
    elif anim == "iface":
        z[0] = 0.8
        lv = 0.5 + 0.4 * math.sin(t * 3.1)
        rv = 0.5 + 0.4 * math.sin(t * 2.6 + 1)
        z[4], z[5], z[6], z[7] = (1.0 if lv > 0.5 else 0.08), lv, rv, (1.0 if rv > 0.5 else 0.08)
    elif anim == "track":
        z[int(t * 2.5) % 4] = 1.0
    elif anim == "bar" or "trackrow" in hl or "playrow" in hl or c["kind"] in ("intro", "outro", "places"):
        lv = 0.5 + 0.45 * math.sin(t * 1.3) * math.sin(t * 0.37 + 1)
        for i in range(4):
            z[i] = max(0.0, min(1.0, lv * 4 - i))
            z[4 + i] = max(0.0, min(1.0, lv * 4 - i - 0.3))
    return z


# ---------------------------------------------------------------------------
# The Wingie2, faltung style. Coordinates are in units of Meng Qi's photo of the panel (enlarged
# 4x and cropped to the panel: 670 x 780), so every control sits where it does on the real one.
# keys1 is the bottom keyboard and keys2 the top (checked on a Wingie).

WG_W, WG_H = 670, 780
WHITE_X = [65, 155, 245, 335, 425, 515, 605]
BLACK_X = [110, 200, 380, 470, 560]
ROW_Y = {"k1b": 632, "k1w": 715, "k2b": 465, "k2w": 548}
KEY_POS = []    # (x, black?) for C..B
_w, _b = iter(WHITE_X), iter(BLACK_X)
for _k in range(12):
    _blk = _k in (1, 3, 6, 8, 10)
    KEY_POS.append((next(_b) if _blk else next(_w), _blk))
WFADER_X = [245, 335, 425]
WFADER_TOP, WFADER_BOT = 125, 375

WG = {
    "mic_l": ("grille", 65, 120, 48), "mic_r": ("grille", 605, 120, 48),
    "source": ("toggle", 150, 115, 18),
    "oct1": ("toggle", 65, 250, 18), "oct2": ("toggle", 155, 250, 18),
    "mode1": ("button", 65, 382, 27), "mode2": ("button", 155, 382, 27),
    "led1": ("led", 513, 382, 9), "led2": ("led", 603, 382, 9),
    "keys1": ("keys", 335, 673, 0), "keys2": ("keys", 335, 506, 0),
    "jacks": ("edge", 335, -22, 0),
}
for _i in range(3):
    WG["f%d" % (_i + 1)] = ("fader", WFADER_X[_i], (WFADER_TOP + WFADER_BOT) // 2, 0)

# the editor's drag loops on its drawn Wingie (shown in the chapters about them)
WG_LOOPS = [(232, 66, "line in"), (232, 166, "mic"), (190, 115, None), (100, 250, None), (190, 250, None),
            (100, 382, None), (190, 382, None), (245, 92, None), (335, 92, None), (425, 92, None),
            (WG_W + 34, 506, "keys2"), (WG_W + 34, 673, "keys1")]


def wg_static(d, c, g, loops=False):
    gr = c["ground"]
    face = mix(gr, INK, 0.11)
    edge = mix(gr, INK, 0.32)
    lab = mix(INK, face, 0.25)
    hole = mix(gr, (0, 0, 0), 0.55)
    x0, y0 = g.P(0, 0)
    x1, y1 = g.P(WG_W, WG_H)
    o = 18 * g.s / 0.83
    d.polygon([(x0 + o, y0 + o), (x1 + o, y0 + o), (x1 + o, y1 + o), (x0 + o, y1 + o)], fill=mix(gr, (0, 0, 0), 0.3))
    d.rounded_rectangle([x0, y0, x1, y1], radius=10 * g.s / 0.83, fill=face, outline=edge, width=2)
    f = panel_font(13 * g.s / 0.83)
    ex, ey = g.P(WG_W / 2, -22)
    s = "usb-c · midi in · line in · out"
    d.text((ex - f.getlength(s) / 2, ey - 8 * g.s / 0.83), s, font=f, fill=mix(INK, gr, 0.5))
    for k in ("mic_l", "mic_r"):
        _, gx, gy, _ = WG[k]
        for row in range(-3, 4):
            n = 4 - abs(row)
            for j in range(n):
                g.circ(d, gx + (j - (n - 1) / 2) * 22, gy + row * 17, 4.5, fill=hole)
    sz = 13 / 0.83 * 0.9
    g.text(d, 158, 66, "LINE IN", lab, sz)
    g.text(d, 158, 166, "MIC", lab, sz)
    g.text(d, 62, 214, "L.OCT", lab, sz)
    g.text(d, 158, 214, "R.OCT", lab, sz)
    g.text(d, 65, 322, "L.MODE", lab, sz)
    g.text(d, 155, 322, "R.MODE", lab, sz)
    for k in ("source", "oct1", "oct2"):
        _, x, y, r = WG[k]
        g.circ(d, x, y, r + 4, fill=mix(face, (0, 0, 0), 0.25))
    for i, x in enumerate(WFADER_X):
        g.rect(d, x - 7, WFADER_TOP, x + 7, WFADER_BOT, 5, fill=hole)
        g.text(d, x, WFADER_BOT + 24, ["MIX", "DECAY", "VOLUME"][i], lab, sz)
    for yb, yw in ((ROW_Y["k1b"], ROW_Y["k1w"]), (ROW_Y["k2b"], ROW_Y["k2w"])):
        for x, blk in KEY_POS:
            g.circ(d, x, yb if blk else yw, 27, fill=(18, 16, 22) if blk else mix(face, INK, 0.75))
    g.text(d, WG_W - 70, 762, "WINGIE2", lab, sz)
    if loops:
        cop = (201, 138, 90)
        for x, y, name in WG_LOOPS:
            g.circ(d, x, y, 11, outline=cop, width=max(2, int(4 * g.s)))
            g.circ(d, x, y, 4, fill=mix(cop, INK, 0.4))
            if name in ("keys1", "keys2"):
                g.text(d, x, y - 30, name, lab, sz)


LAMP = [(200, 40, 40), (170, 70, 200), (235, 200, 60), (250, 250, 245)]


def wg_dynamic(d, c, t, hl, lamp, keys_down, g):
    gr = c["ground"]
    acc = c["accent"]
    face = mix(gr, INK, 0.11)
    pulse = 0.5 + 0.5 * math.sin(t * 5.0)
    w3 = max(2, int(round(3 * g.s / 0.83)))
    for k in hl:
        kind, x, y, r = WG[k]
        if kind == "fader":
            a, b = g.P(x - 26, WFADER_TOP - 14), g.P(x + 26, WFADER_BOT + 14)
            d.rounded_rectangle([a[0], a[1], b[0], b[1]], radius=10 * g.s, outline=acc, width=w3)
        elif kind == "keys":
            a, b = g.P(20, y - 80), g.P(WG_W - 20, y + 80)
            d.rounded_rectangle([a[0], a[1], b[0], b[1]], radius=14 * g.s, outline=acc, width=w3)
        elif kind == "edge":
            a, b = g.P(150, -44), g.P(WG_W - 150, -2)
            d.rounded_rectangle([a[0], a[1], b[0], b[1]], radius=10 * g.s, outline=acc, width=w3)
        else:
            g.circ(d, x, y, r + 14 + 6 * pulse, outline=acc, width=w3)
    for i, x in enumerate(WFADER_X):
        on = ("f%d" % (i + 1)) in hl
        v = 0.5 + (0.38 * math.sin(t * 1.4 + i) if on else 0.0)
        g.circ(d, x, WFADER_BOT - (WFADER_BOT - WFADER_TOP) * v, 15, fill=acc if on else mix(face, INK, 0.8))
    for k in ("source", "oct1", "oct2"):
        _, x, y, r = WG[k]
        on = k in hl
        if k == "source":
            ang = (math.sin(t * 2.2) * 0.9) if on else 0.0
        else:   # three positions: down, middle, up
            ang = ([-0.9, 0.0, 0.9][int(t / 0.9) % 3] if on else 0.0)
        cx, cy = g.P(x, y)
        L = 22 * g.s
        tip = (cx + math.sin(ang) * L, cy - math.cos(ang) * L)
        d.line([(cx, cy), tip], fill=acc if on else mix(face, INK, 0.75), width=max(2, int(5 * g.s / 0.83)))
        rr = 4 * g.s / 0.83
        d.ellipse([tip[0] - rr, tip[1] - rr, tip[0] + rr, tip[1] + rr], fill=INK)
    for k in ("mode1", "mode2"):
        _, x, y, r = WG[k]
        g.circ(d, x, y, r, fill=mix(acc, gr, 0.25) if k in hl else mix(face, INK, 0.45))
    for kb, idx in keys_down:
        x, blk = KEY_POS[idx]
        g.circ(d, x, ROW_Y["k%d%s" % (kb, "b" if blk else "w")], 27, fill=acc)
    for i, lv in enumerate(lamp):
        col = LAMP[min(3, int(lv * 4))]
        g.circ(d, [513, 603][i], 382, 9, fill=col)
        g.circ(d, [513, 603][i], 382, 15, outline=mix(col, gr, 0.5), width=2)


def wg_lamp_keys(c, t, hl, row):
    anim = row.get("anim") if row else None
    lamp = [0.1, 0.1]
    keys = []
    if anim == "slot" or c["kind"] == "slots":
        keys = [(1, int(t / 1.4) % 12)]
        if int(t * 8) % 2 == 0 and (t % 1.4) < 0.4:
            lamp = [0.6, 0.6]
    elif anim == "heavy" or c["kind"] == "heavy":
        if int(t / 3) % 2 == 0 and int(t * 8) % 2 == 0:
            lamp = [0.3, 0.3]
    elif anim == "white":
        if int(t * 10) % 2 == 0 and t % 2.0 < 1.0:
            lamp = [0.9, 0.9]
    elif "keys1" in hl or "keys2" in hl:
        kb = 1 if "keys1" in hl else 2
        keys = [(kb, [0, 4, 7, 11, 7, 4][int(t / 0.6) % 6])]
        lamp = [0.55 + 0.4 * math.sin(t * 3) ** 2, lamp[1]]
    elif c["kind"] in ("intro", "outro"):
        lv = 0.5 + 0.45 * math.sin(t * 1.1)
        lamp = [lv, 0.5 + 0.45 * math.sin(t * 0.8 + 2)]
    if "led1" in hl or "led2" in hl:
        v = (t * 0.25) % 1.0
        lamp = [v, (v + 0.5) % 1.0]
    return lamp, keys


# ---------------------------------------------------------------------------
# Which drawing a highlight key belongs to.

def panel_key(c, k):
    """(drawing, key) for a highlight key, or None. In a chapter with both instruments the key
    says whose it is: s:dots, w:mode1."""
    p = c.get("panel")
    if p == "both":
        if k.startswith("s:") and k[2:] in SP1:
            return ("sp1", k[2:])
        if k.startswith("w:") and k[2:] in WG:
            return ("wingie", k[2:])
        return None
    if p == "sp1" and k in SP1:
        return ("sp1", k)
    if p == "wingie" and k in WG:
        return ("wingie", k)
    return None


def geos(c):
    p = c.get("panel")
    if p == "sp1":
        return {"sp1": SP1_ALONE}
    if p == "wingie":
        return {"wingie": WG_ALONE}
    if p == "both":
        return {"sp1": SP1_BOTH, "wingie": WG_BOTH}
    return {}


def anchor(c, k):
    """Where the line from a row meets a control: just left of it."""
    who, key = panel_key(c, k)
    g = geos(c)[who]
    if who == "sp1":
        _, x0, y0, _, y1 = SP1[key]
        return g.P(x0 - 14, (y0 + y1) / 2)
    kind, x, y, rad = WG[key]
    tx, ty = g.P(x, y)
    if kind == "fader":
        tx -= 26 * g.s
    elif kind == "keys":
        tx = g.P(20, 0)[0]
    elif kind == "edge":
        tx = g.P(150, 0)[0]
        ty = g.P(0, -23)[1]
    else:
        tx -= (rad + 14) * g.s
    return tx, ty


# ---------------------------------------------------------------------------
# Screenshot chapters: the editor where the instrument would be.

SHOT_X, SHOT_Y, SHOT_W = 990, 190, 900
_SHOTS = {}


def make_shots(app, folder):
    """The editor's own screenshots, never touching the serial port, Bluetooth or an input."""
    os.makedirs(folder, exist_ok=True)
    todo = [n for n in narration.SHOTS if not os.path.exists(os.path.join(folder, n))]
    if not todo:
        return
    if not os.path.exists(app):
        raise SystemExit("no editor at %s: build it first, or pass --app" % app)
    kept = open(SETTINGS, "rb").read() if os.path.exists(SETTINGS) else None
    try:
        for name in todo:
            dream, view, theme = narration.SHOTS[name]
            out = os.path.join(folder, name)
            subprocess.run([app, "--no-link", "--no-radio", "--shot", out, os.path.join(ROOT, "examples", dream),
                            "--view", str(view), "--theme", theme], check=True, cwd=ROOT, timeout=60)
            im = Image.open(out).convert("RGB")
            if theme.startswith("journal"):
                # the status bar's file path: covered from 36 % of the width to the edge, bottom 21 px,
                # in the bar's own colour (taken from its empty left part)
                bar = np.array(im)[im.height - 11, 4:300]
                col = tuple(int(v) for v in np.median(bar, axis=0))
                ImageDraw.Draw(im).rectangle([int(im.width * 0.36), im.height - 21, im.width, im.height], fill=col)
            im.save(out)
            print("shot", name)
    finally:
        if kept is not None:
            open(SETTINGS, "wb").write(kept)
        elif os.path.exists(SETTINGS):
            os.remove(SETTINGS)


def shot_image(name, crop, folder):
    """The screenshot, or the part of it given by crop (x, y, w, h), scaled to SHOT_W wide."""
    key = (name, crop)
    if key not in _SHOTS:
        im = Image.open(os.path.join(folder, name)).convert("RGB")
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
# The right-hand side: icosahedron, strips, tracklist; and the rainbow.

PHI = (1 + 5 ** 0.5) / 2
ICO_V = np.array([(-1, PHI, 0), (1, PHI, 0), (-1, -PHI, 0), (1, -PHI, 0), (0, -1, PHI), (0, 1, PHI),
                  (0, -1, -PHI), (0, 1, -PHI), (PHI, 0, -1), (PHI, 0, 1), (-PHI, 0, -1), (-PHI, 0, 1)], float)
ICO_V /= np.linalg.norm(ICO_V[0])
ICO_F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
         (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
         (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
ICO_E = sorted({tuple(sorted((f[i], f[(i + 1) % 3]))) for f in ICO_F for i in range(3)})
# each edge its hue: round the wheel by the angle of its middle about the vertical axis
ICO_EH = [math.degrees(math.atan2(*(ICO_V[a] + ICO_V[b])[[2, 0]])) % 360 for a, b in ICO_E]


def draw_ico(d, t, cx, cy, r, c, white_face=0):
    a, b = t * 0.35, t * 0.22 + 0.4
    ry = np.array([[math.cos(a), 0, math.sin(a)], [0, 1, 0], [-math.sin(a), 0, math.cos(a)]])
    rx = np.array([[1, 0, 0], [0, math.cos(b), -math.sin(b)], [0, math.sin(b), math.cos(b)]])
    v = ICO_V @ ry.T @ rx.T
    pts = [(cx + p[0] * r, cy - p[1] * r) for p in v]
    f = ICO_F[white_face]
    n = np.cross(v[f[1]] - v[f[0]], v[f[2]] - v[f[0]])
    if n[2] > 0:
        d.polygon([pts[k] for k in f], fill=INK)
    order = sorted(range(len(ICO_E)), key=lambda i: v[ICO_E[i][0]][2] + v[ICO_E[i][1]][2])
    for i in order:
        e = ICO_E[i]
        z = (v[e[0]][2] + v[e[1]][2]) / 2
        col = spectral(ICO_EH[i] + c["hue"])
        col = col if z > 0 else mix(col, c["ground"], 0.6)
        d.line([pts[e[0]], pts[e[1]]], fill=col, width=3 if z > 0 else 1)


def skew_rect(d, x, y, w, h, fill, lean=0.55):
    o = h * lean
    d.polygon([(x + o, y), (x + w + o, y), (x + w, y + h), (x, y + h)], fill=fill)


def draw_strips(d, ox, oy, t, c):
    for i in range(6):
        col = spectral(c["hue"] + i * 48)
        x = ox + i * 40
        y = oy + 16 * math.sin(t * 0.5 + i * 0.7)
        L = 95 - i * 8
        d.polygon([(x, y), (x + 34, y - 20), (x + 34, y - 20 + L), (x, y + L)], fill=col)
        d.polygon([(x + 34, y - 20), (x + 68, y), (x + 68, y + L), (x + 34, y - 20 + L)],
                  fill=mix(col, (0, 0, 0), 0.28))


_RAINBOW = {}


def rainbow_strip(w, h, h0=20, h1=340, L=0.74, C=0.14):
    """A horizontal spectral gradient, w x h."""
    key = (w, h, h0, h1, L, C)
    if key not in _RAINBOW:
        cols = np.array([spectral(h0 + (h1 - h0) * x / max(1, w - 1), L, C) for x in range(w)], np.uint8)
        _RAINBOW[key] = Image.fromarray(np.repeat(cols[None, :, :], h, axis=0), "RGB")
    return _RAINBOW[key]


def rainbow_text(im, xy, text, fnt):
    """Text filled with the spectrum, left to right."""
    x, y = xy
    bb = fnt.getbbox(text)
    w, h = bb[2] + 4, bb[3] + 8
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).text((0, 0), text, font=fnt, fill=255)
    im.paste(rainbow_strip(w, h, 15, 330, 0.80, 0.15), (x, y), mask)


def rainbow_bands(d, x, y, w, t=0.0):
    """Seven thin folded bands, red to violet, the title card's rule."""
    for i in range(7):
        col = spectral(25 + i * 45, 0.74, 0.14)
        yy = y + i * 9
        skew_rect(d, x + i * 10, yy, w, 6, col, 1.6)


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


def draw_tracklist(d, c, chapters, tx, ty, n=9, wide=270):
    dim = mix(INK, c["ground"], 0.38)
    d.text((tx, ty - 40), "tracklist", font=F_MONO_S, fill=dim)
    idx = c["index"]
    lo = max(0, min(idx - 3, len(chapters) - n))
    for k, other in enumerate(chapters[lo:lo + n]):
        yy = ty + k * 32
        cur = other["index"] == idx
        if cur:
            skew_rect(d, tx - 10, yy - 2, wide, 28, c["accent"], 0.5)
        else:
            d.rectangle([tx - 22, yy + 9, tx - 15, yy + 16], fill=mix(other["accent"], c["ground"], 0.15))
        col = c["ground"] if cur else (INK if other["index"] < idx else dim)
        d.text((tx, yy), "%s  %s" % (other["code"], other["title"][:16]), font=F_MONO_S, fill=col)


def static_layer(c, chapters, shotdir):
    im = Image.new("RGB", (W, H), c["ground"])
    d = ImageDraw.Draw(im)
    dim = mix(INK, c["ground"], 0.38)
    rule = mix(INK, c["ground"], 0.75)
    d.text((96, 64), c["code"], font=F_CODE, fill=c["accent"])
    d.text((96 + F_CODE.getlength(c["code"]) + 22, 66),
           "sleepcousonique · sp-1 & wingie · %s" % PARTS[c["part"]], font=F_MONO_S, fill=dim)
    card = c["kind"] in ("intro", "outro")
    if card:
        ft = title_font(c["title"], TEXT_R - 96, (120, 112, 104, 96, 88))
        rainbow_text(im, (88, 92), c["title"], ft)
        y = 100 + ft.size + 34
        rainbow_bands(d, 96, y, TEXT_R - 200)
        y += 7 * 9 + 26
    else:
        ft = title_font(c["title"], TEXT_R - 96)
        d.text((90, 100), c["title"], font=ft, fill=INK)
        y = 220
    for ln in wrap(c["sub"], F_SUB, TEXT_R - 96)[:2]:
        d.text((96, y), ln, font=F_SUB, fill=dim)
        y += 40
    y += 8
    nrows = len(c["rows"])
    maxlines = 5 if nrows <= 6 else (3 if nrows <= 8 else 2)
    if card:
        maxlines = 3
    for ln in wrap(clean_md(c["desc"]), F_BODY, TEXT_R - 96)[:maxlines]:
        d.text((96, y), ln, font=F_BODY, fill=INK)
        y += 36
    c["rows_y"] = max(y + 34, 450 if nrows > 8 else 500)
    d.line([(96, c["rows_y"] - 18), (TEXT_R, c["rows_y"] - 18)], fill=rule, width=1)
    if c.get("shot"):
        img, _ = shot_image(c["shot"], c.get("crop"), shotdir)
        d.rectangle([SHOT_X + 14, SHOT_Y + 14, SHOT_X + img.width + 14, SHOT_Y + img.height + 14],
                    fill=mix(c["ground"], (0, 0, 0), 0.3))
        im.paste(img, (SHOT_X, SHOT_Y))
        d.rectangle([SHOT_X, SHOT_Y, SHOT_X + img.width, SHOT_Y + img.height], outline=mix(INK, c["ground"], 0.6))
        idx = c["index"]
        s = "  ·  ".join(o["code"] for o in chapters[max(0, idx - 3):idx + 4])
        d.text((SHOT_X, SHOT_Y + img.height + 40), "tracklist   " + s, font=F_MONO_S, fill=dim)
        return im
    gs = geos(c)
    if "sp1" in gs:
        sp1_static(d, c, gs["sp1"])
    if "wingie" in gs:
        wg_static(d, c, gs["wingie"], c.get("loops", False))
    if c.get("panel") == "both":
        draw_tracklist(d, c, chapters, 1648, 470, wide=250)
    else:
        draw_tracklist(d, c, chapters, 1625, 470)
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
        # each slider a step further round the wheel; the one being spoken in the accent
        own = spectral(c["hue"] + 14 * (i - len(rows) / 2), 0.66, 0.11)
        skew_rect(d, ROW_SX0, y + 6, wv, 18, c["accent"] if on else mix(own, c["ground"], 0.35))
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


def draw_connector(d, c, active, t, shotdir):
    r = c["rows"][active]
    y0, gap = row_geometry(c)
    y = y0 + active * gap + 15
    acc = c["accent"]
    if c.get("shot"):
        if not r.get("box"):
            return
        _, scale = shot_image(c["shot"], c.get("crop"), shotdir)
        x0, y0b, x1, y1 = box_rect(r["box"], scale, c.get("crop"))
        pulse = 0.5 + 0.5 * math.sin(t * 5.0)
        g = 3 + 3 * pulse
        d.rectangle([x0 - g, y0b - g, x1 + g, y1 + g], outline=(10, 12, 14), width=7)
        d.rectangle([x0 - g, y0b - g, x1 + g, y1 + g], outline=acc, width=4)
        tx, ty = SHOT_X - 6, (y0b + y1) / 2
        d.line([(tx, ty), (x0 - g, ty)], fill=acc, width=1)
    else:
        if not r["hl"]:
            return
        tx, ty = anchor(c, r["hl"][0])
    elbow = 965
    d.line([(TEXT_R + 12, y), (elbow, y), (tx, ty)], fill=acc, width=2)
    d.ellipse([TEXT_R + 6, y - 6, TEXT_R + 18, y + 6], fill=acc)


def progress_bar(im, d, c, prog):
    """The film's progress along the bottom: the whole spectrum, shown as far as we've come."""
    d.line([(0, H - 4), (W, H - 4)], fill=mix(INK, c["ground"], 0.82), width=1)
    w = int(W * prog)
    if w > 0:
        im.paste(rainbow_strip(W, 8).crop((0, 0, w, 8)), (0, H - 8))


def render_chapter(args):
    c, chapters, dur, timeline, path, shotdir = args
    if os.path.exists(path) and os.path.getsize(path) > 0 and os.path.exists(path + ".done"):
        return path
    base = static_layer(c, chapters, shotdir)
    nframes = int(round(dur * FPS))
    ff = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", "%dx%d" % (W, H), "-r", str(FPS), "-i", "-", "-c:v", "libx264",
                           "-preset", "medium", "-crf", "19", "-pix_fmt", "yuv420p", path],
                          stdin=subprocess.PIPE)
    nrows = len(c["rows"])
    gs = geos(c)
    for f in range(nframes):
        t = f / FPS
        im = base.copy()
        d = ImageDraw.Draw(im)
        if not c.get("shot"):
            draw_ico(d, t + c["index"] * 3.1, 1755, 225, 120, c, white_face=c["index"] % 20)
        draw_strips(d, 1640 if not c.get("shot") else 1660, 940, t, c)
        active = None
        for start, ri in timeline:
            if t >= start and ri is not None:
                active = ri
        if active is None and nrows and not any(ri is not None for _, ri in timeline):
            active = int(t / max(dur, 0.1) * nrows) % nrows
        draw_rows(d, c, t, active)
        row = c["rows"][active] if active is not None else None
        hl = row["hl"] if row else []
        if "sp1" in gs:
            mine = [panel_key(c, k)[1] for k in hl if panel_key(c, k) and panel_key(c, k)[0] == "sp1"]
            sp1_dynamic(d, c, t, mine, sp1_leds(c, t, row), gs["sp1"])
        if "wingie" in gs:
            mine = [panel_key(c, k)[1] for k in hl if panel_key(c, k) and panel_key(c, k)[0] == "wingie"]
            lamp, keys = wg_lamp_keys(c, t, mine, row)
            wg_dynamic(d, c, t, mine, lamp, keys, gs["wingie"])
        if active is not None:
            draw_connector(d, c, active, t, shotdir)
        progress_bar(im, d, c, (c["start"] + t) / c["total"])
        a = min(1.0, t / 0.45, (dur - t) / 0.45)
        if a < 1.0:
            im = Image.blend(Image.new("RGB", (W, H), mix(c["ground"], (0, 0, 0), 0.6)), im, max(0.0, a))
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
    raise SystemExit("no Kokoro venv: python3.12 -m venv tools/video-manual/.venv && "
                     "tools/video-manual/.venv/bin/pip install kokoro soundfile")


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


BEDS = [("bed-sp1.dream", ["0.5"] * 4), ("bed-wingie.dream", ["0.5"] * 3)]


def make_bed(bed_exe, work, seconds):
    """The three scenes of each bed, 100 s each, the SP-1's and the Wingie's taking turns,
    crossfaded over 10 s, round and round."""
    part, xf = 100.0, 10.0
    scenes = []
    for s in range(3):
        for b, (dream, faders) in enumerate(BEDS):
            p = os.path.join(work, "bed%d_%d.f32" % (b, s))
            if not os.path.exists(p):
                subprocess.run([bed_exe, os.path.join(HERE, dream), str(s), str(part), p] + faders, check=True)
            x = np.fromfile(p, np.float32).reshape(-1, 2)
            x = x[int(8 * SR):]                     # past the first swell
            scenes.append(x / max(1e-6, np.sqrt((x ** 2).mean()) * 8))   # every scene at one level
    n = int(seconds * SR) + SR
    out = np.zeros((n, 2), np.float32)
    pos, k = 0, 0
    fadein = np.linspace(0, 1, int(xf * SR))[:, None]
    while pos < n:
        x = scenes[k % len(scenes)].copy()
        x[:len(fadein)] *= fadein
        x[-len(fadein):] *= fadein[::-1]
        m = min(len(x), n - pos)
        out[pos:pos + m] += x[:m]
        pos += len(x) - len(fadein)
        k += 1
    return out[:n]


def mix_audio(placed, total, bed_exe, work):
    n = int(total * SR) + SR
    voice = np.zeros(n, np.float32)
    for start, v in placed:
        s = int(round(start * SR))
        voice[s:s + len(v)] += v[:max(0, n - s)]
    music = make_bed(bed_exe, work, n / SR)[:n]
    music /= max(1e-6, np.abs(music).max())
    # the bed is plucked (dust on strings and bars), so its peaks stand far above its body: a slow
    # limiter (instant attack, 0.25 s release) brings the body up without touching the tone
    lim = np.abs(music).max(axis=1)
    rel_l = 1 - math.exp(-1 / (0.25 * SR))
    g = np.ones(n, np.float32)
    acc = 0.0
    for i in range(0, n, 64):
        x = lim[i:i + 64].max()
        acc = x if x > acc else acc + rel_l * 64 * (x - acc)
        g[i:i + 64] = min(1.0, 0.25 / max(acc, 1e-6))
    music *= g[:, None]
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
    return mixd.astype(np.float32)


# ---------------------------------------------------------------------------
# The YouTube chapter list.

def stamp(s):
    s = int(s)
    h, m, sec = s // 3600, (s // 60) % 60, s % 60
    return "%d:%02d:%02d" % (h, m, sec) if h else "%d:%02d" % (m, sec)


def youtube_chapters(chapters, starts):
    lines = []
    last = None
    for c, s in zip(chapters, starts):
        t = int(s)
        if last is not None and t - last < 10:
            raise SystemExit("chapter %s starts only %d s after the one before" % (c["code"], t - last))
        lines.append("%s %s" % (stamp(0 if last is None else t), c.get("yt") or c["title"]))
        last = t
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.expanduser("~/Desktop/Sleepcousonique-Video-Manual.mp4"))
    ap.add_argument("--chapters", default=None, help="the YouTube chapter list (default: beside --out)")
    ap.add_argument("--only", help="chapter range a-b (for previews)")
    ap.add_argument("--work", default=None)
    ap.add_argument("--app", default=APP, help="the editor's binary, for the screenshots")
    ap.add_argument("--jobs", type=int, default=max(1, os.cpu_count() - 2))
    ap.add_argument("--still", help="write frames per chapter as PNG at these times (s, comma-separated) and stop")
    ap.add_argument("--print", action="store_true", help="print the spoken text and stop")
    ap.add_argument("--check", action="store_true", help="only check the narration against the catalog")
    ap.add_argument("--shots", action="store_true", help="only make the screenshots")
    a = ap.parse_args()

    work = os.path.abspath(a.work or tempfile.mkdtemp(prefix="scq_video_manual_"))
    os.makedirs(work, exist_ok=True)
    shotdir = os.path.join(work, "shots")
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
        words = 0
        for c in sel:
            for s, ri in segments(c):
                sp = speakable(s)
                words += len(sp.split())
                print("[%s/%s] %s" % (c["code"], ri, sp))
        print("%d words, about %.0f minutes" % (words, words / 150))
        return
    make_shots(a.app, shotdir)
    if a.shots:
        print(shotdir)
        return
    print("work dir", work, "-", len(sel), "chapters")

    lead, gap, tail = 0.7, 0.45, 1.5
    all_segs = [(ci, ri, speakable(s)) for ci, c in enumerate(sel) for s, ri in segments(c)]
    voices = speak_all([s for _, _, s in all_segs], work)
    durs, timelines, placed, starts = [], [], [], []
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
        d = max(t - gap + tail, 10.5)
        if not any(ri is not None for _, ri in tl):
            d = max(d, len(c["rows"]) * 2.2)
        if c["kind"] == "intro":
            d += 2.0                       # a moment of music under the title
        d = round(d * FPS) / FPS           # a whole number of frames: the starts stay exact
        starts.append(pos)
        durs.append(d)
        timelines.append(tl)
        pos += d
    total = pos
    for c, s in zip(sel, starts):
        c["start"], c["total"] = s, total
    print("total %.1f s (%.1f min)" % (total, total / 60))

    if a.still:
        ts = [float(x) for x in a.still.split(",")]
        for c, d, tl in zip(sel, durs, timelines):
            for s in ts:
                render_still(c, chapters, d, tl, min(s, d - 0.6), shotdir,
                             os.path.join(work, "still_%03d_%g.png" % (c["index"], s)))
        return

    free = shutil.disk_usage(os.path.dirname(os.path.expanduser(a.out))).free
    if free < 6e9:
        raise SystemExit("only %.1f GB free: the encode needs room" % (free / 1e9))
    jobs = [(c, chapters, d, tl, os.path.join(work, "v%03d.mp4" % c["index"]), shotdir)
            for c, d, tl in zip(sel, durs, timelines)]
    with Pool(a.jobs) as pool:
        parts = pool.map(render_chapter, jobs)
    # the chapters' real lengths, from the rendered files
    real = []
    for p in parts:
        out = subprocess.run(["ffprobe", "-v", "error", "-count_packets", "-select_streams", "v:0", "-show_entries",
                              "stream=nb_read_packets", "-of", "csv=p=0", p], check=True, capture_output=True,
                             text=True).stdout.strip()
        real.append(int(out) / FPS)
    for c, d, r in zip(sel, durs, real):
        if abs(d - r) > 0.5 / FPS:
            raise SystemExit("chapter %s rendered %.3f s, not %.3f s" % (c["code"], r, d))
    listing = os.path.join(work, "parts.txt")
    open(listing, "w").write("".join("file '%s'\n" % p for p in parts))
    video = os.path.join(work, "video.mp4")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", listing, "-c", "copy",
                    video], check=True)

    audio = os.path.join(work, "audio.f32")
    mix_audio(placed, total, bed_exe, work).tofile(audio)
    out = os.path.expanduser(a.out)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", video, "-f", "f32le", "-ar", str(SR), "-ac", "2",
                    "-i", audio, "-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest",
                    "-movflags", "+faststart", out], check=True)
    starts_real = [sum(real[:i]) for i in range(len(real))]
    txt = a.chapters or os.path.splitext(out)[0] + "-chapters.txt"
    open(txt, "w").write(youtube_chapters(sel, starts_real))
    json.dump([dict(code=c["code"], title=c["title"], start=round(s, 3), seconds=round(r, 3))
               for c, s, r in zip(sel, starts_real, real)], open(os.path.join(work, "chapters.json"), "w"), indent=1)
    print(out)
    print(txt)


def render_still(c, chapters, dur, timeline, t, shotdir, png):
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
        render_chapter((c, chapters, max(dur, 2 * t + 0.1), timeline, tmp, shotdir))
        FPS = fps0
        if os.path.exists(tmp + ".done"):
            os.remove(tmp + ".done")
    finally:
        subprocess.Popen = real
    Image.frombytes("RGB", (W, H), frames[min(1, len(frames) - 1)]).save(png)
    print(png)


if __name__ == "__main__":
    main()
