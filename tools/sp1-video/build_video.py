#!/usr/bin/env python3
"""Sleepcousonique on the SP-1: the video manual, in the faltung look (aluminium edition).

  python3 tools/sp1-video/build_video.py [--out ~/Desktop/Sleepcousonique-SP-1-Video-Manual.mp4]
        [--only 0-3] [--work DIR] [--still 4] [--print] [--check] [--app PATH]

The SP-1 twin of tools/wingie-video (kept separate on purpose). Every chapter is one flat deep
field: a mono code above a lowercase title, the text and faltung sliders on the left; in the
middle a faltung drawing of the SP-1, every control where it sits on the real one (measured from
the photo in ref/), or a screenshot of the editor; a turning icosahedron, a stdio tracklist and
folded strips on the right. While the narrator names a control, its row lights up and the same
control lights up on the SP-1 (or a box on the screenshot), joined by a line.

Screenshots: made here, by the editor's own --shot, in SP-1 mode, with --no-link --no-radio (the
serial port and Bluetooth are never touched, and --shot never opens an input). The editor's
settings file is put back as it was afterwards. In the journal look the file path at the right of
the status bar is covered in the bar's own colour (the repository is public).

Narration: Kokoro, British English "Emma" (bf_emma), run with the Cirrostratus video's venv
(~/monsoon-fw/tools/video/.venv, or tools/sp1-video/.venv if there is one). Words are in
narration.py; every place and hand the narrator names is checked against the engine's catalog
first (--check does only that).

Music: rendered from Sleepcousonique itself. bed.c plays the three scenes of bed.dream (an SP-1
dream: a string, a marimba and a bell rung by dust, rain and wind in a cavern; every scene under
85 % of the SP-1), crossfaded, ducked under the voice.

Needs Pillow, numpy, ffmpeg, a C compiler, and a built editor (build/Sleepcousonique_artefacts).
Never commit the video.
"""
import argparse, hashlib, json, math, os, re, shutil, subprocess, sys, tempfile
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
SHOTS = None    # set in main(): the folder the screenshots are made in

# ---------------------------------------------------------------------------
# Palette, the aluminium edition: grounds of brushed steel at night (faltung deep, lightness
# 0.09-0.16, slate to petrol), so the pale SP-1 stands out; accents from the SP-1 itself (its
# pale red PLAY, the cream strip) and the journal's signal colours.
INK = (244, 243, 236)
GROUNDS = ["#15212a", "#1a2026", "#122429", "#1b2230", "#162628", "#1d2129",
           "#13202d", "#1a2624", "#18202b", "#11242a"]
ACCENTS = ["#e8735f", "#e6c76a", "#6fb8b4", "#f0a070", "#9cb8e8", "#d9b36a", "#7fc49a", "#e88f8f"]
STRIPS = ["#e8735f", "#e8dcb0", "#b8c4cc", "#6fb8b4", "#3d5a66", "#e6c76a", "#8a9aa4", "#c9a37a"]


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
F_PANEL = font("IBMPlexMono-Medium.ttf", 14)
F_GLYPH = font("Inter-Medium.ttf", 18)


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


NUMBERED = {"fader": 4, "button": 4}          # the SP-1's numbered hands
WINGIE_ONLY = {"mic", "keys", "key", "mode", "octave", "source", "lights"}


def known(token, places):
    """Is `token` (place, SP-1 hand, or place.port) in the catalog?"""
    base, _, port = token.partition(".")
    m = re.fullmatch(r"([a-z]+?)(\d+)", base)
    if base not in places and m and m.group(1) in NUMBERED and 1 <= int(m.group(2)) <= NUMBERED[m.group(1)]:
        base = m.group(1)
    if base not in places or base in WINGIE_ONLY:
        return False
    return not port or port in places[base]


