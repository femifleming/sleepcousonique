# Sleepcousonique

*A patch language for the SP-1, written like a dream journal.*

Sleepcousonique grows out of four things: the dream journal at femishonuga.com, Peter Blasser's
Nabra (the knobless tapestry synthesizer), the West Coast thinking of Buchla and Serge, and
the Nord Modular's editor. A patch is a dream. Its modules are **places**: oscillators, filters,
envelopes and the rest, each named for what it is. They are joined by **passages**, which are Nabra's cords with
parts soldered into them. There are no knobs. Every place has a fixed nature and maybe a few
switches, set when you name it; everything else changes only by running something into it.

You can write a dream as plain text (the **journal**) or build it on the **tapestry**. Both
are the same dream: whatever you do on one shows up on the other.

## A dream

```
; anything after a semicolon is a note to yourself
drift 3                         ; natures wander 3% from night to night

2026-10-08
----------
the route is seq 8: 0 7 12 15 12 7 3 -5
the tick is slope
the voice is sine 2
the hall is delay 9

tick.end -> tick.trig route.next
route ~70> voice.pitch
voice -> hall -> out
fader1 -> hall.feedback
```

A line with dashes under it is a **scene**: one dream inside the file. It is usually a date,
but any words will do. Lines before the first scene are the **prelude**, which every scene
shares. To change scene, hold the dots and press the rocker (**•• + FFWD / RWD**). A dream can
also change scene by itself through `scene`.

## Naming

    the hall is delay 9
    the frogs are frogs 5
    the route is seq 4: 0 3 7 12        ; a sequencer and its steps, in semitones
    the dark is value 40                ; a kept value: 0.40
    the env is dusg cycle1 audio        ; switches: see below

`the NAME is PLACE NATURE SWITCHES`. The nature is optional, and each place has its own default. A name can
be anything, even another place's name: inside that dream your name wins, so new places
added to Sleepcousonique later can never break a dream you've already written. You can
also skip naming and use a place directly: `rain -> out` makes a rain, and `sine3` makes a
sine with nature 3.

## Passages

    a -> b        hall:  a plain wire
    a ~> b        fog:   a resistor, half as loud and softened
    a => b        water: a capacitor, slow to fill and slow to drain (60 ms)
    a >| b        door:  a diode, which lets only the positive half through
    a -40> b      a number inside the arrow sets the amount: 40%, or 40 ms for water
    a -> b c d    one to many
    a b -> c      many to one (several cords into one inlet add up, like stacked bananas)
    a -> b -> c   a chain
    40 -> b.x     a kept value (or: value 40 -> b.x)

A dot picks an inlet or outlet: `env.end -> env.trig`. Without a dot you get the
first one. A cord can loop back on itself, and the loop hears the sound from a moment ago.

## Signals

There are three kinds of signal, as on a Buchla. **Sound** swings about −1 to 1 and is drawn
umber. **Current** sits about 0 to 1 and is drawn deep blue. A **pulse** is 0 or 1 and is drawn gold. Any
of them can go into any inlet, as on a Serge. Pitch inlets count octaves: 1.0 is one
octave up. Tuning starts from the 60 Hz hum: `sine 3` is 180 Hz.

## Where the places come from

There are about 125 places, each named for what it is: `vco`, `lowpass`, `adsr`, `delay`,
`quantizer`. The help line says which instrument or idea it comes from: Moog, Buchla (including
the 100 series: 106, 107, 110, 123, 140, 144, 148, 156, 158, 180), Serge (DUSG, Dual Processor,
Peak & Trough), Doepfer A-100, ARP, EMS, Oberheim, Roland, Ciat-Lonbarde, Mutable Instruments,
Make Noise, Music Thing, the TR-808, the Space Echo, Dattorro's plate, Max/MSP and Pd,
SuperCollider and the Nord Modular. The plain modules (mixers, VCAs, slews, envelopes, logic)
cost very little, so big patches fit. Old dreams written with the earlier names still play;
the journal notes what each old name is called now, and `tools/modernize.py` rewrites them. Right-click the tapestry to browse them by family. The full list, with every
inlet and outlet, is at the end of this chapter.

## Switches and params

Some places have switches and params. They are fixed for the dream, like the nature, and are
written after it:

    the env is dusg cycle1 range=audio curve=exp
    the q is quantizer scale=pelog
    the t is trigger length=10

`name=position` sets a switch, `name=number` sets a param. A switch's name alone means its
second position (`cycle1` is `cycle1=on`), and a position word alone picks it when only one
switch has it (`audio`, `pelog`). The first position is the default. On the tapestry,
right-click a place to set its switches. The full list is in the switches column below.

## Difference tones

`difftone` is two pure sines, f1 and f2 = f1 x ratio, made for the tones your own ears add:
f2-f1 (the quadratic difference tone) and 2f1-f2 (the cubic), after Alex Chechile's
*Practical Applications of Difference Tones in Electronic Music Composition and Synthesis*
(2020). The module adds nothing nonlinear: the difference tones happen in the ear.

    the ear is difftone ratio=1.2      ; f1 960 Hz, f2 1152 Hz: hear 192 Hz and 768 Hz
    fader1 -> ear.ratio                ; the ratio inlet adds to the knob

Its **ratio** knob (1 to 3) and **tune** knob (the master pitch) sit on the board. Ratios near
1.2 give the strongest cubic tone, which fades as the ratio widens; the quadratic tone holds
from about 1.1 to 1.5. Keep f1 around 1-2 kHz (the default, nature 16, is 960 Hz), play it at
a healthy level, and send `out` (both tones) to the same ear.

## Scales from the scales folder

