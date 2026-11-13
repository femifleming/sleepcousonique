# Sleepcousonique

A dream-journal patch language for the Teenage Engineering **SP-1**, with a desktop editor.

Write a patch as a dated journal entry, or build it on a splatter-painted tapestry of boards
and cords. Hear it at once on your computer, then send it over USB to an SP-1 running the
Sleepcousonique firmware, where the faders, track buttons, rocker and LEDs play it.

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

Inspired by the dream journal at femishonuga.com, Peter Blasser's **Nabra** (knobless; cords
with parts in them), **Buchla** and **Serge**, and the **Nord Modular** editor.

**The manual:** [docs/manual/Sleepcousonique-Manual.pdf](docs/manual/Sleepcousonique-Manual.pdf)

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

MIT. The firmware's hardware layer is Wakes (Worldwave, MIT); its notices are kept in
`firmware/`.
