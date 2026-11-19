**sleepcousonique**: a dream-journal patch language and modular editor for the Teenage Engineering SP-1.

| download | for |
|---|---|
| `sleepcousonique-macOS.zip` | the editor and **Sleepcousonique Array** for macOS 11+ (universal), the firmware, manual and examples |
| `sleepcousonique-Windows.zip` | the editor for Windows 10/11 (64-bit), the firmware, manual and examples |
| `sleepcousonique-sp1.bin` | the SP-1 firmware on its own |
| `Sleepcousonique-Manual.pdf` | the manual |

**New in 1.4.1: sending and installing, fixed.** A friend's report ("a bug with the .bin, or
the send to SP-1 button, or flashing") came down to these:

- **After flashing, the editor said "Flash the firmware first".** The SP-1 restarts from the
  updater *switched off* (standby: started by the USB cable, it charges and stays quiet), and the
  firmware only answers the editor while it plays. The editor took the silence for a missing
  firmware. It now recognises an SP-1 in standby and says so (in the scene bar and when you send):
  **hold •• for about a second to turn it on**. FLASHING.txt and the manual said it would start
  playing by itself; they now say it waits.
- **send to SP-1 froze the window** for up to 14 s (longer with no SP-1 found), and an SP-1 that
  stopped reading could hang it for good. Sending now runs beside the window, every step has a time
  limit, and the editor no longer opens the SP-1's port twice at once (on Windows the second open
  always failed, which also read as "Flash the firmware first").
- **Plain reasons when a send fails:** SP-1 off, port busy in another program, other firmware on
  the SP-1, an SP-1 on older firmware that can't read a newer dream (flash the one in this zip),
  no SP-1 plugged in. Windows no longer tries COM ports of SP-1s that aren't plugged in.
- **Windows: Sleepcousonique.exe needed the Visual C++ Redistributable** (VCRUNTIME140.dll,
  MSVCP140.dll) and didn't start without it; the runtime is now built in.
- **Homebrew:** the 1.4.0 cask's checksum no longer matched the re-uploaded zip, so new installs
  failed; the cask is 1.4.1 now.
- **The firmware is unchanged: 1.4.0** (the same `sleepcousonique-sp1.bin`, checked: built for the
  SP-1's application slot at 0x20000, never touching the bootloader). If your SP-1 runs 1.4.0
  there's nothing to flash.

Tests: the USB link is now tested against a pretend SP-1 that speaks the firmware's protocol and
compiles dreams with the real engine, in process on macOS and Windows, through a real
pseudo-terminal on macOS, and through the editor's own `--send`. `docs/HARDWARE-CHECK.md` lists the
checks that need a real SP-1.

1.4.0 brought room for bigger patches (about a third less of the SP-1's processor for the same
dreams; 124 KB, 96 places, 320 passages), a load meter refitted to 390 patches measured on SP-1s,
and the bell and marimba fix.

**macOS with Homebrew:** `brew tap femifleming/tap && brew install --cask femifleming/tap/sleepcousonique`

**Flashing the SP-1 (once):** turn it off, hold **T1 + T4** and plug in USB (boot mode), open
[solderless.engineering](https://solderless.engineering) in Chrome or Edge, and flash
`sleepcousonique-sp1.bin`. It restarts switched off: hold **••** to turn it on. After that the editor sends your dreams over USB. The bootloader is
never touched: T1 + T4 always brings the SP-1 back. See FLASHING.md.

The macOS app is ad-hoc signed, not notarised: the cask clears the quarantine; from the zip,
right-click ▸ Open the first time. Unofficial firmware, not affiliated with Teenage Engineering.
