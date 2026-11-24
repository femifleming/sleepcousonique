**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1, and now for Meng Qi's Wingie2.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor and **Sleepcousonique Array** for macOS 11+ (universal), both firmwares, both manuals, examples |
| `sleepcousonique-Windows.zip` | the editor for Windows 10/11 (64-bit), both firmwares, both manuals, examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware 1.5.0 on its own (flash it at solderless.engineering, T1 + T4) |
| `sleepcousonique-wingie.bin` | the Wingie2 firmware 1.5.0 on its own (esptool, at address 0; GPLv3) |
| `Sleepcousonique-Manual.pdf` | the manual |
| `Sleepcousonique-Wingie-Manual.pdf` | the Wingie manual |

## New in 1.5.0

**The Wingie2.** Put `instrument wingie` at the top of a dream and it plays on a Wingie2 running the
Sleepcousonique firmware. The editor switches between SP-1 and Wingie in the Device menu and draws the
real instrument.
- Both microphones are inputs. Both keyboards are places (`keys1` is the **bottom** keyboard, `keys2`
  the top), and so are each key, the Mode buttons, the octaves, the three knobs and the lights.
- **12 slots** on the Wingie, kept through power-off. Hold both Mode buttons and press a bottom key
  to change slot. Hold one Mode button and tap the other to change scene.
- **Its own meters.** The CPU and memory figures were measured on a Wingie: about 70 KB of room, and
  places cost about twice what they do on the SP-1.
- The firmware is built on Meng Qi's Wingie2 code and is GPLv3. The stock firmware goes back on with
  Meng Qi's web flasher.

**239 places** (was 96), most of them for processing:
- **hi-fi granular:** `grains`, `grainfreeze`, `particles`, `paulstretch`, `stretch` and more
  (smooth windows, not Clouds-style);
- **spectral:** `specfilter`, `specfreeze`, `specshift`, `specgate`, `spectraldelay`, `harmonizer`;
- **time:** `shimmer`, `doppler`, `reverse`, `beauty`, `dub`, `swarm` and more;
- **colour:** `transient`, `eq`, `fuzz`, `autowah`, `chloe` and more.
- Every place can run at `samplerate=half` or `quarter` to save work, on both instruments.

**The Wingie as a processing unit:**
- **Line In / Mic switch:** it always picks the input, whatever the dream says. At power-up the
  Wingie now reads it (it used to start on the microphones until the switch was flipped).
- **`line`** (`line.left`, `line.right`, `line.both`): the Wingie's input by its other name, the
  same sound as `mic`.
- **The octave switches are free controls** (0, ½, 1): patch them into anything. The keys no
  longer add them to their pitch.
- **`transpose`** turns any control into whole octaves (or `transpose 7st`, fifths), with a
  `pitch` inlet that passes through: `octave1 -> oct1`, `keys1.pitch -> oct1.pitch`,
  `oct1 -> vco.pitch` is an octave switch again. The Wingie examples use it.
- **`pitch` on the shifters:** `cave`, `pitchshift`, `specshift` and `formantshift` take the
  keys at `pitch` too (it reaches their `shift`).
- **The patcher names the panel:** the hands board says `mix fader1`, `bottom keys1.pitch`,
  `L.oct octave1`, `line/mic source`. The drawn Wingie has drag points by the octave switches,
  Mode buttons, Line/Mic switch, LINE IN and MIC.
- **7 new processing dreams** for line in, measured scene by scene on a Wingie (no dropouts):
  `wingie-proc-glass-cloud` (hi-fi granular), `-prism` (spectral), `-dub-time`, `-caves`
  (resonators from the keys), `-looper`, `-everything` (grain → spectral → time chains) and
  `-follow` (the line drives the patch).

**New in the language:**
- pitch in units: `7st`, `-50ct`, `1oct`;
- `resolution=fine|audio` for audio-rate pitch;
- the `slot` place, so a dream can move to another slot;
- a scene ignores pulses in its first 0.1 s, so one pulse can't skip two scenes.

**Too heavy goes back.** A dream that's too heavy for the instrument no longer stutters: it goes
back to the last dream that fit (on the SP-1, the one kept in the slot; else first night or first
light), and the editor says so.

**The SP-1's load meter** is remeasured place by place on 1.5.0 (522 patches on an SP-1). Its
estimates are off by about 2.5 points of the processor on average, where they were 13.7. Five
example dreams that measured too heavy now use `samplerate=` on their heaviest places.

**Fixed:**
- `shimmer` crashed the SP-1 (a hard reset): its memory was misaligned for the SP-1's processor. A
  new check (`tools/test_alignment.sh`) runs every place under the compiler's alignment checker so
  this can't come back.
- `additive`'s pitch maths returned one octave off internally; it sounds the same as before.
- `grains`, `glisson`, `particles` and `harmonicgrains` could stay silent for up to half a minute
  after density was pulled right down with a large size and brought back up. Fixed in the editor
  and on the Wingie; the SP-1 gets it with its next firmware (it changes no dream, only that wait).
- Six older Wingie example scenes were too heavy for the Wingie; first light put a DC offset on the
  output (its Volume fader went into the room's sound input). Both fixed.

**Flash the SP-1 again:** 1.4 can't read dreams that use the new places. Checked: built for the SP-1's
application slot at 0x20000, never touching the bootloader.

Tests:
- every place, and every inlet of every place (19,106 cases);
- every example played like a gig (`tools/test_dreams.sh`: line in, the hands moved at random,
  scenes, 50 reloads; 2,580 scenes in the deepest run), on every build;
- the octave switches and `transpose`, and the keys reaching every pitch inlet;
- the alignment check;
- every example dream;
- the editor's tests and the USB link against a pretend SP-1 and a pretend Wingie;
- on a real SP-1 and a real Wingie: the fallback, the slot place, scenes, Wingie dreams refused by the
  SP-1, and the meters.