def validate(chapters, places):
    bad = []
    for c in chapters:
        texts = [c["say"], c["desc"]] + [r["spoken"] or "" for r in c["rows"]] + [r["text"] for r in c["rows"]]
        for t in texts:
            for tok in re.findall(r"`([^`]+)`", t):
                if not known(tok, places):
                    bad.append("%s: %s" % (c["code"], tok))
        for r in c["rows"]:
            for k in r["hl"]:
                if k not in PANEL:
                    bad.append("%s: the panel has no %s" % (c["code"], k))
            if r.get("place") and r["place"] not in places:
                bad.append("%s: no place %s" % (c["code"], r["place"]))
            if r.get("box") and not c.get("shot"):
                bad.append("%s: a box but no screenshot" % c["code"])
        if c.get("shot") and c["shot"] not in narration.SHOTS:
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


SAY = [("Sleepcousonique", "Sleep-coo-so-neek"), ("sleepcousonique", "sleep-coo-so-neek"),
       ("SP-1s", "S P ones"), ("SP-1's", "S P one's"), ("SP-1", "S P one"), ("S P 1", "S P one"),
       ("T1", "T one"), ("T2", "T two"), ("T3", "T three"), ("T4", "T four"),
       ("USB-C", "U.S.B. C"), ("USB-MIDI", "U.S.B. midi"), ("USB", "U.S.B."), ("MIDI", "midi"),
       ("TRS", "T.R.S."), ("DIN", "din"), ("LEDs", "L.E.D.s"), ("LED", "L.E.D."), ("LFOs", "L.F.O.s"),
       ("CPU", "C.P.U."), ("Hordijk", "Hordike"), ("Tcherepnin", "Cherepnin"), ("Mycenae", "My-see-nee"),
       ("crossfm", "cross F M"), ("steprandom", "step random"), ("lpg", "L P G"), ("svf", "S V F"),
       ("eq", "E Q"), ("dusg", "D U S G"), ("vco", "V C O"), ("lfo", "L F O"), ("midicc", "midi C C"),
       ("midiclock", "midi clock"), ("reverblite", "reverb lite"), ("choruslite", "chorus lite"),
       ("delaylite", "delay lite"), ("fmlite", "F M lite"), ("supersawlite", "supersaw lite"),
       ("onepole", "one pole"), ("tapeecho", "tape echo"), ("paulstretch", "Paul-stretch"),
       ("difftone", "diff tone"), ("gendy", "Gendy"), ("benjolin", "Benjolin"), ("tri", "try"),
       ("dB", "decibels"), ("PLAY", "play"), ("leds", "L.E.D.s"), ("LFO", "L.F.O.")]


def speakable(s):
    s = clean_md(s)
    for a, b in SAY:
        s = re.sub(r"\b%s\b" % re.escape(a), b, s)
    s = re.sub(r"\b(fader|button)(\d)\b", r"\1 \2", s)
    s = re.sub(r"(?<=[a-z])\.(?=[a-z])", " ", s)          # rocker.up: rocker up
    s = re.sub(r"(\d)\s*%", r"\1 percent", s)
    s = re.sub(r"(?<![\w])\+(\d)", r"plus \1", s)
    s = re.sub(r"(?<![\w])[−-](\d)", r"minus \1", s)
    for a, b in [("→", " to "), ("·", ","), ("—", ","), ("–", " to "), ("/", " or "), ("+", " and ")]:
        s = s.replace(a, b)
    s = re.sub(r"\s+,", ",", s)
    s = re.sub(r",\s*,", ",", s)
    s = re.sub(r",\s*\.", ".", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def place_chapters():
    chs = []
    for p in narration.PLACE_CHAPTERS:
        c = dict(p)
        c["say"] = "The places: %s. %s." % (c["title"], c["sub"][0].upper() + c["sub"][1:])
        chs.append(c)
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
# The SP-1, faltung style. Coordinates are in units of the photo in ref/ (enlarged 4x), with
# (0, 0) at the face's top-left corner: the aluminium face is 417 x 690, the cream strip 82 wide
# on its right; the four faders, the track-row LEDs and T1-T4 sit where they do on the real one.
# PANEL_S scales it into the frame.

PANEL_S = 0.9
PANEL_X, PANEL_Y = 1066, 238
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

PANEL = {
    "rocker": ("tab", -24, 38, 2, 152), "play": ("tab", 436, 66, 529, 190), "dots": ("tab", 436, 506, 529, 630),
    "volm": ("tab", 52, -12, 106, 3), "volp": ("tab", 132, -12, 196, 3),
    "trackrow": ("row", 38, 470, 374, 496), "playrow": ("row", 444, 244, 474, 486),
}
for _i, _x in enumerate(FADER_X):
    PANEL["f%d" % (_i + 1)] = ("fader", _x - 17, SLOT_TOP - 8, _x + 17, SLOT_BOT + 6)
    PANEL["t%d" % (_i + 1)] = ("button", _x - 19, BTN_Y0 - 3, _x + 19, BTN_Y1 + 3)


def P(x, y):
    return PANEL_X + x * PANEL_S, PANEL_Y + y * PANEL_S


def rect(d, x0, y0, x1, y1, r=0, **kw):
    a, b = P(x0, y0), P(x1, y1)
    if r:
        d.rounded_rectangle([a[0], a[1], b[0], b[1]], radius=r * PANEL_S, **kw)
    else:
        d.rectangle([a[0], a[1], b[0], b[1]], **kw)


def circ(d, x, y, r, **kw):
    cx, cy = P(x, y)
    rr = r * PANEL_S
    d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], **kw)