The quantizer knows 38 scales by heart, and any Scala (`.scl`) file in the scales folder,
`Documents/Sleepcousonique/scales` (right-click a quantizer ▸ scale ▸ *open the scales folder*).
Drop a `.scl` file in and use it by its file name, in lower case:

    the q is quantizer scale=young_wtp
    the r is quantizer scale=raga_yaman

It comes with La Monte Young (`young_wtp`, The Well-Tuned Piano; `young_224_288`, the Dream
House harmonics), the 22 shrutis (`shruti22`), sixteen ragas in just intonation (`raga_yaman`,
`raga_bhairav`, `raga_bhairavi`, `raga_todi`, `raga_marwa`, `raga_malkauns`, ...), Wendy Carlos
(`carlos_harmonic`, `carlos_superjust`), Stockhausen's Studie II (`stockhausen_studie2`, 25 steps
of 5:1), Busoni's third and sixth tones, John Pierce's eight-tone scale and Terry Riley's Harp of
New Albion. Scales you delete stay deleted.

The SP-1 has no files, so the editor writes the scales your dream names into the dream itself,
at the end, whenever it plays or sends it:

    tuning young_wtp: 176.6450 203.9100 ... 1200.0000

cents of each degree after the root, the last being the period (usually 1200). You can write a
tuning line yourself too; a long one goes on over several lines with the same name.

## Saving the SP-1's processor

The SP-1 has to finish every block of sound in time. The load meter shows the editor's
estimate; with an SP-1 plugged in it also shows what the SP-1 measures, average and peak. Keep
the peaks under 100%: past that the SP-1 drops blocks (heard as dropouts) and says so.

**samplerate=** runs any place at a lower rate: `full` (48 kHz), `half`, `quarter` or `eighth`
(6 kHz). Half the rate is about half the work. Pitches stay where they are; the sound gets
darker and grittier (the outlets are held between samples, like a sample-rate reducer), and
delays and reverbs get longer. Currents (LFOs, envelopes, followers, quantizers, sequencers)
lose almost nothing at `eighth`.

    the wobble is lfo eighth
    the room is reverb samplerate=half

(There is no bit-depth setting: the SP-1 works in floating point, where fewer bits cost the
same. For the sound of fewer bits, use `bitcrush`.)

**Bare places** do one job with one outlet: `saw`, `tri`, `pulse`, `white`, `follow`
(envelope follower), `onepole` (the gentlest lowpass); `sine` with nothing in its `fm` and `vca`
are already bare. **Lite places** are cut-down versions of heavy ones: `reverblite`,
`choruslite`, `delaylite`, `fmlite`, `supersawlite`, `froglet` (one frog).

## Tune and attenuverters (the knobs on the module)

Every place with a `pitch` inlet (the oscillators, frogs, bell, string, kick, comb...) has a
**tune** knob: its base pitch, in octaves, added to whatever reaches `pitch`. It reaches twelve
octaves either way, from well under a hertz (a slow LFO) to past 15 kHz:

    the a is sine tune=-12          ; one swing every minute or so
    the b is frogs 3 tune=4         ; frogs four octaves up

Every source, and every place with a `pitch` inlet, also has an **attenuverter** on each inlet,
`att.INLET`. It scales what's patched into that inlet, never the inlet's own default: 1 is as is
(and isn't written), 0 shuts the cord out, negative turns it upside down. `att.pitch` goes to
+-10, so one fader can sweep ten octaves; the others go to +-2.

    the c is vco tune=2.5 att.pitch=10 att.fm=-0.5
    fader1 -> c.pitch               ; fader1 now sweeps ten octaves up from c's tune

On the tapestry the tune knob sits in the board's top corner and the attenuverters next to the
inlets. Drag them up and down (shift for fine; tune moves in semitones, or cents with shift);
the sound follows as you drag and the journal line changes when you let go. Double-click a knob
to put it back (tune 0, attenuverter 1).

## Lucid groups (morphs)

    lucid fader2: peaks.fold 0..90, env.rise 20..5, gate.level 0..60

One fader leans on many inlets at once, each with its own range (in hundredths).

## Hands

| word | on the SP-1 | gives |
|---|---|---|
| fader1 – fader4 | the four faders | 0 to 1 |
| button1 – button4 | the track buttons T1–T4 | 1 while held |
| rocker, rocker.up, rocker.down | the rocker (FFWD / RWD) | −1, 0, 1, and pulses (not while •• is held: that's the scene change) |
| play | PLAY | 1 while held |
| dots | the •• button | 1 while held |

On the computer, keys 1–4 are the buttons, ↑ and ↓ are the rocker, space is PLAY and `.` is
the dots (while you are not typing in the journal). `.` + ↑ / ↓ changes scene, as on the SP-1.

## Outputs

`left`, `right` and `out` (both) are where sound leaves. `leds` lights the eight LEDs: sound
into `leds` makes a bar across both rows, and `leds.track` and `leds.play` light one row each.
`scene.next` and `scene.back` go to another scene on a pulse, and `scene.miss` is the
chance of landing on a random one instead.

## Dream logic

- **Drift.** `drift 3` lets the natures of tuned places wander up to 3% each time the dream
  is opened, so the same dream is never quite the same night twice.
- **Forgiveness.** A word one letter away from a place or a name is understood, with a
  note: `delya` is taken as `delay`.
- **Mistakes.** While the journal has mistakes, the last dream that made sense keeps playing.

## The SP-1's limits

The editor shows **load** (how much of the SP-1's processor the dream needs) and **memory**
(its 80 KB of room for delays, grains and so on). Keep both under 100%, and what you hear on
the computer is what the SP-1 plays. Every dream can have up to 64 places, 224 passages and
16 scenes.
