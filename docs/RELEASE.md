**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor and **Sleepcousonique Array** for macOS 11+ (universal), their **Legacy** versions for macOS 10.13+, the firmware, manual and examples |
| `sleepcousonique-Windows.zip` | the editor and its **Legacy** version for Windows 10/11 (64-bit), the firmware, manual and examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware on its own |
| `Sleepcousonique-Manual.pdf` | the manual |

**New in 1.4.0: room for bigger patches.** The SP-1 plays the same dreams with about a third less
of its processor, measured on three SP-1s: the dance scenes that ran at 91–95 % now run at 51–66 %,
*a history of modular* went from 68–85 % to 54–74 %, an empty dream from 9 % to 4 %. The sound is
the same to the bit. Most of a big patch's cost was getting each place its inlets, and the SP-1's
own C library clearing memory a byte at a time: inlets are now built in one pass, places at a lower
`samplerate=` hand each other their short blocks, and the engine is compiled for speed.

- **More room:** 124 KB of memory (was 80), 96 places (was 64), 320 passages (was 224).
- **Meters you can trust:** the load meter was refitted to 390 patches measured on the SP-1s and
  reads within a few points of what the SP-1 itself reports (it read 10–25 points high). MEMORY is
  now **ROOM**: whichever limit the scene is nearest, named beside the number (memory, places,
  passages, or the 16 KB of text a slot keeps); **send to SP-1** refuses a dream too long for a slot.
- **Overload:** a dream a little over budget drops only the blocks it must, instead of every other.
- **Fixed in the editor:** pasting a copy of a place whose name was taken could rename the place
  itself (`the combb is combb`) and its params; dragging a cord's amount below zero wrote text that
  couldn't be read back; a lucid range can now end below zero (`0..-50`).
- **The Array:** no longer freezes at launch when a loopback driver (BlackHole) stops answering
  (it goes on without it and says so); SP-1s the Mac drift-corrects now start in step with the
  first one instead of catching up over 45 s.
- **The firmware is 1.4.0** and has 1.3.0's bell and marimba fix and the engine's self-healing,
  tested on the hardware (a bell and a marimba pitch-thrown ten octaves twice a second keep
  sounding). Flash it as before (T1 + T4); your dream slots are kept. Older firmware stays on
  earlier releases and in `firmware/dist/`.
- For tinkerers: `tools/sp1fleet.py` measures dreams on every SP-1 plugged in at once, and
  `tools/calibrate.py` refits the load meter from what they measure.

**macOS with Homebrew:** `brew tap femifleming/tap && brew install --cask femifleming/tap/sleepcousonique`

**Flashing the SP-1 (once):** turn it off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge, and flash
`sleepcousonique-sp1.bin`. After that the editor sends your dreams over USB. The bootloader is
never touched: T1 + T4 always brings the SP-1 back. See FLASHING.md.

The macOS app is ad-hoc signed, not notarised: the cask clears the quarantine; from the zip,
right-click ▸ Open the first time. Unofficial firmware, not affiliated with Teenage Engineering.
