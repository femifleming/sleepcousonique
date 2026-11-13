**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor for macOS 11+ (universal), plus the firmware, manual and examples |
| `sleepcousonique-Windows.zip` | the editor for Windows 10/11 (64-bit), plus the firmware, manual and examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware on its own |
| `Sleepcousonique-Manual.pdf` | the manual |

**macOS with Homebrew:** `brew tap femifleming/tap && brew install --cask femifleming/tap/sleepcousonique`

**Flashing the SP-1 (once):** turn it off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge, and flash
`sleepcousonique-sp1.bin`. After that the editor sends your dreams over USB. The bootloader is
never touched: T1 + T4 always brings the SP-1 back. See FLASHING.md.

The macOS app is ad-hoc signed, not notarised: the cask clears the quarantine; from the zip,
right-click ▸ Open the first time. Unofficial firmware, not affiliated with Teenage Engineering.
