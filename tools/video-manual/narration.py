"""What the narrator says in the Sleepcousonique video manual (the SP-1 and the Wingie), chapter
by chapter.

Every chapter is a dict: code, title, sub, desc (on screen), say (spoken first), rows, part (which
part of the film: its hue), yt (its name in the YouTube chapter list), and either a panel ("sp1",
"wingie" or "both": the drawing in the middle) or a shot (a screenshot, with an optional crop
x, y, w, h). A row is R(label, text on screen, spoken or None, highlight keys, box, anim, place).
Highlight keys are the drawing's controls; with both instruments drawn they say whose: s:dots,
w:mode1. A box (x, y, w, h) is on the screenshot (1440 x 900 pixels, measured on the editor's own
--shot images). `anim` is what the instrument's lights do while the row is spoken.

Anything in `backticks` is a place, a hand or a place.port: build_video.py checks every one against
the engine's catalog, for the instrument the chapter is about, before anything is spoken.
Backticks are not spoken.

The facts are 1.5.0's: README.md, docs/RELEASE.md, docs/language.md, docs/manual, docs/wingie-manual,
FLASHING.md and the examples. Loads quoted are the editor's estimate (scqtool), which is fitted to
measurements on an SP-1 and on a Wingie.
"""


def R(label, text, spoken=None, hl=(), box=None, anim=None, place=None):
    return dict(label=label, text=text, spoken=spoken, hl=list(hl), box=box, anim=anim, place=place)


SF = ["f1", "f2", "f3", "f4"]
ST = ["t1", "t2", "t3", "t4"]
WF = ["f1", "f2", "f3"]

# The editor's screenshots, made with its own --shot (--no-link --no-radio):
# name: (dream in examples/, view 0 journal / 1 both / 2 tapestry, theme)
SHOTS = {
    "both-journal-light.png": ("first-night.dream", 1, "journal,light"),
    "both-journal-dark.png": ("first-night.dream", 1, "journal,dark"),
    "both-faltung-dark.png": ("first-night.dream", 1, "openprocess,dark"),
    "journal-light.png": ("first-night.dream", 0, "journal,light"),
    "tapestry-journal-light.png": ("time-sound-system.dream", 2, "journal,light"),
    "w-both-journal-dark.png": ("wingie-first-light.dream", 1, "journal,dark"),
    "w-glass-faltung-dark.png": ("wingie-proc-glass-cloud.dream", 2, "openprocess,dark"),
    "w-prism-faltung-light.png": ("wingie-proc-prism.dream", 2, "openprocess,light"),
    "w-dub-journal-light.png": ("wingie-proc-dub-time.dream", 1, "journal,light"),
    "w-everything-modular.png": ("wingie-proc-everything.dream", 2, "modular,dark"),
}

