# Sleepcousonique

*A patch language for the SP-1 and the Wingie, written like a dream journal.*

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

A fader at its top through an attenuverter at its top is always safe: every place keeps
working whatever its inlets are sent. If a patch does run away (a place fed its own sound
until it howls, say), the engine starts that one place again by itself: you hear a gap in
it, the rest of the dream plays on, and nothing needs restarting. (This is the editor from
1.3.0 on. The SP-1 gets the same engine with the first firmware after 1.2.0; until then,
keep `att.pitch` on a bell or a marimba modest when you perform on the SP-1 itself.)

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

## The room: Bluetooth as weather

Phones, watches, earbuds, laptops and tags call out over Bluetooth a few times a second. Two
places let a dream hear them, so it changes with whoever is in the room: `crowd` is how many
there are and how strong, `visitors` is each coming, going and call as it happens. The
computer's own Bluetooth does the listening (see *The room* in section 1), and nothing has
to be paired or connected: the devices are only overheard.

| place | outlets | params |
|---|---|---|
| `crowd` | out, count, near, strength | `max=20`, `rest=0` |
| `visitors` | arrive, leave, pulse, voice | |

**crowd**

| outlet | what it gives | |
|---|---|---|
| `out` | 0 to 1 | how full the room is: the number of devices over `max=`, smoothed (about half a second), stopping at 1 |
| `count` | 0, 1, 2 ... 30 ... | the plain number of devices, in steps. Any number of them: it is a count, not a yes or no |
| `near` | 0 to 1 | the strongest signal: how close the nearest device is. 0 is the edge of hearing (−100 dBm), 1 is within arm's reach (−40 dBm), smoothed |
| `strength` | 0 to 1 | all the signals together (their mean): whether the crowd is around you or far off, smoothed |

`max=` is the count that brings `out` to 1 (`max=20`: 10 devices give 0.5, 20 or more give 1).
`rest=` is the count the crowd holds when no radio is listening: on the SP-1 playing alone,
or on a computer whose Bluetooth is off or not allowed. With `rest=5 max=20` a dream that
leans on the crowd still plays on the SP-1, as a room of five (`near` and `strength` then
sit at 0.5).

**visitors**

| outlet | what it gives |
|---|---|
| `arrive` | a pulse when a device not heard before appears |
| `leave` | a pulse when one has been quiet for eight seconds and is counted as gone |
| `pulse` | a pulse on every call heard: an uneven rhythm, denser the fuller the room |
| `voice` | the number (0 to 1) of the device that called last, held until the next call. Every device has its own, so through a quantizer the room plays one note per device |

    the people is crowd max=12 rest=3
    the rain is rain 2
    the hall is delay 9
    the bell is bell
    the scale is quantizer scale=slendro

    people -> rain.density              ; the fuller the room, the harder it rains
    people.near -40> hall.feedback      ; someone close: the echoes hang longer
    people.count -8> bell.pitch         ; one more device, a step up (8 hundredths of an octave)
    visitors.pulse -> bell.strike       ; every call strikes the bell
    visitors.voice -> scale -> bell.pitch
    visitors.arrive -> scene.next       ; someone comes in: the next scene
    rain bell -> hall -> out

Both are among the *currents* in the editor's place menu. On the tapestry they are boards
like any other, and `max` and `rest` are knobs on the crowd's board.

To move many things with the size of the crowd, use a lucid group:
`lucid people: rain.density 10..90, hall.feedback 20..70, gate.level 40..100`.

What the count is, and isn't:

- **Devices, not people.** Someone with a phone, a watch and earbuds is three. A laptop, a
  speaker or a tag on a set of keys counts too. Set `max=` for the room you are in.
- **Only what calls out.** A device with Bluetooth off is silent, and some only call when
  their screen is on.
- **About a room.** Roughly 10 to 20 m indoors, less through walls. `near` tells close
  from far, not metres.
- **Addresses change.** Phones swap their Bluetooth address every quarter of an hour or so,
  for privacy. For a few seconds such a phone is counted twice (the old address leaves, a
  new one arrives), and its `voice` changes.
- **Nothing is kept.** A device is one number made from the identifier the system gives for
  it, held in memory while the device is near. No names, no addresses, nothing written to
  disk or sent anywhere.


## Two instruments: the SP-1 and the Wingie

A dream is for the SP-1 unless it says otherwise. A line `instrument wingie` anywhere in it
(usually the first) makes it a dream for the **Meng Qi Wingie2**, the handheld resonator with two
microphones. The editor's **SP-1 | Wingie** switch writes and removes that line for you.

Every processing place works on both. The *hands* differ:

| on the Wingie | what it gives |
|---|---|
| `mic` | `left`, `right`, `both`: the two microphones (or line in, by the source switch) |
| `keys1`, `keys2` | a keyboard: `pitch` of the last key (octaves; keys2 an octave up; the octave switch adds), `gate`, `trig`, `key` (0 C .. 1 B) |
| `key1` .. `key24` | one key: 1-12 the left keyboard C to B, 13-24 the right. `out` while held, `trig` on press |
| `mode1`, `mode2` | the Mode buttons: `out` while held, `tap` on a quick press |
| `octave1`, `octave2` | the octave switches: -1, 0 or 1 |
| `source` | 0 microphones, 1 line in |
| `fader1` `fader2` `fader3` | the faders, labelled Mix, Decay and Volume (they are yours: nothing is fixed to them) |
| `lights` | `left`, `right`: the two LEDs. They glow red at rest and climb through purple and yellow to white |

The SP-1's own hands (`button1`-`4`, `rocker`, `play`, `dots`, `leds`, `fader4`) are errors in a
Wingie dream, and the Wingie's in an SP-1 dream.

On the Wingie: hold both Mode buttons and press a left key to play that slot (12 slots); hold one
Mode button and tap the other for the next (right) or previous (left) scene. While both are held
the dream sees neither Mode button nor the keys.

## Pitch units

