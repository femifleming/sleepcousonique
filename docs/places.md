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
    a ?> b        maybe: each wave or pulse gets through by chance (50%; a ?30> b for 30%)
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

## Panel knobs

Every place has the knobs it would have on an analog modular: a delay's time and feedback, a
filter's cutoff and resonance, an envelope's attack and release, an LFO's rate, a reverb's
size. A knob is the inlet's own value; whatever you patch into the inlet adds to it (through
its attenuverter). It is written after the place, with the inlet's name:

    the hall is delay 9 time=0.4 feedback=0.6
    the env is follower attack=0.05 release=0.8

Sound and pulse inlets (in, trig, clock...) have no knob, as on hardware; a place's pitch is
its tune knob. A knob left where it was changes nothing, so old dreams sound the same. On the
tapestry the knob sits beside its inlet (the attenuverter just right of it); drag it (shift
for fine), double-click to put it back. Snapshots keep every knob, and glide them.

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

## MIDI to CV

Three places hear MIDI, like a MIDI-to-CV module. On the SP-1 they hear its USB-MIDI port
(*Sleepcousonique*): from a computer, or from a keyboard with a TRS or DIN socket through a
USB-MIDI host adapter. In the editor they hear every MIDI input the computer has.

| place | outlets | switches and params |
|---|---|---|
| `midi` | pitch, gate, velocity, trig | `channel=0` (0 = any, 1–16), `voice=0` (0 = mono, the last note held; 1–4 = one voice of a chord) |
| `midicc` | out | `channel=0`, `cc=1` (0–127; 128 = pitch bend, −1 to 1; 129 = aftertouch) |
| `midiclock` | clock, reset, run | `div=6` (MIDI clock ticks per pulse: 24 = quarter notes, 6 = sixteenths) |

Pitch is octaves from middle C (note 60 is 0, note 72 is 1), so it goes straight into any
pitch inlet; `tune=` or a quantizer does the rest. `trig` pulses on every new note, even
legato; `reset` pulses on Start, and `run` is 1 while the clock plays.

    the keys is midi channel=1
    the wheel is midicc cc=1
    the beat is midiclock div=6
    the voice is saw
    the gate is lpg
    keys.pitch -> voice
    keys.gate -> gate.level
    wheel ~> gate.level
    beat ?40> rain.density
    voice -> gate -> out

A chord: four `midi` places with `voice=1` to `voice=4`, each into its own oscillator.

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

Every place has a fixed *nature* (the number after its name), and some have switches and params, set when you name it (the first position is the default). Otherwise you change a place only by running something into its inlets. The first inlet and the first outlet need no name: `voice -> gate` means `voice.out -> gate.in`. Every source, and every place with a pitch inlet, also has an attenuverter on each inlet (`att.fm=-0.5`); places with a pitch inlet have a tune knob (`tune=-3`, octaves).