def ptext(d, x, y, s, fill, f=F_PANEL):
    cx, cy = P(x, y)
    d.text((cx - f.getlength(s) / 2, cy - f.size * 0.6), s, font=f, fill=fill)


def panel_static(d, c):
    g = c["ground"]
    lab = mix(INK, g, 0.45)
    shadow = mix(g, (0, 0, 0), 0.35)
    # the shadow, folded off to the lower right as on the faltung sleeves
    x0, y0 = P(-24, -12)
    x1, y1 = P(529, FACE_H)
    d.polygon([(x0 + 20, y0 + 20), (x1 + 20, y0 + 20), (x1 + 20, y1 + 20), (x0 + 20, y1 + 20)], fill=shadow)
    # what sticks out of the edges: the rocker (left), VOL - / VOL + (top), PLAY's and ••'s tabs (right)
    rect(d, -20, 40, 6, 150, 5, fill=CREAM, outline=CREAM_EDGE, width=2)
    rect(d, 57, -8, 102, 6, 3, fill=ALU, outline=ALU_EDGE, width=2)
    rect(d, 137, -8, 192, 6, 3, fill=ALU, outline=ALU_EDGE, width=2)
    rect(d, 490, 70, 527, 185, 6, fill=CREAM, outline=CREAM_EDGE, width=2)
    rect(d, 490, 510, 527, 625, 6, fill=CREAM, outline=CREAM_EDGE, width=2)
    # the face and the strip
    rect(d, 0, 0, STRIP_X1, FACE_H, 16, fill=CREAM, outline=CREAM_EDGE, width=2)
    rect(d, 0, 0, 417, FACE_H, 16, fill=ALU, outline=ALU_EDGE, width=2)
    rect(d, 400, 2, 417, FACE_H - 2, 0, fill=ALU)
    a, b = P(417, 1), P(417, FACE_H - 1)
    d.line([a, b], fill=CREAM_EDGE, width=2)
    # PLAY: the pale red triangle; ••: the two dots
    cx, cy = P(*PLAY_XY)
    s = 9 * PANEL_S
    d.polygon([(cx, cy - s), (cx + s, cy + s * 0.75), (cx - s, cy + s * 0.75)], fill=RED)
    for y in DOTS_Y:
        circ(d, DOTS_X, y, 3.6, fill=(40, 40, 30))
    # the fader slots
    for x in FADER_X:
        rect(d, x - 14, SLOT_TOP, x + 14, SLOT_BOT, 14, fill=(92, 95, 92))
        rect(d, x - 7, SLOT_TOP + 10, x + 7, SLOT_BOT - 8, 7, fill=(14, 15, 12))
    # T1-T4: tall flat buttons with a pressed-in rim
    for x in FADER_X:
        rect(d, x - 16, BTN_Y0, x + 16, BTN_Y1, 4, fill=(72, 74, 72))
        rect(d, x - 13, BTN_Y0 + 3, x + 13, BTN_Y1 - 3, 3, fill=(200, 202, 198))
    # labels: not printed on the SP-1, so they sit outside it, small and dim
    ptext(d, 80, -40, "vol −", lab)
    ptext(d, 165, -40, "vol +", lab)
    for i, x in enumerate(FADER_X):
        ptext(d, x, FACE_H + 24, "t%d" % (i + 1), lab)
    ptext(d, 462, FACE_H + 24, "sp-1", lab)


