**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor and **Sleepcousonique Array** for macOS 11+ (universal), their **Legacy** versions for macOS 10.13+, the firmware, manual and examples |
| `sleepcousonique-Windows.zip` | the editor and its **Legacy** version for Windows 10/11 (64-bit), the firmware, manual and examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware on its own |
| `Sleepcousonique-Manual.pdf` | the manual |

**New in 1.2.0:** maybe-cords (`?>`); MIDI to CV places (`midi`, `midicc`, `midiclock`); the SP-1 as a
USB-MIDI device and, in **interface mode** (hold •• + PLAY for 1 s), as a 2-channel USB sound card;
**Sleepcousonique Array** (several SP-1s as one multichannel output: routing, WAV player, master,
spectrograms); three looks (journal, faltung, modular); light **Legacy** versions of both apps for
old computers; faster power on (0.5 s) and off (1.5 s). Older firmware stays available on earlier
releases (and in `firmware/dist/` by version), so you can always flash back.

**macOS with Homebrew:** `brew tap femifleming/tap && brew install --cask femifleming/tap/sleepcousonique`

**Flashing the SP-1 (once):** turn it off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge, and flash
`sleepcousonique-sp1.bin`. After that the editor sends your dreams over USB. The bootloader is
never touched: T1 + T4 always brings the SP-1 back. See FLASHING.md.

The macOS app is ad-hoc signed, not notarised: the cask clears the quarantine; from the zip,
right-click ▸ Open the first time. Unofficial firmware, not affiliated with Teenage Engineering.
