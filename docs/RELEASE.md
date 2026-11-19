**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor and **Sleepcousonique Array** for macOS 11+ (universal), their **Legacy** versions for macOS 10.13+, the firmware, manual and examples |
| `sleepcousonique-Windows.zip` | the editor and its **Legacy** version for Windows 10/11 (64-bit), the firmware, manual and examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware on its own |
| `Sleepcousonique-Manual.pdf` | the manual |

**New in 1.3.0:** the room: two places, `crowd` and `visitors`, hear the Bluetooth devices near the
computer: how many there are (any number, against `max=`), how strong the nearest and all of them
are, and a pulse when one arrives, when one leaves and on every call heard. The editor on macOS
listens with the Mac's Bluetooth while a dream has one of them (macOS asks once); on Windows and on
the SP-1 alone `crowd` holds its `rest=` count. See *The room* in the manual.

**Fixed:** with a fader patched into an inlet and that inlet's attenuverter turned all the way up,
moving the fader could cut the sound and leave the place dead until the editor was restarted (the
bell and the marimba, through `att.pitch`). Their modes no longer grow when the pitch is thrown
about; the engine now starts any place that runs away again by itself, within a block; an Intel
or Windows crash on a bad control value is closed; and the reverbs no longer change after three
hours of playing. Every place and inlet is swept at both ends of its attenuverter on every build.

**The SP-1 firmware in this release is still 1.2.0's.** It does not yet contain the bell and marimba
fix or the engine's self-healing, so on the SP-1 itself the same move (a fader through
`att.pitch` at the top, thrown about while a bell or a marimba rings) can still upset that dream
until you change scene or send it again. A firmware with the fix, tested on the hardware, follows.
Older firmware stays available on earlier releases (and in `firmware/dist/` by version), so you
can always flash back.

**macOS with Homebrew:** `brew tap femifleming/tap && brew install --cask femifleming/tap/sleepcousonique`

**Flashing the SP-1 (once):** turn it off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge, and flash
`sleepcousonique-sp1.bin`. After that the editor sends your dreams over USB. The bootloader is
never touched: T1 + T4 always brings the SP-1 back. See FLASHING.md.

The macOS app is ad-hoc signed, not notarised: the cask clears the quarantine; from the zip,
right-click ▸ Open the first time. Unofficial firmware, not affiliated with Teenage Engineering.
