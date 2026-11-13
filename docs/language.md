# Sleepcousonique

*A patch language for the SP-1, written like a dream journal.*

Sleepcousonique grows out of four things: the dream journal at femishonuga.com, Peter Blasser's
Nabra (the knobless tapestry synthesizer), the West Coast thinking of Buchla and Serge, and
the Nord Modular's editor. A patch is a dream. Its modules are **places**, things and
creatures from the journal. They are joined by **passages**, which are Nabra's cords with
parts soldered into them. Nothing has knobs. Every place has a fixed nature, and you change it
only by running something into it.

You can write a dream as plain text (the **journal**) or build it on the **tapestry**. Both
are the same dream: whatever you do on one shows up on the other.

## A dream

```
; anything after a semicolon is a note to yourself
drift 3                         ; natures wander 3% from night to night

2026-10-08
----------
the route is train 8: 0 7 12 15 12 7 3 -5
the clock is stairs
the voice is aira 2
the hall is hallway 9

clock.end -> clock.start route.next
route ~70> voice.pitch
voice -> hall -> ears
lean1 -> hall.feedback
lift.up -> journey.next
```

A line with dashes under it is a **scene**: one dream inside the file. It is usually a date,
but any words will do. Lines before the first scene are the **prelude**, which every scene
shares. The rocker can carry you from scene to scene (see `journey`).

## Naming

    the hall is hallway 9
    the frogs are frog 5
    the route is train 4: 0 3 7 12      ; a train and its stations, in semitones
    the dark is bottle 40               ; a kept value: 0.40

`the NAME is PLACE NATURE`. The nature is optional, and each place has its own default. A name can
be anything, even another place's name: inside that dream your name wins, so new places
added to Sleepcousonique later can never break a dream you've already written. You can
also skip naming and use a place directly: `rain -> ears` makes a rain, and `aira3` makes a
fountain with nature 3.

## Passages

    a -> b        hall:  a plain wire
    a ~> b        fog:   a resistor, half as loud and softened
    a => b        water: a capacitor, slow to fill and slow to drain (60 ms)
    a >| b        door:  a diode, which lets only the positive half through
    a -40> b      a number inside the arrow sets the amount: 40%, or 40 ms for water
    a -> b c d    one to many
    a b -> c      many to one (several cords into one inlet add up, like stacked bananas)
    a -> b -> c   a chain
    40 -> b.x     a kept value (or: bottle 40 -> b.x)

A dot picks an inlet or outlet: `stairs.end -> stairs.start`. Without a dot you get the
first one. A cord can loop back on itself, and the loop hears the sound from a moment ago.

## Signals

There are three kinds of signal, as on a Buchla. **Sound** swings about −1 to 1 and is drawn
umber. **Current** sits about 0 to 1 and is drawn deep blue. A **pulse** is 0 or 1 and is drawn gold. Any
of them can go into any inlet, as on a Serge. Pitch inlets count octaves: 1.0 is one
octave up. Tuning starts from the 60 Hz hum: `aira 3` is 180 Hz.

## Where the places come from

There are about a hundred places. Each is named for something in the dream journal, and its
help line says which instrument or idea it comes from: Moog, Buchla, Serge, ARP, EMS,
Oberheim, Roland, Ciat-Lonbarde and Shbobo, Mutable Instruments, Make Noise, Music Thing,
Doepfer, the TR-808, the Space Echo, Dattorro's plate, Max/MSP and Pd, SuperCollider and the
Nord Modular. Right-click the tapestry to browse them by family. The full list, with every
inlet and outlet, is at the end of this chapter.

## Lucid groups (morphs)

    lucid lean2: peaks.fold 0..90, stairs.rise 20..5, window.light 0..60

One fader leans on many inlets at once, each with its own range (in hundredths).

## Hands

| word | on the SP-1 | gives |
|---|---|---|
| lean1 – lean4 | the four faders | 0 to 1 |
| door1 – door4 | the track buttons T1–T4 | 1 while held |
| lift, lift.up, lift.down | the rocker (FFWD / RWD) | −1, 0, 1, and pulses |
| wake | PLAY | 1 while held |
| lucid | the •• button | 1 while held |

On the computer, keys 1–4 are the doors, ↑ and ↓ are the rocker, space is PLAY and `.` is
the dots (while you are not typing in the journal).

## Ears

`left`, `right` and `ears` (both) are where sound leaves. `lamp` lights the eight LEDs: sound
into `lamp` makes a bar across both rows, and `lamp.track` and `lamp.play` light one row each.
`journey.next` and `journey.back` ride to another scene on a pulse, and `journey.miss` is the
chance of getting off at the wrong stop.

## Dream logic

- **Drift.** `drift 3` lets the natures of tuned places wander up to 3% each time the dream
  is opened, so the same dream is never quite the same night twice.
- **Forgiveness.** A word one letter away from a place or a name is understood, with a
  note: `hallwy` is taken as `hallway`.
- **Mistakes.** While the journal has mistakes, the last dream that made sense keeps playing.

## The SP-1's limits

The editor shows **load** (how much of the SP-1's processor the dream needs) and **memory**
(its 80 KB of room for delays, grains and so on). Keep both under 100%, and what you hear on
the computer is what the SP-1 plays. Every dream can have up to 48 places, 160 passages and
16 scenes.