## Sources

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **sine** | 3 | pitch, fm | out | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth | sine oscillator at nature x 60 Hz |
| **crossfm** | 3 | pitch, index, fold | out, a, b | tune=0, samplerate=full / half / quarter / eighth | two sines on harmonics n and n+1; index frequency-modulates a with b, fold wavefolds a; out is both mixed |
| **rain** | 1 | density | out, pulse | samplerate=full / half / quarter / eighth | rain: random impulses ringing tiny resonators, 8 x nature per second; pulse marks each drop |
| **thunder** | 1 | strike | out | samplerate=full / half / quarter / eighth | thunder: a filtered noise rumble when struck, and on its own about nature times a minute |
| **noisesweep** | 1 | level | out | samplerate=full / half / quarter / eighth | noise through a resonant bandpass whose frequency climbs with level |
| **wind** | 1 | gust | out | samplerate=full / half / quarter / eighth | wind: soft noise through a slowly wandering bandpass |
| **frogs** | 3 | trig, pitch | out, pulse | tune=0, samplerate=full / half / quarter / eighth | nature (1-8) chirping FM voices on random timers; a pulse into trig makes them all chirp |
| **shepard** | 1 | rate, pitch | out | tune=0, samplerate=full / half / quarter / eighth | Shepard-Risset glissando: rises forever (rate in octaves per second, negative falls) |
| **bell** | 1 | strike, pitch, decay | out | tune=0, samplerate=full / half / quarter / eighth | modal bell: four inharmonic modes, struck by pulses or by sound |
| **vco** | 4 | pitch, fm, width, sync | saw, square, tri, sine | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth | VCO: saw, square, triangle and sine, pulse width and hard sync (after Moog 921, the Minimoog) |
| **xorsquares** | 2 | pitch, couple | out, a, b | tune=0, samplerate=full / half / quarter / eighth | two cross-coupled square oscillators, their XOR as sound (after Ciat-Lonbarde Fourses / Plumbutter) |
| **rungler** | 4 | pitch, chaos, loop | out, steps, a, b | tune=0, samplerate=full / half / quarter / eighth | two triangle oscillators and a shift register fed back on itself; loop above 0.5 locks the pattern (after Rob Hordijk's Benjolin) |
| **additive** | 2 | pitch, tilt, odd, stretch | out | tune=0, samplerate=full / half / quarter / eighth | additive oscillator: sixteen partials; tilt darkens, odd removes the even ones, stretch makes them inharmonic |
| **pulsar** | 2 | pitch, formant, duty | out | tune=0, samplerate=full / half / quarter / eighth | pulsar synthesis: a formant burst once a period, duty sets how much of it sounds (after Curtis Roads) |
| **fm** | 2 | pitch, ratio, index, feedback | out, mod | tune=0, samplerate=full / half / quarter / eighth | two-operator FM at 240 Hz; nature is the ratio (after Chowning, the DX7) |
| **phasedist** | 2 | pitch, amount, resonant | out | tune=0, samplerate=full / half / quarter / eighth | phase distortion oscillator: amount brightens a cosine (after Casio CZ) |
| **wavetable** | 2 | pitch, scan | out | tune=0, samplerate=full / half / quarter / eighth | wavetable oscillator: scans sine, triangle, saw, square, pulse (after PPG / Waldorf) |
| **chord** | 4 | pitch, type, spread | out | tune=0, samplerate=full / half / quarter / eighth | four-voice just-intonation chord; type picks major, minor, sus, maj7, min7, 4:5:6:7 |
| **supersaw** | 4 | pitch, detune, mix | out | tune=0, samplerate=full / half / quarter / eighth | seven detuned saws (after Roland JP-8000) |
| **formant** | 2 | pitch, vowel | out | tune=0, samplerate=full / half / quarter / eighth | formant voice oscillator through a, e, i, o, u (after FOF / IRCAM CHANT) |
| **hum** | 1 | buzz, level | out | samplerate=full / half / quarter / eighth | 60 Hz mains hum and its harmonics |
| **noise** | 1 | level | white, pink, brown, blue | samplerate=full / half / quarter / eighth | noise: white, pink, brown and blue |
| **crackle** | 1 | chaos, density | out, dust | samplerate=full / half / quarter / eighth | chaotic crackle, and random dust impulses (after SuperCollider Crackle / Dust) |
| **lorenz** | 1 | rate, rho | x, y, z | samplerate=full / half / quarter / eighth | Lorenz attractor, slow as modulation or fast as sound |
| **marimba** | 6 | strike, pitch, decay, bright | out | tune=0, samplerate=full / half / quarter / eighth | modal marimba: struck bar, three modes |
| **string** | 4 | pluck, pitch, damp, bright | out | tune=0, samplerate=full / half / quarter / eighth | Karplus-Strong plucked string |
| **kick** | 1 | strike, pitch, punch, decay | out | tune=0, samplerate=full / half / quarter / eighth | kick drum: a falling sine (after TR-808) |
| **snare** | 1 | strike, tone, snap | out | samplerate=full / half / quarter / eighth | snare drum (after TR-808 / 909) |
| **hihat** | 1 | strike, decay, pitch | out | tune=0, samplerate=full / half / quarter / eighth | metallic hi-hat from six squares (after TR-808) |
| **sinesaw** | 3 | pitch, fm | sine, saw | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth | sine and sawtooth oscillator; range=audio|lfo (after Buchla 158) |
| **square** | 3 | pitch, fm, width | out | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth | square / pulse oscillator, width -1..1; range=audio|lfo (after Buchla 144; Doepfer A-110) |
| **harmonics** | 3 | pitch | h1, h2, h3, h4 | tune=0, samplerate=full / half / quarter / eighth | harmonic generator: the first four harmonics of one pitch, each on its own out (after Buchla 148) |
| **saw** | 3 | pitch | out | tune=0, samplerate=full / half / quarter / eighth | a sawtooth and nothing else: the cheapest saw, at nature x 60 Hz |
| **tri** | 3 | pitch | out | tune=0, samplerate=full / half / quarter / eighth | a triangle and nothing else, at nature x 60 Hz |
| **pulse** | 3 | pitch, width | out | tune=0, samplerate=full / half / quarter / eighth | a pulse wave and nothing else; width -1..1, 0 is square |
| **white** | 0 | — | out | samplerate=full / half / quarter / eighth | white noise and nothing else |
| **fmlite** | 2 | pitch, ratio, index | out | tune=0, samplerate=full / half / quarter / eighth | two-operator FM, no feedback; nature is the ratio (fm, cut down) |
| **supersawlite** | 4 | pitch, detune | out | tune=0, samplerate=full / half / quarter / eighth | three detuned plain saws (supersaw, cut down) |
| **froglet** | 3 | trig, pitch | out | tune=0, samplerate=full / half / quarter / eighth | one frog: a falling chirp on its own timer or on a pulse; silent between chirps costs almost nothing (frogs, cut down) |
| **difftone** | 16 | pitch, ratio | out, f1, f2 | ratio=1.2, tune=0, samplerate=full / half / quarter / eighth | two sines for difference tones: f1 at nature x 60 Hz, f2 = f1 x ratio (knob, 1-3; strongest near 1.2); your ear makes f2-f1 and 2f1-f2 (after Alex Chechile) |

## Shapes

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **folder** | 1 | in, fold | out | samplerate=full / half / quarter / eighth | wavefolder; more fold, more folds |
| **lpg** | 1 | in, level | out | mode=both / lowpass / vca, samplerate=full / half / quarter / eighth | lowpass gate (vactrol-style); pulse level for a pluck |
| **lowpass** | 1 | in, cutoff, res | out, band | samplerate=full / half / quarter / eighth | resonant 2-pole lowpass at nature x 480 Hz, with a bandpass out |
| **filterbank** | 1 | in, band1, band2, band3, band4, band5, band6, band7, band8 | out | samplerate=full / half / quarter / eighth | fixed filter bank: eight resonant bands; patch into a band to raise it |
| **saturator** | 1 | in, burst | out | samplerate=full / half / quarter / eighth | tanh saturator; a pulse into burst kicks the drive up with a crackle that dies away |
| **dcblock** | 1 | in | out | samplerate=full / half / quarter / eighth | DC blocker (highpass at nature x 20 Hz) |
| **vca** | 1 | in, gain | out | response=lin / exp, samplerate=full / half / quarter / eighth | VCA: in times gain |
| **matrix** | 0 | in1, in2, in3, in4, rotate | out1, out2, out3, out4 | samplerate=full / half / quarter / eighth | 4 x 4 rotation matrix mixer; rotate turns it (nature sets the resting angle) |
| **ladder** | 1 | in, cutoff, res | out, band | samplerate=full / half / quarter / eighth | 4-pole transistor ladder lowpass at nature x 480 Hz (after Moog 904A) |
| **svf** | 1 | in, cutoff, res | low, band, high, notch | samplerate=full / half / quarter / eighth | state-variable filter: lowpass, bandpass, highpass and notch at once (after Oberheim SEM, Serge VCFQ) |
| **vowelfilter** | 1 | in, vowel, shift | out | samplerate=full / half / quarter / eighth | vowel / formant filter, a to u (after Doepfer A-104) |
| **phaser** | 1 | in, rate, depth, feedback | out | samplerate=full / half / quarter / eighth | six-stage phaser |
| **flanger** | 1 | in, rate, depth, feedback | out, wet | samplerate=full / half / quarter / eighth | flanger |
| **chorus** | 1 | in, depth | out, l, r | samplerate=full / half / quarter / eighth | three-voice ensemble chorus (after Solina / ARP string ensembles) |
| **ringmod** | 2 | in, mod, pitch | out | tune=0, samplerate=full / half / quarter / eighth | ring modulator, with its own sine carrier if nothing is patched to mod |
| **freqshift** | 1 | in, shift | up, down | samplerate=full / half / quarter / eighth | frequency shifter, up and down at once (after Bode, Buchla 285) |
| **bitcrush** | 1 | in, crush, decimate | out | samplerate=full / half / quarter / eighth | bit crusher and sample-rate reducer |
| **tube** | 1 | in, drive, bias | out | samplerate=full / half / quarter / eighth | asymmetric tube-style saturation |
| **compressor** | 1 | in, amount, side | out, gain | samplerate=full / half / quarter / eighth | compressor; side is an external sidechain |
| **crossfade** | 1 | a, b, mix | out | samplerate=full / half / quarter / eighth | crossfade from a to b |
| **vocoder** | 2 | carrier, modulator | out | samplerate=full / half / quarter / eighth | eight-band vocoder; modulator through carrier, or through its own buzz |
| **follower** | 1 | in, attack, release | out, gate | samplerate=full / half / quarter / eighth | envelope follower, and a gate while something is there |
| **samplehold** | 1 | in, clock | held, track | samplerate=full / half / quarter / eighth | sample & hold and track & hold; samples noise when nothing is patched |
| **logic** | 1 | a, b | and, or, xor, not | samplerate=full / half / quarter / eighth | logic on pulses |
| **comparator** | 1 | in, ref | gate, inverse | samplerate=full / half / quarter / eighth | comparator: high while in is above ref |
| **rectify** | 1 | in | invert, full, half, sign | samplerate=full / half / quarter / eighth | inverter, full-wave and half-wave rectifier, sign |
| **pan** | 1 | in, position | l, r | samplerate=full / half / quarter / eighth | panner: -1 left to 1 right |
| **suboctave** | 1 | in | sub1, sub2 | samplerate=full / half / quarter / eighth | sub-octave divider: one and two octaves down |
| **mixer** | 0 | in1, in2, in3, in4, in5, in6 | out | samplerate=full / half / quarter / eighth | six-input mixer; set levels with amounts on the cords (after Buchla 106; Doepfer A-138) |
| **vcmix** | 0 | in1, in2, in3, in4, level1, level2, level3, level4 | out | samplerate=full / half / quarter / eighth | voltage-controlled mixer: four inputs with a level each (after Buchla 107; Doepfer A-135) |
| **vcgate** | 0 | a, b, levela, levelb | a, b | samplerate=full / half / quarter / eighth | dual voltage-controlled gate / VCA (after Buchla 110) |
| **quadvca** | 0 | in1, in2, in3, in4, cv1, cv2, cv3, cv4 | out1, out2, out3, out4 | response=lin / exp, samplerate=full / half / quarter / eighth | four VCAs; response=lin|exp (after Doepfer A-130 / A-131 / A-132) |
| **duallpg** | 0 | in1, cv1, in2, cv2 | out1, out2 | mode1=both / lowpass / vca, mode2=both / lowpass / vca, samplerate=full / half / quarter / eighth | two low pass gates (vactrol response); mode1 / mode2 = both|lowpass|vca; pulse a cv for a pluck (after Buchla 292) |
| **highpass** | 1 | in, cutoff, res | out | samplerate=full / half / quarter / eighth | resonant 2-pole highpass at nature x 120 Hz (after the Doepfer A-100 filters) |
| **attenuverter** | 0 | in, amount, offset | out | samplerate=full / half / quarter / eighth | attenuverter and offset: in x amount (-1..1) + offset (after Doepfer A-183-2) |
| **sineshaper** | 0 | in, drive | out | samplerate=full / half / quarter / eighth | sine waveshaper: a triangle in becomes a sine; drive folds it further (after Serge waveshapers) |
| **minmax** | 0 | a, b | max, min | samplerate=full / half / quarter / eighth | the higher and the lower of two signals (after Serge Peak & Trough) |
| **cvprocessor** | 0 | a, b, offset, time | out | samplerate=full / half / quarter / eighth | control voltage processor: sums a, b and an offset, then lags it by time (after Buchla 156) |
| **slew** | 0 | in, rise, fall | out | shape=lin / exp, samplerate=full / half / quarter / eighth | slew limiter with separate rise and fall; shape=lin|exp (after Doepfer A-170) |
| **dualprocessor** | 0 | in1, rate1, in2, rate2 | out1, out2 | mode1=lag / slew, mode2=lag / slew, samplerate=full / half / quarter / eighth | two voltage-controlled lag / slew processors: smooth control voltages, or a gentle lowpass on audio (rate 0..1 is 20 Hz-20 kHz); mode1 / mode2 = lag|slew (after the Serge Dual Processor) |
| **follow** | 2 | in | out | samplerate=full / half / quarter / eighth | an envelope follower and nothing else: release nature x 50 ms |
| **onepole** | 4 | in, cutoff | out | samplerate=full / half / quarter / eighth | the gentlest lowpass, one pole at nature x 120 Hz; cutoff in octaves |
| **choruslite** | 1 | in, depth | out | samplerate=full / half / quarter / eighth | one-voice chorus with a triangle sweep (chorus, cut down) |

## Spaces

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **comb** | 2 | in, pitch, decay | out | tune=0, samplerate=full / half / quarter / eighth | comb filter tuned to nature x 60 Hz; pulses pluck it |
| **delay** | 6 | in, time, feedback | out | samplerate=full / half / quarter / eighth | delay, nature x 50 ms long; time scales it |
| **reverb** | 2 | in, size | out, l, r | samplerate=full / half / quarter / eighth | stereo room reverb |
| **granular** | 1 | in, density | out | samplerate=full / half / quarter / eighth | granular: grains of what you patch in, or of its own buffer |
| **spring** | 2 | in, decay | out | samplerate=full / half / quarter / eighth | spring reverb |
| **plate** | 1 | in, decay, damp | out, l, r | samplerate=full / half / quarter / eighth | plate reverb (after Dattorro) |
| **pingpong** | 6 | in, time, feedback | l, r | samplerate=full / half / quarter / eighth | ping-pong delay, nature x 25 ms |
| **tapeecho** | 3 | in, time, feedback, wow | out, wet | samplerate=full / half / quarter / eighth | tape echo with wow, nature x 100 ms (after Roland Space Echo) |
| **freeze** | 1 | in, hold, length | out | samplerate=full / half / quarter / eighth | freeze: hold loops the last nature x 250 ms |
| **sympathetic** | 2 | in, pitch, decay, chord | out | tune=0, samplerate=full / half / quarter / eighth | four sympathetic strings tuned to a chord |
| **pitchshift** | 1 | in, shift | out | samplerate=full / half / quarter / eighth | pitch shifter, shift in octaves |
| **diffuser** | 1 | in, amount | out | samplerate=full / half / quarter / eighth | allpass diffuser: smears in time without echoes |
| **reverblite** | 2 | in, size | out | samplerate=full / half / quarter / eighth | a small cheap room: two cross-fed lines, mono (reverb, cut down) |
| **delaylite** | 6 | in, time, feedback | out | samplerate=full / half / quarter / eighth | delay, nature x 50 ms; time and feedback read once a block, no smoothing (delay, cut down) |

## Currents

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **slope** | 1 | in, trig, rise, fall | out, end | cycle=off / on, range=lfo / audio, curve=lin / exp / log, samplerate=full / half / quarter / eighth | slope / function generator: rises and falls when triggered, follows in otherwise; end into trig cycles (after Serge DUSG) |
| **smoothrandom** | 1 | rate, follow | out | samplerate=full / half / quarter / eighth | smooth random voltage; with follow patched, it wanders around that instead |
| **steprandom** | 1 | clock | out, pulse | samplerate=full / half / quarter / eighth | stepped random, a new value on each clock pulse (or nature / 2 times a second on its own) |
| **glide** | 1 | in, clock, time | out | samplerate=full / half / quarter / eighth | sample & hold with glide: holds in on each clock pulse and slides to it (after Serge Smooth & Stepped) |
| **seq** | 8 | next, back, reset | out, pulse, step | samplerate=full / half / quarter / eighth | step sequencer of semitones: seq 8: 0 3 7 10 |
| **quantizer** | 1 | in, root | out, pulse | scale=nature / justmajor / justminor / justpentatonic / otonal / utonal / chromatic / major / minor / dorian / phrygian / lydian / mixolydian / locrian / harmonicminor / melodicminor / majorpentatonic / minorpentatonic / blues / wholetone / diminished / quartertone / tet19 / tet22 / tet31 / tet53 / bohlenpierce / alpha / beta / gamma / slendro / pelog / rast / bayati / partch / justchromatic / pythagorean / meantone / werckmeister, samplerate=full / half / quarter / eighth | pitch quantizer (octaves in, root transposes); the scale is the nature or scale=: 1 just major, 2 just minor, 3 just pentatonic, 4 otonal 8-15, 5 utonal, 6 chromatic, 7 major, 8 minor, 9 dorian, 10 phrygian, 11 lydian, 12 mixolydian, 13 locrian, 14 harmonic minor, 15 melodic minor, 16 major pentatonic, 17 minor pentatonic, 18 blues, 19 whole tone, 20 diminished, 21 quarter tones (24-TET), 22 19-TET, 23 22-TET, 24 31-TET, 25 53-TET, 26 Bohlen-Pierce, 27-29 Carlos alpha / beta / gamma, 30 slendro, 31 pelog, 32 maqam rast, 33 maqam bayati, 34 Partch 43, 35 just chromatic, 36 Pythagorean, 37 quarter-comma meantone, 38 Werckmeister III |
| **gesture** | 1 | in, record | out | samplerate=full / half / quarter / eighth | gesture recorder: records in while record is held, then loops it (up to 8 s) |
| **ducker** | 1 | in, side | out, lfo | samplerate=full / half / quarter / eighth | ducker: turns in down when side is loud; lfo is a slow sine |
| **value** | 0 | — | out | samplerate=full / half / quarter / eighth | a fixed value: value 40 is 0.40 |
| **adsr** | 1 | gate, attack, decay, sustain, release | out, end | samplerate=full / half / quarter / eighth | ADSR envelope |
| **lfo** | 1 | rate, reset | sine, tri, saw, square | range=lfo / audio, samplerate=full / half / quarter / eighth | LFO, four shapes, 0 to 1 (after Doepfer A-145) |
| **clock** | 4 | rate, reset | beat, eighth, half, bar | samplerate=full / half / quarter / eighth | clock, 40 to 320 bpm, nature beats to the bar |
| **divider** | 8 | clock, reset | d2, d3, d4, dn | samplerate=full / half / quarter / eighth | clock divider by 2, 3, 4 and by nature (after Doepfer A-160) |
| **euclid** | 16 | clock, hits, rotate, reset | hit, miss, step | samplerate=full / half / quarter / eighth | Euclidean rhythm over nature steps |
| **bernoulli** | 1 | in, chance | a, b | samplerate=full / half / quarter / eighth | Bernoulli gate: each pulse goes to a or b by chance (after Mutable Branches) |
| **turing** | 16 | clock, change | out, pulse | samplerate=full / half / quarter / eighth | looping random shift register that slowly rewrites itself; nature is its length (after Music Thing Turing Machine) |
| **drummap** | 1 | clock, x, y, density, reset | kick, snare, hat, accent | samplerate=full / half / quarter / eighth | drum pattern map, x and y move through it (after Mutable Grids) |
| **burst** | 1 | trig, count, rate | out | samplerate=full / half / quarter / eighth | burst generator: one pulse becomes several |
| **logistic** | 1 | clock, chaos | out, pulse | samplerate=full / half / quarter / eighth | logistic map, from order into chaos |
| **phasor** | 1 | rate, reset | ramp, wrap | samplerate=full / half / quarter / eighth | phasor: a 0 to 1 ramp, slow or audio rate |
| **trigdelay** | 1 | in, time | out | samplerate=full / half / quarter / eighth | trigger delay (after Doepfer A-162) |
| **switch** | 4 | in1, in2, in3, in4, next, reset | out, step | samplerate=full / half / quarter / eighth | sequential switch over up to four inputs (after Doepfer A-151) |
| **automaton** | 30 | clock, seed | alive, pulse, cells | samplerate=full / half / quarter / eighth | elementary cellular automaton, nature is the rule (30, 90, 110...) |
| **randomwalk** | 1 | step, clock | out | samplerate=full / half / quarter / eighth | random walk that turns back at the walls (after the Wogglebug) |
| **pulser** | 2 | rate, trig | out, ramp | samplerate=full / half / quarter / eighth | timing pulse generator: a pulse every period (nature / 2 per second, rate +-8 octaves); trig fires it now (after Buchla 140) |
| **dusg** | 0 | in1, trig1, rise1, fall1, in2, trig2, rise2, fall2 | out1, end1, out2, end2 | cycle1=off / on, cycle2=off / on, range=lfo / audio, curve=lin / exp / log, samplerate=full / half / quarter / eighth | dual universal slope generator: two slopes, each an envelope, LFO, audio oscillator, slew or lowpass; cycle1 / cycle2 = off|on, range=lfo|audio, curve=lin|exp|log (after the Serge DUSG) |
| **ad** | 0 | trig, attack, decay | out, end | loop=off / on, range=lfo / audio, curve=exp / lin, samplerate=full / half / quarter / eighth | attack / decay envelope; loop=off|on, range=lfo|audio, curve=exp|lin (after Buchla 180; Doepfer A-142) |
| **ar** | 0 | gate, attack, release | out | curve=exp / lin, samplerate=full / half / quarter / eighth | attack / release envelope, held while the gate is; curve=exp|lin (after Doepfer A-141) |
| **decay** | 0 | trig, decay | out | curve=exp / lin, samplerate=full / half / quarter / eighth | percussive envelope: jumps to 1 on a pulse and dies away; curve=exp|lin |
| **stepseq** | 8 | clock, reset, s1, s2, s3, s4, s5, s6, s7, s8 | out, gate, step | dir=forward / backward / pingpong / random, samplerate=full / half / quarter / eighth | eight-stage sequencer: patch values or faders into s1-s8; nature is the length; dir=forward|backward|pingpong|random (after Buchla 123; Doepfer A-155) |
| **flipflop** | 0 | clock, reset | out, inverse | samplerate=full / half / quarter / eighth | toggle flip-flop: flips on each clock pulse (after Doepfer and Serge logic) |
| **trigger** | 0 | in | out | length=2, samplerate=full / half / quarter / eighth | gate to trigger: a pulse on each rising edge; length=2 (ms) (after Doepfer A-162) |

## Hands (the SP-1's controls)

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **midi** | 0 | — | pitch, gate, velocity, trig | channel=0, voice=0, samplerate=full / half / quarter / eighth | MIDI to CV: pitch (octaves from middle C), gate, velocity, trig on each note; channel=0 is any, voice=1-4 picks one voice of a chord (0 = mono, last note) |
| **midicc** | 0 | — | out | channel=0, cc=1, samplerate=full / half / quarter / eighth | one MIDI controller, 0 to 1: cc=0-127, 128 = pitch bend (-1 to 1), 129 = aftertouch; channel=0 is any |
| **midiclock** | 0 | — | clock, reset, run | div=6, samplerate=full / half / quarter / eighth | MIDI clock: clock pulses every div ticks (24 = quarter notes, 6 = sixteenths), reset on start, run is 1 while playing |
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
