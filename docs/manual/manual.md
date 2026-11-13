# Sleepcousonique

### A dream-journal patch language for the SP-1

Sleepcousonique is a modular synthesizer you write as a dream journal. Its desktop editor
lets you build a patch as text, as a tapestry of boards and cords, or both at once, and
hear it straight away on your computer. Then you send it over USB to an SP-1 running the
Sleepcousonique firmware, where the faders, buttons, rocker and LEDs play it.

You patch the way you would patch a modular, and decide what each fader and button does by
patching it, the way a Shbobo Shnth is programmed from Fish. Then you take the patch with you
and perform on the SP-1 on its own, with its own audio out.

There are no samples, songs or audio files anywhere in it. Every sound is made live by the
patch, on the computer while you build it and on the SP-1 when you play it. The firmware
doesn't read the SP-1's own storage at all.

It is built from four things:

- **The dream journal** (femishonuga.com/dd.html and the LBSE wiki). Patches are written as
  dated entries. The modules are the places, things and creatures that recur in the dreams:
  the fountain labeled aira, long hallways, the warehouse, the train, golden moss, frogs, the
  magician, the Jakes and the Connors.
- **Peter Blasser's Nabra**, the knobless tapestry synthesizer. Nothing has knobs. Every
  place has a fixed nature, and you change it only by patching something into it. Cords
  carry parts: a resistor, a capacitor or a diode.
- **Buchla and Serge.** From Buchla come three kinds of signal (sound, current and pulse),
  timbre before filtering, and the lowpass gate. From Serge come any signal into any inlet,
  stacked cords that add, and the universal slope (`stairs`).
- **The Nord Modular editor.** You patch on screen and hear it live, with morph groups
  (here, *lucid* groups), a load meter, and patches sent to the hardware.

