"""What the narrator says in the Sleepcousonique SP-1 video manual, chapter by chapter.

Every chapter is a dict: code, title, sub, desc (on screen), say (spoken first), rows. A row is
(label, text on screen, spoken or None, panel highlight keys) and, in screenshot chapters, a box
(x, y, w, h) on the screenshot (1440 x 900 pixels, measured on the editor's own --shot images).
A row may also carry `anim`, what the panel's LEDs do while it is spoken.

Anything in `backticks` is a place, a hand or a place.port: build_video.py checks every one
against the engine's catalog before anything is spoken. Backticks are not spoken.

The facts are the code's, on branch wingie: the firmware (firmware/app/src/sp1_scq.c,
sp1_ui_timing.h, sp1_controls.c), the editor (app/) and the engine (engine/), firmware 1.5.0-beta1.
Loads quoted are the editor's estimate (scqtool check), which is fitted to SP-1 measurements.
"""

FADERS = ["f1", "f2", "f3", "f4"]
TRACK = ["t1", "t2", "t3", "t4"]


def R(label, text, spoken=None, hl=(), box=None, anim=None, place=None):
    return dict(label=label, text=text, spoken=spoken, hl=list(hl), box=box, anim=anim, place=place)


# The editor's screenshots, made with the app's own --shot (SP-1 mode, --no-link --no-radio):
# name: (dream in examples/, view 0 journal / 1 both / 2 tapestry, theme)
SHOTS = {
    "both-journal-light.png": ("first-night.dream", 1, "journal,light"),
    "both-journal-dark.png": ("first-night.dream", 1, "journal,dark"),
    "both-faltung-dark.png": ("first-night.dream", 1, "openprocess,dark"),
    "journal-light.png": ("first-night.dream", 0, "journal,light"),
    "tapestry-journal-light.png": ("time-sound-system.dream", 2, "journal,light"),
}