A number can carry a pitch unit: `7st` is seven semitones, `-50ct` fifty cents down, `1oct` an
octave. It works wherever a pitch goes: a kept value into a pitch inlet (`7st -> voice.pitch` is a
fifth up), the tune knob (`tune=-5st`), a pitch knob (`pitch=3st`), and a seq's steps (`seq 4: 0 7st
1oct 350ct`, where bare steps stay semitones as before).

## Resolution: audio-rate control

Most places read their pitch, cutoff and rates once a block (32 samples, 0.7 ms). That is
plenty for hands and slow currents, but a fast oscillator patched into a pitch inlet comes out
stepped. Any place can be told to look closer:

    the voice is vco 4 resolution=fine        ; every 4 samples
    the bell is wingie 2 resolution=audio      ; every sample: true audio-rate FM

It costs the place's once-a-block work 8 (fine) or 32 (audio) times over, so use it where you
hear the difference. It can't be combined with a lower samplerate.

## Too heavy: the last dream that fit

When a dream asks more of the processor than there is, it doesn't fall silent or break up. The
instrument goes back to what last played calmly for two seconds: the previous scene if a heavy
scene did it, else the dream that was playing before (on the SP-1, the dream kept in the slot
when the heavy one only came to try), else the first dream. The console says which.

## Changing slots from inside a dream

`slot` goes to another dream: `the next is slot 7` and a pulse at `next.go` plays slot 7;
`next` and `back` step to the following or previous slot. Like `scene`, it ignores pulses in the
first 0.1 s of a scene (a clock pulses as it starts). In the editor, which has no slots, the
status line says what the instrument would do.

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

The editor shows **load** (how much of the SP-1's processor the dream needs) and **room**
(whichever of its other limits the scene is nearest: its 124 KB of memory for delays, grains and
each place's sound, 96 places, 320 passages, or the 16 KB of text a slot keeps). Keep both under
100%, and what you hear on the computer is what the SP-1 plays. A dream can have up to 16 scenes.


# The places

Every place has a fixed *nature* (the number after its name), and some have switches and params, set when you name it (the first position is the default). Otherwise you change a place only by running something into its inlets. The first inlet and the first outlet need no name: `voice -> gate` means `voice.out -> gate.in`. Every source, and every place with a pitch inlet, also has an attenuverter on each inlet (`att.fm=-0.5`); places with a pitch inlet have a tune knob (`tune=-3`, octaves).


## Sources

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **sine** | 3 | pitch, fm | out | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sine oscillator at nature x 60 Hz |
| **crossfm** | 3 | pitch, index, fold | out, a, b | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two sines on harmonics n and n+1; index frequency-modulates a with b, fold wavefolds a; out is both mixed |
| **rain** | 1 | density | out, pulse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | rain: random impulses ringing tiny resonators, 8 x nature per second; pulse marks each drop |
| **thunder** | 1 | strike | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | thunder: a filtered noise rumble when struck, and on its own about nature times a minute |
| **noisesweep** | 1 | level | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | noise through a resonant bandpass whose frequency climbs with level |
| **wind** | 1 | gust | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | wind: soft noise through a slowly wandering bandpass |
| **frogs** | 3 | trig, pitch | out, pulse | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | nature (1-8) chirping FM voices on random timers; a pulse into trig makes them all chirp |
| **shepard** | 1 | rate, pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | Shepard-Risset glissando: rises forever (rate in octaves per second, negative falls) |
| **bell** | 1 | strike, pitch, decay | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | modal bell: four inharmonic modes, struck by pulses or by sound |
| **vco** | 4 | pitch, fm, width, sync | saw, square, tri, sine | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | VCO: saw, square, triangle and sine, pulse width and hard sync (after Moog 921, the Minimoog) |
| **xorsquares** | 2 | pitch, couple | out, a, b | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two cross-coupled square oscillators, their XOR as sound (after Ciat-Lonbarde Fourses / Plumbutter) |
| **rungler** | 4 | pitch, chaos, loop | out, steps, a, b | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two triangle oscillators and a shift register fed back on itself; loop above 0.5 locks the pattern (after Rob Hordijk's Benjolin) |
| **additive** | 2 | pitch, tilt, odd, stretch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | additive oscillator: sixteen partials; tilt darkens, odd removes the even ones, stretch makes them inharmonic |
| **pulsar** | 2 | pitch, formant, duty | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | pulsar synthesis: a formant burst once a period, duty sets how much of it sounds (after Curtis Roads) |
| **fm** | 2 | pitch, ratio, index, feedback | out, mod | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two-operator FM at 240 Hz; nature is the ratio (after Chowning, the DX7) |
| **phasedist** | 2 | pitch, amount, resonant | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | phase distortion oscillator: amount brightens a cosine (after Casio CZ) |
| **wavetable** | 2 | pitch, scan | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | wavetable oscillator: scans sine, triangle, saw, square, pulse (after PPG / Waldorf) |
| **chord** | 4 | pitch, type, spread | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | four-voice just-intonation chord; type picks major, minor, sus, maj7, min7, 4:5:6:7 |
| **supersaw** | 4 | pitch, detune, mix | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | seven detuned saws (after Roland JP-8000) |
| **formant** | 2 | pitch, vowel | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | formant voice oscillator through a, e, i, o, u (after FOF / IRCAM CHANT) |
| **hum** | 1 | buzz, level | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | 60 Hz mains hum and its harmonics |
| **noise** | 1 | level | white, pink, brown, blue | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | noise: white, pink, brown and blue |
| **crackle** | 1 | chaos, density | out, dust | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | chaotic crackle, and random dust impulses (after SuperCollider Crackle / Dust) |
| **lorenz** | 1 | rate, rho | x, y, z | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | Lorenz attractor, slow as modulation or fast as sound |
| **marimba** | 6 | strike, pitch, decay, bright | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | modal marimba: struck bar, three modes |
| **string** | 4 | pluck, pitch, damp, bright | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | Karplus-Strong plucked string |
| **kick** | 1 | strike, pitch, punch, decay | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | kick drum: a falling sine (after TR-808) |
| **snare** | 1 | strike, tone, snap | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | snare drum (after TR-808 / 909) |
| **hihat** | 1 | strike, decay, pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | metallic hi-hat from six squares (after TR-808) |
| **sinesaw** | 3 | pitch, fm | sine, saw | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sine and sawtooth oscillator; range=audio|lfo (after Buchla 158) |
| **square** | 3 | pitch, fm, width | out | range=audio / lfo, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | square / pulse oscillator, width -1..1; range=audio|lfo (after Buchla 144; Doepfer A-110) |
| **harmonics** | 3 | pitch | h1, h2, h3, h4 | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | harmonic generator: the first four harmonics of one pitch, each on its own out (after Buchla 148) |
| **saw** | 3 | pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a sawtooth and nothing else: the cheapest saw, at nature x 60 Hz |
| **tri** | 3 | pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a triangle and nothing else, at nature x 60 Hz |
| **pulse** | 3 | pitch, width | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a pulse wave and nothing else; width -1..1, 0 is square |
| **white** | 0 | — | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | white noise and nothing else |
| **fmlite** | 2 | pitch, ratio, index | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two-operator FM, no feedback; nature is the ratio (fm, cut down) |
| **supersawlite** | 4 | pitch, detune | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | three detuned plain saws (supersaw, cut down) |
| **froglet** | 3 | trig, pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | one frog: a falling chirp on its own timer or on a pulse; silent between chirps costs almost nothing (frogs, cut down) |
| **difftone** | 16 | pitch, ratio | out, f1, f2 | ratio=1.2, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two sines for difference tones: f1 at nature x 60 Hz, f2 = f1 x ratio (knob, 1-3; strongest near 1.2); your ear makes f2-f1 and 2f1-f2 (after Alex Chechile) |
| **dust** | 4 | density | out, pulse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | random impulses of random height, nature a second at density 0.5 (1/64 to 64 times that) (after SuperCollider's Dust) |
| **knot** | 2 | clock, pitch, rate, lock, cross, shape, decay | left, right, a, b | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two voices, each a triangle-to-square oscillator stepped by an 8-bit XOR shift register (lock holds the loop), with an envelope and a VCA; cross lets the voices clock each other; a and b pulse on each voice's notes (after the Lorre Mill Double Knot) |
| **benjolin** | 2 | pitch, ratio, chaos, cutoff, res, loop | out, tri, pulse, rungler | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two oscillators, a rungler shift register between them, a twin-peak filter on their comparison; chaos is how much the rungler bends everything, loop locks the pattern (after Rob Hordijk's Benjolin) |
| **trainlet** | 2 | pitch, formant, density, bandwidth | out | chroma=0, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a voice from the radiator pipes: trainlets of FOF formant grains (after Roads and Rodet's CHANT). At each period of pitch (nature x 60 Hz) a grain at formant (200 Hz .. 4 kHz) rings out over bandwidth (20 Hz .. 1 kHz); under full density the train comes in bursts of 40-250 ms; chroma= 0-100 adds the formant's harmonics (kept under the Nyquist) |
| **gendy** | 2 | pitch, amp_step, dur_step | out | dist=cauchy / logist / hyperbcos / arcsine / expon, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the electric storm over Mycenae: Iannis Xenakis's GENDY dynamic stochastic synthesis, a wave of 12 breakpoints whose heights and lengths take random walks between mirrors with steps from dist=; amp_step how far heights wander, dur_step how far lengths (and the pitch, around nature x 60 Hz) do. Drawn at 2x and brought down through a half-band, so the corners don't alias |

## Shapes

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **folder** | 1 | in, fold | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | wavefolder; more fold, more folds |
| **lpg** | 1 | in, level | out | mode=both / lowpass / vca, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | lowpass gate (vactrol-style); pulse level for a pluck |
| **lowpass** | 1 | in, cutoff, res | out, band | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | resonant 2-pole lowpass at nature x 480 Hz, with a bandpass out |
| **filterbank** | 1 | in, band1, band2, band3, band4, band5, band6, band7, band8 | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | fixed filter bank: eight resonant bands; patch into a band to raise it |
| **saturator** | 1 | in, burst | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | tanh saturator; a pulse into burst kicks the drive up with a crackle that dies away |
| **dcblock** | 1 | in | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | DC blocker (highpass at nature x 20 Hz) |
| **vca** | 1 | in, gain | out | response=lin / exp, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | VCA: in times gain |
| **matrix** | 0 | in1, in2, in3, in4, rotate | out1, out2, out3, out4 | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | 4 x 4 rotation matrix mixer; rotate turns it (nature sets the resting angle) |
| **ladder** | 1 | in, cutoff, res | out, band | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | 4-pole transistor ladder lowpass at nature x 480 Hz (after Moog 904A) |
| **svf** | 1 | in, cutoff, res | low, band, high, notch | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | state-variable filter: lowpass, bandpass, highpass and notch at once (after Oberheim SEM, Serge VCFQ) |
| **vowelfilter** | 1 | in, vowel, shift | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | vowel / formant filter, a to u (after Doepfer A-104) |
| **phaser** | 1 | in, rate, depth, feedback | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | six-stage phaser |
| **flanger** | 1 | in, rate, depth, feedback | out, wet | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | flanger |
| **chorus** | 1 | in, depth | out, l, r | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | three-voice ensemble chorus (after Solina / ARP string ensembles) |
| **ringmod** | 2 | in, mod, pitch | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | ring modulator, with its own sine carrier if nothing is patched to mod |
| **freqshift** | 1 | in, shift | up, down | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | frequency shifter, up and down at once (after Bode, Buchla 285) |
| **bitcrush** | 1 | in, crush, decimate | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | bit crusher and sample-rate reducer |
| **tube** | 1 | in, drive, bias | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | asymmetric tube-style saturation |
| **compressor** | 1 | in, amount, side | out, gain | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | compressor; side is an external sidechain |
| **crossfade** | 1 | a, b, mix | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | crossfade from a to b |
| **vocoder** | 2 | carrier, modulator | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | eight-band vocoder; modulator through carrier, or through its own buzz |
| **follower** | 1 | in, attack, release | out, gate | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | envelope follower, and a gate while something is there |
| **samplehold** | 1 | in, clock | held, track | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sample & hold and track & hold; samples noise when nothing is patched |
| **logic** | 1 | a, b | and, or, xor, not | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | logic on pulses |
| **comparator** | 1 | in, ref | gate, inverse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | comparator: high while in is above ref |
| **rectify** | 1 | in | invert, full, half, sign | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | inverter, full-wave and half-wave rectifier, sign |
| **pan** | 1 | in, position | l, r | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | panner: -1 left to 1 right |
| **suboctave** | 1 | in | sub1, sub2 | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sub-octave divider: one and two octaves down |
| **mixer** | 0 | in1, in2, in3, in4, in5, in6 | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | six-input mixer; set levels with amounts on the cords (after Buchla 106; Doepfer A-138) |
| **vcmix** | 0 | in1, in2, in3, in4, level1, level2, level3, level4 | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | voltage-controlled mixer: four inputs with a level each (after Buchla 107; Doepfer A-135) |
| **vcgate** | 0 | a, b, levela, levelb | a, b | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | dual voltage-controlled gate / VCA (after Buchla 110) |
| **quadvca** | 0 | in1, in2, in3, in4, cv1, cv2, cv3, cv4 | out1, out2, out3, out4 | response=lin / exp, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | four VCAs; response=lin|exp (after Doepfer A-130 / A-131 / A-132) |
| **duallpg** | 0 | in1, cv1, in2, cv2 | out1, out2 | mode1=both / lowpass / vca, mode2=both / lowpass / vca, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two low pass gates (vactrol response); mode1 / mode2 = both|lowpass|vca; pulse a cv for a pluck (after Buchla 292) |
| **highpass** | 1 | in, cutoff, res | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | resonant 2-pole highpass at nature x 120 Hz (after the Doepfer A-100 filters) |
| **attenuverter** | 0 | in, amount, offset | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | attenuverter and offset: in x amount (-1..1) + offset (after Doepfer A-183-2) |
| **sineshaper** | 0 | in, drive | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sine waveshaper: a triangle in becomes a sine; drive folds it further (after Serge waveshapers) |
| **minmax** | 0 | a, b | max, min | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the higher and the lower of two signals (after Serge Peak & Trough) |
| **cvprocessor** | 0 | a, b, offset, time | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | control voltage processor: sums a, b and an offset, then lags it by time (after Buchla 156) |
| **slew** | 0 | in, rise, fall | out | shape=lin / exp, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | slew limiter with separate rise and fall; shape=lin|exp (after Doepfer A-170) |
| **dualprocessor** | 0 | in1, rate1, in2, rate2 | out1, out2 | mode1=lag / slew, mode2=lag / slew, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two voltage-controlled lag / slew processors: smooth control voltages, or a gentle lowpass on audio (rate 0..1 is 20 Hz-20 kHz); mode1 / mode2 = lag|slew (after the Serge Dual Processor) |
| **follow** | 2 | in | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | an envelope follower and nothing else: release nature x 50 ms |
| **onepole** | 4 | in, cutoff | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the gentlest lowpass, one pole at nature x 120 Hz; cutoff in octaves |
| **choruslite** | 1 | in, depth | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | one-voice chorus with a triangle sweep (chorus, cut down) |
| **fuzz** | 1 | in, drive, bias | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | germanium fuzz: two hot transistors, asymmetric; bias 0 is the sweet spot, below it the battery dies and the sound splutters and gates, above it one side fattens (after the Arbiter Fuzz Face, the Fuzz Factory) |
| **chloe** | 1 | in, drive, cutoff, resonance, gate | out | mode=low / band / high, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the Chloe module: a vacuum tube driven into its grid, a multimode filter (mode=low|band|high) that sings when pushed, and a low-pass gate struck by gate (open when nothing is in gate) |
| **tape** | 1 | in, age, wow | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the cassette that lived in a car: wow, flutter, the head bump, soft saturation, hiss and dropouts as age wears it; nature is the speed (1 is 7.5 ips, 2 is 15) |
| **vinyl** | 1 | in, age | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a record from the archive: crackle and pops, a scratch every turn, rumble, the warp's wow, a worn top end and groove noise; nature is the speed (1 is 33, 1.35 is 45) |
| **radio** | 1 | in, tune, drift | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | shortwave at night: a narrow band, fading in and out with a notch wandering through, whistles where carriers beat, static, an AGC that lifts the noise as the station sinks; tune 0.5 is on the station, drift wanders the dial |
| **bandsplit** | 1 | in, low, high | low, mid, high | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the sound system's crossover: low, mid and high that sum back flat; low and high are the crossover points, 20 Hz to 20 kHz (after Linkwitz-Riley, fourth order) |
| **eq** | 1 | in, low, mid, freq, high | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the mixer's three knobs: low shelf, sweepable mid (freq 80 Hz to 10 kHz), high shelf; 0.5 is flat, 1 is +12 dB, 0 kills the band (after the DJ mixer's isolator) |
| **exciter** | 1 | in, amount | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the air above it: the top split off, given new harmonics and laid back under the sound, brighter without getting louder (after the Aphex Aural Exciter) |
| **limiter** | 1 | in, ceiling, release | out, reduction | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | peak limiter with a block of lookahead: nothing comes out above ceiling, ever; release 5 ms to 1 s, reduction how hard it pulls (0 to 1) |
| **gate** | 1 | in, threshold | out, env, open | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | noise gate: opens above threshold, closes below half of it, holds 50 ms, then falls; env is how open it is, open pulses each time it opens |
| **transient** | 1 | in, attack, sustain | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the hit and the ring: attack above 0.5 sharpens each strike, below softens it; sustain lets the tails bloom or dries them, at any level (after the SPL Transient Designer) |
| **tremolo** | 1 | in, rate, depth | left, right | type=amp / harmonic, shape=0, stereo=off / on, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | tremolo, the amp's bulb blinking: type=amp|harmonic (lows against highs, after the brown Fender amps), shape=0 sine to 1 square, stereo=on throbs the right opposite; rate 0.25 to 30 Hz |
| **autowah** | 1 | in, sensitivity, resonance | out | mode=band / low / high, dir=up / down, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | envelope filter: play harder and it opens (dir=up) or closes (dir=down), mode=band|low|high (after the Musitronics Mu-Tron III) |
| **slicer** | 1 | in, clock, depth | out | pattern=trance / eighths / offbeat / onedrop / gallop / tresillo / euclid5 / stutter / random, len=0.75, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | trance gate, the dub mixer's mute in time: a step per clock pulse through pattern=trance|eighths|offbeat|onedrop|gallop|tresillo|euclid5|stutter|random, len= of a step open, soft edges; alone at nature x 8 steps a second |
| **decimate** | 1 | in, rate, jitter | out | hold=step / linear / zero, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the sample rate through the floor on a shaky clock: rate 100 Hz to 48 kHz, jitter wobbles each sample's length, hold=step|linear|zero (zero: all mirrors and bells) (after the Disting) |
| **diode** | 4 | in, carrier, leak | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | diode ring modulator: in rides carrier through four diodes with their dead zone and soft knees; leak unbalances the ring so the carrier bleeds through; its own sine at nature x 60 Hz if carrier is empty (after Bode, Parker's model) |
| **drive** | 1 | in, drive, tone | out | type=od / dist / rat / crunch, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the pedalboard: type=od|dist|rat|crunch (Tube Screamer, DS-1 into a Big Muff tone stack, ProCo RAT, a valve amp on the edge); tone 0 dark, 1 bright |
| **wavesets** | 1 | in, amount | out | mode=repeat / omit / reverse / shuffle / substitute / average, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a broken string of pearls rethreaded: Trevor Wishart's waveset distortion (after the Composers' Desktop Project). The sound is cut at its rising zero crossings; mode= repeat (each 1-8 times: amount), omit (by chance), reverse (by chance), shuffle (among the last 2-16), substitute (a sine of the same length, amount of the way), average (groups of 1-8 into one shape). 32 KB |
| **morph** | 2 | a, b, blend, smooth | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | cross-synthesis: the magnitudes of a on the phases of b, blend how far; smooth makes it a's spectral envelope on b's fine structure, a vocoder with no bands (FFT 512/1024/2048 by nature, 27 ms late at 2; for the Wingie) |
| **specshift** | 2 | in, shift, stretch | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | spectral pitch shift in octaves, peaks moved with their shapes and phases locked (Laroche-Dolson); stretch pulls the partials apart like a stiff string, or squeezes them (FFT by nature, 27 ms late at 2; for the Wingie) |
| **specgate** | 2 | in, threshold | out | keep=loud / quiet, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | spectral gate: every bin below threshold (-90 to 0 dB) falls away, the hiss under the highway goes and the tones stay; keep=quiet keeps only the hiss (FFT by nature, 27 ms late at 2; for the Wingie) |
| **specfilter** | 2 | in, shape_amount, pitch | out | shape=comb / teeth / tilt / formants / harmonics, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a filter drawn on the spectrum, shape=comb|teeth|tilt|formants|harmonics: shape_amount digs it in (for formants it walks a e i o u, for tilt -6 to +6 dB an octave), pitch moves it from 110 Hz (FFT by nature, 27 ms late at 2; for the Wingie) |
| **resynth** | 3 | in, partials, pitch, freeze | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the tonal skeleton: the loudest peaks of each frame (partials 1 to 40) followed and sung again as clean sines, pitch moves them all, freeze holds them while high (after McAulay-Quatieri, SPEAR; FFT 2048 at nature 3, 32 ms late; for the Wingie) |
| **formantshift** | 2 | in, shift | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | moves the formants (the cepstral envelope) by shift octaves and leaves the pitch where it is: a throat that grows or shrinks (FFT by nature, 27 ms late at 2; for the Wingie) |
| **bassman** | 2 | in, sub, drive | out, sub | octave=one / two, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the sound system's low end: split at nature x 60 Hz, a pure sine an octave below (octave=two: two) locked to the bass, drive into a clean limiter; out is everything, sub the low end alone |
| **harmonizer** | 2 | in, pitch, mix | left, right | chord=major / minor / fifths / octaves / sus / seventh / otonal / utonal, detune=8, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | three voices above (or below) in, from chord=major|minor|fifths|octaves|sus|seventh|otonal|utonal, detune= cents of slow chorus, spread left, right and centre; pitch moves the chord (phase vocoder, 27 ms late; for the Wingie) |

## Spaces

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **comb** | 2 | in, pitch, decay | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | comb filter tuned to nature x 60 Hz; pulses pluck it |
| **delay** | 6 | in, time, feedback | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | delay, nature x 50 ms long; time scales it |
| **reverb** | 2 | in, size | out, l, r | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | stereo room reverb |
| **granular** | 1 | in, density | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | granular: grains of what you patch in, or of its own buffer |
| **spring** | 2 | in, decay | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | spring reverb |
| **plate** | 1 | in, decay, damp | out, l, r | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | plate reverb (after Dattorro) |
| **pingpong** | 6 | in, time, feedback | l, r | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | ping-pong delay, nature x 25 ms |
| **tapeecho** | 3 | in, time, feedback, wow | out, wet | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | tape echo with wow, nature x 100 ms (after Roland Space Echo) |
| **freeze** | 1 | in, hold, length | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | freeze: hold loops the last nature x 250 ms |
| **sympathetic** | 2 | in, pitch, decay, chord | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | four sympathetic strings tuned to a chord |
| **pitchshift** | 1 | in, shift | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | pitch shifter, shift in octaves |
| **diffuser** | 1 | in, amount | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | allpass diffuser: smears in time without echoes |
| **reverblite** | 2 | in, size | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a small cheap room: two cross-fed lines, mono (reverb, cut down) |
| **delaylite** | 6 | in, time, feedback | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | delay, nature x 50 ms; time and feedback read once a block, no smoothing (delay, cut down) |
| **wingie** | 2 | in, pitch, decay, strike | out, odd, even | shape=string / bar / membrane / cave / ratio, ratio=1.5, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the Wingie2's resonator: nine modes at nature x 60 Hz ringing on what comes in, its own maths, anti-feedback guard and input / output stage; decay 0..1 is 0.1 to 10 s, strike rings every mode, odd and even split the modes; shape=string|bar|membrane|cave|ratio (ratio=1.5 stacks each mode that much above the last) (after Meng Qi's Wingie2) |
| **cave** | 1 | in, decay, shift, strike | out, odd, even | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the Wingie2's three factory caves as a resonator: nature 1 Low, 2 Mid, 3 High; shift moves them in octaves (after Meng Qi's Wingie2) |
| **coco** | 1 | in, rate, feedback, freeze, skip, flip, pan | out, verso, inverso | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | 8-bit delay whose clock is the time: nature seconds long at full rate, slower is longer and lower; freeze holds the loop, skip jumps the head, flip turns it round; verso and inverso are out panned by pan and against it (after Peter Blasser's Coco) |
| **cocoquantus** | 0.5 | in, right, rate, feedback, freeze, petals, skip, flip | left, right, petal | auto=off / skip / flip / both, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | two Cocos (nature seconds each; in feeds both unless right is patched) and a five-petal Quantussy inside that wobbles their clocks and swings their verso / inverso across; petals is its speed; auto=off|skip|flip|both lets the petals skip and flip (after Peter Blasser's Cocoquantus) |
| **listen** | 1 | in, listen, decay, strike | out, locked | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | tuned to the ground: while listen is held it listens to what comes in; when it lets go it tunes its nine Wingie resonators to the nine strongest resonances it heard, and locked pulses; until then it rings the Low cave times nature (after the Wingie's Tuned to the Ground) |
| **sittingroom** | 1 | in, generations | out, room | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | what comes in played into a room and recorded again, generation after generation (generations 0..1 is 0 to 32 passes) until only the room's resonances are left; nature is the room's size (1: 5.2 x 4.1 x 2.7 m), the name picks its modes (after Alvin Lucier, I Am Sitting in a Room) |
| **talkingdrum** | 2 | in, squeeze, pitch, strike, depth, decay | out | mode=rise / fall, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a membrane resonator at nature x 60 Hz whose pitch is squeezed up to depth octaves: mode=rise climbs with how loud in is, mode=fall starts each stroke high and falls; squeeze squeezes it by hand (after the talking drum) |
| **metastaseis** | 8 | in, go, pitch, stagger, density, decay | out, l, r, pos | chorda=unison / cluster / fifths / minor / major / diminished / wholetone / harmonic / pentatonic / fourths, chordb=cluster / unison / fifths / minor / major / diminished / wholetone / harmonic / pentatonic / fourths, cycle=off / on, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | nine single resonances gliding in straight lines of pitch from chord A to chord B in nature seconds; go turns them round, stagger starts each a little after the one below, density strikes them with dust; chorda= chordb= unison|cluster|fifths|minor|major|diminished|wholetone|harmonic|pentatonic|fourths, cycle=off|on (after Iannis Xenakis, Metastaseis) |
| **beauty** | 1 | in, time, spread, feedback, jitter | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | multitap delay: eight taps across a span (20 ms to 0.8 s, longer at lower samplerates), each drifting in level, pan and brightness of its own; spread fans the taps out (0 all together), jitter makes their times wander (after ppooll's Beauty) |
| **pt2399** | 1 | in, time, feedback | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the PT2399 echo chip: 30 to 340 ms, companded, darker and grittier as the time grows, the clock's whine; turning time bends the pitch, feedback past 0.8 screams (after the Chloe's delay and every PT2399 pedal) |
| **dub** | 1 | in, throw, time, feedback, tone | out, wet | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | dub echo send: only while throw is up does in go into the echo (up to 0.8 s); a spring splash, saturation and a tone that cuts lows and highs live inside the loop, so feedback past 0.8 runs away and can be pulled back; out is in plus the echo (after King Tubby, Lee Perry, the Space Echo) |
| **reverse** | 1 | in, size, mix | out | feedback=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | reverse delay: slices of 30 ms to 0.4 s (longer at lower samplerates) played backwards, two heads crossfading; mix from dry to all reversed; feedback= sends it round again |
| **stutter** | 1 | in, trig, length, decay | out | shrink=0, drop=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | beat repeat: a pulse into trig repeats the last slice (10 to 400 ms) in place of the sound, each repeat quieter by decay (trig held high: no fading); shrink= shortens each repeat (0-0.9), drop= bends each one in semitones (after Ableton's Beat Repeat) |
| **looper** | 1 | in, record, overdub, speed, erase | out, position, end | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | loop pedal: hold record to catch a loop (as long as you hold it, up to 0.42 s at full rate, 0.83 s at half, 3.3 s at eighth), overdub to layer, speed 1 is as played, negative backwards; erase starts again; position 0 to 1, end pulses each turn (after the Echoplex Digital Pro) |
| **tapeloop** | 1 | in, punch, speed, age | left, right | seconds=6, gap=0.37, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a six-second tape loop for the voice: punch records over it (some of the old stays), age wears it (duller and quieter each pass, hiss, wow and flutter), speed in octaves; left is the head just after the record head, right a second head further round; seconds= (0.5-12) and gap= (where the right head sits, 0-1) (after Frippertronics, Basinski) |
| **eis** | 1 | in, rate, depth, feedback | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | four delays of a quarter to half a second whose times wander slowly, feeding each other through a matrix and turning around the stereo field; rate how fast, depth how far (after Pauline Oliveros' Expanded Instrument System) |
| **shimmer** | 1 | in, size, shimmer | left, right | interval=12, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | shimmer reverb: the tail goes round through an octave-up pitch shifter, so it climbs; size 1.5 to 30 s, shimmer how much climbs; interval= in semitones. Heavy: samplerate=half on the SP-1 (after Eno and Lanois, Valhalla Shimmer) |
| **cloud** | 1 | in, freeze, density, size, pitch, spread | left, right | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | texture cloud: up to 24 clean grains of the live sound (Hann windows, Hermite reads, 0.4 s buffer at full rate); freeze holds it, density how many overlap, size 12 ms to 0.5 s, pitch in octaves, spread scatters them in time, place and stereo. Heavy at full density: at home on the Wingie (after Curtis Roads, Barry Truax) |
| **concat** | 1 | in, record, bright, loud, follow, trig | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | concatenative sampler: record cuts the input into 25 ms slices (the last 16 kept, each known by loudness and brightness); the slice nearest bright and loud plays, crossfading into the next; follow aims at the live input's own features and loudness instead; trig picks now (after Coalescence, CataRT) |
| **smear** | 1 | in, blur | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | spectral blur: a 512-point phase vocoder holds each partial's level and frequency for longer, blur from none to seconds, a little across frequency too. Heavy: at home on the Wingie, or samplerate=half on the SP-1 |
| **specfreeze** | 1 | in, freeze, blend | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | spectral freeze: while freeze is up the spectrum is held and sung on with drifting phases; blend lets the live sound seep into what is held; down, the sound passes. Heavy: at home on the Wingie, or samplerate=half on the SP-1 (after ppooll's spectral freeze and the Disting EX's) |
| **cavern** | 1 | in, size, dark, hold | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a long dark reverb for drones: eight slowly moving lines through a Hadamard matrix, size 3 s to a minute, dark how fast the highs go; hold rings for ever and lets nothing new in. Heavy: samplerate=half on the SP-1 (after Jot's feedback delay networks) |
| **doppler** | 1 | in, position, distance, speed | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a source moving past on a line: position -1 to 1 (60 m each way), distance of the line 1 to 40 m, speed sends it by again and again (up to 60 m/s); pitch falls as it passes, it fades and dulls with distance, the far ear hears it later (after Chowning, Xenakis, Leitner) |
| **haas** | 1 | in, width | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | stereo widener: lows stay in the middle, highs open into two complementary combs and a small lead to the left; mono-compatible (after Haas, Gerzon) |
| **swarm** | 1 | in, density, chaos, pitch | left, right | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a swarm of insects: up to twenty tiny clean grains of the live sound, scattered in time, pitch and stereo by a logistic map; low chaos keeps a pattern, high chaos sets them loose; pitch in octaves (after Xenakis, Di Scipio) |
| **widener** | 1 | in, right, width | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | mid and side: in is the left (or the only sound), right the right; width 0 mono, 0.5 as it was, 1 sides doubled, the bass kept in the middle; with only in, its own mono-safe stereo (after Lauridsen) |
| **autopan** | 1 | in, rate, depth | left, right | mode=lfo / random / follow, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the sound walking between the stacks: mode=lfo|random|follow (a sine, a new place each tick, or quiet left and loud right); rate 0.05 to 25 Hz |
| **rotary** | 1 | in, fast | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the Leslie behind the organ: horn and drum turning at their own speeds, the horn's doppler, both their throb, two microphones; fast above 0.5 spins them up, the heavy drum taking its time (after the Leslie 122) |
| **grains** | 6 | in, position, spray, size, density, pitch, spread, freeze | left, right | window=hann / gauss / tukey / blackman / expodec / rexpodec, sync=async / sync, reverse=0, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a field of glass beads catching the room: the hi-fi granulator of what comes in (after Roads, Truax, Granulator II). position 0 now .. 1 the tape's end, spray scatters further back, size 4 ms .. 1 s, density a quarter to 24 grains deep (the level holds), pitch in octaves, spread the stereo width, freeze holds the tape; window=, sync= (a grid or Poisson), reverse= % of grains backwards. Tape: nature tenths of a second (6: 0.6 s, 58 KB; samplerate=half doubles it). Hermite reads, pitched-up grains read at 2x through a half-band, so an octave up doesn't alias; for the Wingie when dense |
| **stretch** | 6 | in, speed, pitch, hold | out | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a slow-motion replay of the evening: WSOLA time-stretch of what comes in (after Verhelst's waveform-similarity overlap-add). speed 1 as it was, 0.1 ten times slower, 0 stands still, negative backwards; pitch in octaves (-2..+1) on its own; a held note stays a clean note. A tape behind it jumps on, where the search finds the sound goes on best; speed 0 or hold stops recording and loops. Tape: nature tenths of a second (6: 0.6 s; samplerate=half doubles it), about 0.1 s late; for the Wingie |
| **paulstretch** | 2 | in, stretch, hold | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | one minute becomes one hour, a glacier: extreme spectral stretch (after Paul Nasca's PaulStretch). Windows of 2048 (nature 1) or 4096 samples (nature 2) given random phases, half over the next; stretch 0..1 is 1x to 1000x, hold stops time and lets the window breathe. The FFT is spread evenly over the blocks. 60 KB, a 0.3-0.5 s tape (samplerate=half doubles it and the window). Wingie weight |
| **psola** | 1 | in, pitch, formant | out, period | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a choir boy grown up overnight: pitch-synchronous overlap-add (after Moulines and Charpentier's TD-PSOLA). Tracks the period (YIN) and lays two-period grains at the new one: pitch in octaves, the formants stay (a voice stays a voice); formant in octaves moves them on purpose; period is the tracked pitch, octaves from middle C. About two periods late; for the Wingie |
| **glisson** | 4 | in, density, size, glide, spread | left, right | window=hann / gauss / tukey / blackman / expodec / rexpodec, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | swallows diving from the bridge: glissons, grains that glide in pitch as they sound (after Curtis Roads' Microsound). glide is the range in octaves (negative falls), spread how many go the other way, how far their start pitches and stereo places scatter; size 10 ms .. 0.5 s; window=. Tape: nature tenths of a second (4: 0.4 s; samplerate=half doubles it); for the Wingie |
| **grainfreeze** | 6 | in, capture, blend, drift | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the photograph that breathes: a pulse into capture holds the last moment as a seamless drone of overlapping gauss grains (after Clouds' freeze, done clean); drift wanders where they read and their pitch (up to half a semitone); blend from what comes in (0) to the held moment (1); a capture crossfades in 30 ms. Until the first pulse it captures itself every half tape. Holds half the tape: nature tenths of a second (6: 0.3 s held; samplerate=half doubles it); for the Wingie |
| **graindelay** | 6 | in, time, jitter, pitch, feedback, density | left, right | tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a mirrored corridor that sings back: granular delay (after Portal and the GR-1, but clean). Grains read the line at time (10 ms .. the tape) give or take jitter, pitched in octaves and spread in the stereo, and feed back through themselves (pitch 1 and feedback: shimmer); density 1 - 12 deep. Tape: nature tenths of a second (6: 0.6 s; samplerate=half doubles it); for the Wingie |
| **scrub** | 6 | in, position, record, smooth | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a hand on the reel: scrubbing. record (a pulse records at least a quarter of the tape, held keeps recording), then position 0..1 moves the head across it, smoothed by smooth (1 ms .. 1 s); slower than normal the grains keep their pitch, faster it rises like tape, stopped holds a grain loop. Before any recording it scrubs the live tape (1 = now). Tape: nature tenths of a second (6: 0.6 s; samplerate=half doubles it); for the Wingie |
| **harmonicgrains** | 3 | in, pitch, density, size | left, right | chord=octaves / fifths / major / minor / harmonic / slendro / wholetone / cluster, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the choir in the empty church: a granular harmoniser, grains of what comes in transposed onto the tones of chord= on pitch (octaves), every tone its own place in the stereo; density 1 - 24 deep, size 20 - 300 ms. Tape: nature tenths of a second (3: 0.3 s; samplerate=half doubles it); for the Wingie |
| **beatgrains** | 6 | in, clock, slices, pitch, length | left, right | burst=1, pattern=flat / up / down / octaves / fifths / random, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the DJ's hand on the platter: tempo-locked stutters. Every clock pulse fires a grain (or burst= of them across the beat, left and right) starting on one of the last 1-16 beats (slices), pitch stepping by pattern= in semitones, length 5% .. twice its share of the beat; 2 ms edges keep attacks sharp. Unclocked: four a second on slices of the tape. Tape: nature tenths of a second (6: 0.6 s; samplerate=half doubles it) |
| **granularverb** | 6 | in, size, density | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a cathedral of sand: a reverb made of grains, dozens of 10-35 ms grains reading a long delay at random times, fed back and gently darkened; size the reach and the decay (0.5 .. 16 s), density 4 - 32 deep. Tape: nature tenths of a second (6: 0.6 s; samplerate=half doubles it); for the Wingie |
| **particles** | 4 | in, density, screen, pitch | left, right | dist=gauss / uniform / cauchy / expon / logist, tune=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | dust in a projector beam: a stochastic cloud after Xenakis's screens, every grain's onset, pitch, length, stereo place and moment drawn from dist=; screen morphs from A (a tight shimmer: unison, 15 ms, near now) to B (two octaves on a Xenakis sieve, 180 ms, half a second back, wide, some reversed); density 2 - 400 a second, pitch the centre in octaves. Tape: nature tenths of a second (4: 0.4 s; samplerate=half doubles it) |
| **convolve** | 2 | in, capture, mix | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the room becomes the effect: a pulse into capture listens for the next loud sound (a clap in a stairwell, under a bridge) and convolves in with what the room gave back, nature x 0.1 s of it (2.5 is all the house holds), as loud as what comes in; until then a made-up room (partitioned FFT convolution, 21 ms late; for the Wingie) |
| **spectraldelay** | 3 | in, time, spread, feedback | out | curve=rising / falling / random, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | every bin its own delay and feedback: time up to nature x 0.1 s, spread fans them across the spectrum by curve=rising|falling|random; echoes that arpeggiate the spectrum (FFT 1024, 32 ms late; for the Wingie) |
| **binaural** | 1 | in, azimuth, elevation, distance | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | puts a sound around your head on headphones: azimuth 0 ahead, 0.25 right, 0.5 behind; elevation -1 to 1; distance 0.25 to 10 m, moving smoothly (Brown-Duda head: interaural delay, head shadow, pinna notch) |
| **ambi** | 1 | in, azimuth, elevation, width | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | first-order ambisonics on two speakers: the sound placed at azimuth (0 ahead, 0.25 right) and elevation, decoded as UHJ stereo; width 0 mono, 0.5 UHJ, 1 super-stereo (after Gerzon) |
| **crossfeed** | 1 | in1, in2, amount | left, right | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | headphone crossfeed, in1 left and in2 right: each ear hears the other side low and late, as from speakers; amount 0 is off (after Bauer, bs2b) |
| **deverb** | 2 | in, amount | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | takes the room out: the slow part of each bin is subtracted, so tails and steady tones sink and attacks stand; negative amount pours the room back in (FFT by nature, 27 ms late at 2; for the Wingie) |

## Currents

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **slope** | 1 | in, trig, rise, fall | out, end | cycle=off / on, range=lfo / audio, curve=lin / exp / log, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | slope / function generator: rises and falls when triggered, follows in otherwise; end into trig cycles (after Serge DUSG) |
| **smoothrandom** | 1 | rate, follow | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | smooth random voltage; with follow patched, it wanders around that instead |
| **steprandom** | 1 | clock | out, pulse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | stepped random, a new value on each clock pulse (or nature / 2 times a second on its own) |
| **glide** | 1 | in, clock, time | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sample & hold with glide: holds in on each clock pulse and slides to it (after Serge Smooth & Stepped) |
| **seq** | 8 | next, back, reset | out, pulse, step | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | step sequencer of semitones: seq 8: 0 3 7 10 |
| **quantizer** | 1 | in, root | out, pulse | scale=nature / justmajor / justminor / justpentatonic / otonal / utonal / chromatic / major / minor / dorian / phrygian / lydian / mixolydian / locrian / harmonicminor / melodicminor / majorpentatonic / minorpentatonic / blues / wholetone / diminished / quartertone / tet19 / tet22 / tet31 / tet53 / bohlenpierce / alpha / beta / gamma / slendro / pelog / rast / bayati / partch / justchromatic / pythagorean / meantone / werckmeister, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | pitch quantizer (octaves in, root transposes); the scale is the nature or scale=: 1 just major, 2 just minor, 3 just pentatonic, 4 otonal 8-15, 5 utonal, 6 chromatic, 7 major, 8 minor, 9 dorian, 10 phrygian, 11 lydian, 12 mixolydian, 13 locrian, 14 harmonic minor, 15 melodic minor, 16 major pentatonic, 17 minor pentatonic, 18 blues, 19 whole tone, 20 diminished, 21 quarter tones (24-TET), 22 19-TET, 23 22-TET, 24 31-TET, 25 53-TET, 26 Bohlen-Pierce, 27-29 Carlos alpha / beta / gamma, 30 slendro, 31 pelog, 32 maqam rast, 33 maqam bayati, 34 Partch 43, 35 just chromatic, 36 Pythagorean, 37 quarter-comma meantone, 38 Werckmeister III |
| **gesture** | 1 | in, record | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | gesture recorder: records in while record is held, then loops it (up to 8 s) |
| **ducker** | 1 | in, side | out, lfo | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | ducker: turns in down when side is loud; lfo is a slow sine |
| **value** | 0 | — | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a fixed value: value 40 is 0.40 |
| **adsr** | 1 | gate, attack, decay, sustain, release | out, end | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | ADSR envelope |
| **lfo** | 1 | rate, reset | sine, tri, saw, square | range=lfo / audio, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | LFO, four shapes, 0 to 1 (after Doepfer A-145) |
| **clock** | 4 | rate, reset | beat, eighth, half, bar | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | clock, 40 to 320 bpm, nature beats to the bar |
| **divider** | 8 | clock, reset | d2, d3, d4, dn | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | clock divider by 2, 3, 4 and by nature (after Doepfer A-160) |
| **euclid** | 16 | clock, hits, rotate, reset | hit, miss, step | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | Euclidean rhythm over nature steps |
| **bernoulli** | 1 | in, chance | a, b | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | Bernoulli gate: each pulse goes to a or b by chance (after Mutable Branches) |
| **turing** | 16 | clock, change | out, pulse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | looping random shift register that slowly rewrites itself; nature is its length (after Music Thing Turing Machine) |
| **drummap** | 1 | clock, x, y, density, reset | kick, snare, hat, accent | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | drum pattern map, x and y move through it (after Mutable Grids) |
| **burst** | 1 | trig, count, rate | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | burst generator: one pulse becomes several |
| **logistic** | 1 | clock, chaos | out, pulse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | logistic map, from order into chaos |
| **phasor** | 1 | rate, reset | ramp, wrap | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | phasor: a 0 to 1 ramp, slow or audio rate |
| **trigdelay** | 1 | in, time | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | trigger delay (after Doepfer A-162) |
| **switch** | 4 | in1, in2, in3, in4, next, reset | out, step | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | sequential switch over up to four inputs (after Doepfer A-151) |
| **automaton** | 30 | clock, seed | alive, pulse, cells | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | elementary cellular automaton, nature is the rule (30, 90, 110...) |
| **randomwalk** | 1 | step, clock | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | random walk that turns back at the walls (after the Wogglebug) |
| **pulser** | 2 | rate, trig | out, ramp | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | timing pulse generator: a pulse every period (nature / 2 per second, rate +-8 octaves); trig fires it now (after Buchla 140) |
| **dusg** | 0 | in1, trig1, rise1, fall1, in2, trig2, rise2, fall2 | out1, end1, out2, end2 | cycle1=off / on, cycle2=off / on, range=lfo / audio, curve=lin / exp / log, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | dual universal slope generator: two slopes, each an envelope, LFO, audio oscillator, slew or lowpass; cycle1 / cycle2 = off|on, range=lfo|audio, curve=lin|exp|log (after the Serge DUSG) |
| **ad** | 0 | trig, attack, decay | out, end | loop=off / on, range=lfo / audio, curve=exp / lin, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | attack / decay envelope; loop=off|on, range=lfo|audio, curve=exp|lin (after Buchla 180; Doepfer A-142) |
| **ar** | 0 | gate, attack, release | out | curve=exp / lin, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | attack / release envelope, held while the gate is; curve=exp|lin (after Doepfer A-141) |
| **decay** | 0 | trig, decay | out | curve=exp / lin, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | percussive envelope: jumps to 1 on a pulse and dies away; curve=exp|lin |
| **stepseq** | 8 | clock, reset, s1, s2, s3, s4, s5, s6, s7, s8 | out, gate, step | dir=forward / backward / pingpong / random, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | eight-stage sequencer: patch values or faders into s1-s8; nature is the length; dir=forward|backward|pingpong|random (after Buchla 123; Doepfer A-155) |
| **flipflop** | 0 | clock, reset | out, inverse | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | toggle flip-flop: flips on each clock pulse (after Doepfer and Serge logic) |
| **trigger** | 0 | in | out | length=2, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | gate to trigger: a pulse on each rising edge; length=2 (ms) (after Doepfer A-162) |
| **petals** | 1 | rate, couple | a, b, c, d | form=tri / square / pulse, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | five triangle cores in a ring, each hurried or held back by its neighbours' signs: lurching, never quite periodic; rate climbs 12 octaves into a drone; form=tri|square|pulse (after Peter Blasser's Quantussy) |
| **relabi** | 1 | rate, smear | pulse, sum, strength | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | almost a beat: four sines at irrational ratios summed, a pulse at each upward zero crossing, strength how steep it was; smear lets each sine wander (after John Berndt's relabi) |
| **haze** | 2 | trig, lo, hi, glide | out | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | on each trigger it glides to a new random value between lo and hi (glide 0..1 is 0 to 2 s); untriggered it moves on its own every nature seconds or so |
| **fourbit** | 1 | in, capture, clock, reset | violin, piano, cello, all | movement=one / two / three, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a capture pulse takes 64 bits from the sign of in; three loops of 64, 68 and 72 steps play them against each other, a pulse for every 1 (16ths at 120 BPM x nature until a clock is patched); movement=one|two|three (after Four Bit Modulo Phase Triplex for Violin, Piano and Cello) |
| **rollz** | 2 | speed, spread, drama | a, b, c, d | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | a ring of five pulse oscillators, nature times a second; each fire pulls its neighbours forward and pushes the far ones back (drama), so the rhythm rolls and lurches instead of keeping a grid; four lanes out (after Peter Blasser's Rollz-5) |
| **probee** | 1 | trig, width | a, b, c, d | spread=uniform / gauss / bimodal, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | on every trigger four new random values, held; width narrows them around 0.5; spread=uniform|gauss|bimodal (after the Probee Max for Live device) |
| **onset** | 1 | in, sensitivity | pulse, strength, env | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | an ear for attacks: each strike in the sound becomes a pulse, over a threshold that rises when it gets busy (spectral flux, lite; like the Wingie's threshold sequencer); strength how hard, env the loudness |
| **pitchtrack** | 1 | in | pitch, confidence, gate | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | an ear for pitch: one voice followed, 60 Hz to 1.5 kHz, about 40 times a second; pitch in octaves from nature x 60 Hz (plays any pitch inlet), held when nothing sings; gate while it hears a note (after YIN) |
| **crowd** | 0 | — | out, count, near, strength | max=20, rest=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the Bluetooth devices near the computer: out is how full the room is (0 to 1, the count over max=), count the plain number, near the strongest signal, strength all of them together; rest= is the count it holds where there is no radio (the SP-1 on its own) |
| **visitors** | 0 | — | arrive, leave, pulse, voice | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | the same devices as they come and go: a pulse when one arrives, when one leaves and on every signal heard; voice is the number (0 to 1) of the one heard last |

## Hands (the SP-1's and the Wingie's controls)

| place | nature | in | out | switches | what it is |
|---|---|---|---|---|---|
| **midi** | 0 | — | pitch, gate, velocity, trig | channel=0, voice=0, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | MIDI to CV: pitch (octaves from middle C), gate, velocity, trig on each note; channel=0 is any, voice=1-4 picks one voice of a chord (0 = mono, last note) |
| **midicc** | 0 | — | out | channel=0, cc=1, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | one MIDI controller, 0 to 1: cc=0-127, 128 = pitch bend (-1 to 1), 129 = aftertouch; channel=0 is any |
| **midiclock** | 0 | — | clock, reset, run | div=6, samplerate=full / half / quarter / eighth, resolution=block / fine / audio | MIDI clock: clock pulses every div ticks (24 = quarter notes, 6 = sixteenths), reset on start, run is 1 while playing |
| **fader** | 1 | — | out | — | faders: fader1 - fader4, 0 to 1 (the Wingie: fader1 Mix, fader2 Decay, fader3 Volume) |
| **mic** | 0 | — | left, right, both | — | the Wingie's microphones (or line in, by its source switch): left, right and both mixed |
| **keys** | 1 | — | pitch, gate, trig, key | — | a Wingie keyboard: keys1 left, keys2 right (an octave up). pitch of the last key in octaves with the octave switch, gate while any is held, trig on each press, key 0 (C) to 1 (B) |
| **key** | 1 | — | out, trig | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | one Wingie key: key 1-12 the left keyboard C to B, 13-24 the right; 1 while held, trig on press |
| **mode** | 1 | — | out, tap | — | the Wingie's Mode buttons: mode1 left, mode2 right; 1 while held, tap pulses on a quick press |
| **octave** | 1 | — | out | — | the Wingie's octave switches: octave1 left, octave2 right; -1, 0 or 1 |
| **source** | 0 | — | out | — | the Wingie's source switch: 0 microphones, 1 line in |
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
| **lights** | 0 | left, right | — | — | the Wingie's two LEDs: red at rest, then purple, yellow and white as what comes in climbs to 1 |
| **slot** | 1 | go, next, back | — | samplerate=full / half / quarter / eighth, resolution=block / fine / audio | another dream: a pulse at go plays the slot this one is named for (slot 7), next and back step through the slots (the Wingie has 12, the SP-1 5) |
| **scene** | 0 | next, back, miss | — | — | goes to the next or previous scene on a pulse; miss is the chance of landing on a random one |
