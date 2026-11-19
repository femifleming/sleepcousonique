# Building Sleepcousonique

### How a dream journal became a patch language, and how to make your own

This is the story of how Sleepcousonique was built: the decisions, the language, the code and
the mistakes. It ends with a way into making things like it yourself. It is the companion
to *Small Languages for Sound*, which grows music languages from Forth, PostScript and Uxntal,
languages made of **words on a stack**. Sleepcousonique is the other big family: a language
made of **things joined by wires**. Most modular environments are this kind: Max, Pd,
Reaktor, the Nord Modular, VCV Rack and SuperCollider's synth graphs.

You don't need to read the source to follow along. Every idea is shown small, and the folder
`docs/guide/tinydream/` holds a complete patch language in one C file (about 260 lines) that
you can build, run and grow through the exercises at the end.

---

# 1. What got built

<div class="diagram">
<svg viewBox="0 0 760 300" xmlns="http://www.w3.org/2000/svg" font-family="Menlo, monospace" font-size="12">
  <defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="#1d1b19"/></marker></defs>
  <rect x="10" y="20" width="150" height="70" rx="6" fill="#f3ead2" stroke="#7a4a2a"/>
  <text x="85" y="48" text-anchor="middle" font-weight="bold">journal</text>
  <text x="85" y="68" text-anchor="middle">the .dream text</text>
  <rect x="10" y="120" width="150" height="70" rx="6" fill="#f3ead2" stroke="#7a4a2a"/>
  <text x="85" y="148" text-anchor="middle" font-weight="bold">tapestry</text>
  <text x="85" y="168" text-anchor="middle">boards and cords</text>
  <path d="M85,90 L85,120" stroke="#1d1b19" marker-end="url(#a)"/>
  <path d="M95,120 L95,90" stroke="#1d1b19" marker-end="url(#a)"/>
  <text x="85" y="212" text-anchor="middle" fill="#7a4a2a">edits = text edits</text>
  <rect x="230" y="70" width="190" height="90" rx="6" fill="#e6ecf7" stroke="#1f3f8f" stroke-width="2"/>
  <text x="325" y="98" text-anchor="middle" font-weight="bold" fill="#1f3f8f">the engine (C99)</text>
  <text x="325" y="118" text-anchor="middle">parse → graph → run</text>
  <text x="325" y="138" text-anchor="middle">no malloc, no libm</text>
  <path d="M160,55 L230,100" stroke="#1d1b19" marker-end="url(#a)"/>
  <rect x="500" y="20" width="240" height="80" rx="6" fill="#fbf8f0" stroke="#1d1b19"/>
  <text x="620" y="48" text-anchor="middle" font-weight="bold">desktop app (JUCE)</text>
  <text x="620" y="68" text-anchor="middle">sound card, editor, meters</text>
  <rect x="500" y="140" width="240" height="80" rx="6" fill="#fbf8f0" stroke="#1d1b19"/>
  <text x="620" y="168" text-anchor="middle" font-weight="bold">SP-1 firmware (Zephyr)</text>
  <text x="620" y="188" text-anchor="middle">Wakes' hardware + the engine</text>
  <path d="M420,100 L500,60" stroke="#1d1b19" marker-end="url(#a)"/>
  <path d="M420,130 L500,180" stroke="#1d1b19" marker-end="url(#a)"/>
  <path d="M620,100 L620,140" stroke="#d2a531" stroke-width="2" stroke-dasharray="5,3" marker-end="url(#a)"/>
  <text x="610" y="125" text-anchor="end" fill="#a87a12">send: the text, over USB</text>
  <text x="325" y="260" text-anchor="middle" fill="#7a4a2a">the same engine file compiled twice: what you hear on the computer is what the SP-1 plays</text>
</svg>
</div>

Five pieces, and one idea holding them together:

| piece | what it is | lines |
|---|---|---|
| **engine** | the language and the sound: a parser that turns text into a graph, and a runner that turns the graph into samples. ~100 places | ~3,700 |
| **desktop app** | the journal, the tapestry, the SP-1 hands, meters and sending | ~3,000 |
| **firmware glue** | the engine on the SP-1: controls, LEDs, flash slots, the USB link | ~360 (plus Wakes) |
| **tests** | a command-line tool, a probe for every place, edit tests for the tapestry | ~250 |
| **docs** | the manual, generated partly from the engine itself | |

**The one idea: the text is the truth.** A patch is a text file. The tapestry is just another
way of looking at that text: dragging a cord *writes a line*, and deleting one *rewrites a
line*. The SP-1 receives the text, not some compiled blob. So a patch can be pasted into an
email, kept in a notes app, compared line by line, and opened in ten years by anything that
can read text.

