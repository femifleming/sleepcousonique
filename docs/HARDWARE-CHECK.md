# checks that need an SP-1

Everything the editor does over USB is tested without hardware (`ScqSerialTest`, `tools/test_send.sh`:
a pretend SP-1 that speaks the firmware's protocol and compiles dreams with the real engine). These
are the things only a real SP-1 can prove. Do them in order; each says what to look for. Write the
date and what you saw next to each when you've done it.

Before starting: quit every other program that might hold the SP-1's port (a second Sleepcousonique,
the Array, a serial monitor, `tools/sp1fleet.py`). Keep a known-good `.bin` (Wakes, the tape looper)
nearby, though nothing here writes the bootloader.

## 1. The .bin flashes and boots (macOS, and Windows if you have one)

1. SP-1 off, unplugged. Hold **T1 + T4**, plug in USB, let go when T1 lights (boot mode).
2. solderless.engineering in Chrome or Edge, custom firmware, `sleepcousonique-sp1.bin` from the
   1.4.1 zip (it is the 1.4.0 firmware, sha1 `1c08e500ca5aabacf89d4a4648b8990019fcc67c`). Flash.
3. **Look for:** the updater says done; the SP-1 restarts and sits **dark / charge bar, no sound**
   (standby). That is right.
4. Hold **••** about a second. **Look for:** it plays the first night.

## 2. The editor sees standby and on (the reported bug)

1. With the SP-1 plugged in and **off** (unplug, plug in again without touching ••), open the editor.
2. **Look for:** within ~10 s the scene bar says **SP-1 is off (standby): hold •• to turn it on**.
   Press **send to SP-1**: the window stays live, and the message says it's switched off, not
   "flash the firmware".
3. Hold **••** to turn it on. **Look for:** within ~10 s **SP-1 connected  cu.usbmodem…  slot 1
   scene 1**, the dot breathing green; moving a fader moves it in the hands panel.

Same from a terminal, without the window (macOS):

```
/Applications/Sleepcousonique.app/Contents/MacOS/Sleepcousonique --send examples/first-night.dream
```

prints `sent to /dev/cu.usbmodem…` (exit 0) when on, the standby message (exit 1) when off.

## 3. Sending

1. Open `examples/a-history.dream`, **send to SP-1**. **Look for:** status *sent to the SP-1 on …*
   within a few seconds; the SP-1 plays it; the window never freezes.
2. Send a long dream (near the 16 KB a slot keeps: ROOM shows *text*). **Look for:** sent, and after
   switching the SP-1 off and on (hold •• 1.5 s, then ••) it still plays it (it was kept).
3. While sending, keep moving a fader. **Look for:** no error, hands keep following after.
4. Pull the cable halfway through a send of a long dream. **Look for:** within ~20 s a message
   that the SP-1 went away or stopped taking it; plugging back in reconnects by itself.
5. Open a serial monitor on the SP-1's port (e.g. `screen /dev/cu.usbmodem101`), then send from the
   editor. **Look for:** on Windows "in use by another program"; on macOS it may still go (macOS
   lets two programs open a port) but the monitor eats replies: quit it.

## 4. Older firmware (only if you have an SP-1 on 1.2.0)

Send a 1.4 dream that uses more than 64 places (or a place added after 1.2). **Look for:** "found N
mistakes … running firmware 1.2.0 … Flash the sleepcousonique-sp1.bin that came with this editor".

## 5. Windows

1. Unzip `sleepcousonique-Windows.zip` on a PC **without** the Visual C++ Redistributable (or
   check Settings ▸ Apps for "Microsoft Visual C++ 2015-2022 Redistributable (x64)" and note it).
   **Look for:** `Sleepcousonique.exe` starts (1.4.0 needed VCRUNTIME140.dll).
2. Plug in the SP-1 (on). Device Manager ▸ Ports (COM & LPT): **USB Serial Device (COMn)**, driver
   usbser. **Look for:** the editor's scene bar says **SP-1 connected  COMn**.
3. Repeat checks 2 and 3 above on Windows. Also unplug and replug into a different USB port (Windows
   gives it a new COM number): **Look for:** it is found again, old COM numbers are not tried.
4. Flashing from Windows: boot mode, Chrome or Edge, solderless. **Look for:** the browser lists the
   SP-1 when it asks to connect, with no driver installed. If it doesn't, note what Device Manager
   shows for it (don't install Zadig/WinUSB on the COM port).

## 6. The Array (macOS, SP-1s in interface mode)

`/Applications/Sleepcousonique Array.app`: with one or more SP-1s on, it lists them (SCQ WHO), **show**
flashes the right one's LEDs, the mode button switches it to a sound card and back.

## Results

### 2026-11-19, one SP-1 on the Mac (volume off), editor 1.4.1 from this tree

SP-1: USB 2fe3:5c51 "Sleepcousonique", bcdDevice 0x0602, `/dev/cu.usbmodem1101`, firmware **1.4.0**,
serial F72160C3720AE25C, unit 2. Slots before: 1 = 5755 bytes (playing), 2 = 428, 3 empty, 4 empty,
5 = 375. Nothing was flashed; slots 1, 2 and 5 (your dreams) were never written.

| check | how | result |
|---|---|---|
| firmware version, identity | `sp1fleet.py who`, `ScqSerialTest --ask PORT "SCQ WHO" "SCQ ME"` (the Array's call) | PASS: 1.4.0, `SCQ ME F72160C3720AE25C 2 DREAM` |
| the editor's greeting | `ScqSerialTest --hello PORT` (new code, real timings) | PASS: sleepcousonique 1.4.0 in 491 ms |
| send to SP-1, small | `Sleepcousonique --send quiet.dream` (the window's Link), slot 3 playing | PASS: `sent to /dev/cu.usbmodem1101`, 1.4 s |
| send to SP-1, the most a slot keeps | the same, a 16,300-byte dream | PASS: sent in 1.5 s; the SP-1 reports `slot 3: 16300 bytes, 0 mistakes` (kept) |
| a dream the SP-1 can't read | `--send` a dream with an unknown place | PASS: "found 2 mistakes … kept playing the one it had"; slot 3 unchanged |
| a dream too long | `--send` 16,500 bytes | PASS: "no room for a dream this long"; slot 3 unchanged |
| the window's live link | `Sleepcousonique --shot … --wait 14000` | PASS: scene bar "SP-1 connected cu.usbmodem1101 slot 3 scene 1", green dot, faders followed, load meter shows the SP-1's |
| put back | sent `examples/first-night.dream` to slot 3, `SCQ SLOT 1` | done: playing slot 1 (5755 bytes, 0 mistakes) again. Slot 3 now holds the first night's text, which is exactly what an empty slot plays (byte-identical to the firmware's built-in default) |

Still to do with the SP-1 in hand (needs someone to press its buttons): check 2 (standby: plug in
while off, see the editor say so, hold •• and see it connect), check 3.4 (cable pulled mid-send),
check 5 (Windows) and check 1 (flashing: only if you want to reflash; 1.4.1 ships the same 1.4.0
firmware, so there is nothing new to flash).

## What I'd run for you

With the SP-1 plugged in and on, in a terminal (quit the editor first: it holds the port):

```
cd ~/sleepcousonique
/Applications/Sleepcousonique.app/Contents/MacOS/Sleepcousonique --send examples/first-night.dream
python3 tools/sp1fleet.py who           # each SP-1: port, firmware version, serial, unit, playing slot
python3 tools/sp1fleet.py slots         # which of its five slots hold a dream
```