BEFORE = [
    dict(code="sq00", title="sleepcousonique", sub="on the sp-1: a dream journal you can carry",
         desc="Build a dream in the editor, hear it at once, send it over USB, unplug, and play it on the "
              "SP-1's own faders and buttons.",
         say="Sleepcousonique, on the SP-1. Sleepcousonique is a patch language written like a dream journal: "
             "each patch is a dream, its modules are places, and the cords between them are passages. You build "
             "a dream in the editor and hear it at once. Then you send it over USB to an SP-1 running the "
             "Sleepcousonique firmware, unplug, and its faders, buttons and lights play it on their own.",
         rows=[R("places", "over 200", hl=[]), R("slots", "5", hl=[]), R("scenes", "16 a dream", hl=[]),
               R("memory", "124 kb a scene", hl=[]), R("firmware", "1.5.0-beta1", hl=[])], kind="intro"),

    dict(code="fl01", title="flashing", sub="is not sending",
         desc="Flash the firmware once, in the browser, in boot mode (T1 + T4). Send dreams every time, from the "
              "editor, with the SP-1 switched on normally.",
         say="Flashing, and sending: two different jobs.",
         rows=[R("flash · once", "the firmware · chrome or edge",
                 "Flashing installs the firmware, once, and again only to update it: in Chrome or Edge, on "
                 "solderless dot engineering.", hl=[]),
               R("boot mode", "hold t1 + t4 · plug in usb",
                 "Turn the SP-1 off and unplug it. Hold T1 and T4 and plug in the cable. Let go when T1 lights.",
                 hl=["t1", "t4"], anim="boot"),
               R("the updater", "custom firmware · sleepcousonique-sp1.bin",
                 "In the updater, choose the custom firmware, pick sleepcousonique S P 1 dot bin, connect, and "
                 "flash.", hl=[], anim="boot"),
               R("standby", "dark · charging · silent",
                 "It restarts switched off, in standby, charging, and silent. That is normal, not a failed flash.",
                 hl=["playrow"], anim="standby"),
               R("hold ••", "on · the first night",
                 "Hold the dots for half a second to turn it on. It plays the first night.",
                 hl=["dots"], anim="poweron"),
               R("send · every time", "plugged in normally · no buttons held",
                 "Sending puts a dream on it, every time you change one: plug it in with no buttons held, turn it "
                 "on, and press send to SP-1. Sending never needs boot mode, and the editor never flashes.",
                 hl=["dots"])], kind="steps"),

    dict(code="pn02", title="the sp-1's hands", sub="every fader and button is yours",
         desc="Nothing on the panel is fixed: the dream says what each fader, button and light does. Only the "
              "volume buttons stay the SP-1's own.",
         say="The SP-1's hands. Nothing on the panel is fixed: the dream says what each control does.",
         rows=[R("fader1 … fader4", "the four faders · 0 to 1",
                 "The four faders are `fader1` to `fader4`, from zero at the bottom to one at the top.", hl=FADERS),
               R("button1 … button4", "the track buttons t1 to t4",
                 "The track buttons, T1 to T4, are `button1` to `button4`, one while held. The SP-1 reads one at "
                 "a time: a chord of them is ignored.", hl=TRACK),
               R("rocker", "ffwd +1 · rwd −1 · up and down pulse",
                 "On the left edge, the rocker: fast forward is plus one, rewind minus one. `rocker.up` and "
                 "`rocker.down` pulse on each press.", hl=["rocker"]),
               R("play", "the red triangle · 1 while held",
                 "On the strip, PLAY: `play`, one while held.", hl=["play"]),
               R("dots", "•• · 1 while held · the shift",
                 "Low on the strip, the dots: `dots`. They are also the SP-1's shift.", hl=["dots"]),
               R("vol − · vol +", "the volume · never the dream's",
                 "On the top edge, volume down and up: the SP-1's own volume, in three decibel steps.",
                 hl=["volm", "volp"]),
               R("leds.track", "the track row · above t1 to t4",
                 "The lights above the track buttons are the track row: `leds.track`.", hl=["trackrow"],
                 anim="track"),
               R("leds.play", "the play row · from •• to play",
                 "The other four, on the strip between the dots and PLAY, are the play row: `leds.play`. Sound "
                 "into `leds` lights both rows as a bar.", hl=["playrow"], anim="bar")],
         kind="panel"),

    dict(code="pw03", title="the dots", sub="on, off, and the shift",
         desc="Hold •• half a second: on. Hold it 1.5 s, touching nothing else: off. With the rocker, the volume "
              "buttons or PLAY, it reaches scenes, slots and interface mode.",
         say="The dots turn the SP-1 on and off, and held with another control, reach its own business.",
         rows=[R("hold •• 0.5 s", "on · the track row fills",
                 "Hold them half a second to turn it on: the track row fills, and it plays the dream it played "
                 "last.", hl=["dots"], anim="poweron"),
               R("hold •• 1.5 s", "off · touching nothing else",
                 "Hold them a second and a half, touching nothing else, to turn it off. Touch another control and it "
                 "stays on, so gestures with the dots are safe.", hl=["dots"], anim="poweroff"),
               R("tap ••", "the battery · on the play row",
                 "Tap them, and the play row shows the battery.", hl=["dots", "playrow"], anim="battery"),
               R("•• + ffwd / rwd", "next · previous scene",
                 "The dots and the rocker: the next or previous scene, shown on the track row. The rocker never "
                 "reaches the dream then.", hl=["dots", "rocker", "trackrow"], anim="scene"),
               R("•• + vol + / −", "next · previous slot",
                 "The dots and volume up or down: the next or previous of the five dream slots.",
                 hl=["dots", "volp", "volm"], anim="slot"),
               R("•• + play 1 s", "interface mode · a usb sound card",
                 "Hold the dots and PLAY for a second, and the SP-1 becomes a two-channel USB sound card. Hold them "
                 "again, or send it a dream, and it dreams again.", hl=["dots", "play"], anim="iface")],
         kind="combos"),

    dict(code="ed04", title="the editor", sub="journal, tapestry, or both", shot="both-journal-light.png",
         desc="One dream, two ways: plain text on the left, boards and cords on the right. Whatever you do on one "
              "shows up on the other.",
         say="The editor. Install it from the Homebrew tap or the releases page, and the first night plays at once.",
         rows=[R("sp-1 | wingie", "the instrument switch",
                 "This switch says which instrument the dream is for. For the SP-1, leave it on SP-1.",
                 box=(148, 8, 124, 30)),
               R("journal", "the dream as text",
                 "On the left, the journal: the dream as text. Half a second after you stop typing, the sound "
                 "follows.", box=(0, 80, 548, 550)),
               R("tapestry", "the same dream as boards",
                 "On the right, the tapestry: the same dream, as boards and cords.", box=(550, 80, 890, 550)),
               R("journal · both · tapestry", "what fills the window",
                 "These buttons choose what fills the window.", box=(760, 8, 252, 30)),
               R("sp-1 hands", "a grey board, always there",
                 "Two grey boards are always there: the SP-1's hands, and its outputs, LEDs and scene changes.",
                 box=(594, 124, 94, 204)),
               R("scenes", "a button each · + scene",
                 "A button for every scene, and plus scene, which adds today's date.", box=(10, 50, 414, 28)),
               R("send to sp-1", "cmd-u · the slot that is playing",
                 "And send to SP-1, command U, sends the dream to the slot that's playing.", box=(10, 8, 128, 30))],
         kind="shot"),

    dict(code="lk05", title="the looks", sub="journal, faltung, modular", shot="both-faltung-dark.png",
         desc="View > Look: journal (the original), faltung (the openprocess layout) or modular (the 2004 patch "
              "editor). View > Light or dark. The same dream in every look.",
         say="View, Look: journal, the original; faltung; or modular, after the patch editors of 2004. And light, "
             "dark, or follow the computer.",
         rows=[R("faltung", "the openprocess layout",
                 "This is faltung, dark: places as folded blocks in their family's colour, and thin coloured "
                 "cables.", box=(546, 164, 630, 456)),
               R("the browser", "a tile for every place",
                 "Along the top, a tile for every place, by family, and the SP-1's hands. Hover to read one, click "
                 "to add it.", box=(212, 8, 968, 80)),
               R("find a place", "by name or by what it does",
                 "Find a place searches by name, or by what it does; return adds the first.", box=(104, 10, 102, 40)),
               R("settings", "the selected place",
                 "Select a place, and its settings appear on the right: drag a bar, and it plays as you drag.",
                 box=(1188, 196, 250, 424)),
               R("the hands", "rails and blocks",
                 "Along the bottom, the SP-1's hands, as rails and folded blocks.", box=(112, 696, 764, 140))],
         kind="shot"),

    dict(code="hp06", title="hands on screen", sub="play the sp-1 from the computer", shot="both-journal-dark.png",
         crop=(0, 700, 900, 176),
         desc="Faders, track buttons, the rocker, PLAY and the dots, and what each does in the scene that plays. "
              "With the SP-1 plugged in, the panel follows it.",
         say="Along the bottom of the editor, a stand-in for the SP-1.",
         rows=[R("faders", "drag · their copper loops",
                 "Drag a fader to move it; drag its copper loop onto an inlet, and it's patched there.",
                 box=(18, 706, 350, 132)),
               R("track buttons", "held while the mouse is down",
                 "The track buttons are held while the mouse is down.", box=(22, 840, 336, 28)),
               R("rocker · play · dots", "up · down · play · ••",
                 "Then the rocker, PLAY and the dots.", box=(380, 732, 210, 134)),
               R("keys", "1 to 4 · ↑ ↓ · space · .",
                 "Or the keyboard: one to four, the arrows, space for PLAY, and full stop for the dots. Full stop "
                 "and an arrow changes scene.", box=None),
               R("the patch map", "what your hands do in this scene",
                 "On the right, what your hands do in this scene. Click a row to find it in the journal.",
                 box=(670, 710, 230, 122)),
               R("sp-1 live", "the panel follows it",
                 "With an SP-1 plugged in and on, the panel follows it live, scene changes too.",
                 box=(8, 702, 590, 168))], kind="shot"),

    dict(code="wr07", title="writing a dream", sub="notes, scenes, the prelude and names", shot="journal-light.png",
         crop=(0, 76, 810, 290),
         desc="A dream is plain text: write it in any editor, keep it in a notes app, paste it from an email.",
         say="Writing a dream. A dream is plain text.",
         rows=[R("; notes", "after a semicolon",
                 "Anything after a semicolon is a note to yourself.", box=(30, 79, 768, 43)),
               R("drift 3", "natures wander 3 %",
                 "Drift three lets tuned places wander up to three percent each night. The SP-1 counts its nights, "
                 "so it's never quite the same twice.", box=(30, 122, 64, 16)),
               R("the prelude", "lines before the first scene",
                 "Lines before the first scene are the prelude, which every scene shares.", box=(30, 79, 768, 59)),
               R("a scene", "a line with dashes under it",
                 "A line with dashes under it is a scene, usually a date. A dream can have sixteen.",
                 box=(30, 149, 84, 30)),
               R("naming", "the name is place nature",
                 "The window is `lpg`; the dice are `steprandom` two: a name, a place, its nature. Inside your "
                 "dream, your name wins.", box=(30, 191, 200, 73)),
               R("cords", "stairs.end -> stairs.trig",
                 "Then the cords: stairs end into stairs trig, and the staircase climbs forever.",
                 box=(30, 275, 568, 86)),
               R("mistakes", "the last dream that made sense",
                 "A word one letter off is understood, with a note. And while there are mistakes, the last dream "
                 "that made sense keeps playing.", box=None)], kind="shot"),

    dict(code="ps08", title="passages", sub="cords with parts soldered in", shot="tapestry-journal-light.png",
         desc="After Peter Blasser's Nabra: a wire, a resistor, a capacitor, a diode, or chance. A number inside the "
              "arrow sets the amount.",
         say="Passages: cords with parts soldered in, after Peter Blasser's Nabra.",
         rows=[R("->", "hall · a plain wire", "Arrow is a hall: a plain wire.", box=(286, 9, 88, 28)),
               R("~>", "fog · a resistor", "Tilde arrow is fog: a resistor, half as loud and softened.",
                 box=(378, 9, 88, 28)),
               R("=>", "water · a capacitor", "Equals arrow is water: a capacitor, slow to fill and drain.",
                 box=(470, 9, 88, 28)),
               R(">|", "door · a diode", "Arrow bar is a door: a diode, only the positive half.",
                 box=(562, 9, 88, 28)),
               R("?>", "maybe · by chance",
                 "Question arrow is maybe: through by chance. A number inside any arrow sets the amount.",
                 box=(654, 9, 88, 28)),
               R("a dot", "an inlet or an outlet",
                 "A dot picks an inlet or outlet; cords into one inlet add up, like stacked bananas.",
                 box=(1234, 186, 76, 152)),
               R("cables", "braided · thin · beaded",
                 "Audio leads are fat and braided, currents thin, pulses beaded. Drag outlet to inlet to patch.",
                 box=(196, 180, 770, 270))], kind="shot"),

    dict(code="sg09", title="signals and switches", sub="sound, current, pulse",
         desc="Three kinds of signal, as on a Buchla; any of them into any inlet, as on a Serge. Switches are "
              "fixed for the dream, written after the nature.",
         say="Signals: three kinds, as on a Buchla, and any of them into any inlet, as on a Serge.",
         rows=[R("sound", "about −1 to 1 · umber", "Sound swings about minus one to one, drawn umber.", hl=[]),
               R("current", "about 0 to 1 · deep blue",
                 "Current sits about zero to one, drawn deep blue: the faders are currents.", hl=FADERS),
               R("pulse", "0 or 1 · gold",
                 "A pulse is zero or one, drawn gold, like a held track button.", hl=TRACK),
               R("pitch", "octaves · from the 60 hz hum",
                 "Pitch counts octaves, from the sixty hertz hum: sine three is a hundred and eighty hertz.", hl=[]),
               R("pitch units", "7st · -50ct · 1oct",
                 "Or carry a unit: seven S T, minus fifty C T, one O C T.", hl=[]),
               R("switches", "range=audio · scale=pelog",
                 "Switches and params are fixed for the dream, written after the nature. A position word alone "
                 "picks it.", hl=[]),
               R("resolution", "fine · audio",
                 "And resolution fine or audio reads a place's controls every four samples, or every one, for true "
                 "audio-rate F M, at eight or thirty-two times the cost.", hl=[])], kind="panel"),

    dict(code="kn10", title="knobs", sub="tune, attenuverters and lucid groups", shot="tapestry-journal-light.png",
         desc="Every place has the knobs it would have on an analog modular. A knob is the inlet's own value; what "
              "you patch in adds to it, through the attenuverter.",
         say="Knobs: every place has the ones it would have on an analog modular.",
         rows=[R("panel knobs", "time= · feedback= · tone=",
                 "A knob is the inlet's own value, written after the place with the inlet's name; whatever you "
                 "patch in adds to it.", box=(1018, 218, 28, 56)),
               R("tune", "the base pitch · in octaves",
                 "Every place with a pitch inlet has a tune knob: its base pitch, twelve octaves either way.",
                 box=(670, 150, 28, 38)),
               R("attenuverters", "att.inlet · −2 to 2",
                 "And every source has an attenuverter on each inlet: nought shuts the cord out, negative turns it "
                 "over. On pitch it goes to ten, so one fader sweeps ten octaves.", box=(603, 186, 20, 66)),
               R("on the tapestry", "drag · shift for fine · double-click",
                 "Drag a knob, shift for fine, double-click to put it back. The journal follows.",
                 box=(514, 140, 192, 124)),
               R("lucid groups", "one fader · many inlets",
                 "A lucid group leans one fader on many inlets, each with its own range, like the Nord Modular's "
                 "morph.", box=None)], kind="shot"),
]