![The editor: the journal on the left, the tapestry on the right, the SP-1's hands below](fig/view1.png)

---

# 1. The editor

## Opening it

Open **Sleepcousonique.app**. The first time, it opens *first night*, an example dream with
three scenes. `File ▸ Load a history of modular` opens a second example: six scenes, from a
1964 Moog room to 2016 eurorack, each built from the places of its era. Sound comes out of your computer at once. The **sound** button (top right)
picks the audio device.

`File ▸ Open` opens a `.dream` file, and `File ▸ Save` saves one (⌘O, ⌘S). A dream is a plain
text file. You can write it in any text editor, keep it in a notes app, or paste it from
an email.

## Three views

The **journal / both / tapestry** buttons choose what fills the window.

- **Journal.** The dream as plain text. Type here, or paste a whole patch in. About half a
  second after you stop typing, the dream is understood again and the sound follows. If a
  line has a mistake, the last dream that made sense keeps playing, and the mistakes are
  listed under the text. Click a mistake to jump to its line.
- **Tapestry.** The same dream as boards on a splatter-painted canvas, joined by cords.
- **Both.** Side by side. Drag the bar between them to share the space.

The journal and the tapestry are one dream. Whatever you do on the tapestry is written into
the journal, and whatever you write in the journal appears on the tapestry.

![The journal alone](fig/view0.png)

## The tapestry

Each **board** is a place. The black label tape names the place, and the italic line under it
gives the board's own name and its nature. Inlets are the copper loops on the left and
outlets the loops on the right. The dot in an outlet's loop shows what kind of signal comes
out: **umber** for sound, **deep blue** for current, **gold** for pulses.

Two grey boards are always there. **SP-1 hands** on the left holds the faders, buttons, rocker,
PLAY and the dots. **SP-1 ears** on the right holds where sound leaves, the eight LEDs (the
*lamp*, lit live on the board) and the *journey* between scenes.

| to | do this |
|---|---|
| add a place | right-click the canvas ▸ sources / shapes / spaces / currents. Each entry says what the place does and, in brackets, what it comes from |
| patch | drag from an outlet to an inlet (or from an inlet back to an outlet) |
| choose the cord's part | the **-> hall / ~> fog / => water / >\| door** buttons choose the part for new cords |
| change a cord | right-click it: change its part, its amount, show it in the journal, or remove it |
| set a cord's amount | double-click it |
| change a kept value | drag a value tag (the little white bottle by an inlet) up or down; hold shift for fine steps. The sound follows as you drag |
| move boards | drag them; shift-click to pick several |
| remove | select a board or cord and press delete |
| copy as text | ⌘C copies the selected boards and the cords between them *as journal text* |
| paste text | ⌘V pastes journal text (from anywhere) onto the tapestry, at the mouse. Names that are already used are renamed |
| find in the journal | double-click a board |
| move around | drag the empty canvas or scroll; ⌘-scroll or pinch zooms; ⌘0 fits everything |

Hovering an outlet shows its live value at the bottom of the window, like a probe. Board
positions are kept in the file as notes at the very end, lines starting with `;@`. The engine
ignores them.

![The tapestry](fig/view2.png)

## The hands panel: your mappings

Along the bottom is a stand-in for the SP-1:

- **four faders** (`lean1`–`lean4`), each with a copper loop. Drag the loop onto any inlet on
  the tapestry to map that fader there.
- **four track buttons** (`door1`–`door4`), held while the mouse is down.
- **the rocker** (`lift ^` / `lift v`), **PLAY** (`wake`) and **the dots** (`lucid`).
- **the eight LEDs**, showing what the lamp is doing.
- **what your hands do in this scene:** every control, and every inlet the playing scene
  sends it to (for example `lean1  hall.feedback, voice.index`). This is your performance map
  for the SP-1. Click a row to find that mapping in the journal. A dash means the control
  does nothing in this scene.

**With the SP-1 plugged in and on, the panel follows it live.** Its faders move with yours,
its buttons light as you press them, its LEDs show what the SP-1's LEDs show, and **•• +
rocker** on the SP-1 changes the app's scene too. A green **SP-1 live** light shows the
connection. The mouse and keys keep working: a hardware control only takes over when you
move it.

When you are not typing in the journal, the computer keyboard plays the hands. **1–4** are
the doors, **↑ ↓** the rocker, **space** is PLAY and **.** is the dots. **.** + **↑ ↓** changes
scene.

## Scenes

Every dated heading in the journal is a scene, and each scene is its own patch. The scene
buttons under the toolbar switch between them. On the SP-1, and on the hands panel, hold the
dots and press the rocker (**•• + FFWD / RWD**, or `.` + ↑ / ↓ on the keyboard). The rocker on
its own is always `lift`, free to patch. **+ scene** adds today's date as a new scene. A
patch can also travel by itself: anything sent into `journey.next` or `journey.back` changes
scene, and `journey.miss` is the chance of getting off at the wrong stop.

## The meters

- **load:** how much of the SP-1's processor the scene needs. Every place's cost was measured
  on a real SP-1, so this is close to what the SP-1 will report itself (within a few percent).
  It turns amber above 85% and red above 100%. Keep every scene under about 90%: a scene over
  100% stutters on the SP-1, and **send to SP-1** warns you before sending one. Plain wires
  cost nothing; cords with parts in them, or several cords into one inlet, cost a little;
  reverbs, choruses and the vocoder cost the most.
- **memory:** how much of the SP-1's 80 KB of room for delays, grains and gestures the
  scene uses.
- **ears:** the level coming out.

What fits these meters plays on the SP-1 as it plays here, because the computer runs the
same engine code the SP-1 runs.

## Sending a dream to the SP-1

Connect the SP-1 (running the Sleepcousonique firmware, see section 3) by USB and press
**send to SP-1** (⌘U). The editor finds it, sends the whole file, and the SP-1 starts playing
it and keeps it in the playing slot. Nothing is reflashed. A dream with mistakes is not
sent.

---