---

# 2. The decisions, and why

Building something like this is mostly deciding. These are the decisions that shaped
everything else, roughly in the order they were made.

## Start from the hardware's limits

Before any language existed, the SP-1 set the rules:

- **64 MHz** Cortex-M4F processor, with a floating-point unit
- **256 KB of RAM**, for everything
- **48,000 samples a second**, which leaves about **1,333 processor cycles per sample** in total
- 4 faders, 4 track buttons, a rocker, PLAY, a dots button, 8 LEDs

Every later choice had to fit inside these. That's why the editor shows **load** and
**memory** meters: they are these limits, made visible while you patch.

## Why the engine is written in C, and "freestanding"

The engine had to run in two places: inside a Mac app, and on a chip with no operating
system to speak of. The only language that is comfortable in both is **C**, and specifically
*freestanding* C99:

- **No `malloc`.** All memory is reserved up front (a "pool": 80 KB at first, 124 KB since 1.4). On a tiny device,
  memory that can run out halfway through a performance is a bug waiting to happen. If a
  patch is too big, you find out when you load it, not in the middle of a song.
- **No math library.** `sin`, `exp2`, `tanh` and `log2` are written by hand in `scq_math.h`,
  in a few lines each. They are fast enough for the chip, and both machines compute
  *exactly the same function*, so the computer is a faithful preview.
- **No operating system.** The engine never opens a file, prints or sleeps. The app or the
  firmware around it does all of that.

This is the single most important decision: **one engine, compiled twice.** The desktop
app and the SP-1 firmware don't each have their own idea of what `delay` sounds like.
They share one.

## Why JUCE for the app

The desktop app is C++ with JUCE. Nabra, Partials and Quell were made the same way, and it
gives sound-card access, windows and drawing on Mac, Windows and Linux. The app links the C
engine directly. There's no translation layer: the app calls `scq_load()` and `scq_render()`
exactly as the firmware does.

(The first plan was a web page with the engine compiled to WebAssembly. You asked for a
desktop app instead, and because the engine was plain C, switching cost nothing.)

## Why Zephyr and Wakes for the firmware

The SP-1's audio chips have to be started in one particular order, the faders share a power
rail with the buttons, and the power button has to work even when everything else has
crashed. Wakes had already discovered all of that on real hardware. So the firmware
**keeps Wakes' hardware layer and replaces only its synth**. Rewriting proven hardware code
is the easiest way to brick a device, and borrowing it (with its licence kept) is
what open source is for.

## Why a language at all, instead of only boxes and wires

Pure visual patchers (Max, Reaktor) save patches in formats people don't read. Pure text
languages (SuperCollider) are hard to *see*. Sleepcousonique does both because the
text is designed to be **readable as a patch**: one line is roughly one cord. That's what
makes the tapestry possible. Every board and cord maps back to a line you could have typed.

---

# 3. Designing the language

## Start from a world, not from syntax

The language wasn't designed by choosing brackets. It started from two worlds you already
had:

1. **The dream journal.** Its form became the file: dated entries, scenes, notes to yourself.
   At first its places became the vocabulary too (a delay was a `hallway`, a reverb a
   `warehouse`, a clock a `tram`), but a patch is easier to read when a module is called what
   it is, so in November 2026 every place got a plain name (`delay`, `reverb`, `clock`). The
   old names still work, with a note.
2. **Peter Blasser's Nabra.** It has no knobs: every module has a fixed character, and you
   change it only by patching into it. Cords can carry parts: a resistor, a capacitor, a
   diode.

From those two came almost every rule:

| from | became |
|---|---|
| Nabra's fixed modules | a place has one number, its **nature**, plus a few fixed switches, and no knobs |
| Nabra's cords with parts | four passages: `->` wire, `~>` resistor (fog), `=>` capacitor (water), `>\|` diode (door) |
| journal entries | a patch is dated entries: a date with dashes under it is a **scene** |
| the journal's 60 Hz just-intonation writing | tuning starts from 60 Hz: `sine 3` is 180 Hz |
| Buchla | three kinds of signal: sound, current, pulse |
| Serge | any signal into any inlet, and cords into one inlet add up |
| Nord Modular | **lucid** groups: one fader moving many things |
| dreams themselves | `drift` (natures wander night to night), forgiving misspellings, missed stops |

**This is the most transferable lesson in the guide.** A language feels like something when its
rules come from somewhere. Fish feels like Shbobo because Blasser designed both. Orca feels
like a grid of little machines because it is one. Pick a world first, and let the grammar
fall out of it.

