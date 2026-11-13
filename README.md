<p align="center"><img src="docs/icon.png" width="128" alt="sleepcousonique icon"></p>

# sleepcousonique

A dream-journal patch language and modular editor for the Teenage Engineering **SP-1**.
Patch it on the computer, flash the firmware once, then play the patch on the SP-1 alone:
its faders, track buttons, rocker and LEDs do whatever your dream says.

![sleepcousonique](docs/screenshot.png)

Write a patch as a dated journal entry, or build it on the tapestry of boards and cords;
every visual edit is a text edit, and every text edit redraws the tapestry. Hear it at once
on the computer, then send it over USB to an SP-1 running the Sleepcousonique firmware.

```
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

- **129 places** from across modular history (Moog, Buchla, Serge, Doepfer, Ciat-Lonbarde,
  Hordijk, the DX7...), bare and lite versions for big patches, `samplerate=` on every one.
- **difference tones** (after Alex Chechile), a **quantizer** with 38 scales plus any Scala
  `.scl` file in your scales folder (La Monte Young, ragas, Carlos, Stockhausen, Riley...).
- **knobs on the modules**: tune and attenuverters, live while you drag; undo / redo for everything.
- **cables**: hanging or routed around the boards like Max, opacity, slack, room light with glow.
- **looks**: openprocess (dark by default) or the original journal, light or dark, or follow the computer.

Inspired by the dream journal at femishonuga.com, Peter Blasser's **Nabra** (knobless; cords
with parts in them), **Buchla** and **Serge**, and the **Nord Modular** editor.

**[The manual (PDF)](docs/manual/Sleepcousonique-Manual.pdf)** · **[Flashing the SP-1](FLASHING.md)**

## Install

**macOS** (11+, Apple silicon and Intel)

```
brew tap femifleming/tap
brew install --cask femifleming/tap/sleepcousonique
```

or download `sleepcousonique-macOS.zip` from [releases](https://github.com/femifleming/sleepcousonique/releases)
(not notarised: right-click ▸ Open the first time).

**Windows** (10/11, 64-bit): download `sleepcousonique-Windows.zip` from
[releases](https://github.com/femifleming/sleepcousonique/releases), unzip, run `Sleepcousonique.exe`.

Each zip holds the editor, the SP-1 firmware `sleepcousonique-sp1.bin`, the manual,
`FLASHING.txt` and the example dreams.

## Flash the SP-1

Once: turn the SP-1 off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge and flash
`sleepcousonique-sp1.bin`. Details and recovery: [FLASHING.md](FLASHING.md). The bootloader
is never touched, so T1 + T4 always brings the SP-1 back. Unofficial firmware, not affiliated
with Teenage Engineering.

| folder | what |
|---|---|
| `engine/` | the engine: freestanding C, shared by the editor and the SP-1 |
| `app/` | the desktop editor (JUCE): journal, tapestry, SP-1 hands, sending over USB |
| `firmware/` | the SP-1 firmware: the engine on Wakes' hardware layer (Zephyr 4.3.1) |
| `examples/` | dreams |
| `docs/` | the language, the places, the manual |
| `tools/` | `scqtool` (check / stats / catalog), doc and embed scripts |
| `tests/` | the editor's text-edit tests |

## Build the editor

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # fetches JUCE 9.0.5 (or -DFETCHCONTENT_SOURCE_DIR_JUCE=...)
cmake --build build --target Sleepcousonique ScqTests scqtool
./build/ScqTests_artefacts/Release/ScqTests
./build/scqtool stats examples/first-night.dream
```

## Build the firmware

See [firmware/README.md](firmware/README.md).

## Licence

MIT (see LICENSE). The firmware's hardware layer is Wakes (Worldwave, MIT); its notices are kept in
`firmware/`.