def ring(d, kind, x0, y0, x1, y1, acc, pulse):
    e = 8 + 6 * pulse
    a, b = P(x0 - e, y0 - e), P(x1 + e, y1 + e)
    box = [a[0], a[1], b[0], b[1]]
    d.rounded_rectangle(box, radius=12, outline=(10, 12, 14), width=7)   # dark under, so it shows on aluminium
    d.rounded_rectangle(box, radius=12, outline=acc, width=3)


LIT = (255, 248, 226)


def panel_dynamic(d, c, t, hl, leds):
    acc = c["accent"]
    pulse = 0.5 + 0.5 * math.sin(t * 5.0)
    press = 0.5 + 0.5 * math.sin(t * 3.2) > 0.45
    # faders: the caps move when talked about
    for i, x in enumerate(FADER_X):
        on = ("f%d" % (i + 1)) in hl
        v = 0.5 + (0.42 * math.sin(t * 1.4 + i) if on else 0.0)
        cy = (SLOT_BOT - 18) - (SLOT_BOT - SLOT_TOP - 36) * v
        circ(d, x, cy, 12.5, fill=acc if on else (226, 226, 220), outline=(60, 62, 58), width=2)
    # T1-T4 pressed
    for i, x in enumerate(FADER_X):
        if ("t%d" % (i + 1)) in hl and press:
            rect(d, x - 13, BTN_Y0 + 3, x + 13, BTN_Y1 - 3, 3, fill=acc)
    # the tabs pressed: rocker (tilting ^ / v), PLAY, ••, VOL
    if "rocker" in hl:
        up = math.sin(t * 2.4) > 0
        rect(d, -20, 40 if up else 95, 6, 95 if up else 150, 5, fill=acc)
    for k, box in (("play", (499, 72, 527, 183)), ("dots", (499, 512, 527, 623)),
                   ("volm", (57, -8, 102, 6)), ("volp", (137, -8, 192, 6))):
        if k in hl and press:
            rect(d, *box, 4, fill=acc)
    # the rocker's marks
    ptext(d, -7, 66, "^", (70, 64, 44), F_GLYPH)
    ptext(d, -7, 126, "v", (70, 64, 44), F_PANEL)
    # the eight LEDs: 0-3 the track row (above T1-T4), 4-7 the play row (•• end first)
    pos = [(x, LED_Y) for x in FADER_X] + [(DOTS_X, y) for y in PLAYROW_Y]
    for k, (x, y) in enumerate(pos):
        v = max(0.0, min(1.0, leds[k]))
        if v > 0.03:
            circ(d, x, y, 9 + 5 * v, fill=mix(ALU if k < 4 else CREAM, LIT, 0.45 * v))
            circ(d, x, y, 4.2, fill=mix((70, 70, 60), LIT, v))
        else:
            circ(d, x, y, 3.2, fill=(46, 47, 42) if k < 4 else (150, 144, 118))
    # rings round what is being talked about
    for k in hl:
        kind, x0, y0, x1, y1 = PANEL[k]
        ring(d, kind, x0, y0, x1, y1, acc, pulse)


