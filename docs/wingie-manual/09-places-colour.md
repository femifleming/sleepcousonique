## 9. The places: colour, dynamics and listening

Pedals, desks, old media and ears: the places that colour a sound, hold it in shape, move it
around, or listen to it and give back currents and pulses. Most are light on both processor and
memory, so they're the seasoning on any Wingie dream.

#### fuzz — germanium fuzz
*Dreamt: a Fuzz Face with a tired battery.*

Two hot transistors, asymmetric. `bias` 0 is the sweet spot; below it the battery dies and the
sound splutters and gates, above it one side fattens.

<!-- place fuzz -->

```dream
the face is fuzz
mic.both -> face -> out
fader1 -> face.drive
fader2 -> face.bias
```

#### chloe — the Chloe module
*Dreamt: a valve glowing behind a singing filter.*

A vacuum tube driven into its grid, a multimode filter (`mode=`) that sings when pushed, and a
low-pass gate struck by `gate` (open when nothing is in it).

<!-- place chloe -->

```dream
the valve is chloe band
mic.both -> valve -> out
fader1 -> valve.drive
fader2 -> valve.cutoff
fader3 -> valve.resonance
keys1.gate -> valve.gate
```

#### tape — the cassette that lived in a car
*Dreamt: summer, a glovebox, a mixtape.*

Wow, flutter, the head bump, soft saturation, hiss and dropouts as `age` wears it; nature is the
speed (1 is 7.5 ips, 2 is 15).

<!-- place tape -->

```dream
the glovebox is tape
mic.both -> glovebox -> out
fader1 -> glovebox.age
fader2 -> glovebox.wow
```

#### vinyl — a record from the archive
*Dreamt: the crackle before the song.*

Crackle and pops, a scratch every turn, rumble, the warp's wow, a worn top end; nature is the speed
(1 is 33, 1.35 is 45).

<!-- place vinyl -->

```dream
the archive is vinyl
mic.both -> archive -> out
fader1 -> archive.age
```

#### radio — shortwave at night
*Dreamt: a station sinking into the sea.*

A narrow band fading in and out, a notch wandering through, whistles where carriers beat, static,
and an AGC that lifts the noise as the station sinks; `tune` 0.5 is on the station.

<!-- place radio -->

```dream
the night is radio
mic.both -> night -> out
fader1 -> night.tune
fader2 -> night.drift
```

#### bandsplit — the crossover
*Dreamt: a sound system's three stacks.*

Low, mid and high that sum back flat; `low` and `high` are the crossover points (fourth-order
Linkwitz–Riley).

<!-- place bandsplit -->

```dream
the stacks is bandsplit
the ring is wingie 2.18 string
mic.both -> stacks
stacks.low -> ring
ring stacks.high -> out
```

#### eq — the mixer's three knobs
*Dreamt: the DJ's isolator.*

Low shelf, sweepable mid (`freq` 80 Hz to 10 kHz), high shelf: 0.5 flat, 1 is +12 dB, 0 kills the
band.

<!-- place eq -->

```dream
the desk is eq
mic.both -> desk -> out
fader1 -> desk.low
fader2 -> desk.mid
fader3 -> desk.high
```

#### exciter — the air above it
*Dreamt: the Aphex box in the rack.*

The top split off, given new harmonics and laid back under the sound: brighter without louder.

<!-- place exciter -->

```dream
the air is exciter
mic.both -> air -> out
fader1 -> air.amount
```

#### limiter — nothing above the ceiling
*Dreamt: a hand on the shoulder.*

A peak limiter with a block of lookahead: nothing comes out above `ceiling`, ever; `reduction` is
how hard it pulls. A good last place before `out` on a resonant dream.

<!-- place limiter -->

```dream
the strings is wingie 2.18 string
the hand is limiter
mic.both -> strings -> hand -> out
fader2 -> strings.decay
fader3 -> hand.ceiling
hand.reduction -> lights.left
```

#### gate — noise gate
*Dreamt: silence, until someone speaks.*

Opens above `threshold`, closes below half of it, holds 50 ms; `env` is how open it is, `open`
pulses each time it opens: a trigger from the room.

<!-- place gate -->

```dream
the door is gate
the skin is wingie 3 membrane
mic.both -> door -> out
fader1 -> door.threshold
door.open -> skin.strike
skin -> out
```

#### transient — the hit and the ring
*Dreamt: a drum with its edges sharpened.*

`attack` above 0.5 sharpens each strike, below softens it; `sustain` lets the tails bloom or dries
them, at any level.

<!-- place transient -->

