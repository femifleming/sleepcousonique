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
  dated entries. The modules (places) have plain names for what they are: `vco`, `lowpass`,
  `adsr`, `dusg`, `delay`, `quantizer`.
- **Peter Blasser's Nabra**, the knobless tapestry synthesizer. There are no knobs. Every
  place has a fixed nature and maybe a few switches, set in the journal; everything else
  changes only by patching something into it. Cords
  carry parts: a resistor, a capacitor or a diode.
- **Buchla and Serge.** From Buchla come three kinds of signal (sound, current and pulse),
  timbre before filtering, and the lowpass gate. From Serge come any signal into any inlet,
  stacked cords that add, and the universal slope (`slope`, `dusg`).
- **The Nord Modular editor.** You patch on screen and hear it live, with morph groups
  (here, *lucid* groups), a load meter, and patches sent to the hardware.

![The editor: the journal on the left, the tapestry on the right, the SP-1's hands below](fig/view1.png)

---

# 1. The editor

## Installing it

| | |
|---|---|
| **macOS** (11 or later, Apple silicon or Intel) | `brew tap femifleming/tap` then `brew install --cask femifleming/tap/sleepcousonique`; or download `sleepcousonique-macOS.zip` from the releases page and drag **Sleepcousonique.app** to Applications |
| **Windows** (10 or 11, 64-bit) | download `sleepcousonique-Windows.zip` from the releases page, unzip it anywhere and run **Sleepcousonique.exe** |

Releases: **github.com/femifleming/sleepcousonique/releases**. Each zip also holds the SP-1
firmware (`sleepcousonique-sp1.bin`), this manual, `FLASHING.txt` and the example dreams.
The macOS app is not notarised: the Homebrew cask clears the download quarantine for you;
from the zip, right-click the app ▸ Open the first time.

## Opening it

Open **Sleepcousonique**. The first time, it opens *first night*, an example dream with
three scenes. `File ▸ Load a history of modular` opens a second example: six scenes, from a
1964 Moog room to 2016 eurorack, each built from the places of its era. Sound comes out of your computer at once. The **sound** button (top right)
picks the audio device.

`File ▸ Open` opens a `.dream` file, and `File ▸ Save` saves one (⌘O, ⌘S). A dream is a plain
text file. You can write it in any text editor, keep it in a notes app, or paste it from
an email.

## Looks

`View ▸ Look` picks how the editor looks: **journal** (the original and the default: splatter
canvas, cream boards, Dymo labels) or **openprocess** (after the openprocess bundle: the place
browser grids along the top, a settings column for the selected place, checker rails and
rings for the SP-1's hands, the bracket meters and the wordmark). `View ▸ Light or dark`
picks **light** (the default), **dark**, or **follow the computer**, which changes with macOS's
or Windows' own appearance setting. All of it is remembered.

![openprocess, dark](fig/look-openprocess-dark.png)

![openprocess, light](fig/look-openprocess-light.png)

In the openprocess look, click a square in the browser grids to put that place on the
tapestry (hover to read what it is). Select a place and its settings appear in the column on
the right: drag a bar (it plays as you drag), double-click to put it back, click a switch to
step it. The undo and redo buttons sit beside the passages; the **?** squares open the
references and the flashing guide.

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
PLAY and the dots. **SP-1** on the right holds the outputs, the eight LEDs (`leds`, lit live on
the board) and `scene` changes. Right-click a place to set its switches (a DUSG's cycle and
range, a quantizer's scale); the journal line changes with it.

| to | do this |
|---|---|
| add a place | right-click the canvas ▸ sources / shapes / spaces / currents. Each entry says what the place does and, in brackets, what it comes from |
| patch | drag from an outlet to an inlet (or from an inlet back to an outlet) |
| choose the cord's part | the **-> hall / ~> fog / => water / >\| door** buttons choose the part for new cords |
| remove a cord | option-click it (option-click a loop to pull out every cord there) |
| stack a cord | cmd-drag a cord: a new one leaves from the same outlet (or just drag from the outlet again) |
| see code in the patch | select lines in the journal: their places and cords light up gold in the tapestry |
| lights, cables | View ▸ Room light dims the canvas and boards while cords and LEDs glow; View ▸ Cable opacity, Cable slack (tight to very loose), Cable routing (hanging curves, or around the boards like Max) |
| find my patch | the button at the bottom right, double-click the empty canvas, or cmd-0; right-click ▸ find... jumps to any place |
| undo, redo | cmd-Z, shift-cmd-Z (or the Edit menu): cords, moves, places, switches, knobs and typing, all in one history |
| change a cord | right-click it: change its part, its amount, show it in the journal, or remove it |
| set a cord's amount | double-click it |
| turn a knob | drag a board's **tune** knob (top corner: base pitch, in octaves) or an inlet's **attenuverter** (the little knob by each inlet of a source) up or down; shift for fine steps, double-click to reset. The sound follows as you drag |
| change a kept value | drag a value tag (the little white tag by an inlet) up or down; hold shift for fine steps. The sound follows as you drag |
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

- **four faders** (`fader1`–`fader4`), each with a copper loop. Drag the loop onto any inlet on
  the tapestry to map that fader there.
- **four track buttons** (`button1`–`button4`), held while the mouse is down.
- **the rocker** (`rocker ^` / `rocker v`), **PLAY** (`play`) and **the dots** (`dots`).
- **the eight LEDs**, showing what `leds` is doing.
- **what your hands do in this scene:** every control, and every inlet the playing scene
  sends it to (for example `fader1  hall.feedback, voice.index`). This is your performance map
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

## Snapshots and the timeline

The strip above the SP-1's hands (View ▸ Snapshots and timeline) is for composing, after
openprocess. A **snapshot** keeps what's sounding: every cord amount and kept value, every
tune, attenuverter and param, the four faders, and the scene, by name, so it still works
after you edit the dream.

| to | do this |
|---|---|
| keep a snapshot | click an empty square (shift-click a kept one to keep it again; option-click clears it) |
| go to it | click it: **cut** jumps there, **glide** gets there smoothly over the time beside it (click the time to change it: 0.5 to 30 s) |
| put it on the timeline | drag its square onto the lane |
| set a segment's length | drag its right edge (half seconds) or right-click ▸ length |
| cut or glide | right-click a segment: **cut** holds its snapshot, **glide** morphs from it into the next one over its length (the white ramp) |
| play | the red flag (or space with the strip selected); **loop** goes round again; click the lane to move the playhead |
| remove a segment | select it and press delete, or right-click ▸ remove |

Everything the timeline plays goes the same way as your own knobs: to the sound here, and to
every SP-1 in Bluetooth mode at once. Snapshots and the timeline are kept in the dream as
`;~snap` and `;~timeline` lines at the end, which the engine ignores; undo covers them.

## Scenes

Every dated heading in the journal is a scene, and each scene is its own patch. The scene
buttons under the toolbar switch between them. On the SP-1, and on the hands panel, hold the
dots and press the rocker (**•• + FFWD / RWD**, or `.` + ↑ / ↓ on the keyboard). The rocker on
its own is always `rocker`, free to patch. **+ scene** adds today's date as a new scene. A
patch can also change scene by itself: anything sent into `scene.next` or `scene.back` changes
scene, and `scene.miss` is the chance of landing on a random one.

## The meters

- **load:** how much of the SP-1's processor the scene needs. Every place's cost was measured
  on a real SP-1, so this is close to what the SP-1 will report itself (within a few percent).
  It turns amber above 85% and red above 100%. Keep every scene under about 90%: a scene over
  100% stutters on the SP-1, and **send to SP-1** warns you before sending one. Plain wires
  cost nothing; cords with parts in them, or several cords into one inlet, cost a little;
  reverbs, choruses and the vocoder cost the most.
- **memory:** how much of the SP-1's 80 KB of room for delays, grains and gestures the
  scene uses.
- **out:** the level coming out.

What fits these meters plays on the SP-1 as it plays here, because the computer runs the
same engine code the SP-1 runs.

## Sending a dream to the SP-1

Connect the SP-1 (running the Sleepcousonique firmware, see section 3) by USB and press
**send to SP-1** (⌘U). The editor finds it, sends the whole file, and the SP-1 starts playing
it and keeps it in the playing slot. Nothing is reflashed. A dream with mistakes is not
sent.

---