def led_state(c, t, row):
    """What the eight LEDs do while a row is spoken (or the chapter plays)."""
    anim = row.get("anim") if row else None
    hl = row["hl"] if row else []
    z = [0.0] * 8
    if anim == "boot":                      # T1 lit, nothing else
        z[0] = 1.0
    elif anim == "standby":                 # the play row: the charge bar, breathing
        b = 0.35 + 0.12 * math.sin(t * 2 * math.pi / 2.2)
        z[4:7] = [b, b, b]
    elif anim == "poweron":                 # the track row fills, then it plays
        ph = t % 2.4
        for i in range(4):
            z[i] = 1.0 if ph > 0.15 + i * 0.09 and ph < 1.3 else 0.0
    elif anim == "poweroff":                # each track LED fades, T1 to T4
        ph = t % 2.4
        for i in range(4):
            z[i] = max(0.0, min(1.0, 1.0 - (ph - 0.7 - i * 0.2) / 0.15)) if ph < 1.6 else 0.0
    elif anim == "battery":                 # tap: the charge bar on the play row for 1.5 s
        if t % 2.6 < 1.5:
            z[4:7] = [0.35, 0.35, 0.35]
    elif anim == "scene":                   # a bar of (scene % 4) + 1 on the track row
        s = int(t / 1.3) % 4
        if t % 1.3 < 0.6:
            for i in range(s + 1):
                z[i] = 1.0
    elif anim == "iface":                   # unit 1 in binary; the levels on the play row
        z[0] = 0.8
        lv = 0.5 + 0.4 * math.sin(t * 3.1)
        rv = 0.5 + 0.4 * math.sin(t * 2.6 + 1)
        z[4], z[5], z[6], z[7] = (1.0 if lv > 0.5 else 0.08), lv, rv, (1.0 if rv > 0.5 else 0.08)
    elif anim == "track":
        z[int(t * 2.5) % 4] = 1.0
    elif anim == "bar" or "trackrow" in hl or "playrow" in hl or c["kind"] in ("intro", "outro", "places"):
        lv = 0.5 + 0.45 * math.sin(t * 1.3) * math.sin(t * 0.37 + 1)
        for i in range(4):                  # sound into leds: a bar across both rows
            z[i] = max(0.0, min(1.0, lv * 4 - i))
            z[4 + i] = max(0.0, min(1.0, lv * 4 - i - 0.3))
    return z


# ---------------------------------------------------------------------------
# Screenshot chapters: the editor where the SP-1 would be.

SHOT_X, SHOT_Y, SHOT_W = 990, 190, 900
_SHOTS = {}


def make_shots(app, folder):
    """The editor's own screenshots, in SP-1 mode, never touching the serial port or Bluetooth."""
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


