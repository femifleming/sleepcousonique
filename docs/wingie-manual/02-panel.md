## 2. The panel on the Wingie

Same panel, same jacks. What changes is that nothing is fixed any more: every key, button,
switch and fader is a **hand**, and the dream says what each one does.

| On the panel | In a dream | What it gives |
|---|---|---|
| two microphones (the grilles top left and right) | `mic.left`, `mic.right`, `mic.both` | sound: the Wingie's input, whichever the Line In / Mic switch picks |
| Line In / Mic switch (Line Input ↑ / Microphone ↓) | `source` | it always picks the input, whatever the dream; `source` reads it: 0 microphones, 1 line in |
| L.Octave / R.Octave switches | `octave1`, `octave2` | free controls: 0 down, 0.5 middle, 1 up; through `transpose` they move a pitch by octaves |
| L.Mode / R.Mode buttons | `mode1`, `mode2` | `out` 1 while held, `tap` a pulse when let go within 0.6 s |
| Mix fader | `fader1` | 0 to 1 |
| Decay fader | `fader2` | 0 to 1 |
| Volume fader | `fader3` | 0 to 1 |
| bottom keyboard (`keys1`, C to B) | `keys1`, `key1` … `key12` | `keys1.pitch` of the last key in octaves, `gate`, `trig`, `key` (0 for C … 1 for B) |
| top keyboard (`keys2`, C to B) | `keys2`, `key13` … `key24` | the same, an octave up |
| the two LEDs right of the faders | `lights.left`, `lights.right` | red at rest, then purple, yellow and white as what comes in climbs to 1 |
| line in, out, MIDI in (TRS) and USB-C on the edge | `line.left`, `line.right`, `line.both` (the same sound as `mic`), `out`, `midi` `midicc` `midiclock` | the line in sounds when the switch is up; the MIDI jack feeds the MIDI-to-CV places, as the SP-1's USB-MIDI does |

> **The faders are yours.** On the stock Wingie, Mix is dry/wet, Decay the resonator's ring and
> Volume the input level. In Sleepcousonique they're numbers from 0 to 1 and **nothing is fixed
> to them**: Volume is only a volume if your dream makes it one. There is no hidden master
> volume, so start with your headphones low.

### Panel combinations

| Do this | And |
|---|---|
| hold **both** Mode buttons, press a key on the **bottom** keyboard | play that slot: C is slot 1 … B is slot 12. The LEDs flash yellow |
| hold the **left** Mode, tap the **right** | the next scene |
| hold the **right** Mode, tap the **left** | the previous scene |
| hold **both** Mode buttons **at power-up** | slot 1 goes back to first light |

While both Mode buttons are down, the dream sees no keys, so choosing a slot never plays a
note in the dream you're leaving. Nor does a combination give the dream a stray tap: the Mode
button you pressed first stays held, as far as the dream knows, and is let go only once it has
been down 0.65 s (longer than a tap); the second button never shows at all. A single Mode button
pressed on its own belongs to the dream (`mode1.tap`, `mode2.out`).

> **Which keyboard is which.** `keys1` is the Wingie's **bottom** keyboard and `keys2` the **top**
> one (checked on a Wingie: the top row's C to E arrive as `keys2`). The stock firmware calls them
> the left and right channel's keyboards; on the instrument they sit one above the other, and the
> editor draws them the same way: `keys2` on top, `keys1` below.

### The LEDs

The two LEDs are the dream's own (`lights.left`, `lights.right`): anything patched in lights
them, red at rest, purple past a quarter, yellow past a half, white past three quarters. A few
things borrow them for a moment:

- **a short white blink**: a dream has arrived from the editor and is kept in its slot;
- **a fast white flicker, about a second**: a dream arrived with mistakes, and the Wingie goes on
  playing the one it had;
- **a short yellow flash**: a new slot is playing (from the panel, or because the dream asked
  with `slot`);
- **purple flashing for about a second**: the dream was too heavy, and the Wingie went back to
  the last one that fit (chapter 4).

### Pitch on the keys

`keys1.pitch` is in octaves: C is 0, B is 11/12, and `keys2.pitch` sits one octave above, as on
the stock Wingie. The octave switches are yours to patch anywhere; to use one as an octave switch,
put a `transpose` between it, the keys and the pitch (`octave1 -> oct1`, `keys1.pitch ->
oct1.pitch`, `oct1 -> ring.pitch`). The examples do. Patch it into any `pitch` inlet: a place at nature 2.18 (2.18 × 60 Hz ≈ 131 Hz, the C
below middle C) then plays in tune from C.
