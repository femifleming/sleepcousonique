"""What the narrator says in the Sleepcousonique Wingie video manual, chapter by chapter.

Every chapter is a dict: code, title, sub, desc (on screen), say (spoken first), rows. A row is
(label, text on screen, spoken or None, panel highlight keys) and, in screenshot chapters, a box
(x, y, w, h) on the screenshot (1440 x 900 pixels).

Anything in `backticks` is a place or a hand: build_video.py checks every one against the
engine's catalog (place, or place.port) before anything is spoken. Backticks are not spoken.

The chapters about the places themselves are not here: they are read from the manual
(docs/wingie-manual/05..09), one row per place, so the video always matches it.
"""

KEYS1 = ["keys1"]
KEYS2 = ["keys2"]
FADERS = ["f1", "f2", "f3"]
MODES = ["mode1", "mode2"]


def R(label, text, spoken=None, hl=(), box=None):
    return dict(label=label, text=text, spoken=spoken, hl=list(hl), box=box)


BEFORE = [
    dict(code="sq00", title="sleepcousonique", sub="on the wingie: a dream journal for a handheld resonator",
         desc="The patch language of the SP-1, now on Meng Qi's Wingie2. 237 places, 12 dream slots, "
              "scenes on the Mode buttons, and every key, switch and fader yours.",
         say="Sleepcousonique, on the Wingie. Sleepcousonique is a patch language written like a dream journal: "
             "each patch is a dream, its modules are places, and the cords between them are passages. "
             "Until now it slept only in the SP-1. Now the same engine runs on Meng Qi's Wingie2, the handheld "
             "resonator with two microphones. Every key, switch and fader is yours, and the dream says what it does.",
         rows=[R("places", "237", hl=[]), R("slots", "12", hl=[]), R("scenes", "16 a dream", hl=[]),
               R("memory", "70 kb a scene", hl=[]), R("firmware", "1.5.0-beta1", hl=[])], kind="intro"),
    dict(code="fl01", title="flashing", sub="and going back to stock",
         desc="Quit the editor. Plug in the Wingie. tools/flash_wingie.sh writes the whole image at address "
              "zero with esptool. It wakes up playing first light.",
         say="Flashing.",
         rows=[R("1", "quit the editor · plug in usb-c",
                 "Quit the editor, and anything else that holds the Wingie's serial port. Plug the Wingie in with "
                 "a USB-C cable that carries data.", hl=[]),
               R("2", "tools/flash_wingie.sh",
                 "Run tools, flash wingie dot S H. It finds the port by itself, and writes the whole image at "
                 "address zero with esptool, at four hundred and sixty thousand baud. It takes under a minute.", hl=[]),
               R("3", "first light",
                 "The Wingie restarts, fades in, and plays first light: the microphones ring two sets of "
                 "sympathetic strings, tuned by the two keyboards.", hl=["mic_l", "mic_r", "keys1", "keys2"]),
               R("settings", "note your stock settings first",
                 "The image covers the place where the stock firmware keeps its settings. If you changed your "
                 "caves or tunings, note them down first.", hl=[]),
               R("back", "meng qi's web flasher",
                 "To go back, open Meng Qi's web flasher in Chrome or Edge, connect the Wingie, and install. "
                 "Nothing can break: the bootloader in the chip's ROM is never written.", hl=[])], kind="steps"),
    dict(code="pn02", title="the panel", sub="every key, switch and fader is a hand",
         desc="Same panel, same jacks. Nothing is fixed any more: the dream says what each hand does.",
         say="The panel. Let's go round it.",
         rows=[R("mic", "two microphones · or line in",
                 "Top left and right, the two microphones: `mic.left`, `mic.right`, and `mic.both`.",
                 hl=["mic_l", "mic_r"]),
               R("source", "microphone ↓ · line input ↑",
                 "The source switch picks microphones or the line input. The dream can read it too, as `source`.",
                 hl=["source"]),
               R("octave1 · octave2", "−1 · 0 · +1",
                 "The two octave switches are `octave1` and `octave2`: minus one, zero or one. They also move the "
                 "keyboards' pitch.", hl=["oct1", "oct2"]),
               R("mode1 · mode2", "held · tap",
                 "The Mode buttons are `mode1` and `mode2`: one while held, and a tap pulse on a quick press.",
                 hl=["mode1", "mode2"]),
               R("fader1 2 3", "mix · decay · volume: yours",
                 "The faders are labelled Mix, Decay and Volume, and in a dream they're `fader1`, `fader2` and "
                 "`fader3`, from zero to one. Nothing is fixed to them: Volume is only a volume if your dream "
                 "makes it one. So start with your headphones low.", hl=FADERS),
               R("lights", "red · purple · yellow · white",
                 "The two lights are the dream's own: `lights.left` and `lights.right`, red at rest, then purple, "
                 "yellow and white as what comes in climbs.", hl=["led1", "led2"]),
               R("keys1", "bottom keyboard · c to b",
                 "The bottom keyboard is `keys1`: its pitch in octaves, a gate, a trigger, and which key.",
                 hl=KEYS1),
               R("keys2", "top keyboard · an octave up",
                 "The top keyboard is `keys2`, an octave higher. One key alone is `key1` to `key24`.",
                 hl=KEYS2),
               R("midi", "trs midi in",
                 "And the MIDI jack on the edge feeds the `midi`, `midicc` and `midiclock` places.", hl=[])],
         kind="panel"),
    dict(code="sl03", title="slots and scenes", sub="both mode buttons and a key",
         desc="Hold both Mode buttons and press a key on the bottom keyboard: C is slot 1, B is slot 12. Hold one Mode "
              "button and tap the other for the next or previous scene.",
         say="Slots and scenes. The Wingie keeps twelve dreams, and every dream can have up to sixteen scenes.",
         rows=[R("both + key", "slot 1 … 12",
                 "Hold both Mode buttons and press a key on the bottom keyboard: C is slot one, B is slot twelve. "
                 "The lights flash yellow.", hl=["mode1", "mode2", "keys1"]),
               R("left + tap right", "next scene",
                 "Hold the left Mode button and tap the right one for the next scene.", hl=["mode1", "mode2"]),
               R("right + tap left", "previous scene",
                 "Hold the right one and tap the left for the scene before.", hl=["mode2", "mode1"]),
               R("both held", "no notes · no stray taps",
                 "While both are down, the dream sees no keys, so choosing a slot never plays a note. And the "
                 "Mode button you pressed first stays held for the dream until it has been down longer than a "
                 "tap, so a combination never gives the dream a stray tap.", hl=["mode1", "mode2"]),
               R("white", "a dream arrived · mistakes",
                 "When a dream arrives from the editor, the lights blink white. A fast white flicker means it "
                 "had mistakes, and the Wingie goes on playing the dream it had.", hl=["led1", "led2"]),
               R("at power-up", "slot 1 → first light",
                 "Hold both while you power up, and slot one goes back to first light.", hl=["mode1", "mode2"])],
         kind="slots"),
    dict(code="ed04", title="the editor", sub="wingie mode", shot="editor-journal-dark.png",
         desc="One switch makes a dream a Wingie dream. The journal and the tapestry are the same dream; "
              "the Wingie sits along the bottom.",
         say="The editor in Wingie mode.",
         rows=[R("sp-1 | wingie", "the instrument switch",
                 "The switch at the top says which instrument the dream is for. It writes the line, instrument "
                 "Wingie, at the top of the dream, or takes it away.", box=(148, 6, 126, 34)),
               R("file menu", "new dream for the wingie",
                 "File, New dream for the Wingie, starts one: a comb rung by the microphones, tuned by the left "
                 "keyboard.", box=(0, 80, 550, 290)),
               R("journal", "the dream as text",
                 "On the left, the journal: the dream as text.", box=(0, 80, 550, 550)),
               R("tapestry", "the same dream as boards",
                 "On the right, the tapestry: the same dream, as boards and cords. The Wingie's hands are one "
                 "board, and its outputs and lights another.", box=(550, 80, 890, 550)),
               R("status", "wingie connected · slot · scene",
                 "Up here, whether a Wingie is connected, and which slot and scene it plays. When the Wingie has "
                 "to go back to a lighter dream, the status line says so.",
                 box=(1240, 50, 200, 26)),
               R("send to wingie", "into the slot that plays",
                 "Send to Wingie puts the dream into the slot that is playing, and it plays at once. The Device menu "
                 "sends to any of the twelve slots.", box=(8, 6, 132, 34))], kind="shot"),
    dict(code="hp05", title="hands on screen", sub="play the wingie from the computer", shot="editor-journal-dark.png", crop=(0, 700, 975, 180),
         desc="Click the keys or type them: z row for the bottom keyboard, q row for the top, a and p for "
              "the Mode buttons. Listen feeds the computer's microphone in.",
         say="The Wingie, on screen.",
         rows=[R("bottom keys", "z s x d c v g b h n j m",
                 "Click the keys, or type the bottom row of the computer keyboard, z to m, for the bottom keyboard.",
                 box=(15, 708, 222, 160)),
               R("top keys", "q 2 w 3 e r 5 t 6 y 7 u",
                 "The q row plays the top keyboard. A and P are the two Mode buttons.", box=(395, 708, 222, 160)),
               R("faders · source", "mix · decay · volume · mic | line",
                 "Drag the faders; click mic or line. Drag a fader, a keyboard or the microphones onto a place on "
                 "the tapestry, and it's patched.", box=(243, 708, 145, 132)),
               R("listen", "the computer's microphone",
                 "In the simulation, the microphones are silent until you press listen: then the computer's "
                 "microphone is the Wingie's. Wear headphones.", box=(243, 840, 110, 30)),
               R("patch map", "what your hands do in this scene",
                 "The patch map lists every hand and where it goes in the scene that's playing.",
                 box=(630, 708, 335, 130)),
               R("wingie live", "the panel follows the real one",
                 "With a Wingie plugged in, the panel follows it: its keys, faders, switches, scene and slot.",
                 box=(15, 708, 600, 160))], kind="shot"),
    dict(code="mt06", title="the meters", sub="what the wingie measures, every second", shot="editor-journal-dark.png", crop=(965, 700, 475, 180),
         desc="The editor's estimate, and what the Wingie reports once a second: CPU, control core, heap, "
              "pool, dropped blocks, levels, and a minute of history.",
         say="The meters.",
         rows=[R("load", "the estimate, this scene",
                 "Load is the editor's estimate for the scene, from costs measured on a real Wingie: ok, tight "
                 "past eighty-five percent, too heavy past a hundred.", box=(975, 728, 220, 40)),
               R("cpu", "measured: average and peak",
                 "CPU is what the Wingie itself measures, the average and the peak. With no Wingie plugged in, as "
                 "here, it says not connected.", box=(975, 768, 220, 40)),
               R("core · heap · pool", "the second core · free memory · the pool",
                 "The control core, the free heap, and the pool: the engine's memory, seventy kilobytes a scene. "
                 "Until a Wingie reports, the pool is the editor's estimate.",
                 box=(975, 812, 225, 32)),
               R("dropped", "late blocks",
                 "Dropped counts blocks that were late. Any at all is a stutter you hear.", box=(1090, 826, 110, 18)),
               R("history", "a minute of reports",
                 "The graph is the last minute: the average filled, the peaks as a line, the estimate dashed.",
                 box=(1200, 726, 236, 118)),
               R("levels", "in l · in r · out l · out r",
                 "And the levels in and out, in decibels.", box=(1200, 842, 236, 26))], kind="shot"),
    dict(code="wr07", title="writing a wingie dream", sub="what's different",
         desc="instrument wingie at the top. The hands by name. Pitch units, resolution, samplerate. "
              "Memory before processor.",
         say="Writing a Wingie dream. Everything in the language holds: places, passages, natures, switches, "
             "knobs, lucid groups and scenes. Here is what's different.",
         rows=[R("instrument wingie", "the first line",
                 "A Wingie dream says instrument Wingie, usually on the first line. Without it, the Wingie's hands "
                 "are mistakes; with it, the SP-1's are.", hl=[]),
               R("the hands", "by name, never placed",
                 "The hands are always there: use them by name, like `keys1.pitch` into a resonator's pitch, or "
                 "`fader2` into its decay.", hl=["keys1", "f2"]),
               R("pitch units", "7st · -50ct · 1oct",
                 "A number can carry a pitch unit: seven S T is seven semitones, minus fifty C T is fifty cents "
                 "down, one O C T an octave.", hl=[]),
               R("resolution", "fine · audio",
                 "Resolution fine reads a place's controls every four samples, audio every sample: true "
                 "audio-rate F M, at eight or thirty-two times the cost. It only works at full rate; with a "
                 "lower samplerate the check warns and ignores it.", hl=[]),
               R("memory", "70 kb a scene",
                 "On the Wingie, memory runs out before the processor. Each scene has seventy kilobytes. A grain "
                 "or time place's nature is its tape in tenths of a second: six is point six seconds, about "
                 "fifty-eight kilobytes.", hl=[]),
               R("samplerate=half", "twice the seconds",
                 "Samplerate half keeps twice the seconds in the same memory, at half the work, a little darker.",
                 hl=[]),
               R("scenes", "16 a dream · 70 kb each",
                 "Every scene starts fresh, with its own memory. Put the heavy things in different scenes, and "
                 "step between them with the Mode buttons, or with `scene.next`.", hl=["mode1", "mode2"]),
               R("slot", "a dream that leads to another",
                 "And the `slot` place goes to another dream: a pulse at `slot.go`, `slot.next` or `slot.back`.",
                 hl=[])], kind="panel"),
    dict(code="th08", title="too heavy", sub="the last dream that fit",
         desc="Four late blocks in half a second: the Wingie goes back to what last played calmly for two "
              "seconds. The previous scene, the dream before, or first light.",
         say="Too heavy. When a dream asks more than there is, the Wingie doesn't break up.",
         rows=[R("1", "the previous scene",
                 "If a heavy scene did it, the Wingie goes back to the previous scene that played calmly for two "
                 "seconds.", hl=["led1", "led2"]),
               R("2", "the dream before",
                 "Else, to the dream that was playing before.", hl=["led1", "led2"]),
               R("3", "first light",
                 "Else, to first light, which always fits. The lights flash purple, and the editor's status line "
                 "says which.", hl=["led1", "led2"]),
               R("2.8 %", "an empty dream",
                 "Measured on a Wingie: an empty dream costs two point eight percent of the processor, first light "
                 "about thirty, four reverbs thirty-eight.", hl=[]),
               R("85 %", "the heavy spectral places",
                 "A few spectral places are heavy even here: morph about eighty-six percent, and the harmonizer "
                 "more than all of it, so run it at samplerate half. Keep a scene under about eighty-five "
                 "percent.", hl=[]),
               R("the house is full", "memory, checked before sending",
                 "Memory is checked before a dream is sent: a scene that doesn't fit is a mistake, the house is "
                 "full, and never reaches the Wingie.", hl=[])], kind="heavy"),
]