# The places, by family: a few from each, and the ones that suit the SP-1. Each row is one place;
# the panel lights the hands it is paired with.
PLACE_CHAPTERS = [
    dict(code="so11", title="sources", sub="oscillators, weather and storms",
         desc="Things that make sound or pulses on their own: oscillators, drums, weather, chaos.",
         rows=[R("crossfm", "two sines on neighbouring harmonics",
                 "`crossfm`: two sines on neighbouring harmonics, with a folder. The first night's, `fader1` on its "
                 "index.", hl=["f1"], place="crossfm"),
               R("rain", "drops ringing tiny resonators", "`rain`: drops ringing tiny resonators.", place="rain"),
               R("frogs · froglet", "chirping voices · one frog",
                 "`frogs`: chirping voices on random timers. `froglet` is one frog, almost free.", place="frogs"),
               R("bell", "four inharmonic modes",
                 "`bell`: four inharmonic modes. Put `button1` on `bell.strike`.", hl=["t1"], place="bell"),
               R("vco", "after the moog 921", "`vco`: four waves and hard sync, after the Moog nine twenty-one.",
                 place="vco"),
               R("benjolin", "after rob hordijk", "`benjolin`: oscillators, a rungler and a twin-peak filter, after "
                 "Rob Hordijk.", place="benjolin"),
               R("gendy", "xenakis's storm", "`gendy`: Xenakis's dynamic stochastic synthesis.", place="gendy"),
               R("kick", "after the tr-808", "`kick`: a falling sine, after the eight-oh-eight. Strike it with "
                 "`play`.", hl=["play"], place="kick")], kind="places"),
    dict(code="sh12", title="shapes", sub="filters, folders, pedals",
         desc="Things that change a sound: filters and gates, folders and saturators, pedals and tapes, and the "
              "spectral places.",
         rows=[R("folder", "more fold, more folds", "`folder`: more fold, more folds. `fader2` on `folder.fold`.",
                 hl=["f2"], place="folder"),
               R("lpg", "vactrol-style · a pulse plucks it",
                 "`lpg`: a vactrol-style lowpass gate; a pulse plucks it.", place="lpg"),
               R("ladder", "after the moog 904a",
                 "`ladder`: after the Moog nine-oh-four A, `fader3` on its cutoff.", hl=["f3"], place="ladder"),
               R("svf", "low, band, high and notch", "`svf`: lowpass, bandpass, highpass and notch at once.",
                 place="svf"),
               R("tape", "the cassette that lived in a car",
                 "`tape`: the cassette that lived in a car.", place="tape"),
               R("fuzz", "germanium · bias", "`fuzz`: germanium, with a dying battery.", place="fuzz"),
               R("vocoder", "eight bands · heavy",
                 "`vocoder`: eight bands, and heavy: alone, about half the SP-1.", place="vocoder"),
               R("choruslite", "one voice · cut down",
                 "And `choruslite`: one voice, at about a third of a `chorus`.", place="choruslite")],
         kind="places"),
    dict(code="sp13", title="spaces", sub="echoes, rooms and grains",
         desc="Things that hold a sound in time: delays, reverbs, freezes and grains. They share the scene's "
              "124 KB of memory.",
         rows=[R("delay", "nature × 50 ms",
                 "`delay`: the first night's second scene, `fader1` on its feedback.", hl=["f1"], place="delay"),
               R("reverb · reverblite", "a room · a small cheap room",
                 "`reverb`, and `reverblite`, a small cheap room at about half the cost.", place="reverb"),
               R("plate", "after dattorro", "`plate`: Dattorro's plate.", place="plate"),
               R("dub", "only while throw is up",
                 "`dub`: an echo send. Put `button1` on `dub.throw`, and throw it.", hl=["t1"], place="dub"),
               R("stutter", "beat repeat", "`stutter`: a beat repeat. Hold `button2` on its trig.", hl=["t2"],
                 place="stutter"),
               R("freeze", "hold loops the last moment", "`freeze`: hold loops the last moment; `play` makes a good "
                 "hold.", hl=["play"], place="freeze"),
               R("cavern · shimmer", "heavy: samplerate=half",
                 "`cavern` and `shimmer` are heavy on the SP-1: run them at samplerate half.", place="cavern"),
               R("grains", "the hi-fi granulator",
                 "`grains`: nature six is six tenths of a second of tape, nearly half the memory.", place="grains")],
         kind="places"),
    dict(code="cu14", title="currents", sub="slopes, sequencers and clocks",
         desc="Things that move other things: slopes and envelopes, sequencers and quantizers, clocks, "
              "randomness. Cheap enough to run at samplerate=eighth.",
         rows=[R("slope", "end into trig cycles", "`slope`: its end into its trig cycles, after Serge.",
                 place="slope"),
               R("dusg", "the serge dual universal slope", "`dusg`: two slopes: envelopes, LFOs, oscillators.",
                 place="dusg"),
               R("seq", "seq 8: 0 7 12 15", "`seq`: semitones. `rocker.up` into `seq.next` steps it by hand.",
                 hl=["rocker"], place="seq"),
               R("quantizer", "38 scales · and your own", "`quantizer`: thirty-eight scales, and your own.",
                 place="quantizer"),
               R("euclid · turing", "rhythms · a register that rewrites itself",
                 "`euclid` and `turing`: Euclidean rhythms, and a register that slowly rewrites itself.",
                 place="euclid"),
               R("lfo", "four shapes · fine at eighth", "`lfo`: four shapes; at samplerate eighth it loses almost "
                 "nothing.", place="lfo"),
               R("gesture", "record while held, then loop",
                 "`gesture`: hold `button4`, move `fader4`, and it loops what you did.", hl=["t4", "f4"],
                 place="gesture"),
               R("petals", "after blasser's quantussy", "`petals`: five cores in a ring, lurching, after Peter "
                 "Blasser.", place="petals")], kind="places"),
]

