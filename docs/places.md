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


# The places

Every place has a fixed *nature* (the number after its name), and some have switches and params, set when you name it (the first position is the default). Otherwise you change a place only by running something into its inlets. The first inlet and the first outlet need no name: `voice -> gate` means `voice.out -> gate.in`.


## Sources

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **sine** | 3 | pitch, fm | out | range=audio / lfo | sine oscillator at nature x 60 Hz |
| **crossfm** | 3 | pitch, index, fold | out, a, b | — | two sines on harmonics n and n+1; index frequency-modulates a with b, fold wavefolds a; out is both mixed |
| **rain** | 1 | density | out, pulse | — | rain: random impulses ringing tiny resonators, 8 x nature per second; pulse marks each drop |
| **thunder** | 1 | strike | out | — | thunder: a filtered noise rumble when struck, and on its own about nature times a minute |
| **noisesweep** | 1 | level | out | — | noise through a resonant bandpass whose frequency climbs with level |
| **wind** | 1 | gust | out | — | wind: soft noise through a slowly wandering bandpass |
| **frogs** | 3 | trig, pitch | out, pulse | — | nature (1-8) chirping FM voices on random timers; a pulse into trig makes them all chirp |
| **shepard** | 1 | rate, pitch | out | — | Shepard-Risset glissando: rises forever (rate in octaves per second, negative falls) |
| **bell** | 1 | strike, pitch, decay | out | — | modal bell: four inharmonic modes, struck by pulses or by sound |
| **vco** | 4 | pitch, fm, width, sync | saw, square, tri, sine | range=audio / lfo | VCO: saw, square, triangle and sine, pulse width and hard sync (after Moog 921, the Minimoog) |
| **xorsquares** | 2 | pitch, couple | out, a, b | — | two cross-coupled square oscillators, their XOR as sound (after Ciat-Lonbarde Fourses / Plumbutter) |
| **rungler** | 4 | pitch, chaos, loop | out, steps, a, b | — | two triangle oscillators and a shift register fed back on itself; loop above 0.5 locks the pattern (after Rob Hordijk's Benjolin) |
| **additive** | 2 | pitch, tilt, odd, stretch | out | — | additive oscillator: sixteen partials; tilt darkens, odd removes the even ones, stretch makes them inharmonic |
| **pulsar** | 2 | pitch, formant, duty | out | — | pulsar synthesis: a formant burst once a period, duty sets how much of it sounds (after Curtis Roads) |
| **fm** | 2 | pitch, ratio, index, feedback | out, mod | — | two-operator FM at 240 Hz; nature is the ratio (after Chowning, the DX7) |
| **phasedist** | 2 | pitch, amount, resonant | out | — | phase distortion oscillator: amount brightens a cosine (after Casio CZ) |
| **wavetable** | 2 | pitch, scan | out | — | wavetable oscillator: scans sine, triangle, saw, square, pulse (after PPG / Waldorf) |
| **chord** | 4 | pitch, type, spread | out | — | four-voice just-intonation chord; type picks major, minor, sus, maj7, min7, 4:5:6:7 |
| **supersaw** | 4 | pitch, detune, mix | out | — | seven detuned saws (after Roland JP-8000) |
| **formant** | 2 | pitch, vowel | out | — | formant voice oscillator through a, e, i, o, u (after FOF / IRCAM CHANT) |
| **hum** | 1 | buzz, level | out | — | 60 Hz mains hum and its harmonics |
| **noise** | 1 | level | white, pink, brown, blue | — | noise: white, pink, brown and blue |
| **crackle** | 1 | chaos, density | out, dust | — | chaotic crackle, and random dust impulses (after SuperCollider Crackle / Dust) |
| **lorenz** | 1 | rate, rho | x, y, z | — | Lorenz attractor, slow as modulation or fast as sound |
| **marimba** | 6 | strike, pitch, decay, bright | out | — | modal marimba: struck bar, three modes |
| **string** | 4 | pluck, pitch, damp, bright | out | — | Karplus-Strong plucked string |
| **kick** | 1 | strike, pitch, punch, decay | out | — | kick drum: a falling sine (after TR-808) |
| **snare** | 1 | strike, tone, snap | out | — | snare drum (after TR-808 / 909) |
| **hihat** | 1 | strike, decay, pitch | out | — | metallic hi-hat from six squares (after TR-808) |
| **sinesaw** | 3 | pitch, fm | sine, saw | range=audio / lfo | sine and sawtooth oscillator; range=audio|lfo (after Buchla 158) |
| **square** | 3 | pitch, fm, width | out | range=audio / lfo | square / pulse oscillator, width -1..1; range=audio|lfo (after Buchla 144; Doepfer A-110) |
| **harmonics** | 3 | pitch | h1, h2, h3, h4 | — | harmonic generator: the first four harmonics of one pitch, each on its own out (after Buchla 148) |

## Shapes

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **folder** | 1 | in, fold | out | — | wavefolder; more fold, more folds |
| **lpg** | 1 | in, level | out | mode=both / lowpass / vca | lowpass gate (vactrol-style); pulse level for a pluck |
| **lowpass** | 1 | in, cutoff, res | out, band | — | resonant 2-pole lowpass at nature x 480 Hz, with a bandpass out |
| **filterbank** | 1 | in, band1, band2, band3, band4, band5, band6, band7, band8 | out | — | fixed filter bank: eight resonant bands; patch into a band to raise it |
| **saturator** | 1 | in, burst | out | — | tanh saturator; a pulse into burst kicks the drive up with a crackle that dies away |
| **dcblock** | 1 | in | out | — | DC blocker (highpass at nature x 20 Hz) |
| **vca** | 1 | in, gain | out | response=lin / exp | VCA: in times gain |
| **matrix** | 0 | in1, in2, in3, in4, rotate | out1, out2, out3, out4 | — | 4 x 4 rotation matrix mixer; rotate turns it (nature sets the resting angle) |
| **ladder** | 1 | in, cutoff, res | out, band | — | 4-pole transistor ladder lowpass at nature x 480 Hz (after Moog 904A) |
| **svf** | 1 | in, cutoff, res | low, band, high, notch | — | state-variable filter: lowpass, bandpass, highpass and notch at once (after Oberheim SEM, Serge VCFQ) |
| **vowelfilter** | 1 | in, vowel, shift | out | — | vowel / formant filter, a to u (after Doepfer A-104) |
| **phaser** | 1 | in, rate, depth, feedback | out | — | six-stage phaser |
| **flanger** | 1 | in, rate, depth, feedback | out, wet | — | flanger |
| **chorus** | 1 | in, depth | out, l, r | — | three-voice ensemble chorus (after Solina / ARP string ensembles) |
| **ringmod** | 2 | in, mod, pitch | out | — | ring modulator, with its own sine carrier if nothing is patched to mod |
| **freqshift** | 1 | in, shift | up, down | — | frequency shifter, up and down at once (after Bode, Buchla 285) |
| **bitcrush** | 1 | in, crush, decimate | out | — | bit crusher and sample-rate reducer |
| **tube** | 1 | in, drive, bias | out | — | asymmetric tube-style saturation |
| **compressor** | 1 | in, amount, side | out, gain | — | compressor; side is an external sidechain |
| **crossfade** | 1 | a, b, mix | out | — | crossfade from a to b |
| **vocoder** | 2 | carrier, modulator | out | — | eight-band vocoder; modulator through carrier, or through its own buzz |
| **follower** | 1 | in, attack, release | out, gate | — | envelope follower, and a gate while something is there |
| **samplehold** | 1 | in, clock | held, track | — | sample & hold and track & hold; samples noise when nothing is patched |
| **logic** | 1 | a, b | and, or, xor, not | — | logic on pulses |
| **comparator** | 1 | in, ref | gate, inverse | — | comparator: high while in is above ref |
| **rectify** | 1 | in | invert, full, half, sign | — | inverter, full-wave and half-wave rectifier, sign |
| **pan** | 1 | in, position | l, r | — | panner: -1 left to 1 right |
| **suboctave** | 1 | in | sub1, sub2 | — | sub-octave divider: one and two octaves down |
| **mixer** | 0 | in1, in2, in3, in4, in5, in6 | out | — | six-input mixer; set levels with amounts on the cords (after Buchla 106; Doepfer A-138) |
| **vcmix** | 0 | in1, in2, in3, in4, level1, level2, level3, level4 | out | — | voltage-controlled mixer: four inputs with a level each (after Buchla 107; Doepfer A-135) |
| **vcgate** | 0 | a, b, levela, levelb | a, b | — | dual voltage-controlled gate / VCA (after Buchla 110) |
| **quadvca** | 0 | in1, in2, in3, in4, cv1, cv2, cv3, cv4 | out1, out2, out3, out4 | response=lin / exp | four VCAs; response=lin|exp (after Doepfer A-130 / A-131 / A-132) |
| **duallpg** | 0 | in1, cv1, in2, cv2 | out1, out2 | mode1=both / lowpass / vca, mode2=both / lowpass / vca | two low pass gates (vactrol response); mode1 / mode2 = both|lowpass|vca; pulse a cv for a pluck (after Buchla 292) |
| **highpass** | 1 | in, cutoff, res | out | — | resonant 2-pole highpass at nature x 120 Hz (after the Doepfer A-100 filters) |
| **attenuverter** | 0 | in, amount, offset | out | — | attenuverter and offset: in x amount (-1..1) + offset (after Doepfer A-183-2) |
| **sineshaper** | 0 | in, drive | out | — | sine waveshaper: a triangle in becomes a sine; drive folds it further (after Serge waveshapers) |
| **minmax** | 0 | a, b | max, min | — | the higher and the lower of two signals (after Serge Peak & Trough) |
| **cvprocessor** | 0 | a, b, offset, time | out | — | control voltage processor: sums a, b and an offset, then lags it by time (after Buchla 156) |
| **slew** | 0 | in, rise, fall | out | shape=lin / exp | slew limiter with separate rise and fall; shape=lin|exp (after Doepfer A-170) |
| **dualprocessor** | 0 | in1, rate1, in2, rate2 | out1, out2 | mode1=lag / slew, mode2=lag / slew | two voltage-controlled lag / slew processors: smooth control voltages, or a gentle lowpass on audio (rate 0..1 is 20 Hz-20 kHz); mode1 / mode2 = lag|slew (after the Serge Dual Processor) |

## Spaces

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **comb** | 2 | in, pitch, decay | out | — | comb filter tuned to nature x 60 Hz; pulses pluck it |
| **delay** | 6 | in, time, feedback | out | — | delay, nature x 50 ms long; time scales it |
| **reverb** | 2 | in, size | out, l, r | — | stereo room reverb |
| **granular** | 1 | in, density | out | — | granular: grains of what you patch in, or of its own buffer |
| **spring** | 2 | in, decay | out | — | spring reverb |
| **plate** | 1 | in, decay, damp | out, l, r | — | plate reverb (after Dattorro) |
| **pingpong** | 6 | in, time, feedback | l, r | — | ping-pong delay, nature x 25 ms |
| **tapeecho** | 3 | in, time, feedback, wow | out, wet | — | tape echo with wow, nature x 100 ms (after Roland Space Echo) |
| **freeze** | 1 | in, hold, length | out | — | freeze: hold loops the last nature x 250 ms |
| **sympathetic** | 2 | in, pitch, decay, chord | out | — | four sympathetic strings tuned to a chord |
| **pitchshift** | 1 | in, shift | out | — | pitch shifter, shift in octaves |
| **diffuser** | 1 | in, amount | out | — | allpass diffuser: smears in time without echoes |

## Currents

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **slope** | 1 | in, trig, rise, fall | out, end | cycle=off / on, range=lfo / audio, curve=lin / exp / log | slope / function generator: rises and falls when triggered, follows in otherwise; end into trig cycles (after Serge DUSG) |
| **smoothrandom** | 1 | rate, follow | out | — | smooth random voltage; with follow patched, it wanders around that instead |
| **steprandom** | 1 | clock | out, pulse | — | stepped random, a new value on each clock pulse (or nature / 2 times a second on its own) |
| **glide** | 1 | in, clock, time | out | — | sample & hold with glide: holds in on each clock pulse and slides to it (after Serge Smooth & Stepped) |
| **seq** | 8 | next, back, reset | out, pulse, step | — | step sequencer of semitones: seq 8: 0 3 7 10 |
| **quantizer** | 1 | in, root | out, pulse | scale=nature / justmajor / justminor / justpentatonic / otonal / utonal / chromatic / major / minor / dorian / phrygian / lydian / mixolydian / locrian / harmonicminor / melodicminor / majorpentatonic / minorpentatonic / blues / wholetone / diminished / quartertone / tet19 / tet22 / tet31 / tet53 / bohlenpierce / alpha / beta / gamma / slendro / pelog / rast / bayati / partch / justchromatic / pythagorean / meantone / werckmeister | pitch quantizer (octaves in, root transposes); the scale is the nature or scale=: 1 just major, 2 just minor, 3 just pentatonic, 4 otonal 8-15, 5 utonal, 6 chromatic, 7 major, 8 minor, 9 dorian, 10 phrygian, 11 lydian, 12 mixolydian, 13 locrian, 14 harmonic minor, 15 melodic minor, 16 major pentatonic, 17 minor pentatonic, 18 blues, 19 whole tone, 20 diminished, 21 quarter tones (24-TET), 22 19-TET, 23 22-TET, 24 31-TET, 25 53-TET, 26 Bohlen-Pierce, 27-29 Carlos alpha / beta / gamma, 30 slendro, 31 pelog, 32 maqam rast, 33 maqam bayati, 34 Partch 43, 35 just chromatic, 36 Pythagorean, 37 quarter-comma meantone, 38 Werckmeister III |
| **gesture** | 1 | in, record | out | — | gesture recorder: records in while record is held, then loops it (up to 8 s) |
| **ducker** | 1 | in, side | out, lfo | — | ducker: turns in down when side is loud; lfo is a slow sine |
| **value** | 0 | — | out | — | a fixed value: value 40 is 0.40 |
| **adsr** | 1 | gate, attack, decay, sustain, release | out, end | — | ADSR envelope |
| **lfo** | 1 | rate, reset | sine, tri, saw, square | range=lfo / audio | LFO, four shapes, 0 to 1 (after Doepfer A-145) |
| **clock** | 4 | rate, reset | beat, eighth, half, bar | — | clock, 40 to 320 bpm, nature beats to the bar |
| **divider** | 8 | clock, reset | d2, d3, d4, dn | — | clock divider by 2, 3, 4 and by nature (after Doepfer A-160) |
| **euclid** | 16 | clock, hits, rotate, reset | hit, miss, step | — | Euclidean rhythm over nature steps |
| **bernoulli** | 1 | in, chance | a, b | — | Bernoulli gate: each pulse goes to a or b by chance (after Mutable Branches) |
| **turing** | 16 | clock, change | out, pulse | — | looping random shift register that slowly rewrites itself; nature is its length (after Music Thing Turing Machine) |
| **drummap** | 1 | clock, x, y, density, reset | kick, snare, hat, accent | — | drum pattern map, x and y move through it (after Mutable Grids) |
| **burst** | 1 | trig, count, rate | out | — | burst generator: one pulse becomes several |
| **logistic** | 1 | clock, chaos | out, pulse | — | logistic map, from order into chaos |
| **phasor** | 1 | rate, reset | ramp, wrap | — | phasor: a 0 to 1 ramp, slow or audio rate |
| **trigdelay** | 1 | in, time | out | — | trigger delay (after Doepfer A-162) |
| **switch** | 4 | in1, in2, in3, in4, next, reset | out, step | — | sequential switch over up to four inputs (after Doepfer A-151) |
| **automaton** | 30 | clock, seed | alive, pulse, cells | — | elementary cellular automaton, nature is the rule (30, 90, 110...) |
| **randomwalk** | 1 | step, clock | out | — | random walk that turns back at the walls (after the Wogglebug) |
| **pulser** | 2 | rate, trig | out, ramp | — | timing pulse generator: a pulse every period (nature / 2 per second, rate +-8 octaves); trig fires it now (after Buchla 140) |
| **dusg** | 0 | in1, trig1, rise1, fall1, in2, trig2, rise2, fall2 | out1, end1, out2, end2 | cycle1=off / on, cycle2=off / on, range=lfo / audio, curve=lin / exp / log | dual universal slope generator: two slopes, each an envelope, LFO, audio oscillator, slew or lowpass; cycle1 / cycle2 = off|on, range=lfo|audio, curve=lin|exp|log (after the Serge DUSG) |
| **ad** | 0 | trig, attack, decay | out, end | loop=off / on, range=lfo / audio, curve=exp / lin | attack / decay envelope; loop=off|on, range=lfo|audio, curve=exp|lin (after Buchla 180; Doepfer A-142) |
| **ar** | 0 | gate, attack, release | out | curve=exp / lin | attack / release envelope, held while the gate is; curve=exp|lin (after Doepfer A-141) |
| **decay** | 0 | trig, decay | out | curve=exp / lin | percussive envelope: jumps to 1 on a pulse and dies away; curve=exp|lin |
| **stepseq** | 8 | clock, reset, s1, s2, s3, s4, s5, s6, s7, s8 | out, gate, step | dir=forward / backward / pingpong / random | eight-stage sequencer: patch values or faders into s1-s8; nature is the length; dir=forward|backward|pingpong|random (after Buchla 123; Doepfer A-155) |
| **flipflop** | 0 | clock, reset | out, inverse | — | toggle flip-flop: flips on each clock pulse (after Doepfer and Serge logic) |
| **trigger** | 0 | in | out | length=2 | gate to trigger: a pulse on each rising edge; length=2 (ms) (after Doepfer A-162) |

## Hands (the SP-1's controls)

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **fader** | 1 | — | out | — | faders: fader1 - fader4, 0 to 1 |
| **button** | 1 | — | out | — | track buttons: button1 - button4, 1 while held |
| **rocker** | 0 | — | out, up, down | — | the rocker: out is -1, 0 or 1; up and down pulse |
| **play** | 0 | — | out | — | PLAY, 1 while held |
| **dots** | 0 | — | out | — | the dots button, 1 while held |

## Ears (outputs, LEDs, scenes)

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **left** | 0 | in | — | — | left output |
| **right** | 0 | in | — | — | right output |
| **out** | 0 | in | — | — | both outputs |
| **leds** | 0 | in, track, play | out | — | the eight LEDs: sound lights both rows as a bar; track and play light one row each |
| **scene** | 0 | next, back, miss | — | — | goes to the next or previous scene on a pulse; miss is the chance of landing on a random one |
