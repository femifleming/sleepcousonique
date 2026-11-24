## 7. The places: spectral and space

The spectral places take the sound apart into frequencies (an FFT), change it there, and put it
back: about 27 ms late at nature 2. They're heavy on an SP-1 and at home on the Wingie. The
space places put a sound somewhere: around your head, between two speakers, under a sound
system.

#### convolve — the room becomes the effect
*Dreamt: a clap in a stairwell, under a bridge.*

A pulse into `capture` listens for the next loud sound and convolves what comes in with what
the room gave back: nature × 0.1 s of it (the default, 1, is 0.1 s). Until the first capture, a
made-up room. Clap, press, and you're playing inside that room.

<!-- place convolve -->

```dream
the stairwell is convolve
mic.both -> stairwell -> out
mode1.tap -> stairwell.capture
fader1 -> stairwell.mix
```

#### morph — cross-synthesis
*Dreamt: one sound wearing the other's skin.*

The magnitudes of `a` on the phases of `b`, `blend` how far; `smooth` makes it `a`'s spectral
envelope on `b`'s fine structure: a vocoder without bands. Here the left microphone speaks
through the right. On the Wingie, `samplerate=half` (at full rate it takes about 86 %).

<!-- place morph -->

```dream
the skin is morph samplerate=half
mic.left -> skin.a
mic.right -> skin.b
fader1 -> skin.blend
fader2 -> skin.smooth
skin -> out
```

#### specshift — spectral pitch shift
*Dreamt: the same voice, a floor up.*

Peaks moved with their shapes and phases locked (Laroche–Dolson); `stretch` pulls the partials
apart like a stiff string, or squeezes them together. Half rate on the Wingie keeps it well
under the limit.

<!-- place specshift -->

```dream
the floor is specshift samplerate=half
mic.both -> floor -> out
keys1.pitch -> floor.shift
fader1 -> floor.stretch
```

#### specgate — spectral gate
*Dreamt: the hiss under the highway goes, the tones stay.*

Every bin below `threshold` (−90 to 0 dB) falls away. `keep=quiet` keeps only what the gate would
have thrown away: the hiss alone.

<!-- place specgate -->

```dream
the sieve is specgate
mic.both -> sieve -> out
fader1 -> sieve.threshold
```

#### spectraldelay — echoes that arpeggiate the spectrum
*Dreamt: every colour of the sound coming back at its own time.*

Every bin its own delay and feedback: `time` up to nature × 0.1 s, `spread` fans the times across
the spectrum by `curve=`.

<!-- place spectraldelay -->

```dream
the prism is spectraldelay 2 rising
mic.both -> prism -> out
fader1 -> prism.time
fader2 -> prism.spread
fader3 -> prism.feedback
```

#### specfilter — a filter drawn on the spectrum
*Dreamt: a, e, i, o, u, sung by a room.*

`shape=` comb, teeth, tilt, formants or harmonics; `shape_amount` digs it in (formants walk
through the vowels), `pitch` moves it from 110 Hz.

<!-- place specfilter -->

```dream
the vowel is specfilter formants
mic.both -> vowel -> out
fader1 -> vowel.shape_amount
keys1.pitch -> vowel.pitch
```

#### resynth — the tonal skeleton
*Dreamt: the sound remembered as sines.*

The loudest peaks of each frame (`partials`, 1 to 40) followed and sung again as clean sines;
`pitch` moves them all, `freeze` holds them while high. After McAulay–Quatieri and SPEAR.

<!-- place resynth -->

```dream
the bones is resynth
mic.both -> bones -> out
fader1 -> bones.partials
keys1.pitch -> bones.pitch
mode1 -> bones.freeze
```

#### formantshift — a throat that grows or shrinks
*Dreamt: the giant and the bird, same pitch.*

Moves the formants (the cepstral envelope) by `shift` octaves and leaves the pitch where it is.
Heavy: at full rate it is more than a Wingie has, so the example runs it at `samplerate=half`.

<!-- place formantshift -->

```dream
the throat is formantshift samplerate=half
mic.both -> throat -> out
fader1 -> throat.shift
```

#### binaural — around your head
*Dreamt: someone whispering behind you.*

For headphones: `azimuth` 0 ahead, 0.25 right, 0.5 behind; `elevation` −1 to 1; `distance` 0.25 to
10 m, moving smoothly (a Brown–Duda head: interaural delay, head shadow, pinna notch).

<!-- place binaural -->

```dream
the whisper is binaural
the orbit is lfo
mic.both -> whisper
orbit -> whisper.azimuth
fader1 -> whisper.distance
whisper.left -> left
whisper.right -> right
```

#### ambi — first-order ambisonics on two speakers
*Dreamt: a circle of loudspeakers folded into two.*

A sound placed at `azimuth` and `elevation`, decoded as UHJ stereo; `width` 0 mono, 0.5 UHJ, 1
super-stereo. After Gerzon.

<!-- place ambi -->

```dream
the circle is ambi
mic.both -> circle
fader1 -> circle.azimuth
fader2 -> circle.width
circle.left -> left
circle.right -> right
```

#### crossfeed — speakers in your headphones
*Dreamt: the room you aren't in.*

Each ear hears the other side low and late, as from speakers; `amount` 0 is off. After Bauer and
bs2b.

<!-- place crossfeed -->

```dream
the room is crossfeed
mic.left -> room.in1
mic.right -> room.in2
fader1 -> room.amount
room.left -> left
room.right -> right
```

#### bassman — the sound system's low end
*Dreamt: a bass you feel in the floor.*

Split at nature × 60 Hz, a pure sine an octave below (`octave=two`: two) locked to the bass,
`drive` into a clean limiter; `sub` is the low end alone.

<!-- place bassman -->

```dream
the floor is bassman
the low is wingie 1.09 string
mic.both -> low
keys1.pitch -> low.pitch
low -> floor -> out
fader1 -> floor.sub
```

#### harmonizer — three voices from one
*Dreamt: you, singing with yourself.*

Three voices above (or below) what comes in, from `chord=`, with `detune=` cents of slow chorus,
spread left, right and centre; `pitch` moves the chord. The heaviest place here: on the Wingie
it needs `samplerate=half`.

<!-- place harmonizer -->

```dream
the three is harmonizer minor samplerate=half
mic.both -> three
keys1.pitch -> three.pitch
fader1 -> three.mix
three.left -> left
three.right -> right
```

#### deverb — takes the room out
*Dreamt: the stairwell, dried.*

The slow part of each bin is subtracted, so tails and steady tones sink and attacks stand;
a negative `amount` pours the room back in.

<!-- place deverb -->

```dream
the dry is deverb
mic.both -> dry -> out
fader1 -> dry.amount
```