## The whole grammar

Sleepcousonique has only four kinds of line:

```
scene    :=  any words  NEWLINE  ---...
naming   :=  "the" NAME ("is"|"are") PLACE [NUMBER] [":" NUMBER...]
passage  :=  ends ARROW ends (ARROW ends)...
lucid    :=  "lucid" SOURCE ":" TARGET LOW..HIGH ("," TARGET LOW..HIGH)...
ends     :=  END (END)...            ; several ends: one to many, many to one
END      :=  NAME | NAME.PORT | NUMBER | "value" NUMBER
ARROW    :=  "->" | "~>" | "=>" | ">|"    ; "-40>" puts an amount inside the arrow
```

Plus `drift N`, and comments after `;`.

That's small on purpose. A language you can hold in your head is one you can *play*. Each
rule was tested against one question: could someone write this from memory at 3am?

## What was tried and rejected

- **Fish's nested lists** `(left (wave (horn ...)))`. You asked for your own language, and
  nesting hides the graph: a wire that goes *back* (feedback) is hard to write as a tree.
  One line per cord shows loops plainly: `hall => hall.length`.
- **Real sentences** ("the rain wakes the frogs"). They're lovely to read but miserable to
  parse reliably, and a patch that sometimes means something else is a bad instrument.
  `rain -> frogs` keeps the dream-like vocabulary with a grammar that never guesses.
- **Knobs.** Nabra says no. Values come from `value`s you patch in, and the editor draws them
  *like* knobs. You get the feel without breaking the rule.

## Errors in the voice of the world

A language's mistakes are part of its character. Sleepcousonique says *"nothing called 'hal'
lives here"*, *"the house is full"* when memory runs out, and *"dreamt 'delya' as a place you know"* when
it forgives a misspelling within one letter. Writing good error messages is half the work of
a friendly language, and they're the part people see most.

---

# 4. How the code works

Here is the engine's journey from text to sound, in the order it happens.

## Step 1: reading the text into tokens

The **tokenizer** cuts a line into words, numbers and arrows. The interesting part is the arrow,
because `-40>` is one token (a wire at 40%), while `-40` on its own is a number:

```c
if (c == '-' || c == '~' || c == '=') {
    int j = i + 1;
    while (j < n && (is_digit(s[j]) || s[j] == '.')) j++;   /* skip an amount */
    if (j < n && s[j] == '>') {                              /* ...then it was an arrow */
        o->t = T_ARROW;
        o->kind = c == '-' ? K_HALL : (c == '~' ? K_FOG : K_WATER);
        ...
```

Look ahead, decide, and consume. Nearly every tokenizer is a loop of "what does this
character start?"

## Step 2: understanding a line

Each line is one statement. The parser looks at the first token: `the` means naming, `lucid` means
a morph, and anything else is a chain of passages. A chain is read as **groups separated by
arrows**, and every end in one group connects to every end in the next:

```
voice -> peaks -> gate -> out leds
  └─ group ─┘ └─ group ─┘ └ group ┘ └ group ┘
```

That one rule gives you chains, one-to-many and many-to-one at no extra cost.

## Step 3: names become things

`resolve()` turns a word into a node (a running place), in this order:

1. a name you've already given (`voice`)
2. a place word, which creates itself (`rain` makes a rain)
3. a place word with a number (`sine3` makes a sine with nature 3)
4. a word one letter away from either, with a gentle note
5. otherwise: *nothing called that lives here*

Then `.port` picks an inlet or outlet by name. **A big lesson hides here:** the first
version refused a name like `pond` if a place called `pond` existed. When `pond` was later added
as a new filter, an old patch broke. The fix was to let *your* names win, so new vocabulary
can never break old writing. Languages live a long time, so design for growing them.

## Step 4: the graph

After parsing there is a list of **nodes** (places) and **cords** (passages). Two things happen.

**Cords are sorted by where they arrive**, so when a node runs, its incoming cords sit
side by side in memory.

**Nodes are put in order** with a depth-first walk: before a node runs, everything feeding it
runs first. A loop like `hall => hall.length` is cut where the walk finds it. That cord simply
hears the previous block, and that is all feedback is:

```c
static int visit(int k, uint8_t *mark, int *pos) {
    if (mark[k] == 2) return 0;        /* done already */
    if (mark[k] == 1) return 0;        /* we're inside it: a loop, cut it here */
    mark[k] = 1;
    for (each cord arriving at k) visit(cord.src, mark, pos);
    mark[k] = 2;
    order[(*pos)++] = k;
}
```

## Step 5: running it, in blocks

