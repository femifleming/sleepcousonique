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
```

A line with dashes under it is a **scene**: one dream inside the file. It is usually a date,
but any words will do. Lines before the first scene are the **prelude**, which every scene
shares. To change scene, hold the dots and press the rocker (**•• + FFWD / RWD**). A dream can
also travel by itself through `journey`.

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
| lift, lift.up, lift.down | the rocker (FFWD / RWD) | −1, 0, 1, and pulses (not while •• is held: that's the scene change) |
| wake | PLAY | 1 while held |
| lucid | the •• button | 1 while held |

On the computer, keys 1–4 are the doors, ↑ and ↓ are the rocker, space is PLAY and `.` is
the dots (while you are not typing in the journal). `.` + ↑ / ↓ changes scene, as on the SP-1.

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
| **turntable** | 4 | pitch, fm, width, sync | saw, square, tri, sine | the turntables in the radio basement: the classic oscillator, saw / square / triangle / sine, width and hard sync (after Moog 921, the Minimoog) |
| **pendant** | 2 | pitch, tilt | out, a, b | sacred sandwiched multivibrator pendants: two squares that lean on each other, their xor as sound (after Ciat-Lonbarde's Fourses and Plumbutter) |
| **goblins** | 4 | pitch, chaos, loop | out, rungler, a, b | three hooded goblins on the rooftop: two triangles and a shift register eating its own tail; loop above 0.5 locks the pattern (after Rob Hordijk's Benjolin / rungler) |
| **multiwave** | 2 | pitch, tilt, odd, stretch | out | the Lakhovsky multiple wave oscillator: sixteen partials, tilt darkens, odd takes the even ones away, stretch bends them like a bell (after Lakhovsky; additive synthesis; Harmor) |
| **pulsar** | 2 | pitch, formant, duty | out | undulating pulsar wavetables: a burst of formant once a period, duty sets how much of it sounds (after Curtis Roads' pulsar synthesis; Kyma) |
| **torpedo** | 2 | pitch, ratio, index, feedback | out, mod | the smiling torpedo: two-operator FM at 240 Hz; nature is the ratio (after Chowning and the Yamaha DX7; Buchla 259's FM) |
| **beacon** | 2 | pitch, bend, resonant | out | the inevitably spherical probe beacon: phase distortion, bend brightens a cosine (after Casio CZ; Nord Modular's PD oscillator) |
| **record** | 2 | pitch, scan | out | the 7-inch with the light purple glitter label: one wave scanned from sine to triangle to saw to square to pulse (after PPG and Waldorf wavetables; Plaits) |
| **throne** | 4 | pitch, chord, spread | out | the velvet throne: a four-note chord in just intonation; chord picks major, minor, sus, maj7, min7, 4:5:6:7 (after Plaits' chords; string machines) |
| **rave** | 4 | pitch, detune, mix | out | the warehouse rave with ripple subwoofers: seven detuned saws (after Roland's JP-8000 supersaw) |
| **nuns** | 2 | pitch, vowel | out | nuns into cats into fairies, as the speaker preaches: a singing voice through a, e, i, o, u (after FOF and IRCAM's CHANT; formant oscillators) |
| **hum** | 1 | buzz, level | out | the 60 Hz hum the journal tunes to: mains buzz and its harmonics (after the just-intonation control writing) |
| **faucet** | 1 | level | white, pink, brown, blue | the generative freshwater faucet: four colours of noise (after every noise source since the Buchla 265) |
| **glitter** | 1 | chaos, density | crackle, dust | golden glitter: chaotic crackle and scattered dust (after SuperCollider's Crackle and Dust) |
| **spillway** | 1 | rate, rho | x, y, z | crashing into the icy spillway: a Lorenz attractor, slow as control or fast as sound (after Lorenz; chaos modules) |
| **marimba** | 6 | strike, pitch, decay, bright | out | the marimba: a struck wooden bar, three modes (after modal synthesis; Mutable Elements) |
| **rebar** | 4 | pluck, pitch, damp, bright | out | swinging on rebar: a plucked string (after Karplus-Strong; Fish's string; Mutable Rings) |
| **trebuchet** | 1 | strike, pitch, punch, decay | out | Connor's swinging wooden trebuchet: a kick drum, a sine that falls (after the TR-808 bass drum; Mutable Peaks) |
| **balloon** | 1 | strike, tone, snap | out | the balloon pop, the speculative gunshot: a snare (after the TR-808 and 909 snares) |
| **grill** | 1 | strike, decay, pitch | out | the sizzle of the grill: a metallic hi-hat from six squares (after the TR-808 cymbal circuit) |

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
| **shaft** | 1 | in, cut, res | out, band | the vertical shaft you climb out of: a four-pole transistor ladder at nature x 480 Hz (after Moog's 904A) |
| **pond** | 1 | in, cut, res | low, band, high, notch | the night pond: lowpass, bandpass, highpass and notch at once (after the Oberheim SEM; Serge VCFQ) |
| **preacher** | 1 | in, vowel, shift | out | as the speaker preaches: a vowel filter, a to u (after the Doepfer A-104 Trautonium formant filter) |
| **glass** | 1 | in, rate, depth, feedback | out | climbing through glass: a six-stage phaser (after the Mu-Tron Bi-Phase; the Small Stone) |
| **conduit** | 1 | in, rate, depth, feedback | out, wet | conduit to conduit, like stargates: a flanger (after the Electric Mistress; bucket-brigade delays) |
| **fairies** | 1 | in, depth | out, l, r | nuns into cats into fairies: a three-voice ensemble chorus (after the Solina and ARP string ensembles) |
| **pulley** | 2 | in, mod, pitch | out | the pulley system: a ring modulator, with its own carrier if nothing is patched to mod (after Bode; EMS VCS3; Buchla 285) |
| **stargate** | 1 | in, shift | up, down | a secret portal to other cities: a frequency shifter, up and down at once (after Harald Bode; Buchla 285) |
| **flashdrive** | 1 | in, crush, hold | out | the stolen 516 GB flash drive: fewer bits, fewer samples (after the SP-1200; decimators) |
| **tungsten** | 1 | in, drive, bias | out | tungsten lamplight at night: a warm, uneven tube stage (after tube preamps) |
| **pillow** | 1 | in, squeeze, side | out, gain | muffled into a pillow: a compressor; side listens to something else (after the LA-2A and 1176; Fish's press) |
| **blur** | 1 | a, b, mix | out | scenes blurring into each other: a crossfade from a to b |
| **radio** | 2 | carrier, voice | out | the DJ radio station: an eight-band vocoder; voice speaks through carrier, or through its own buzz (after the EMS Vocoder; Mutable Warps) |
| **sonder** | 1 | in, attack, release | out, gate | a glowing presence: an envelope follower, and a gate when something is there (after the ARP 2600 envelope follower) |
| **camera** | 1 | in, clock | held, track | taking photos of the basement: sample & hold and track & hold; samples noise when nothing is patched (after the ARP 2600; Buchla 266) |
| **velcro** | 1 | a, b | and, or, xor, not | the key velcroed to the hood: logic on pulses (after Serge and Doepfer logic) |
| **threshold** | 1 | in, ref | gate, inverse | standing at the threshold: a comparator, high while in is above ref (after Serge and Buchla comparators) |
| **puddle** | 1 | in | flip, fold, half, sign | reflections in a puddle: inverted, rectified, half-rectified and the sign (after Serge and Make Noise rectifiers; Fish's reflect) |
| **van** | 1 | in, place | l, r | the van in the driveway: places a sound from left (-1) to right (1) (after Buchla 227 panning) |
| **ripple** | 1 | in | down, down2 | the ripple subwoofers: one and two octaves under whatever comes in (after the Boss OC-2; sub-octave dividers) |

## Spaces

| place | nature | in | out | what it is |
|---|---|---|---|---|
| **tunnel** | 2 | in, pitch, decay | out | tubular acoustics: a comb tuned to nature x 60 Hz. Pulses pluck it |
| **hallway** | 6 | in, length, feedback | out | the long grey hallway: a delay, nature x 50 ms long |
| **warehouse** | 2 | in, size | out, l, r | a big room with wooden floors: reverb |
| **moss** | 1 | in, bloom | out | golden moss in a night pond: grains of what you plant, or of itself |
| **cabin** | 2 | in, decay | out | the cabin: a spring reverb, chirp and boing (after the Accutronics springs in the Buchla, VCS3 and ARP 2600) |
| **hotel** | 1 | in, decay, damp | out, l, r | the empty hotel: a plate reverb, two figure-eight tanks (after Jon Dattorro's plate; the EMT 140) |
| **balcony** | 6 | in, length, feedback | l, r | the party on the balcony: echoes that cross from ear to ear, nature x 25 ms (after ping-pong delays) |
| **dub** | 3 | in, time, feedback, wow | out, wet | dub echoes of the steaming train horn: a warm, wobbling tape echo, nature x 100 ms (after the Roland Space Echo; Echoplex; Serge's Wilson delay) |
| **memories** | 1 | in, hold, length | out | the rooms are memories: hold to freeze the last moment and stay inside it, nature x 250 ms (after Mutable Clouds' freeze) |
| **wires** | 2 | in, pitch, decay, chord | out | copper wiring in the plexiglass sculpture: four sympathetic strings tuned to a chord (after Mutable Rings; Fish's zither; sitar drones) |
| **persona** | 1 | in, shift | out | a morphing persona: moves the pitch, in octaves, without changing the time (after Eventide's H910) |
| **steam** | 1 | in, amount | out | steam off the train: smears a sound in time without an echo (after Schroeder's allpass diffusers) |

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
| **sleep** | 1 | gate, attack, decay, sustain, release | out, end | falling asleep and waking: attack, decay, sustain, release while the gate is held (after ARP and Moog envelopes; Buchla 281) |
| **moon** | 1 | rate, reset | sine, tri, saw, square | the moon over the night: an LFO, four shapes, 0 to 1 (after Doepfer's A-145) |
| **tram** | 4 | rate, reset | beat, eighth, half, bar | the aboveground tram: a clock from 40 to 320 bpm, nature beats to the bar (after Pamela's Workout; Tempi) |
| **floors** | 8 | clock, reset | d2, d3, d4, dn | the elevator's floors: a clock divided by 2, 3, 4 and by its nature (after Doepfer's A-160) |
| **maze** | 16 | clock, hits, rotate, reset | hit, miss, step | the endless underground maze: Euclidean rhythms over nature steps (after Toussaint; Euclidean sequencers) |
| **crossroads** | 1 | in, chance | a, b | tight turns on the estate: each pulse goes to a or b by chance (after Mutable Branches) |
| **notebook** | 16 | clock, change | out, pulse | the pink notebook: a looping pattern that slowly rewrites itself; nature is its length (after Music Thing's Turing Machine) |
| **party** | 1 | clock, x, y, density, reset | kick, snare, hat, accent | the house party you missed: kick, snare and hat patterns from a map; x and y move through it (after Mutable Grids) |
| **fire** | 1 | trig, count, rate | out | the school hall on fire: one pulse becomes a burst (after burst generators; Nord Modular's ratchets) |
| **insomniac** | 1 | clock, order | out, pulse | the insomniac lying awake: the logistic map, from order into chaos (after Robert May's logistic map) |
| **hourglass** | 1 | rate, reset | ramp, wrap | sand running through: a ramp from 0 to 1, slow or as fast as sound (after Max/MSP and Pd's phasor~) |
| **waitingroom** | 1 | in, time | out | waiting to be called: each pulse, again, later (after Doepfer's A-162 trigger delay) |
| **concierge** | 4 | in1, in2, in3, in4, next, reset | out, step | the hotel concierge: steps between up to four things on each pulse (after Doepfer's A-151; Buchla sequential switches) |
| **lilypads** | 30 | clock, seed | alive, pulse, cells | lily pads on the flooded street: a cellular automaton, nature is the rule (30, 90, 110...); pulse when the middle pad is alive (after Wolfram) |
| **wander** | 1 | step, clock | out | wandering the empty school: a random walk that turns back at the walls (after the Wogglebug; Buchla 266) |

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
