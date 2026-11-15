# flashing the sp-1

Sleepcousonique runs on a Teenage Engineering **SP-1** with its own firmware,
`sleepcousonique-sp1.bin` (in the release zip, and in `firmware/dist/`). You flash it once;
after that the editor sends your dreams over USB and you never need to flash again to change
a patch.

You need: the SP-1, a USB-C data cable, and **Chrome or Edge** (the updater uses WebUSB).

1. **Turn the SP-1 off** and unplug it.
2. **Boot mode:** hold **T1 + T4** (the first and last track buttons) and plug in USB.
   Let go when T1 lights.
3. Open **[solderless.engineering](https://solderless.engineering)** in Chrome or Edge.
4. Choose **custom firmware / .bin**, pick `sleepcousonique-sp1.bin`, connect to the SP-1
   when the browser asks, and flash.
5. The SP-1 restarts into Sleepcousonique and plays the first night. Open the editor: the
   scene bar says **SP-1 connected** when it hears it.

## If something goes wrong

The bootloader is never touched, so the SP-1 can always come back: unplug, hold **T1 + T4**,
plug in, and flash any `.bin` again: this one, Wakes, the tape looper or anything else.

- Nothing sounds and the editor's load meter says **TOO HEAVY**: the dream is more than the
  SP-1 can play. Send a lighter one (lite places, `samplerate=half`).
- Reflashing may clear the five dream slots on the SP-1: keep your `.dream` files.
- **Windows:** the SP-1 shows up as a USB serial (COM) port with Windows' own driver; nothing
  to install. **macOS:** it appears as `/dev/cu.usbmodem…`.

This firmware is unofficial and not affiliated with or endorsed by Teenage Engineering.

## Going back to an older version

Every firmware in `firmware/dist/` is named after its version (`sleepcousonique-sp1-v1.1.1.bin`,
`sleepcousonique-sp1-v1.2.0-beta1.bin`, ...); the plain `sleepcousonique-sp1.bin` is always the
last released one. Flashing an older .bin works exactly like flashing a new one (boot mode: hold
T1 + T4 while plugging in). Your five dream slots live in their own part of the flash, away
from the firmware, so they should survive; your .dream files on the computer are the real copy,
so send them again if a slot comes back empty. The editor's status bar (or `SCQ HELLO` in a serial
terminal) shows which version the SP-1 is running.