AFTER = [
    dict(code="rc19", title="recipes", sub="seven whole dreams in the manual",
         desc="From the quiet to the loud. Each checked scene by scene against the Wingie's pool.",
         say="Recipes. The manual has seven whole dreams to start from.",
         rows=[R("the ground", "listen · dust · sittingroom",
                 "Thanks to the ground beneath my feet: hold the left Mode button, the place tunes the Wingie, "
                 "and Lucier's room plays it back.", hl=["mode1", "mic_l", "mic_r"]),
               R("bath", "cocoquantus · strings · cavern",
                 "Bath: two tapes in warm water, rocked by five petals, over strings in a cavern.",
                 hl=["mic_l", "mic_r", "keys1", "f1", "f3"]),
               R("drunk with wings", "rollz · membranes · dub",
                 "Pleasantly drunk with wings: a Rollz ring lurching through membranes, thrown into a dub echo.",
                 hl=["mode2", "f1", "f2", "f3"]),
               R("the cistern", "eis · specfreeze · shimmer",
                 "Deep listening in the cistern, after Pauline Oliveros.", hl=["mode1", "f1", "f2", "f3"]),
               R("mycenae", "gendy · particles · metastaseis",
                 "Mycenae at the bus stop: Xenakis by the roadside.", hl=["keys1", "f1", "f2"]),
               R("under the bridge", "convolve · bassman · dub",
                 "Under the bridge: a clap becomes the room.", hl=["mode1", "mode2", "f1", "f3"]),
               R("four bit", "fourbit · three resonators · slot",
                 "And four bit: three players reading the same sixty-four bits, drifting apart.",
                 hl=["mode1", "keys2", "f2", "f3"])], kind="panel"),
    dict(code="sq∞", title="sleep well", sub="sleepcousonique · on the wingie",
         desc="Thanks to Meng Qi for the Wingie2 and its open firmware, to Peter Blasser, and to Lucier, "
              "Oliveros and Xenakis.",
         say="That's Sleepcousonique on the Wingie. Thanks to Meng Qi for the Wingie2 and for opening its "
             "firmware, to Peter Blasser, and to Alvin Lucier, Pauline Oliveros and Iannis Xenakis, whose pieces "
             "live here as places. Now hold it up, and let the room dream.",
         rows=[R("places", "237", hl=[]), R("examples", "98 checked", hl=[]), R("licence", "gplv3 (wingie pins)", hl=[])],
         kind="outro"),
]

