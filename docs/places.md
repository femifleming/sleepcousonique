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

`the NAME is PLACE NATURE`. The nature is optional, and each place has its own default. You can
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


# The places

Every place has a fixed *nature* (the number after its name). Nothing has knobs: you change a place only by running something into its inlets. The first inlet and the first outlet need no name: `voice -> window` means `voice.out -> window.in`.


## Sources

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **aira** | 3 | pitch, fm | out | the fountain labeled aira: a pure tone, nature x 60 Hz |
| **twin** | 3 | pitch, index, timbre | out, a, b | two partials, 60n and 60(n+1); index bends a with b, timbre folds it. Their difference is the hum |
| **rain** | 1 | density | out, pulse | drops on tiny resonators, 8 x nature per second; pulse marks each drop |
| **thunder** | 1 | strike | out | a rumble when struck, and on its own about nature times a minute |
| **flood** | 1 | level | out | rising water: noise whose band climbs with the level |
| **wind** | 1 | gust | out | a cold draft: soft noise through a wandering band |
| **frog** | 3 | wake, pitch | out, pulse | nature frogs (1-8) croaking on their own; a pulse into wake makes them all croak |
| **elevator** | 1 | rate, pitch | out | the lift into the night sky: rises forever (rate in octaves per second, negative falls) |
| **bell** | 1 | strike, pitch, decay | out | the cargo train's deep bell: four modes, struck by edges or by sound |

## Shapes

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **mountains** | 1 | in, fold | out | dark umber peaks: a wavefolder. More fold, more peaks |
| **window** | 1 | in, light | out | a lamp on a photocell (lowpass gate). Pulse the light for a pluck |
| **basement** | 1 | in, cut, res | out, band | the house show downstairs: a warm resonant lowpass at nature x 480 Hz |
| **horns** | 1 | in, band1, band2, band3, band4, band5, band6, band7, band8 | out | horn-loaded speakers: eight fixed resonant bands; patch into a band to raise it |
| **lighter** | 1 | in, flick | out | the lighter that explodes: a flick of fire and drive |
| **copper** | 1 | in | out | the grounding rod: takes the DC out |
| **blanket** | 1 | in, gain | out | a blanket over the sound: plain amplifier |
| **crystal** | 0 | in1, in2, in3, in4, turn | out1, out2, out3, out4 | the crystal matrix: four in, four out, mixed by turning (nature sets the resting angle) |

## Spaces

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **tunnel** | 2 | in, pitch, decay | out | tubular acoustics: a comb tuned to nature x 60 Hz. Pulses pluck it |
| **hallway** | 6 | in, length, feedback | out | the long grey hallway: a delay, nature x 50 ms long |
| **warehouse** | 2 | in, size | out, l, r | a big room with wooden floors: reverb |
| **moss** | 1 | in, bloom | out | golden moss in a night pond: grains of what you plant, or of itself |

## Currents

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **stairs** | 1 | in, start, rise, fall | out, end | the spiral staircase (universal slope): rises and falls when started, follows otherwise; end into start cycles |
| **jakes** | 1 | rate, follow | out | they look like people but aren't: smooth wandering random, or an imitation of follow |
| **connors** | 1 | clock | out, pulse | stepped random, new on each clock pulse (or nature / 2 times a second on its own) |
| **escalator** | 1 | in, clock, glide | out | smooth & stepped: holds the input on each pulse and glides to it |
| **train** | 8 | next, back, reset | out, pulse, step | the train and its stations (semitones): the train route is train 8: 0 3 7 10 |
| **pufferfish** | 1 | in | out, pulse | plays quantized notes: just major, just minor, pentatonic, otonal, utonal (nature 1-5) |
| **cat** | 1 | in, learn | out | learns a gesture while learn is held, then mirrors it back, over and over (up to 8 s) |
| **magician** | 1 | in, shush | out, trance | shushes the sound when shush is loud; trance is a slow swing |
| **bottle** | 0 | — | out | a kept value: bottle 40 is 0.40 |

## Hands (the SP-1's controls)

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **lean** | 1 | — | out | faders: lean1 - lean4, 0 to 1 |
| **door** | 1 | — | out | track buttons: door1 - door4, 1 while held |
| **lift** | 0 | — | out, up, down | the rocker: out is -1, 0 or 1; up and down pulse |
| **wake** | 0 | — | out | PLAY, 1 while held |
| **lucid** | 0 | — | out | the dots button, 1 while held |

## Ears (where things leave)

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **left** | 0 | in | — | left ear |
| **right** | 0 | in | — | right ear |
| **ears** | 0 | in | — | both ears |
| **lamp** | 0 | in, track, play | out | the eight LEDs: sound lights both rows as a bar; track and play light one row each |
| **journey** | 0 | next, back, miss | — | rides to the next or last scene on a pulse; miss is the chance of the wrong stop |