CHAPTERS = [
    # ------------------------------------------------------------------ i · the dream
    dict(code="sq00", part=0, panel="both", kind="intro", yt="Sleepcousonique: one editor, two instruments",
         title="sleepcousonique", sub="the video manual · the sp-1 and the wingie · 1.5.0",
         desc="A patch language written like a dream journal, for Teenage Engineering's SP-1 and Meng Qi's "
              "Wingie2. One editor, one language, two instruments.",
         say="Sleepcousonique. A patch language written like a dream journal, for two instruments: Teenage "
             "Engineering's SP-1, and Meng Qi's Wingie2. This is the video manual for version one point five: "
             "one editor, one language, and both instruments, from the first line of a dream to the last place "
             "in the processing chain.",
         rows=[R("places", "239"), R("sp-1", "5 slots · 124 kb a scene"), R("wingie", "12 slots · 70 kb a scene"),
               R("scenes", "16 a dream"), R("version", "1.5.0")]),

    dict(code="ov01", part=0, panel="both", kind="panel", yt="Dreams, places, passages and hands",
         title="a dream journal", sub="dreams, places, passages and hands",
         desc="A patch is a dream, its modules are places, the cords between them are passages, and the "
              "instrument's controls are hands. Nothing on either panel is fixed.",
         say="First, the idea.",
         rows=[R("dream", "a patch, as a journal entry",
                 "Each patch is a dream, written as a dated journal entry, in plain text. You can keep one in a "
                 "notes app, or paste it from an email."),
               R("places", "239 modules, named for what they are",
                 "Its modules are places: two hundred and thirty-nine of them, from oscillators, filters and "
                 "sequencers to granular clouds, spectral freezes and dub echoes."),
               R("passages", "cords with parts soldered in",
                 "The cords between them are passages: after Peter Blasser's Nabra, cords with parts soldered "
                 "into them."),
               R("hands", "the instrument's own controls",
                 "The instrument's faders, buttons and keys are hands. Nothing on either panel is fixed: the "
                 "dream says what every control does.", hl=["s:f1", "w:f1"]),
               R("the sp-1", "4 faders · 4 buttons · rocker · play · ••",
                 "On the SP-1: four faders, four track buttons, the rocker, PLAY, the dots, and eight lights.",
                 hl=["s:f1", "s:f2", "s:f3", "s:f4"]),
               R("the wingie", "mic or line in · 2 keyboards · 3 faders",
                 "On the Wingie: two microphones or a line input, two keyboards, two Mode buttons, two octave "
                 "switches, three faders and two lights.", hl=["w:keys2", "w:keys1", "w:mic_l"]),
               R("one engine", "the computer plays what the instrument plays",
                 "And one engine, the same code on the computer and on both instruments, so what you hear in "
                 "the editor is what the instrument plays.")]),

    # ------------------------------------------------------------------ ii · the editor
    dict(code="in02", part=1, shot="both-journal-light.png", kind="shot", yt="Installing the editor",
         title="install the editor", sub="macos and windows",
         desc="From the Homebrew tap, or the zip on the releases page. Each zip holds the editor, both "
              "firmwares, both manuals and the example dreams.",
         say="Installing the editor.",
         rows=[R("homebrew", "brew install --cask sleepcousonique",
                 "On a Mac, the easiest way is the Homebrew tap: tap it once, then brew install, cask, "
                 "sleepcousonique. The tap's name is on the project's page."),
               R("releases", "sleepcousonique-macos.zip · -windows.zip",
                 "Or download the zip for macOS or for Windows from the releases page on GitHub. The Mac app "
                 "isn't notarised, so the first time, right-click it and choose Open."),
               R("in the zip", "the editor · both firmwares · both manuals",
                 "Each zip holds the editor, both firmwares, the SP-1's and the Wingie's, both manuals, and the "
                 "example dreams."),
               R("first night", "it plays at once",
                 "Open it, and the first night plays at once, on the computer: an example dream with three "
                 "scenes.", box=(0, 80, 548, 550)),
               R("sound", "the audio device",
                 "The sound button, top right, picks the audio device.", box=(1360, 8, 72, 30)),
               R("sp-1 | wingie", "the instrument switch",
                 "And this switch, or the Device menu, says which instrument the dream is for.",
                 box=(148, 8, 124, 30))]),

    dict(code="ed03", part=1, shot="both-journal-light.png", kind="shot", yt="The journal and the tapestry",
         title="journal and tapestry", sub="one dream, two ways",
         desc="Plain text on the left, boards and cords on the right. Whatever you do on one shows up on the "
              "other.",
         say="The editor shows one dream two ways.",
         rows=[R("journal", "the dream as text",
                 "On the left, the journal: the dream as text. Half a second after you stop typing, the sound "
                 "follows.", box=(0, 80, 548, 550)),
               R("tapestry", "the same dream as boards",
                 "On the right, the tapestry: the same dream, as boards and cords. Every edit there is an edit "
                 "to the text.", box=(550, 80, 890, 550)),
               R("journal · both · tapestry", "what fills the window",
                 "These buttons choose what fills the window.", box=(760, 8, 252, 30)),
               R("the hands board", "always there",
                 "The instrument's hands are a grey board that is always there; its outputs, lights and scene "
                 "changes another.", box=(594, 124, 94, 204)),
               R("scenes", "a button each · + scene",
                 "Along the top, a button for every scene, and plus scene, which adds one dated today.",
                 box=(10, 50, 414, 28)),
               R("the bottom", "the hands · the patch map · the meters",
                 "And along the bottom, the instrument itself, what your hands do in this scene, and the "
                 "meters.", box=(0, 700, 1440, 176))]),

    dict(code="lk04", part=1, shot="both-faltung-dark.png", kind="shot", yt="The looks: journal, faltung, modular",
         title="the looks", sub="journal, faltung, modular",
         desc="View > Look: journal (the original), faltung (the openprocess layout) or modular (after the "
              "patch editors of 2004). Light, dark, or follow the computer.",
         say="View, Look: journal, the original; faltung; or modular, after the patch editors of two thousand "
             "and four. Each one light or dark, or following the computer.",
         rows=[R("faltung", "the openprocess layout",
                 "This is faltung, dark: places as folded blocks in their family's colour, and thin coloured "
                 "cables.", box=(546, 164, 630, 456)),
               R("the browser", "a tile for every place",
                 "Along the top, a tile for every place, by family. Hover to read one, click to add it. It lists "
                 "only what the chosen instrument has.", box=(212, 8, 968, 80)),
               R("find a place", "by name or by what it does",
                 "Find a place searches by name, or by what it does; return adds the first.",
                 box=(104, 10, 102, 40)),
               R("settings", "the selected place",
                 "Select a place, and its settings appear on the right: drag a bar, and it plays as you drag.",
                 box=(1188, 196, 250, 424)),
               R("the same dream", "in every look",
                 "Whatever the look, it's the same dream, and the same text underneath.")]),

    # ------------------------------------------------------------------ iii · writing a dream
    dict(code="wr05", part=2, shot="journal-light.png", crop=(0, 76, 810, 290), kind="shot",
         yt="Writing a dream: notes, scenes, names, cords",
         title="writing a dream", sub="notes, scenes, the prelude and names",
         desc="A dream is plain text: write it in any editor, keep it in a notes app, paste it from an email.",
         say="Writing a dream. Here is the first night, line by line.",
         rows=[R("; notes", "after a semicolon",
                 "Anything after a semicolon is a note to yourself.", box=(30, 79, 768, 43)),
               R("drift 3", "natures wander 3 %",
                 "Drift three lets tuned places wander up to three percent each night, so the same dream is "
                 "never quite the same twice.", box=(30, 122, 64, 16)),
               R("the prelude", "lines before the first scene",
                 "Lines before the first scene are the prelude, which every scene shares.", box=(30, 79, 768, 59)),
               R("a scene", "a line with dashes under it",
                 "A line with dashes under it is a scene, usually a date.", box=(30, 149, 84, 30)),
               R("naming", "the name is place nature",
                 "The window is `lpg`; the dice are `steprandom` two: a name, a place, its nature. Inside your "
                 "dream, your name wins, so new places can never break an old dream.", box=(30, 191, 200, 73)),
               R("cords", "stairs.end -> stairs.trig",
                 "Then the cords: stairs end into stairs trig, and the staircase climbs forever.",
                 box=(30, 275, 568, 86)),
               R("mistakes", "the last dream that made sense",
                 "A word one letter off is understood, with a note. And while there are mistakes, the last dream "
                 "that made sense keeps playing.")]),

    dict(code="ps06", part=2, shot="tapestry-journal-light.png", kind="shot", yt="Passages: hall, fog, water, door, maybe",
         title="passages", sub="cords with parts soldered in",
         desc="After Peter Blasser's Nabra: a wire, a resistor, a capacitor, a diode, or chance. A number inside "
              "the arrow sets the amount.",
         say="Passages: cords with parts soldered in.",
         rows=[R("->", "hall · a plain wire", "Arrow is a hall: a plain wire.", box=(286, 9, 88, 28)),
               R("~>", "fog · a resistor", "Tilde arrow is fog: a resistor, half as loud and softened.",
                 box=(378, 9, 88, 28)),
               R("=>", "water · a capacitor", "Equals arrow is water: a capacitor, slow to fill and drain.",
                 box=(470, 9, 88, 28)),
               R(">|", "door · a diode", "Arrow bar is a door: a diode, only the positive half.",
                 box=(562, 9, 88, 28)),
               R("?>", "maybe · by chance",
                 "Question arrow is maybe: through by chance. A number inside any arrow sets the amount: tilde "
                 "seventy arrow is seventy percent.", box=(654, 9, 88, 28)),
               R("a dot", "an inlet or an outlet",
                 "A dot picks an inlet or outlet; cords into one inlet add up, like stacked bananas.",
                 box=(1234, 186, 76, 152)),
               R("cables", "braided · thin · beaded",
                 "Audio leads are fat and braided, currents thin, pulses beaded. Drag from outlet to inlet to "
                 "patch.", box=(196, 180, 770, 270))]),

    dict(code="sg07", part=2, panel="sp1", kind="panel", yt="Signals: sound, current, pulse, pitch",
         title="signals", sub="sound, current, pulse and pitch",
         desc="Three kinds of signal, as on a Buchla; any of them into any inlet, as on a Serge. Switches are "
              "fixed for the dream, written after the nature.",
         say="Signals: three kinds, as on a Buchla, and any of them into any inlet, as on a Serge.",
         rows=[R("sound", "about −1 to 1 · umber", "Sound swings about minus one to one, drawn umber."),
               R("current", "about 0 to 1 · deep blue",
                 "Current sits about nought to one, drawn deep blue: the faders are currents.", hl=SF),
               R("pulse", "0 or 1 · gold",
                 "A pulse is nought or one, drawn gold, like a held track button.", hl=ST),
               R("pitch", "octaves · from the 60 hz hum",
                 "Pitch counts octaves, from the sixty hertz hum: sine three is a hundred and eighty hertz."),
               R("switches", "range=audio · scale=pelog",
                 "Switches and params are fixed for the dream, written after the nature, like range equals "
                 "audio. A position word alone picks it.")]),

    dict(code="kn08", part=2, shot="tapestry-journal-light.png", kind="shot", yt="Knobs, tune and attenuverters",
         title="knobs", sub="tune, attenuverters and lucid groups",
         desc="Every place has the knobs it would have on an analog modular. A knob is the inlet's own value; "
              "what you patch in adds to it, through the attenuverter.",
         say="Knobs: every place has the ones it would have on an analog modular.",
         rows=[R("panel knobs", "time= · feedback= · tone=",
                 "A knob is the inlet's own value, written after the place with the inlet's name: delay nine, "
                 "time equals nought point four. Whatever you patch in adds to it.", box=(1018, 218, 28, 56)),
               R("tune", "the base pitch · in octaves",
                 "Every place with a pitch inlet has a tune knob: its base pitch, twelve octaves either way.",
                 box=(670, 150, 28, 38)),
               R("attenuverters", "att.inlet · −2 to 2",
                 "And every source has an attenuverter on each inlet: nought shuts the cord out, negative turns "
                 "it over. On pitch it goes to ten, so one fader sweeps ten octaves.", box=(603, 186, 20, 66)),
               R("on the tapestry", "drag · shift for fine · double-click",
                 "Drag a knob, shift for fine, double-click to put it back. The journal follows.",
                 box=(514, 140, 192, 124)),
               R("lucid groups", "one fader · many inlets",
                 "A lucid group leans one fader on many inlets, each with its own range, like the Nord Modular's "
                 "morph.")]),

    dict(code="un09", part=2, panel="both", kind="panel", yt="Pitch units, samplerate and resolution",
         title="units and rates", sub="7st · samplerate= · resolution=",
         desc="Pitch in semitones, cents or octaves; any place at half, a quarter or an eighth of the rate; "
              "and audio-rate control where you hear the steps.",
         say="Three things new or newly everywhere in one point five: pitch units, samplerate on every place, "
             "and resolution.",
         rows=[R("pitch units", "7st · -50ct · 1oct",
                 "A number can carry a pitch unit wherever a pitch goes: seven S T is seven semitones, minus "
                 "fifty C T is fifty cents down, one O C T is an octave."),
               R("in a seq", "seq 4: 0 7st 1oct",
                 "In a sequence, bare steps stay semitones, and a unit can go on any step. Tune equals minus "
                 "five S T works too."),
               R("samplerate=", "full · half · quarter · eighth",
                 "Samplerate runs any place at a lower rate: full, half, quarter or eighth. Half the rate is "
                 "about half the work, on both instruments."),
               R("what it does", "darker · grittier · longer",
                 "Pitches stay put; the sound gets darker and grittier, and delays longer. Currents, like "
                 "L.F.O.s and envelopes, lose almost nothing, even at eighth."),
               R("memory", "half the rate · twice the seconds",
                 "And a tape at half rate holds twice the seconds in the same memory."),
               R("resolution=", "fine · audio",
                 "Places read their controls once a block, every thirty-two samples. Resolution fine reads them "
                 "every four samples, audio every sample: true audio-rate F M, at eight or thirty-two times "
                 "that work."),
               R("not both", "resolution needs full rate",
                 "Resolution only works at full rate: with a lower samplerate the check warns, and ignores it.")]),

    dict(code="sc10", part=2, panel="both", kind="panel", yt="Scenes, and the slot place",
         title="scenes and slots", sub="inside a dream, and between dreams",
         desc="Up to 16 scenes a dream, each starting fresh. scene and slot let a dream move itself; both "
              "ignore pulses in a scene's first 0.1 s.",
         say="Scenes, and slots.",
         rows=[R("a scene", "16 a dream · each starts fresh",
                 "A dream can have sixteen scenes. Each starts fresh, with its own places and its own memory."),
               R("on the panel", "•• + rocker · a mode button and a tap",
                 "You step through them on the instrument: on the SP-1 with the dots and the rocker, on the "
                 "Wingie with one Mode button held and a tap on the other.", hl=["s:dots", "s:rocker", "w:mode1", "w:mode2"]),
               R("scene", "scene.next · scene.back · scene.miss",
                 "A dream can change scene itself: a pulse into `scene.next` or `scene.back`. `scene.miss` is "
                 "the chance of landing on a random one."),
               R("slot", "slot 7 · go · next · back",
                 "New in one point five, the `slot` place goes to another dream. A slot named seven, and a pulse "
                 "at go plays slot seven; next and back step through them. The SP-1 has five slots, the Wingie "
                 "twelve."),
               R("0.1 s", "pulses ignored as a scene starts",
                 "Scene and slot both ignore pulses in a scene's first tenth of a second, because a clock pulses "
                 "as it starts. So one pulse can never skip two scenes."),
               R("in the editor", "no slots: the status line says",
                 "The editor has no slots, so its status line says what the instrument would do.")]),

    dict(code="mr11", part=2, panel="both", kind="panel", yt="Scales, MIDI and the room",
         title="scales, midi, the room", sub="tunings in the dream, midi in, bluetooth as weather",
         desc="38 scales and any Scala file, written into the dream as it's sent; MIDI to CV on both instruments; "
              "and crowd and visitors, the Bluetooth devices near the computer.",
         say="Scales, MIDI, and the room.",
         rows=[R("38 scales", "just · pelog · slendro · partch …",
                 "The `quantizer` knows thirty-eight scales, from just major to pelog, slendro and Partch."),
               R("the scales folder", "documents › sleepcousonique › scales",
                 "And any Scala file in the scales folder in Documents, by its name. It comes with La Monte "
                 "Young, the twenty-two shrutis, sixteen ragas, Wendy Carlos, Stockhausen and Terry Riley."),
               R("tuning lines", "they travel inside the dream",
                 "Neither instrument has files, so when the editor sends a dream it writes the scales it names "
                 "into the dream, as tuning lines."),
               R("midi", "pitch · gate · velocity · trig",
                 "`midi` is MIDI to CV: pitch, gate, velocity and trig. `midicc` is one controller, `midiclock` "
                 "the clock."),
               R("where it comes in", "sp-1: usb-midi · wingie: trs midi in",
                 "The SP-1 hears its U.S.B. MIDI port; the Wingie its T.R.S. MIDI jack, on the edge.",
                 hl=["w:jacks"]),
               R("crowd · visitors", "the bluetooth devices nearby",
                 "`crowd` is how many Bluetooth devices are near the computer; `visitors` pulses as they come "
                 "and go. Nothing is paired, kept or sent."),
               R("rest=", "on the instrument alone",
                 "Neither instrument listens to Bluetooth, so there `crowd` holds its rest count: give it the "
                 "room you expect.")]),

    dict(code="tl12", part=2, shot="both-journal-light.png", kind="shot", yt="Snapshots and the timeline",
         title="snapshots", sub="and the timeline, for composing",
         desc="A snapshot keeps what's sounding. The timeline cuts or glides between them. They live in the "
              "dream as notes the engine ignores, and play in the editor.",
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
               R("in the dream", "notes at the end",
                 "They're kept as notes at the end of the dream. They play in the editor; on an instrument, use "
                 "a lucid group or scenes.")]),

    # ------------------------------------------------------------------ iv · the sp-1
    dict(code="sp13", part=3, panel="sp1", kind="panel", yt="The SP-1: its hands",
         title="the sp-1's hands", sub="every fader and button is yours",
         desc="Nothing on the panel is fixed: the dream says what each fader, button and light does. Only the "
              "volume buttons stay the SP-1's own.",
         say="The SP-1. Nothing on its panel is fixed: the dream says what each control does.",
         rows=[R("fader1 … fader4", "the four faders · 0 to 1",
                 "The four faders are `fader1` to `fader4`, from nought at the bottom to one at the top.", hl=SF),
               R("button1 … button4", "the track buttons t1 to t4",
                 "The track buttons, T1 to T4, are `button1` to `button4`, one while held. The SP-1 reads one at "
                 "a time: a chord of them is ignored.", hl=ST),
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
                 "into `leds` lights both rows as a bar.", hl=["playrow"], anim="bar")]),

    dict(code="fl14", part=3, panel="sp1", kind="steps", yt="The SP-1: flashing is not sending",
         title="flashing", sub="is not sending",
         desc="Flash the firmware once, in the browser, in boot mode (T1 + T4). Send dreams every time, from the "
              "editor, with the SP-1 switched on normally.",
         say="Flashing, and sending: two different jobs.",
         rows=[R("flash · once", "the firmware · chrome or edge",
                 "Flashing installs the firmware, once, and again only to update it: in Chrome or Edge, on "
                 "solderless dot engineering. One point five needs a fresh flash."),
               R("boot mode", "hold t1 + t4 · plug in usb",
                 "Turn the SP-1 off and unplug it. Hold T1 and T4 and plug in the cable. Let go when T1 lights.",
                 hl=["t1", "t4"], anim="boot"),
               R("the updater", "custom firmware · sleepcousonique-sp1.bin",
                 "In the updater, choose custom firmware, pick sleepcousonique S P 1 dot bin, connect, and "
                 "flash.", anim="boot"),
               R("standby", "dark · charging · silent",
                 "It restarts switched off, in standby: charging, and silent. That is normal, not a failed flash.",
                 hl=["playrow"], anim="standby"),
               R("hold ••", "about half a second · on",
                 "Hold the dots for about half a second to turn it on. It plays the first night.",
                 hl=["dots"], anim="poweron"),
               R("send · every time", "plugged in normally · no buttons held",
                 "Sending puts a dream on it, every time you change one: plug it in with no buttons held, turn it "
                 "on, and press send to SP-1, command U. Sending never needs boot mode, and the editor never "
                 "flashes.", hl=["dots"]),
               R("t1 + t4: not to send", "boot mode hides it from the editor",
                 "In boot mode the editor can't see the SP-1 at all. If only T1 is lit, unplug, plug in again "
                 "holding nothing, and hold the dots.", hl=["t1", "t4"], anim="boot")]),

    dict(code="dt15", part=3, panel="sp1", kind="combos", yt="The SP-1: the dots, on, off and the shift",
         title="the dots", sub="on, off, and the shift",
         desc="Hold •• half a second: on. Hold it 1.5 s, touching nothing else: off. With the rocker, the volume "
              "buttons or PLAY, it reaches scenes, slots and interface mode.",
         say="The dots turn the SP-1 on and off, and, held with another control, reach its own business.",
         rows=[R("hold •• 0.5 s", "on · the track row fills",
                 "Hold them half a second to turn it on: the track row fills, and it plays the dream it played "
                 "last.", hl=["dots"], anim="poweron"),
               R("hold •• 1.5 s", "off · touching nothing else",
                 "Hold them a second and a half, touching nothing else, to turn it off. Touch another control and "
                 "it stays on, so gestures with the dots are safe.", hl=["dots"], anim="poweroff"),
               R("tap ••", "the battery · on the play row",
                 "Tap them, and the play row shows the battery.", hl=["dots", "playrow"], anim="battery"),
               R("•• + ffwd / rwd", "next · previous scene",
                 "The dots and the rocker: the next or previous scene, shown on the track row. The rocker never "
                 "reaches the dream then.", hl=["dots", "rocker", "trackrow"], anim="scene"),
               R("•• + vol + / −", "next · previous slot",
                 "The dots and volume up or down: the next or previous of the five dream slots.",
                 hl=["dots", "volp", "volm"], anim="slot"),
               R("•• + play 1 s", "interface mode · a usb sound card",
                 "Hold the dots and PLAY for a second, and the SP-1 becomes a two-channel U.S.B. sound card. Hold "
                 "them again, or send it a dream, and it dreams again.", hl=["dots", "play"], anim="iface")]),

    dict(code="hs16", part=3, shot="both-journal-dark.png", crop=(0, 700, 900, 176), kind="shot",
         yt="The SP-1 on screen", title="the sp-1 on screen", sub="play it from the computer",
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
                 "and an arrow changes scene."),
               R("the patch map", "what your hands do in this scene",
                 "On the right, what your hands do in this scene. Click a row to find it in the journal.",
                 box=(670, 710, 230, 122)),
               R("sp-1 live", "the panel follows it",
                 "With an SP-1 plugged in and on, the panel follows it live, scene changes too.",
                 box=(8, 702, 590, 168))]),

    dict(code="sd17", part=3, panel="sp1", kind="slots", yt="The SP-1: sending, slots and scenes",
         title="sending", sub="five slots on the sp-1",
         desc="send to SP-1 (cmd-U) fills the slot that is playing; the Device menu sends to any of the five. "
              "•• + VOL steps through the slots, •• + the rocker through the scenes.",
         say="Sending, slots and scenes on the SP-1.",
         rows=[R("send to sp-1", "cmd-u · ctrl-u",
                 "Send to SP-1, command U, or control U on Windows: the SP-1 plays the dream at once, and keeps it "
                 "in the slot that's playing.", hl=["dots"]),
               R("device menu", "send to slot 1 … 5", "The Device menu sends to any of the five slots."),
               R("checked first", "mistakes · over 85 % · 16 kb",
                 "Mistakes are never sent; a scene over eighty-five percent asks first; a slot keeps sixteen "
                 "kilobytes of text."),
               R("•• + vol", "slot 1 … 5 · empty: the first night",
                 "The dots and volume step through the slots; an empty one plays the first night.",
                 hl=["dots", "volp", "volm"], anim="slot"),
               R("•• + the rocker", "scene 1 … 16",
                 "The dots and the rocker step through the scenes.", hl=["dots", "rocker", "trackrow"], anim="scene"),
               R("the other way round", "a wingie dream on an sp-1",
                 "A dream for the Wingie isn't sent to an SP-1: the editor says so, and the SP-1 refuses it.")]),

    dict(code="mt18", part=3, shot="both-faltung-dark.png", kind="shot", yt="The SP-1: load, CPU and room meters",
         title="the sp-1's meters", sub="the estimate, and what the sp-1 measures",
         desc="LOAD is the editor's estimate for the scene; SP-1 CPU what the SP-1 reports every five seconds; "
              "ROOM whichever other limit is nearest.",
         say="The SP-1's meters.",
         rows=[R("load", "the estimate · this scene",
                 "Load is the editor's estimate for the scene: tight past eighty-five percent, too heavy past a "
                 "hundred. In one point five it was measured again, place by place, on five hundred and "
                 "twenty-two patches: it's off by about two and a half points.", box=(72, 842, 394, 57)),
               R("sp-1 cpu", "measured · peak and average",
                 "SP-1 CPU is what the SP-1 measures, every five seconds: peak, average, and dropped blocks.",
                 box=(470, 842, 394, 57)),
               R("room", "memory · places · cords · text",
                 "Room is the nearest other limit: a hundred and twenty-four kilobytes of memory, ninety-six "
                 "places, three hundred and twenty cords, or sixteen kilobytes of text.", box=(868, 842, 396, 57)),
               R("the same engine", "what fits, plays the same",
                 "What fits here plays the same on the SP-1: it's the same engine.")]),

    dict(code="sv19", part=3, panel="sp1", kind="panel", yt="The SP-1: saving the processor",
         title="saving the processor", sub="bare places, lite places, samplerate",
         desc="The SP-1 has to finish every block of sound in time. Keep a scene under about 85 %: bare and lite "
              "places, and samplerate=half on the heavy ones.",
         say="Saving the SP-1's processor. It has to finish every block of sound in time.",
         rows=[R("bare places", "saw · tri · pulse · white · follow · onepole",
                 "Bare places do one job: `saw`, `tri`, `pulse`, `white`, `follow`, `onepole`."),
               R("lite places", "cut-down versions of heavy ones",
                 "Lite places are cut down: `reverblite`, `choruslite`, `delaylite`, `fmlite`, `supersawlite`, "
                 "`froglet`."),
               R("a reverb", "a quarter · a lite one, a sixth",
                 "By the estimate, a dream of just a `reverb` needs about a quarter of the SP-1; a `reverblite`, "
                 "about a sixth."),
               R("the heavy ones", "cavern: 115 % → 65 % at half",
                 "A `cavern` alone is too heavy at full rate. At samplerate half it takes about two thirds."),
               R("the first night", "a half to three fifths a scene",
                 "The first night's three scenes take between a half and three fifths."),
               R("memory", "124 kb a scene",
                 "And memory is a hundred and twenty-four kilobytes a scene.")]),

    dict(code="th20", part=3, panel="sp1", kind="heavy", yt="The SP-1: too heavy goes back",
         title="too heavy", sub="the last dream that fit, on the sp-1",
         desc="New in 1.5: when a scene keeps dropping blocks, the SP-1 goes back to the last scene that played "
              "two seconds calmly, the dream kept in its slot, or the first night.",
         say="Too heavy. Since one point five, a dream that's too heavy for the SP-1 no longer stutters: it goes "
             "back to what last fit.",
         rows=[R("1", "the last scene that fit",
                 "First, to the last scene that played two seconds without a dropped block.", hl=["trackrow"]),
               R("2", "the dream kept in the slot",
                 "Else, if the heavy dream only came to try, to the one kept in its slot."),
               R("3", "the first night",
                 "Else, to the first night, which always fits. The editor's status line says which."),
               R("6.5 %", "an empty dream",
                 "An empty dream takes about six and a half percent of the SP-1."),
               R("the house is full", "memory, checked before it plays",
                 "Memory never stutters: a scene that doesn't fit is a mistake, the house is full, and is never "
                 "sent.")]),

    # ------------------------------------------------------------------ v · the wingie
    dict(code="wp21", part=4, panel="wingie", kind="panel", yt="The Wingie: its panel",
         title="the wingie's panel", sub="every key, switch and fader is a hand",
         desc="Same panel, same jacks. Nothing is fixed any more: the dream says what each hand does.",
         say="The Wingie. Same panel, same jacks, but nothing is fixed any more. Let's go round it.",
         rows=[R("mic · line", "two microphones · or line in",
                 "Top left and right, the two microphones. In a dream the input is `mic.left`, `mic.right` and "
                 "`mic.both`, or by its other name, `line`.", hl=["mic_l", "mic_r"]),
               R("line in · mic", "the switch picks the input",
                 "Beside the left grille, the Line In, Mic switch: up for the line input, down for the "
                 "microphones. `source` reads it.", hl=["source"]),
               R("octave1 · octave2", "l.oct · r.oct · 0 · 0.5 · 1",
                 "Below it, the two octave switches, `octave1` and `octave2`: free three-way controls.",
                 hl=["oct1", "oct2"]),
               R("mode1 · mode2", "l.mode · r.mode · held · tap",
                 "The round Mode buttons are `mode1` and `mode2`: one while held, and a tap pulse on a quick "
                 "press.", hl=["mode1", "mode2"]),
               R("fader1 2 3", "mix · decay · volume",
                 "The faders are labelled Mix, Decay and Volume, and in a dream they're `fader1`, `fader2` and "
                 "`fader3`, from nought to one. Nothing is fixed to them: Volume is only a volume if your dream "
                 "makes it one. So start with your headphones low.", hl=WF),
               R("lights", "red · purple · yellow · white",
                 "Beside them, the two lights: `lights.left` and `lights.right`, red at rest, then purple, "
                 "yellow and white as what comes in climbs.", hl=["led1", "led2"]),
               R("keys1", "the bottom keyboard · c to b",
                 "The bottom keyboard is `keys1`: the pitch of the last key in octaves, a gate, a trigger, and "
                 "which key.", hl=["keys1"]),
               R("keys2", "the top keyboard · an octave up",
                 "The top keyboard is `keys2`, an octave higher. One key alone is `key1` to `key24`: one to "
                 "twelve along the bottom, thirteen to twenty-four along the top.", hl=["keys2"]),
               R("the edge", "usb-c · midi in · line in · out",
                 "And on the edge: U.S.B. C, the T.R.S. MIDI input, line in, and the output.", hl=["jacks"])]),

    dict(code="wf22", part=4, panel="wingie", kind="steps", yt="The Wingie: flashing, and going back to stock",
         title="flashing the wingie", sub="and going back to stock",
         desc="Quit the editor. Plug in the Wingie. Write sleepcousonique-wingie.bin at address 0 with esptool, "
              "or run tools/flash_wingie.sh. Meng Qi's web flasher puts stock back.",
         say="Flashing the Wingie.",
         rows=[R("1", "quit the editor · plug in usb-c",
                 "Quit the editor, and anything else that holds the Wingie's serial port. Plug the Wingie in with "
                 "a U.S.B. C cable that carries data.", hl=["jacks"]),
               R("2", "esptool · 0x0 · sleepcousonique-wingie.bin",
                 "Write sleepcousonique Wingie dot bin at address zero with esptool. It's one image: bootloader, "
                 "partition table and program. It takes under a minute."),
               R("or", "tools/flash_wingie.sh",
                 "From the repository, tools, flash Wingie dot S H does the same, and finds the port by itself."),
               R("3", "first light",
                 "The Wingie restarts, fades in, and plays first light: the microphones ring two sets of "
                 "sympathetic strings, tuned by the two keyboards.", hl=["mic_l", "mic_r", "keys2", "keys1"]),
               R("your settings", "note your stock settings first",
                 "The image covers the place where the stock firmware keeps its settings. If you changed your "
                 "caves or tunings, note them down first."),
               R("back to stock", "meng qi's web flasher",
                 "To go back, open Meng Qi's web flasher in Chrome or Edge, connect the Wingie, and install. "
                 "Nothing can break: the bootloader in the chip's ROM is never written."),
               R("gplv3", "built on meng qi's wingie2 code",
                 "The Wingie firmware is built on Meng Qi's own Wingie2 code, so it is G P L version three.")]),

    dict(code="ws23", part=4, panel="wingie", kind="slots", yt="The Wingie: twelve slots and scenes",
         title="slots and scenes", sub="both mode buttons and a bottom key",
         desc="Hold both Mode buttons and press a key on the bottom keyboard: C is slot 1, B is slot 12. Hold one "
              "Mode button and tap the other for the next or previous scene.",
         say="Slots and scenes. The Wingie keeps twelve dreams, through power-off, and every dream can have up "
             "to sixteen scenes.",
         rows=[R("both + bottom key", "slot 1 … 12",
                 "Hold both Mode buttons and press a key on the bottom keyboard: C is slot one, B is slot twelve. "
                 "The lights flash yellow.", hl=["mode1", "mode2", "keys1"], anim="slot"),
               R("left + tap right", "next scene",
                 "Hold the left Mode button and tap the right one for the next scene.", hl=["mode1", "mode2"]),
               R("right + tap left", "previous scene",
                 "Hold the right one and tap the left for the scene before.", hl=["mode2", "mode1"]),
               R("both held", "no notes · no stray taps",
                 "While both are down, the dream sees no keys, so choosing a slot never plays a note. And a "
                 "combination never gives the dream a stray tap.", hl=["mode1", "mode2"]),
               R("send to wingie", "into the slot that plays",
                 "Send to Wingie puts the dream into the slot that's playing, and it plays at once; the Device "
                 "menu sends to any of the twelve. The lights blink white.", hl=["led1", "led2"], anim="white"),
               R("mistakes", "a fast white flicker",
                 "A fast white flicker means the dream had mistakes, and the Wingie goes on playing the one it "
                 "had.", hl=["led1", "led2"], anim="white"),
               R("at power-up", "both held: slot 1 → first light",
                 "Hold both while you power up, and slot one goes back to first light.", hl=["mode1", "mode2"])]),

    dict(code="li24", part=4, panel="wingie", kind="panel", yt="The Wingie as a processing unit: line in",
         title="line in", sub="the wingie as a processing unit",
         desc="The Line In / Mic switch always picks the input, never the dream. line and mic are the same sound; "
              "source reads the switch.",
         say="New in one point five: the Wingie as a processing unit for line in.",
         rows=[R("the switch", "line in ↑ · microphones ↓",
                 "The Line In, Mic switch always picks the input: up, the line input; down, the two microphones. "
                 "Whatever the dream says, the switch decides.", hl=["source"]),
               R("at power-up", "it reads the switch",
                 "And the Wingie now reads the switch when it powers up, so line in is line in from the start.",
                 hl=["source"]),
               R("line", "line.left · line.right · line.both",
                 "In a dream, `line.left`, `line.right` and `line.both` are the input.", hl=["jacks"]),
               R("mic", "the same sound",
                 "`mic.left`, `mic.right` and `mic.both` are the same sound, by its other name. Write whichever "
                 "reads better.", hl=["mic_l", "mic_r"]),
               R("source", "0 microphones · 1 line in",
                 "And `source` reads the switch: nought for the microphones, one for line in. It's for knowing "
                 "which, never for choosing.", hl=["source"]),
               R("seven dreams", "examples/wingie-proc-*",
                 "Seven new example dreams are built for line in, and measured scene by scene on a Wingie, with "
                 "no dropouts. Set the switch to line, and play anything into it."),
               R("howl", "a resonator hears its speaker",
                 "On the microphones, a resonator can hear its own speaker and howl, as on the stock Wingie: use "
                 "headphones, lower the decay, or put a `limiter` before the output.", hl=["mic_l", "mic_r"])]),

    dict(code="oc25", part=4, panel="wingie", kind="panel", yt="The Wingie: octave switches and transpose",
         title="the octave switches", sub="free controls, and transpose",
         desc="octave1 and octave2 are free controls (0, 0.5, 1) and don't move the keys. transpose turns any "
              "control into octave steps, wherever you want one.",
         say="The octave switches.",
         rows=[R("octave1 · octave2", "0 down · 0.5 middle · 1 up",
                 "L dot oct and R dot oct are `octave1` and `octave2`: free three-way controls, nought down, a "
                 "half in the middle, one up.", hl=["oct1", "oct2"]),
               R("free", "they don't move the keys",
                 "They no longer change the keys: `keys1.pitch` is just the key. Patch the switches into anything "
                 "you like: a decay, a mix, a filter.", hl=["oct1", "keys1"]),
               R("transpose", "any control into octave steps",
                 "When you want an octave switch, `transpose` turns any control into whole octaves: under a third "
                 "is a step down, over two thirds a step up, in between, none.", hl=["oct1"]),
               R("three lines", "octave1 -> oct1 · keys1.pitch -> oct1.pitch · oct1 -> vco.pitch",
                 "Octave one into a transpose called oct one; keys one pitch into its pitch inlet, which passes "
                 "through; and oct one into a pitch. An octave switch again, exactly where you want it. The "
                 "Wingie examples all do this.", hl=["oct1", "keys1"]),
               R("transpose 7st", "the nature is the step",
                 "Its nature is the step: transpose two jumps two octaves, transpose seven S T a fifth."),
               R("anything drives it", "a fader · a gate",
                 "Anything can drive it: a fader becomes a three-way switch, a gate a one-octave jump.",
                 hl=["f1"]),
               R("pitch on the shifters", "cave · pitchshift · specshift · formantshift",
                 "And `cave`, `pitchshift`, `specshift` and `formantshift` are tuned by an inlet called shift, "
                 "but pitch reaches it too, so the keys play them directly.", hl=["keys1"])]),

    dict(code="hb26", part=4, shot="w-both-journal-dark.png", crop=(560, 112, 400, 300), kind="shot",
         yt="The Wingie on the tapestry: the hands board",
         title="the hands board", sub="named by the panel",
         desc="On the tapestry, the Wingie's hands are one board, always there. Since 1.5.0 each row says where it "
              "is on the panel.",
         say="On the tapestry, the Wingie's hands are one board, always there. In one point five it names each "
             "row by the panel, so you can find it on the instrument.",
         rows=[R("mix fader1", "decay fader2 · volume fader3",
                 "Mix, fader one; decay, fader two; volume, fader three.", box=(592, 145, 104, 34)),
               R("bottom keys1.pitch", "top keys2.pitch",
                 "Bottom, keys one: its pitch, gate, trig and key. Then top, keys two.", box=(592, 178, 104, 54)),
               R("l.mode mode1", "r.mode mode2",
                 "L mode, mode one, and R mode, mode two, each with its tap.", box=(592, 263, 104, 32)),
               R("l.oct octave1", "r.oct octave2",
                 "L oct, octave one, and R oct, octave two.", box=(592, 306, 104, 22)),
               R("line/mic source", "the switch, to read",
                 "Line, mic: `source`, the switch.", box=(592, 328, 104, 11)),
               R("input", "mic.left … · line.left …",
                 "And input: the microphones' outlets, and the line's, the same sound under two names.",
                 box=(592, 338, 104, 66)),
               R("patch", "drag a dot onto an inlet",
                 "Drag from a row's dot onto any inlet, and it's patched.", box=(686, 118, 14, 290))]),

    dict(code="wo27", part=4, shot="w-both-journal-dark.png", crop=(0, 578, 965, 300), kind="shot",
         yt="The Wingie on screen: keys, drag loops, listen",
         title="the wingie on screen", sub="play it from the computer",
         desc="Click the keys or type them: z row for the bottom keyboard, q row for the top, a and p for the "
              "Mode buttons. Drag the loops onto the tapestry to patch.",
         say="Along the bottom of the editor in Wingie mode, the Wingie as it is, keys2 on top.",
         rows=[R("bottom keys", "z s x d c v g b h n j m",
                 "Click the keys, or type the bottom row of the computer keyboard, z to m, for the bottom "
                 "keyboard.", box=(18, 800, 234, 64)),
               R("top keys", "q 2 w 3 e r 5 t 6 y 7 u",
                 "The q row plays the top keyboard. A and P are the two Mode buttons.", box=(18, 738, 234, 64)),
               R("faders · switches", "drag · click the top or bottom half",
                 "Drag the faders. Click the top half of a switch to move it up, the bottom half to move it down.",
                 box=(18, 598, 200, 140)),
               R("drag loops", "faders · line in · mic · switches · mode",
                 "And the little copper loops: drag one onto a place on the tapestry, and that hand is patched. "
                 "There's one above each fader, beside LINE IN and MIC, the Line, Mic switch, each octave switch "
                 "and each Mode button.", box=(18, 598, 180, 140)),
               R("keys2 · keys1", "just outside the right edge",
                 "And just outside the right edge, one for each keyboard: keys two above, keys one below.",
                 box=(256, 752, 34, 96)),
               R("listen", "the computer's microphone",
                 "The microphones are silent in the simulation until you press listen: then the computer's "
                 "microphone is the Wingie's. Wear headphones.", box=(808, 584, 154, 24)),
               R("patch map", "what your hands do in this scene",
                 "The patch map lists every hand and where it goes in the scene that's playing.",
                 box=(288, 584, 520, 136))]),

    dict(code="ww28", part=4, panel="wingie", kind="panel", yt="Writing a Wingie dream",
         title="writing for the wingie", sub="what's different",
         desc="instrument wingie at the top. The hands by name. Memory before processor: 70 KB a scene.",
         say="Writing a Wingie dream. Everything in the language holds. Here is what's different.",
         rows=[R("instrument wingie", "the first line",
                 "A Wingie dream says instrument Wingie, usually on the first line. The switch writes it for you. "
                 "File, New dream for the Wingie, starts one."),
               R("mistakes", "the other instrument's hands",
                 "Without it, the Wingie's hands are mistakes; with it, the SP-1's are: the buttons, the rocker, "
                 "PLAY, the dots, the eight L.E.D.s and fader four."),
               R("the hands", "by name, never placed",
                 "The hands are always there: use them by name, like `keys1.pitch` into a resonator's pitch, or "
                 "`fader2` into its decay.", hl=["keys1", "f2"]),
               R("memory", "70 kb a scene",
                 "On the Wingie, memory usually runs out before the processor. Each scene has seventy kilobytes. "
                 "A grain or time place's nature is its tape in tenths of a second: six is point six seconds, "
                 "about fifty-eight kilobytes."),
               R("samplerate=half", "twice the seconds",
                 "Samplerate half keeps twice the seconds in the same memory, at half the work, a little darker."),
               R("scenes", "70 kb each",
                 "Every scene has its own seventy kilobytes. Put the heavy things in different scenes, and step "
                 "between them with the Mode buttons.", hl=["mode1", "mode2"]),
               R("the room", "the editor hears the computer's mic",
                 "The editor hears the computer's microphone, the Wingie its own: the dream is the same, the room "
                 "is not.")]),

    dict(code="wm29", part=4, shot="w-both-journal-dark.png", crop=(965, 578, 475, 300), kind="shot",
         yt="The Wingie's meters, and too heavy",
         title="the wingie's meters", sub="what the wingie measures, every second",
         desc="The editor's estimate, and what the Wingie reports once a second: CPU, control core, heap, pool, "
              "dropped blocks, levels, and a minute of history.",
         say="The Wingie's meters. Its costs were measured on a Wingie: places cost it about twice what they cost "
             "the SP-1, but it has far more cycles to spend.",
         rows=[R("load", "the estimate, this scene",
                 "Load is the editor's estimate for the scene: ok, tight past eighty-five percent, too heavy past "
                 "a hundred.", box=(975, 603, 220, 40)),
               R("cpu", "measured: average and peak",
                 "CPU is what the Wingie itself measures, every second, the average and the peak. With no Wingie "
                 "plugged in, as here, it says not connected.", box=(975, 643, 220, 45)),
               R("core · heap · pool", "the second core · free memory · the pool",
                 "The control core, the free heap, and the pool: the engine's memory, about seventy kilobytes a "
                 "scene. Until a Wingie reports, the pool is the estimate.", box=(975, 688, 225, 30)),
               R("dropped", "late blocks",
                 "Dropped counts blocks that were late. Any at all is a stutter you hear.", box=(1068, 701, 124, 17)),
               R("history", "a minute of reports",
                 "The graph is the last minute: the average filled, the peaks as a line, the estimate dashed.",
                 box=(1200, 600, 236, 240)),
               R("too heavy", "back to what last fit",
                 "And when a scene is too heavy, four late blocks in half a second, the Wingie goes back to the "
                 "previous scene that played calmly, else the dream before, else first light. The lights flash "
                 "purple, and the status line says which.")]),

    # ------------------------------------------------------------------ vi · processing
    dict(code="pr30", part=5, panel="wingie", kind="panel", yt="Processing: four families",
         title="processing", sub="four families, on both instruments",
         desc="Most of 1.5.0's new places are for processing: hi-fi granular, spectral, time and colour. Every one "
              "plays on both instruments, at any samplerate.",
         say="Processing. Most of the new places in one point five are for processing what comes in, in four "
             "families. They play on both instruments, but they're at home on the Wingie's line in.",
         rows=[R("hi-fi granular", "grains · stretch · paulstretch …",
                 "Hi-fi granular: `grains`, `grainfreeze`, `particles`, `paulstretch`, `stretch`, with smooth "
                 "windows, not the Clouds sound.", hl=["source"]),
               R("spectral", "specfilter · specfreeze · specshift …",
                 "Spectral: the sound taken apart into its frequencies, `specfilter`, `specfreeze`, `specshift`, "
                 "`spectraldelay`, `harmonizer`."),
               R("time", "shimmer · dub · reverse · beauty …",
                 "Time: echoes, loops and rooms, `shimmer`, `doppler`, `reverse`, `beauty`, `dub`, `swarm`."),
               R("colour", "transient · eq · fuzz · autowah …",
                 "Colour: pedals, tapes and desks, `transient`, `eq`, `fuzz`, `autowah`, `chloe`."),
               R("on the sp-1", "heavier: samplerate=half",
                 "On the SP-1 they cost more, so the heavy ones run at samplerate half or a quarter."),
               R("on the wingie", "one big tape a scene",
                 "On the Wingie, memory is the limit: one big tape a scene, and the light places around it.")]),

    dict(code="gr31", part=5, shot="w-glass-faltung-dark.png", kind="places", yt="Hi-fi granular",
         title="hi-fi granular", sub="grains and tapes · wingie-proc-glass-cloud",
         desc="wingie-proc-glass-cloud on the tapestry: grains of the line, pitched by the bottom keys, denser "
              "the louder you play.",
         say="Hi-fi granular. On the tapestry, the glass cloud dream's first scene, the bead curtain.",
         rows=[R("grains", "a field of glass beads",
                 "`grains`: a field of glass beads catching the room. Position, spray, size from four "
                 "milliseconds to a second, density, pitch, spread and freeze. Its nature is its tape, in "
                 "tenths of a second.", box=(980, 210, 132, 128), place="grains"),
               R("the keys", "transpose → grains.pitch",
                 "Here the bottom keys pitch the grains, through a `transpose` on the left octave switch.",
                 box=(540, 210, 118, 70), place="transpose"),
               R("the follower", "louder · denser",
                 "And a `follower` makes them denser the louder you play.", box=(835, 210, 130, 80),
                 place="follower"),
               R("stretch", "slow motion",
                 "`stretch`: a slow-motion replay of the evening. Speed one as it was, nought standing still, "
                 "negative backwards; a held note stays clean.", place="stretch"),
               R("paulstretch", "a glacier",
                 "`paulstretch`: one minute becomes one hour, up to a thousand times. On the Wingie, run it at "
                 "samplerate half.", place="paulstretch"),
               R("grainfreeze", "the photograph that breathes",
                 "`grainfreeze`: one moment, held, still moving.", place="grainfreeze"),
               R("graindelay · cloud", "a corridor · a cloud",
                 "`graindelay`, a corridor that sings back; and `particles`, Xenakis's stochastic cloud.",
                 place="particles"),
               R("scenes", "slow motion · the photograph · glass rain",
                 "The dream's other scenes are slow motion, the photograph that breathes, and glass rain.",
                 box=(1185, 125, 255, 35))]),

    dict(code="sp32", part=5, shot="w-prism-faltung-light.png", kind="places", yt="Spectral",
         title="spectral", sub="the sound taken apart · wingie-proc-prism",
         desc="wingie-proc-prism on the tapestry: a spectral freeze that mode1 or any top key holds, then a filter "
              "drawn on the harmonics of the bottom keys, in a small room.",
         say="Spectral: the sound taken apart. The prism dream's first scene, the held chord.",
         rows=[R("specfilter", "a filter drawn on the spectrum",
                 "`specfilter`: a filter drawn on the spectrum. Comb, teeth, tilt, formants or harmonics; here, "
                 "on the harmonics of the key you play.", box=(835, 210, 133, 80), place="specfilter"),
               R("specfreeze", "one chord, standing in the air",
                 "`specfreeze`: while freeze is up, the spectrum is held and sung on; blend lets the live sound "
                 "seep in. Here the left Mode button, or any top key, holds it.", box=(983, 210, 134, 80), place="specfreeze"),
               R("specshift", "the same voice, a floor up",
                 "`specshift`: spectral pitch shift, peaks moved with their shapes.", place="specshift"),
               R("spectraldelay", "echoes that arpeggiate",
                 "`spectraldelay`: every frequency its own echo, so the echoes arpeggiate the spectrum.",
                 place="spectraldelay"),
               R("resynth", "the tonal skeleton",
                 "`resynth`: the loudest peaks followed and sung again as clean sines.", place="resynth"),
               R("formantshift", "the giant and the bird",
                 "`formantshift`: a throat that grows or shrinks, the pitch left where it is.",
                 place="formantshift"),
               R("harmonizer · morph", "the heaviest: samplerate=half",
                 "`harmonizer`, three voices from one, and `morph`, cross-synthesis, are the heaviest: on the "
                 "Wingie, run them at samplerate half.", place="harmonizer"),
               R("scenes", "echoes · skeleton · the cave",
                 "Then echoes that arpeggiate, the tonal skeleton, and sung through the cave.",
                 box=(1185, 125, 255, 35))]),

    dict(code="tm33", part=5, shot="w-dub-journal-light.png", kind="places", yt="Time: echoes, loops, rooms",
         title="time", sub="echoes, loops, rooms · wingie-proc-dub-time",
         desc="wingie-proc-dub-time in the journal look: a dub send thrown by mode1, through a band filter on the "
              "bottom keys, out across a ping-pong.",
         say="Time. The dub time dream's first scene: the siren in the yard.",
         rows=[R("dub", "king tubby's hand on the send",
                 "`dub`: only while throw is up does the sound go into the echo, with a spring splash and a tone "
                 "inside the loop. Here, `mode1` throws it.", box=(1120, 112, 105, 74), place="dub"),
               R("svf · pingpong", "a band filter · a ping-pong",
                 "The echo runs through a band filter the bottom keys place, and out across a `pingpong`.",
                 box=(1003, 112, 102, 74), place="pingpong"),
               R("tapeloop", "frippertronics",
                 "`tapeloop`: a six-second loop; punch records over it, age wears it away.", place="tapeloop"),
               R("eis", "pauline oliveros in the cistern",
                 "`eis`: four delays whose times wander, feeding each other, after Pauline Oliveros' Expanded "
                 "Instrument System.", place="eis"),
               R("pt2399", "the echo chip",
                 "`pt2399`: every cheap delay pedal you loved; turn the time and the pitch bends.", place="pt2399"),
               R("shimmer · reverse", "the climbing reverb · words before they're said",
                 "`shimmer`, the reverb that climbs an octave; `reverse`, the words before they're said.",
                 place="shimmer"),
               R("doppler · swarm", "a train going past · insects at dusk",
                 "`doppler`, a source going past; and `swarm`, insects at dusk.", place="doppler"),
               R("scenes", "frippertronics · eis · the chip and the coco",
                 "Then frippertronics, the expanded instrument, and the chip and the coco.",
                 box=(180, 50, 476, 28))]),

    dict(code="co34", part=5, panel="wingie", kind="places", yt="Colour: pedals, tapes and desks",
         title="colour", sub="pedals, tapes and desks",
         desc="The last stage of a chain, or the only one: fuzz and valves, tape and vinyl, the mixer's EQ, and "
              "ears that hear attacks and pitch.",
         say="Colour: pedals, tapes and desks.",
         rows=[R("fuzz", "a fuzz face, a tired battery",
                 "`fuzz`: germanium, with a tired battery.", place="fuzz"),
               R("chloe", "a valve behind a singing filter",
                 "`chloe`: a valve glowing behind a singing filter, and a low-pass gate.", place="chloe"),
               R("tape", "the cassette that lived in a car",
                 "`tape`: the cassette that lived in a car.", place="tape"),
               R("eq", "the dj's isolator",
                 "`eq`: the mixer's three knobs.", hl=WF, place="eq"),
               R("transient", "a drum with its edges sharpened",
                 "`transient`: attack sharpens or softens each hit, sustain blooms or dries the tail.",
                 place="transient"),
               R("autowah", "the mu-tron, quacking",
                 "`autowah`: play harder and it opens.", place="autowah"),
               R("onset · pitchtrack", "ears for attacks and pitch",
                 "`onset` and `pitchtrack`: ears that turn the line's attacks and pitch into pulses and currents.",
                 place="pitchtrack"),
               R("limiter", "nothing above the ceiling",
                 "And `limiter`: nothing above the ceiling, ever. A good last place before the output.",
                 place="limiter")]),

    dict(code="ch35", part=5, shot="w-everything-modular.png", kind="shot", yt="Whole chains, with feedback",
         title="whole chains", sub="grains → spectrum → time · wingie-proc-everything",
         desc="wingie-proc-everything in the modular look: five whole chains, each with feedback from the end "
              "back to the start.",
         say="Whole chains. The everything dream, in the modular look: five scenes, each grains into spectrum "
             "into time, or the other way round.",
         rows=[R("grains", "pitched by the bottom keys",
                 "The river starts as grains of the line, pitched by the bottom keys and denser the louder you "
                 "play.", box=(965, 210, 123, 122)),
               R("specfreeze", "any top key holds it",
                 "Then a spectral freeze that any held top key stops.", box=(965, 342, 120, 74)),
               R("tapeecho", "fader2 its feedback",
                 "Then a tape echo, its feedback on the Decay fader.", box=(965, 420, 120, 60)),
               R("flipflop · vca", "tap mode2: the echo runs back",
                 "Tap the right Mode button, and a latch opens a way from the echo back into the grains: the "
                 "river feeds itself.", box=(565, 280, 102, 52)),
               R("key24 · key23", "the next scene · the one before",
                 "And here, the top B and A sharp change scene: `key24` into `scene.next`, `key23` into "
                 "`scene.back`, no Mode combination needed.", box=(430, 210, 105, 268)),
               R("the scenes", "river · glass · cave · glacier · cathedral",
                 "The whole river, stretch to glass, cave glass prism, the glacier, and the cathedral.",
                 box=(1185, 125, 255, 35)),
               R("under the limit", "each measured on a wingie",
                 "Every scene was measured on a Wingie: under the limit, with no dropouts.")]),

    dict(code="ex36", part=5, panel="wingie", kind="panel", yt="The seven processing dreams",
         title="seven dreams for line in", sub="examples/wingie-proc-*",
         desc="Every one: Mix, Decay and Volume steer the stages, the keys pitch or strike, the Mode buttons "
              "hold, throw and latch. Set the switch to line.",
         say="All seven processing dreams share one layout of the hands, so you can pick one up and play.",
         rows=[R("glass-cloud", "hi-fi granular",
                 "Glass cloud: line in as hi-fi grains, a slow-motion replay, a photograph that breathes.",
                 hl=["source"]),
               R("prism", "spectral",
                 "Prism: frozen, filtered on the keys' harmonics, echoed bin by bin, sung through a cave."),
               R("dub-time", "dub · tape loop · eis · pt2399",
                 "Dub time: thrown, looped and worn.", hl=["mode1"]),
               R("caves", "resonators from the keys",
                 "Caves: the Wingie's own resonators, and some cousins, ringing on line in, played from the keys.",
                 hl=["keys1", "keys2"]),
               R("looper", "a tape loop · a reel · a gesture",
                 "Looper: line in caught and played like an instrument."),
               R("everything", "grain → spectral → time",
                 "Everything: the five whole chains."),
               R("follow", "the line drives the patch",
                 "And follow: the line in plays the patch, its pitch, loudness and attacks; the hands only steer."),
               R("the hands", "mix · decay · volume · keys · mode",
                 "In each, Mix, Decay and Volume steer the stages; the keys pitch or strike; the Mode buttons "
                 "hold, throw and latch.", hl=WF)]),

    # ------------------------------------------------------------------ vii · limits
    dict(code="lm37", part=6, panel="both", kind="panel", yt="Limits and numbers",
         title="limits and numbers", sub="what fits on each instrument",
         desc="The SP-1: 124 KB, 96 places, 320 passages a scene, 5 slots. The Wingie: 70 KB a scene, the same "
              "96 and 320, 12 slots. Both: 16 scenes, 16 KB of text a slot.",
         say="Limits and numbers.",
         rows=[R("sp-1", "124 kb · 96 places · 320 passages",
                 "On the SP-1, a scene has a hundred and twenty-four kilobytes of memory, ninety-six places and "
                 "three hundred and twenty passages.", hl=["s:f1", "s:f2", "s:f3", "s:f4"]),
               R("wingie", "70 kb a scene",
                 "On the Wingie, seventy kilobytes a scene, and the same ninety-six places and three hundred and "
                 "twenty passages.", hl=["w:f1", "w:f2", "w:f3"]),
               R("slots", "sp-1 5 · wingie 12 · 16 kb each",
                 "Five slots on the SP-1, twelve on the Wingie, each up to sixteen kilobytes of text. Sixteen "
                 "scenes a dream on both."),
               R("an empty dream", "sp-1 6.5 % · wingie 2.8 %",
                 "An empty dream costs the SP-1 six and a half percent, the Wingie two point eight. First light "
                 "takes about thirty percent of the Wingie."),
               R("85 %", "keep a scene under it",
                 "Keep a scene under about eighty-five percent on the load meter. Past that it may glitch, and a "
                 "dream that goes on missing its deadline falls back to the last one that fit."),
               R("the heaviest", "morph 86 % · harmonizer 129 %",
                 "A few spectral places are heavy even on the Wingie: `morph` about eighty-six percent, the "
                 "`harmonizer` more than all of it, so run it at samplerate half."),
               R("memory first", "one big tape a scene",
                 "And on the Wingie, memory runs out first: shorter natures, samplerate half, or another scene.")]),

    dict(code="ts38", part=6, panel="sp1", kind="steps", yt="Troubleshooting the SP-1",
         title="when the sp-1 won't", sub="standby, boot mode, empty slots",
         desc="The bootloader is never touched: whatever happens, boot mode and the updater can bring the SP-1 "
              "back.",
         say="When the SP-1 won't.",
         rows=[R("off (standby)", "hold •• to turn it on",
                 "The editor says the SP-1 is off, in standby: hold the dots to turn it on, and send again.",
                 hl=["dots"], anim="standby"),
               R("no sp-1 plugged in", "not in boot mode?",
                 "No SP-1 is plugged in: if only T1 is lit, it's in boot mode. Plug in again holding nothing.",
                 hl=["t1"], anim="boot"),
               R("in use", "one program at a time",
                 "In use by another program: quit the serial monitor or the second editor. Use a data cable, "
                 "straight into the computer."),
               R("older firmware", "flash the .bin that came with it",
                 "Mistakes from older firmware: flash the S P 1 dot bin that came with your editor."),
               R("empty slots", "after a reflash",
                 "After a reflash the slots may be empty: your dream files are the real copy."),
               R("always back", "the bootloader is never touched",
                 "And the bootloader is never touched, so boot mode can always bring it back.", hl=["t1", "t4"],
                 anim="boot")]),

    dict(code="tw39", part=6, panel="wingie", kind="steps", yt="Troubleshooting the Wingie",
         title="when the wingie won't", sub="ports, silence, howls and slots",
         desc="Most things are a held port, a fader the dream uses as volume, or a dream too heavy for a scene.",
         say="When the Wingie won't.",
         rows=[R("no port", "a charge-only cable · a held port",
                 "Esptool can't find a port: a charge-only cable, or the port is held. Quit the editor and "
                 "serial monitors. If it can't connect, hold boot, tap reset, and let go of boot.", hl=["jacks"]),
               R("nothing comes out", "a fader the dream uses as volume",
                 "Nothing comes out: the dream sends nothing to the output, or a fader it uses as volume is down.",
                 hl=WF),
               R("silent line in", "the switch",
                 "Line in is silent: check the Line In, Mic switch. It decides, not the dream.", hl=["source"]),
               R("it howls", "headphones · a limiter",
                 "It howls: a resonator hearing its own speaker. Use headphones, lower the decay, or a `limiter`.",
                 hl=["mic_l", "mic_r"]),
               R("first light, not mine", "an empty slot",
                 "It plays first light, not your dream: the slot is empty, or both Mode buttons were down at "
                 "power-up.", hl=["mode1", "mode2"]),
               R("it changed dream", "too heavy · purple",
                 "It changed dream by itself: too heavy, the lights flashed purple. Lighten the scene, or watch "
                 "dropped in the meters. Or the dream has a `slot` place that was pulsed.", hl=["led1", "led2"],
                 anim="heavy"),
               R("the house is full", "70 kb",
                 "The editor says the house is full: shorter natures, samplerate half, or another scene.")]),

    # ------------------------------------------------------------------ viii · sleep well
    dict(code="sq∞", part=7, panel="both", kind="outro", yt="Licences and thanks",
         title="sleep well", sub="sleepcousonique · the sp-1 and the wingie",
         desc="The repository is MIT. The Wingie firmware is GPLv3: it includes Meng Qi's Wingie2 code. Thanks to "
              "Meng Qi, Peter Blasser and Worldwave.",
         say="That's Sleepcousonique, on the SP-1 and the Wingie. The repository is M.I.T. licensed; the SP-1 "
             "firmware's hardware layer is Wakes, from Worldwave, also M.I.T. The Wingie firmware is G P L version "
             "three, because it includes Meng Qi's Wingie2 code. Thanks to Meng Qi for the Wingie2 and for opening "
             "its firmware; to Peter Blasser; to Teenage Engineering for the SP-1; and to Alvin Lucier, Pauline "
             "Oliveros and Iannis Xenakis, whose pieces live here as places. Now write down what you dreamt, send "
             "it, and unplug.",
         rows=[R("repository", "mit"), R("sp-1 firmware", "mit · wakes (worldwave)"),
               R("wingie firmware", "gplv3 · meng qi's wingie2 code"), R("places", "239"),
               R("unofficial", "not affiliated with teenage engineering")]),
]