def shot_image(name, crop=None, folder=None):
    """The screenshot, or the part of it given by crop (x, y, w, h), scaled to SHOT_W wide."""
    key = (name, crop)
    if key not in _SHOTS:
        im = Image.open(os.path.join(folder or SHOTS, name)).convert("RGB")
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
# Other drawing (as in the Cirrostratus and Wingie videos).

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
    d.text((96 + F_CODE.getlength(c["code"]) + 22, 66), "sleepcousonique · on the sp-1", font=F_MONO_S, fill=dim)
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
        img, _ = shot_image(c["shot"], c.get("crop"), c.get("shotdir"))
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
        _, scale = shot_image(c["shot"], c.get("crop"), c.get("shotdir"))
        x0, y0b, x1, y1 = box_rect(r["box"], scale, c.get("crop"))
        pulse = 0.5 + 0.5 * math.sin(t * 5.0)
        g = 3 + 3 * pulse
        d.rectangle([x0 - g, y0b - g, x1 + g, y1 + g], outline=(10, 12, 14), width=7)
        d.rectangle([x0 - g, y0b - g, x1 + g, y1 + g], outline=acc, width=4)
        tx, ty = SHOT_X - 6, (y0b + y1) / 2   # to the screenshot's edge, beside the box
        d.line([(tx, ty), (x0 - g, ty)], fill=acc, width=1)
    else:
        if not r["hl"]:
            return
        _, x0, py0, _, py1 = PANEL[r["hl"][0]]
        tx, ty = P(x0 - 14, (py0 + py1) / 2)
    elbow = 965
    d.line([(TEXT_R + 12, y), (elbow, y), (tx, ty)], fill=acc, width=2)
    d.ellipse([TEXT_R + 6, y - 6, TEXT_R + 18, y + 6], fill=acc)


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
        row = c["rows"][active] if active is not None else None
        if not c.get("shot"):
            panel_dynamic(d, c, t, row["hl"] if row else [], led_state(c, t, row))
        if active is not None:
            draw_connector(d, c, active, t)
        prog = (c["index"] + t / dur) / len(chapters)
        skew_rect(d, 0, H - 8, W * prog, 8, c["accent"], 0.5)
        a = min(1.0, t / 0.45, (dur - t) / 0.45)
        if a < 1.0:
            im = Image.blend(Image.new("RGB", (W, H), (8, 12, 14)), im, max(0.0, a))
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
    raise SystemExit("no Kokoro venv: python3.12 -m venv tools/sp1-video/.venv && "
                     "tools/sp1-video/.venv/bin/pip install kokoro soundfile")


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
            subprocess.run([bed_exe, os.path.join(HERE, "bed.dream"), str(s), str(part), p, "0.5", "0.5", "0.5", "0.5"],
                           check=True)
        x = np.fromfile(p, np.float32).reshape(-1, 2)
        x = x[int(8 * SR):]                     # past the first swell
        scenes.append(x / max(1e-6, np.sqrt((x ** 2).mean()) * 8))   # the three scenes at one level
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
    global SHOTS
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.expanduser("~/Desktop/Sleepcousonique-SP-1-Video-Manual.mp4"))
    ap.add_argument("--only", help="chapter range a-b (for previews)")
    ap.add_argument("--work", default=None)
    ap.add_argument("--app", default=APP, help="the editor's binary, for the screenshots")
    ap.add_argument("--jobs", type=int, default=max(1, os.cpu_count() - 2))
    ap.add_argument("--still", help="write one frame per chapter as PNG at this time (s) and stop")
    ap.add_argument("--print", action="store_true", help="print the spoken text and stop")
    ap.add_argument("--check", action="store_true", help="only check the narration against the catalog")
    a = ap.parse_args()

    work = os.path.abspath(a.work or tempfile.mkdtemp(prefix="scq_sp1_video_"))
    os.makedirs(work, exist_ok=True)
    SHOTS = os.path.join(work, "shots")
    scqtool, bed_exe = build_tools(work)
    chapters = build_chapters()
    validate(chapters, catalog(scqtool))
    for c in chapters:
        c["shotdir"] = SHOTS          # the render workers are fresh processes: they get it with the chapter
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
    make_shots(a.app, SHOTS)
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
    json.dump([dict(code=c["code"], title=c["title"], seconds=round(d, 2)) for c, d in zip(sel, durs)],
              open(os.path.join(work, "chapters.json"), "w"), indent=1)

    if a.still:
        ts = [float(x) for x in a.still.split(",")]
        for c, d, tl in zip(sel, durs, timelines):
            for s in ts:
                render_still(c, chapters, d, tl, min(s, d - 0.6),
                             os.path.join(work, "still_%03d_%g.png" % (c["index"], s)))
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
    # the bed is plucked (dust on strings and bars), so its peaks stand far above its body: a slow
    # limiter (instant attack, 0.25 s release) brings the body up 12 dB without touching the tone
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
        render_chapter((c, chapters, max(dur, 2 * t + 0.1), timeline, tmp))
        FPS = fps0
        if os.path.exists(tmp + ".done"):
            os.remove(tmp + ".done")
    finally:
        subprocess.Popen = real
    Image.frombytes("RGB", (W, H), frames[min(1, len(frames) - 1)]).save(png)
    print(png)


if __name__ == "__main__":
    main()