```dream
the edge is transient
mic.both -> edge -> out
fader1 -> edge.attack
fader2 -> edge.sustain
```

#### widener — mid and side
*Dreamt: the stereo, opened like a book.*

`in` is the left, `right` the right; `width` 0 mono, 0.5 as it was, 1 sides doubled, the bass kept
in the middle.

<!-- place widener -->

```dream
the book is widener
mic.left -> book
mic.right -> book.right
fader1 -> book.width
book.left -> left
book.right -> right
```

#### autopan — walking between the stacks
*Dreamt: a sound pacing the room.*

`mode=` lfo (a sine), random (a new place each tick) or follow (quiet left, loud right); `rate`
0.05 to 25 Hz.

<!-- place autopan -->

```dream
the pace is autopan random
mic.both -> pace
fader1 -> pace.rate
pace.left -> left
pace.right -> right
```

#### tremolo — the amp's bulb blinking
*Dreamt: a brown Fender in a basement.*

`type=harmonic` throbs lows against highs; `shape=` 0 sine to 1 square; `stereo=on` throbs the
right opposite.

<!-- place tremolo -->

```dream
the bulb is tremolo harmonic stereo=on
mic.both -> bulb
fader1 -> bulb.rate
fader2 -> bulb.depth
bulb.left -> left
bulb.right -> right
```

#### rotary — the Leslie
*Dreamt: the cabinet behind the organ.*

Horn and drum at their own speeds, the horn's doppler, both their throb, two microphones; `fast`
above 0.5 spins them up, the heavy drum taking its time.

<!-- place rotary -->

```dream
the cabinet is rotary
mic.both -> cabinet
mode1 -> cabinet.fast
cabinet.left -> left
cabinet.right -> right
```

#### autowah — envelope filter
*Dreamt: the Mu-Tron, quacking.*

Play harder and it opens (`dir=up`) or closes (`dir=down`); `mode=` band, low or high.

<!-- place autowah -->

```dream
the quack is autowah
mic.both -> quack -> out
fader1 -> quack.sensitivity
fader2 -> quack.resonance
```

#### slicer — trance gate
*Dreamt: the dub mixer's mute, in time.*

A step per `clock` pulse through `pattern=`, `len=` of each step open, soft edges; alone it runs at
nature × 8 steps a second.

<!-- place slicer -->

```dream
the mute is slicer onedrop
mic.both -> mute -> out
fader1 -> mute.depth
```

#### onset — an ear for attacks
*Dreamt: the Wingie's threshold sequencer, listening.*

Each strike in the sound becomes a pulse, over a threshold that rises when it gets busy; `strength`
how hard, `env` the loudness. Tap the Wingie's case and it plays the dream.

<!-- place onset -->

```dream
the ear is onset
the walk is seq 5: 0 3 7 10 12
the bar is wingie 2.18 bar
mic.both -> ear
fader1 -> ear.sensitivity
ear.pulse -> walk.next bar.strike
walk -> bar.pitch
bar -> out
```

#### pitchtrack — an ear for pitch
*Dreamt: the instrument humming along with you.*

One voice followed, 60 Hz to 1.5 kHz, about 40 times a second; `pitch` in octaves (plays any pitch
inlet), held when nothing sings; `gate` while it hears a note.

<!-- place pitchtrack -->

```dream
the listener is pitchtrack
the twin is wingie 2.18 string
mic.both -> listener
listener.pitch -> twin.pitch
listener.gate -> twin.strike
twin -> out
```

#### decimate — sample rate through the floor
*Dreamt: a Disting on a shaky clock.*

`rate` 100 Hz to 48 kHz, `jitter` wobbles each sample's length, `hold=zero` is all mirrors and
bells.

<!-- place decimate -->

```dream
the floor is decimate zero
mic.both -> floor -> out
fader1 -> floor.rate
fader2 -> floor.jitter
```

#### diode — diode ring modulator
*Dreamt: Bode's bells.*

The sound rides `carrier` through four diodes, with their dead zone and soft knees; `leak`
unbalances the ring. With nothing in `carrier`, its own sine at nature × 60 Hz.

<!-- place diode -->

```dream
the ring is diode
mic.both -> ring -> out
keys1.pitch -> ring.carrier
fader1 -> ring.leak
```

#### drive — the pedalboard
*Dreamt: a Tube Screamer, a RAT, an amp on the edge.*

`type=` od, dist, rat or crunch; `tone` 0 dark, 1 bright.

<!-- place drive -->

```dream
the board is drive rat
mic.both -> board -> out
fader1 -> board.drive
fader2 -> board.tone
```