# The place chapters: (code, title, sub, manual file, first place, last place) — split so a screen holds them.
PLACE_CHAPTERS = [
    ("pw09", "the wingie's own", "resonator, caves, coco", "05-places-wingie.md", "wingie", "listen"),
    ("pw10", "the wingie's own, ii", "lucier, xenakis, rollz", "05-places-wingie.md", "sittingroom", "probee"),
    ("gr11", "hi-fi granular", "grains and tapes", "06-places-grain.md", "grains", "wavesets"),
    ("gr12", "hi-fi granular, ii", "choirs, storms, dust", "06-places-grain.md", "harmonicgrains", "particles"),
    ("sp13", "spectral", "the sound taken apart", "07-places-spectral.md", "convolve", "formantshift"),
    ("sp14", "space", "around your head, under the floor", "07-places-spectral.md", "binaural", "deverb"),
    ("tm15", "time", "echoes and loops", "08-places-time.md", "beauty", "eis"),
    ("tm16", "time, ii", "reverbs, freezes, swarms", "08-places-time.md", "shimmer", "swarm"),
    ("co17", "colour", "pedals, tapes, desks", "09-places-colour.md", "fuzz", "transient"),
    ("co18", "colour, ii", "motion and listening", "09-places-colour.md", "widener", "drive"),
]
