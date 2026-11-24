# flashing the sp-1

Sleepcousonique runs on a Teenage Engineering **SP-1** with its own firmware,
`sleepcousonique-sp1.bin` (in the release zip, and in `firmware/dist/`). You flash it once;
after that the editor sends your dreams over USB and you never need to flash again to change
a patch.

## Flashing is not sending

These are two different jobs. People mix them up, so read this first:

| | **send to SP-1** (in the editor) | **flash the firmware** (this page) |
|---|---|---|
| what it does | puts a *dream* (your patch) on the SP-1 | installs the Sleepcousonique *firmware* |
| how often | every time you change a patch | **once** (and again only to update the firmware) |
| where | the Sleepcousonique editor | [solderless.engineering](https://solderless.engineering) in Chrome or Edge |
| the SP-1 | **switched on normally** (hold ••), plugged in by USB, no buttons held | in **boot mode**: T1 + T4 held while plugging in |

- **T1 + T4 (boot mode) is only for flashing, in the browser.** It starts the SP-1's bootloader,
  not Sleepcousonique. In boot mode the editor cannot see the SP-1 at all, so **send to SP-1**
  says "No SP-1 is plugged in".
- **The editor never flashes.** Its **send to SP-1** does not install firmware and never needs
  boot mode: plug the SP-1 in normally, turn it on with **••**, press send.
- **Stuck in boot mode** (T1 lit, nothing else, the editor sees nothing)? Unplug, plug in
  again *without* holding any buttons, hold **••** about half a second to turn it on, and send.

## Flashing the firmware (once)

You need: the SP-1, a USB-C data cable, and **Chrome or Edge** (the updater uses WebUSB).

1. **Turn the SP-1 off** and unplug it.
2. **Boot mode:** hold **T1 + T4** (the first and last track buttons) and plug in USB.
   Let go when T1 lights.
3. Open **[solderless.engineering](https://solderless.engineering)** in Chrome or Edge.
4. Choose **custom firmware / .bin**, pick `sleepcousonique-sp1.bin`, connect to the SP-1
   when the browser asks, and flash.
5. When the updater says it's done, the SP-1 restarts into Sleepcousonique **switched off
   (standby)**: it was started by the USB cable, so it sits there charging, dark or showing
   the charge bar, and makes no sound. That is normal, not a failed flash. **Hold •• for
   about half a second** to turn it on: it plays the first night.
6. Open the editor: the scene bar says **SP-1 connected** when it hears it. Now **send to
   SP-1** works. (While the SP-1 is off the scene bar says *SP-1 is off (standby)*, and a send
   tells you to turn it on: the firmware only listens while it plays.)

After this you are done with boot mode. Every later **send to SP-1** is just: plug in normally
(no T1 + T4), turn on with **••**, send. The editor never flashes anything itself: flashing is
only ever done by the updater in the browser, with the SP-1 in boot mode. The editor's **send to
SP-1** sends a *dream* (your patch) to an SP-1 that already runs this firmware.

## If something goes wrong

The bootloader is never touched, so the SP-1 can always come back: unplug, hold **T1 + T4**,
plug in, and flash any `.bin` again: this one, Wakes, the tape looper or anything else.

- The sound breaks up and the editor's load meter says **too heavy**: the dream is more than
  the SP-1 can play, so it leaves out a block now and then to keep the sound running (since
  1.4 only as many as it must). Send a lighter one (lite places, `samplerate=half`).
- Reflashing may clear the five dream slots on the SP-1: keep your `.dream` files.
- **send to SP-1 says "switched off (standby)"**: hold **••** about half a second to turn it on
  and send again. Every time the SP-1 is plugged in while off (and right after flashing) it
  waits in standby.
- **send to SP-1 says "not as Sleepcousonique" / "Flash sleepcousonique-sp1.bin"**: the SP-1
  answers, but runs other firmware (Wakes, the tape looper, the stock one). Flash it as above.
- **send to SP-1 says "No SP-1 is plugged in"**: first, make sure the SP-1 is **not in boot
  mode**. If you held T1 + T4 while plugging in (T1 lit, nothing else), that is the flashing
  mode and the editor can't see it: unplug, plug in again without holding anything, hold **••**
  to turn it on. Otherwise use a USB-C *data* cable (some are charge only), plug straight into
  the computer rather than a hub, and turn the SP-1 on.
- **send to SP-1 says "in use by another program"**: only one program at a time can hold the
  SP-1's port. Quit the other one: a second Sleepcousonique, a serial monitor (Arduino, PuTTY,
  screen), `tools/sp1fleet.py`, or a browser tab using Web Serial.
- **send to SP-1 says the SP-1 "found N mistakes" and runs older firmware**: flash the
  `sleepcousonique-sp1.bin` that came with your editor; newer dreams use places and room the
  older firmware doesn't have.
- **Windows:** the SP-1 (running Sleepcousonique) shows up as a USB serial (COM) port with
  Windows' own driver (usbser, built into Windows 10 and 11): nothing to install, and **don't**
  install Zadig/WinUSB on its serial port (that removes the COM port; if it happened, Device
  Manager ▸ the SP-1 ▸ Uninstall device ▸ tick "delete the driver", then plug it in again).
  Boot mode (T1 + T4) is a different USB device that the browser reaches through WebUSB; Chrome
  and Edge need no driver for it either. **macOS:** the SP-1 appears as `/dev/cu.usbmodem…`.
- **Windows: Sleepcousonique.exe doesn't start, saying VCRUNTIME140.dll or MSVCP140.dll is
  missing**: that was 1.4.0 and earlier; 1.4.1 has the runtime built in. (Or install
  Microsoft's "Visual C++ Redistributable for Visual Studio 2015–2022", x64.)

This firmware is unofficial and not affiliated with or endorsed by Teenage Engineering.

## Going back to an older version

Every firmware in `firmware/dist/` is named after its version (`sleepcousonique-sp1-v1.1.1.bin`,
`sleepcousonique-sp1-v1.2.0-beta1.bin`, ...); the plain `sleepcousonique-sp1.bin` is always the
last released one. Flashing an older .bin works exactly like flashing a new one (boot mode: hold
T1 + T4 while plugging in). Your five dream slots live in their own part of the flash, away
from the firmware, so they should survive; your .dream files on the computer are the real copy,
so send them again if a slot comes back empty. The editor's status bar (or `SCQ HELLO` in a serial
terminal) shows which version the SP-1 is running.
