## 8. The places: time

Delays, loops, reverbs and freezes: everything that keeps the past and plays it again. Like the
grains, their memory is the limit on the Wingie: most take 20 to 40 KB (`tapeloop` and `eis` about
40 KB each, so not both in one scene), and the grey line under each example says how much.
`cavern`, `shimmer` and `smear` are heavier on the processor than on memory.

#### beauty — a multitap delay
*Dreamt: eight echoes, each with a mind of its own.*

Eight taps across a span of 20 ms to 0.8 s, each drifting in level, pan and brightness; `spread`
fans them out, `jitter` makes their times wander. After ppooll's Beauty.

<!-- place beauty -->

```dream
the eight is beauty
mic.both -> eight
fader1 -> eight.time
fader2 -> eight.feedback
fader3 -> eight.spread
eight.left -> left
eight.right -> right
```

#### pt2399 — the echo chip
*Dreamt: every cheap delay pedal you loved.*

The PT2399: 30 to 340 ms, companded, darker and grittier as the time grows, with the clock's
whine; turning `time` bends the pitch, `feedback` past 0.8 screams. After the Chloe's delay.

<!-- place pt2399 -->

```dream
the chip is pt2399
mic.both -> chip -> out
fader1 -> chip.time
fader2 -> chip.feedback
```

#### dub — the dub echo send
*Dreamt: King Tubby's hand on the send.*

Only while `throw` is up does the sound go into the echo (up to 0.8 s); a spring splash,
saturation and a tone control live inside the loop, so `feedback` past 0.8 runs away and can be
pulled back.

<!-- place dub -->

```dream
the send is dub
mic.both -> send -> out
mode1 -> send.throw
fader1 -> send.time
fader2 -> send.feedback
fader3 -> send.tone
```

#### reverse — reverse delay
*Dreamt: the words before they're said.*

Slices of 30 ms to 0.4 s played backwards, two heads crossfading; `mix` from dry to all reversed;
`feedback=` sends it round again.

<!-- place reverse -->

```dream
the before is reverse feedback=0.4
mic.both -> before -> out
fader1 -> before.size
fader2 -> before.mix
```

#### stutter — beat repeat
*Dreamt: the record skipping on purpose.*

A pulse into `trig` repeats the last slice (10 to 400 ms) in place of the sound, each repeat
quieter by `decay`; `shrink=` shortens each repeat, `drop=` bends each down in semitones.

<!-- place stutter -->

```dream
the skip is stutter shrink=0.3 drop=1
mic.both -> skip -> out
mode1 -> skip.trig
fader1 -> skip.length
```

#### looper — the loop pedal
*Dreamt: a phrase caught in the hand.*

Hold `record` to catch a loop (up to 0.42 s at full rate, 0.83 s at half, 3.3 s at eighth),
`overdub` to layer, `speed` 1 as played and negative backwards, `erase` to start again.

<!-- place looper -->

```dream
the catch is looper samplerate=eighth
mic.both -> catch -> out
mode1 -> catch.record
mode2 -> catch.overdub
fader1 -> catch.speed
```

#### tapeloop — Frippertronics
*Dreamt: Basinski's tape, wearing away.*

A six-second loop: `punch` records over it (some of the old stays), `age` wears it, duller and
quieter each pass, with hiss, wow and flutter; `speed` in octaves. `left` is the head just after the
record head, `right` a second head further round.

<!-- place tapeloop -->

```dream
the frip is tapeloop
mic.both -> frip
mode1 -> frip.punch
fader1 -> frip.age
frip.left -> left
frip.right -> right
```

#### eis — the Expanded Instrument System
*Dreamt: Pauline Oliveros in the cistern.*

Four delays of a quarter to half a second whose times wander slowly, feeding each other through a
matrix and turning around the stereo; `rate` how fast, `depth` how far. Play long tones into it
and listen.

<!-- place eis -->