AFTER = [
    dict(code="sv15", title="saving the processor", sub="bare places, lite places, samplerate",
         desc="The SP-1 has to finish every block of sound in time. Keep a scene under about 85 %: bare and lite "
              "places, and samplerate=half on the heavy ones.",
         say="Saving the SP-1's processor. It has to finish every block of sound in time.",
         rows=[R("bare places", "saw · tri · pulse · white · follow · onepole",
                 "Bare places do one job: `saw`, `tri`, `pulse`, `white`, `follow`, `onepole`.", hl=[]),
               R("lite places", "cut-down versions of heavy ones",
                 "Lite places are cut down: `reverblite`, `choruslite`, `delaylite`, `fmlite`, `supersawlite`, "
                 "`froglet`.", hl=[]),
               R("a quarter · a sixth", "reverb · reverblite",
                 "By the editor's estimate, a dream of just a `reverb` needs a quarter of the SP-1; a `reverblite`, "
                 "a sixth.", hl=[]),
               R("samplerate=", "full · half · quarter · eighth",
                 "Samplerate half is about half the work. Pitches stay put; the sound gets darker, and delays "
                 "longer.", hl=[]),
               R("the heavy ones", "cavern · paulstretch · harmonizer",
                 "A `cavern` drops from three quarters to under a half. `paulstretch` fits at half, the "
                 "`harmonizer` only at quarter.", hl=[]),
               R("memory", "124 kb a scene",
                 "And memory is a hundred and twenty-four kilobytes a scene. A lower samplerate doesn't shrink it.",
                 hl=[])], kind="panel"),

    dict(code="mi16", title="midi and the room", sub="usb-midi in, and bluetooth as weather",
         desc="The SP-1 is a USB-MIDI device named Sleepcousonique. crowd and visitors hear the Bluetooth devices "
              "near the computer; on the SP-1 alone, the crowd holds its rest= count.",
         say="MIDI, and the room.",
         rows=[R("midi", "pitch · gate · velocity · trig",
                 "`midi` hears the SP-1's USB-MIDI port, or in the editor, the computer's: pitch, gate, velocity, "
                 "trig. Four of them play a chord.", hl=[]),
               R("midicc · midiclock", "a controller · the clock",
                 "`midicc` is one controller; `midiclock` gives clock, reset and run.", hl=[]),
               R("a keyboard", "a usb-midi host adapter",
                 "A keyboard with a TRS or DIN socket needs a USB-MIDI host adapter.", hl=[]),
               R("crowd · visitors", "the bluetooth devices nearby",
                 "`crowd` is how many Bluetooth devices are near the computer; `visitors` pulses as they come and "
                 "go. Nothing is paired, kept or sent.", hl=[]),
               R("rest=", "the sp-1 on its own",
                 "The SP-1 never uses Bluetooth, so alone, `crowd` holds its rest count: give it the room you "
                 "expect.", hl=[])], kind="panel"),

    dict(code="tl17", title="snapshots", sub="and the timeline, for composing", shot="both-journal-light.png",
         desc="A snapshot keeps what's sounding. The timeline cuts or glides between them. They live in the dream "
              "as notes the engine ignores, and play in the editor.",
         say="Snapshots and the timeline, for composing, after openprocess.",
         rows=[R("keep", "click an empty square",
                 "A snapshot keeps what's sounding: cords, values, knobs, faders and the scene. Click an empty "
                 "square.", box=(8, 636, 176, 58)),
               R("cut · glide", "click a kept one",
                 "Click a kept one: cut jumps there, glide gets there over the time beside it.",
                 box=(232, 636, 80, 32)),
               R("the lane", "drag a square onto it",
                 "Drag squares onto the lane; drag a segment's edge to set its length.", box=(330, 640, 1100, 48)),
               R("play", "the red flag · loop", "The red flag plays it, and loop goes round.",
                 box=(196, 636, 34, 34)),
               R("in the dream", ";~snap · ;~timeline",
                 "They're kept as notes at the end of the dream. They play in the editor; on the SP-1, use a lucid "
                 "group.", box=None)], kind="shot"),

    dict(code="sc18", title="scales and tunings", sub="the scales folder, written into the dream",
         desc="38 scales by heart, and any Scala file in Documents/Sleepcousonique/scales. The SP-1 has no files, "
              "so the tunings travel inside the dream.",
         say="Scales, and tunings.",
         rows=[R("38 scales", "just · pelog · slendro · partch …",
                 "The `quantizer` knows thirty-eight scales, from just major to pelog, slendro and Partch.", hl=[]),
               R("the scales folder", "documents › sleepcousonique › scales",
                 "And any Scala file in Documents, Sleepcousonique, scales, by its name: scale equals young W T P.",
                 hl=[]),
               R("it comes with", "young · 22 shrutis · 16 ragas · carlos",
                 "It comes with La Monte Young, the twenty-two shrutis, sixteen ragas, Wendy Carlos, Stockhausen and "
                 "Terry Riley.", hl=[]),
               R("tuning lines", "written into the dream",
                 "The SP-1 has no files, so when the editor plays or sends a dream, it writes the scales it names "
                 "into the dream, as tuning lines. You can write your own.", hl=[])], kind="panel"),

    dict(code="sd19", title="sending", sub="slots and scenes on the sp-1",
         desc="send to SP-1 (cmd-U) fills the slot that is playing; the Device menu sends to any of the five. "
              "•• + VOL steps through the slots, •• + the rocker through the scenes.",
         say="Sending, slots and scenes.",
         rows=[R("send to sp-1", "cmd-u · ctrl-u",
                 "Send to SP-1, command U: the SP-1 plays the dream, and keeps it in the slot that's playing.",
                 hl=["dots"]),
               R("device menu", "send to slot 1 … 5", "The Device menu sends to any of the five slots.", hl=[]),
               R("checked first", "mistakes · over 85 % · 16 kb",
                 "Mistakes aren't sent; a scene over eighty-five percent asks first; a slot keeps sixteen "
                 "kilobytes of text.", hl=[]),
               R("•• + vol", "slot 1 … 5 · empty: the first night",
                 "The dots and volume step through the slots; an empty one plays the first night.",
                 hl=["dots", "volp", "volm"], anim="slot"),
               R("•• + the rocker", "scene 1 … 16",
                 "The dots and the rocker step through the scenes.", hl=["dots", "rocker", "trackrow"], anim="scene"),
               R("scene", "next · back · miss",
                 "A dream can change scene itself: a pulse into `scene.next` or `scene.back`.", hl=[]),
               R("slot", "go · next · back · new in 1.5",
                 "New in one point five, `slot` goes to another dream: a pulse at `slot.go`, `slot.next` or "
                 "`slot.back`. Like scene, it ignores a scene's first tenth of a second.", hl=[])], kind="slots"),

    dict(code="mt20", title="the meters", sub="the estimate, and what the sp-1 measures", shot="both-faltung-dark.png",
         desc="LOAD is the editor's estimate for the scene; SP-1 CPU what the SP-1 reports every five seconds; "
              "ROOM whichever other limit is nearest.",
         say="The meters.",
         rows=[R("load", "the estimate · this scene",
                 "Load is the editor's estimate for the scene, fitted to SP-1 measurements: tight past eighty-five "
                 "percent, too heavy past a hundred.", box=(72, 842, 394, 57)),
               R("sp-1 cpu", "measured · peak and average",
                 "SP-1 CPU is what the SP-1 measures, every five seconds: peak, average, and dropped blocks.",
                 box=(470, 842, 394, 57)),
               R("room", "memory · places · cords · text",
                 "Room is the nearest other limit: memory, ninety-six places, three hundred and twenty cords, or "
                 "sixteen kilobytes of text.", box=(868, 842, 396, 57)),
               R("the same engine", "what fits, plays the same",
                 "What fits here plays the same on the SP-1: it's the same engine.", box=None)], kind="shot"),

    dict(code="th21", title="too heavy", sub="the last dream that fit",
         desc="New in 1.5: when a scene keeps dropping blocks, the SP-1 goes back to the last scene that played "
              "two seconds calmly, the dream kept in its slot, or the first night.",
         say="Too heavy. Since one point five, when a scene keeps dropping blocks for a second, the SP-1 goes back "
             "to what last fit.",
         rows=[R("1", "the last scene that fit",
                 "First, to the last scene that played two seconds without a dropped block.", hl=["trackrow"]),
               R("2", "the dream kept in the slot",
                 "Else, if the dream only came to try, to the one kept in its slot.", hl=[]),
               R("3", "the first night",
                 "Else, to the first night, which always fits. The editor's status line says which.", hl=[]),
               R("6.5 %", "an empty dream",
                 "An empty dream takes about six and a half percent; the first night's first scene, about half.",
                 hl=[]),
               R("the house is full", "memory, checked before it plays",
                 "Memory never stutters: a scene that doesn't fit is a mistake, the house is full.", hl=[])],
         kind="heavy"),

    dict(code="ts22", title="when it won't", sub="standby, boot mode, empty slots",
         desc="The bootloader is never touched: whatever happens, boot mode and the updater can bring the SP-1 "
              "back.",
         say="When it won't.",
         rows=[R("off (standby)", "hold •• to turn it on",
                 "SP-1 is off, standby: hold the dots to turn it on, and send again.", hl=["dots"], anim="standby"),
               R("no sp-1 plugged in", "not in boot mode?",
                 "No SP-1 is plugged in: if only T1 is lit, it's in boot mode. Plug in again holding nothing.",
                 hl=["t1"], anim="boot"),
               R("in use", "one program at a time",
                 "In use: quit the serial monitor or the second editor. Use a data cable, not a hub.", hl=[]),
               R("empty slots", "after a reflash",
                 "After a reflash the slots may be empty: your dream files are the real copy.", hl=[]),
               R("30 s", "the dots always win",
                 "Holding the dots thirty seconds always turns it off.", hl=["dots"], anim="poweroff"),
               R("always back", "the bootloader is never touched",
                 "And the bootloader is never touched, so boot mode can always bring it back.", hl=["t1", "t4"])],
         kind="steps"),

    dict(code="sq∞", title="sleep well", sub="sleepcousonique · on the sp-1",
         desc="Thanks to Peter Blasser, to Worldwave for Wakes, to Teenage Engineering for the SP-1, and to Buchla, "
              "Serge and Clavia.",
         say="That's Sleepcousonique on the SP-1. Thanks to Peter Blasser; to Worldwave, for Wakes; to Teenage "
             "Engineering for the SP-1; and to Buchla, Serge and Clavia. Now write down what you dreamt, send it, "
             "and unplug.",
         rows=[R("places", "over 200", hl=[]), R("examples", "first night · a history of modular", hl=[]),
               R("licence", "mit · wakes mit", hl=[])], kind="outro"),
]