Sound is computed **16 samples at a time** (a *block*), not one by one. For each node, in
order:

1. **gather** its inputs: an unpatched inlet hears its default, and patched ones add up every
   cord. The cord's part does its work here: fog is a tiny lowpass, water is a slow slide,
   and a door lets only the positive half through.
2. **run** the place's function, which fills its outlets for the block.

Ears add into the left and right outputs, and a soft clip (`tanh`) keeps them in range.
Blocks matter for speed: the per-node bookkeeping happens once per 16 samples instead of
every sample. That's what lets a patch interpreter fit in 1,333 cycles per sample.

## Anatomy of a place

Every place is one function plus one row in a table. Here's the whole sine:

```c
/* sine — a sine oscillator at nature x 60 Hz */
static void run_sine(node *n, float in[MAX_IN][B])
{
    float base = 60.f * n->nature, ph = n->s[0];
    LOOP {
        float f = base * scq_exp2(in[0][i]) * (1.f + in[1][i]);
        ph += f * INV_SR;
        ph -= scq_floor(ph);
        n->out[0][i] = scq_sin2pi(ph);
    }
    n->s[0] = ph;
}

P("sine", FAM_SOURCE, PF_DRIFT, 30, 3, "pitch fm", "0 0", "out", run_sine, 0, 0,
  "sine oscillator at nature x 60 Hz"),
```

The table row is the place's whole public life: its name, family, cost, default nature,
inlets, their defaults, outlets, function and help. The **editor reads this same table**. The
add-place menus, the tapestry's loops and the manual's reference pages are all generated from
it, so adding a place in one spot updates everything. A single source of truth again.

## The parts that only make sense on a small device

- **The memory pool.** Places that need buffers (delays, reverbs, grains) ask for bytes when the
  scene is built. When the pool runs dry, you get *"the house is full"*. Big buffers are stored
  as 16-bit numbers to fit twice as much.
- **Scene changes on the SP-1** are built on the main thread, not the audio thread. The audio
  fades out and waits in silence, and the main loop builds the new scene, then the audio fades
  back in. Building takes a few milliseconds, and a block is only 2 ms.
- **The engine lock.** While a new dream loads, the audio thread *tries* a lock and, if it's
  busy, plays silence for that block rather than waiting. An audio thread must never
  wait, or the sound stutters and the device can lock up.

## The editor's trick: rewriting lines

When you delete one cord on the tapestry, its line might hold several cords (`voice -> hall
left right`). The editor doesn't try to edit that line cleverly. It **regenerates** it: the
engine reports every cord with the line it came from, the editor writes each remaining cord
back as its own simple line, and the line's comment is kept. Clever text surgery is fragile,
while regenerating from structure is simple and safe. There are tests for exactly this
(`tests/ModelTest.cpp`).

---

# 5. How it was actually written

The order of work, because order matters as much as code:

1. **Read the hardware first.** Pin maps, Wakes' notes and how the audio clock works, before
   writing anything.
2. **Engine before interface.** The language and runner were written and *heard* (rendered to
   files and measured) before any window existed.
3. **A command-line tool from day one.** `scqtool check` prints what the engine understood,
   `stats` renders each scene and reports level, silence and NaN, and `probe` watches every
   outlet of every place.
4. **Every place tested alone.** `tools/test_places.sh` builds a tiny patch around each
   place, feeds it a clock and a sound, and fails on silence, NaN or runaway values.
5. **Then the app, then the firmware**, each wrapped around the engine without changing
   it.
6. **Screenshots as tests.** The app can photograph its own window (`--shot`), so the
   interface was checked by looking, not guessed.

## Bugs that taught something

These are worth more than the code, because they're the kind you'll meet too.

| bug | what it taught |
|---|---|
| a staircase patched into itself never started | **feedback loops need a first push.** On a Serge, the end output is high at rest, so the loop kicks itself off. The fix: one end pulse at birth |
| the bell was nearly silent | **check the maths of gain.** A resonator's peak is about 1/sin(w), not 1/(1−r). Measure, don't assume |
| the catalog listed 59 of 103 places | **fixed buffers overflow silently.** The text buffer was 8 KB. Count the output, not just the input |
| a new filter called `pond` broke an old patch | **let user names win** over built-in words |
| `drift ~> voice.index` errored | **keywords shouldn't steal names.** `drift` is a setting only when a single number follows it |
| the cellular automaton never pulsed | **test with the defaults people will use.** Rule 90 rarely reaches the edge cell, so it now reads the middle |
| one scene went over the 80 KB of memory | **make limits visible.** The memory meter turns a crash into a number you can see |

---

# 6. Getting into it yourself