```dream
the cistern is eis
mic.both -> cistern
fader1 -> cistern.rate
fader2 -> cistern.feedback
cistern.left -> left
cistern.right -> right
```

#### shimmer — the climbing reverb
*Dreamt: Eno and Lanois's halo.*

The tail goes round through an octave-up shifter, so it climbs; `size` 1.5 to 30 s, `shimmer` how
much climbs; `interval=` in semitones.

<!-- place shimmer -->

```dream
the halo is shimmer interval=7
mic.both -> halo
fader1 -> halo.size
fader2 -> halo.shimmer
halo.left -> left
halo.right -> right
```

#### cloud — a texture cloud
*Dreamt: Clouds, after the rain.*

Up to 24 clean grains of the live sound: `freeze` holds it, `density` how many overlap, `size` 12 ms
to 0.5 s, `pitch` in octaves, `spread` scatters them. At home on the Wingie at full density.

<!-- place cloud -->

```dream
the after is cloud
mic.both -> after
mode1 -> after.freeze
fader1 -> after.density
fader2 -> after.size
keys1.pitch -> after.pitch
after.left -> left
after.right -> right
```

#### concat — the concatenative sampler
*Dreamt: the room answering with its own words.*

`record` cuts the input into 25 ms slices (the last 16 kept, each known by loudness and
brightness); the slice nearest `bright` and `loud` plays. `follow` aims at the live sound's own
features instead.

<!-- place concat -->

```dream
the answer is concat
mic.both -> answer -> out
mode1 -> answer.record
fader1 -> answer.bright
fader2 -> answer.loud
fader3 -> answer.follow
```

#### smear — spectral blur
*Dreamt: a watercolour left out in the rain.*

Each partial's level and frequency held for longer, `blur` from none to seconds. Heavy: at home
on the Wingie.

<!-- place smear -->

```dream
the wash is smear
mic.both -> wash -> out
fader1 -> wash.blur
```

#### specfreeze — spectral freeze
*Dreamt: one chord, standing still in the air.*

While `freeze` is up the spectrum is held and sung on with drifting phases; `blend` lets the live
sound seep into what is held.

<!-- place specfreeze -->

```dream
the still is specfreeze
mic.both -> still -> out
mode1 -> still.freeze
fader1 -> still.blend
```

#### cavern — a long dark reverb
*Dreamt: a cave under the cave.*

Eight slowly moving lines through a Hadamard matrix, `size` 3 s to a minute, `dark` how fast the
highs go; `hold` rings for ever and lets nothing new in. For drones.

<!-- place cavern -->

```dream
the under is cavern
mic.both -> under
fader1 -> under.size
fader2 -> under.dark
mode1 -> under.hold
under.left -> left
under.right -> right
```

#### doppler — a source going past
*Dreamt: a train, a scooter, a bird.*

On a line 60 m each way: `position` −1 to 1, `distance` of the line 1 to 40 m, `speed` sends it by
again and again. The pitch falls as it passes; the far ear hears it later.

<!-- place doppler -->

```dream
the train is doppler
mic.both -> train
fader1 -> train.speed
fader2 -> train.distance
train.left -> left
train.right -> right
```

#### haas — stereo widener
*Dreamt: one microphone, two ears.*

Lows stay in the middle, highs open into two complementary combs and a small lead to the left;
mono-compatible.

<!-- place haas -->

```dream
the wide is haas
mic.left -> wide
fader1 -> wide.width
wide.left -> left
wide.right -> right
```

#### swarm — a swarm of insects
*Dreamt: a summer field at dusk.*

Up to twenty tiny grains of the live sound scattered in time, pitch and stereo by a logistic map:
low `chaos` keeps a pattern, high sets them loose.

<!-- place swarm -->

```dream
the dusk is swarm
mic.both -> dusk
fader1 -> dusk.chaos
fader2 -> dusk.density
dusk.left -> left
dusk.right -> right
```