## The path

**Stage 1: read small languages.** You already love several, so read them as a builder now:

- **Orca** (Hundred Rabbits): a whole language that is a grid. Read its operator list and notice
  how each operator is one character with fixed inputs, just like a place.
- **Uxntal**: an entire computer in 32 instructions. It shows how little a language needs.
- **Fish / SHLISP** (`pblasser/shbobo`): read `tokes.c`, the opcode table. It is the ancestor of
  Sleepcousonique's place table.
- **Pd** or **Max**: patch something, then open the `.pd` file in a text editor. It's a
  graph written as lines, the same idea as a `.dream` file.
- Your own guide, **Small Languages for Sound**, for the stack-language side.

**Stage 2: build tinydream, then break it.** Everything in section 4, in one file you can read
in an evening:

```
cd ~/sleepcousonique/docs/guide/tinydream
cc -O2 -o tinydream tinydream.c -lm
./tinydream first.dream first.wav 4
open first.wav
```

Then do the exercises below, in order. Each one is a real feature of Sleepcousonique, made small.

**Stage 3: make it real-time.** Rendering to a file is the easiest way to start, because you
can listen, measure and repeat. After that, play to the sound card. In C, the smallest route
is the single-file library **miniaudio**: its callback asks for N samples, and you call
`tick()` N times.

**Stage 4: put it in an object.** A language becomes an instrument when it lives in
something with knobs and buttons: an SP-1, a Daisy Seed, a Teensy, a Raspberry Pi Pico. The
pattern from section 2 holds: freestanding C, fixed memory, an engine that never waits.

**Stage 5: build an editor around it.** Only now: the text is the truth, and the picture is
a view of it.

## Exercises (tinydream)

1. **A new place.** Add `noise` (white noise). Use a simple random generator in the node's
   `state`, so it needs no library. *(You're writing a place: one function, one table row.)*
2. **A new passage.** Add `~>` as a resistor: half as loud and softened by a one-pole lowpass
   inside the cord. The cord needs a little memory of its own. *(This is a Nabra part.)*
3. **Numbers as sources.** Let `40 -> fold.amount` work by treating a number as a constant at
   40%. *(This is a `value`.)*
4. **Outlets with names.** Give `saw` a second outlet, `square`, and let `voice.square -> out`
   pick it. Right now cords only read outlet 0.
5. **Scenes.** Read a line with dashes under it as the start of a new scene, then render only
   the scene named on the command line.
6. **Clean edges.** Replace the naive saw with polyBLEP (look at `saw_blep` in
   `engine/scq_more_sources.inc`) and listen to the difference at high pitches.
7. **Blocks.** Change `tick()` to compute 16 samples at a time. Time it before and after on a
   long render (`time ./tinydream ...`).
8. **Forgiveness.** When a word isn't found, look for a place one letter away and accept it
   with a note. *(See `near()` in `engine/scq_core.c`.)*
9. **Your own world.** Rename every place after something from *your* world (a city, a
   garden, a book) and write the error messages in its voice. This exercise is the
   point of all the others.

## Books and places to learn from

- **Crafting Interpreters**, Robert Nystrom (free online). The best book on building a
  language from scratch, from tokenizer to virtual machine. Read part one; part two is in C.
- **Designing Sound**, Andy Farnell. Synthesis thought of as patches, in Pd.
- **The Audio Programming Book**, Boulanger & Lazzarini. C for sound, from first
  principles.
- **Julius O. Smith's online books** (ccrma.stanford.edu/~jos): filters, delays, physical
  models. The `rebar`, `wires` and `hotel` places come from this world.
- **musicdsp.org** and the **KVR DSP forum**: snippets and folklore, like the polyBLEP and the
  Hilbert coefficients used here.
- **esolangs.org**: a thousand small languages, each one an idea.
- **Peter Blasser's *Stores at the Mall*** (Wesleyan, 2015): instruments as languages, by the
  person whose work runs through all of this.
- The source of **Wakes**, **Mutable Instruments' eurorack repo** and **Pd**. Read real
  instruments' code and compare it with what you'd have done.

## The short version

1. Pick a world before a syntax.
2. Keep the grammar small enough to remember at 3am.
3. Make the text the truth and every picture a view of it.
4. One engine, everywhere: no malloc, nothing that waits, limits you can see.
5. Hear it before you build a window. Test every piece alone.
6. Write the errors in the language's own voice.

---

*Sleepcousonique: engine, editor and firmware at `~/sleepcousonique`. tinydream at
`docs/guide/tinydream/`. The stack-language companion: `~/esolang-guide/small-languages-for-sound.pdf`.*
