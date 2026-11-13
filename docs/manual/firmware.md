# 3. The SP-1

## Patch here, perform there

This is the Shnth idea. You build a patch in the editor and decide what every fader and
button does by patching it. You send it over USB once, then unplug: the SP-1 runs the patch
on its own, from battery, through its speaker or headphones. Nothing on the device needs the
computer after that. It turns on into the dream it was playing last.

## What runs on it

The Sleepcousonique firmware keeps the hardware layer of **Wakes** (Worldwave's SP-1
firmware, MIT licensed). That layer covers the power-on hold, standby and charging, the
watchdog, the codec start-up order, the fader and button ladders, the LEDs and the USB
device. In place of Wakes' Plaits and Marbles voice it runs the Sleepcousonique engine,
the same C code the editor runs.

## Installing

1. Take `firmware/dist/sleepcousonique-sp1.bin` (124 KB), or build it yourself (see *Building*, below).
2. Open the **Solderless** updater (solderless.engineering) in Chrome or Edge and connect the
   SP-1.
3. Follow its steps to flash the `.bin`. If the SP-1 ever needs rescuing, hold **T1 + T4**
   while plugging in USB: that is the bootloader's recovery entry, and nothing here
   touches the bootloader.

> **Not yet run on a real SP-1.** The firmware builds (124 KB of flash, 182 KB of the 256 KB
> RAM), and the engine is the one tested in the editor, but nobody has flashed this image
> onto a device yet. Treat the first flash as a
> test. Keep the Wakes or tape-looper `.bin` nearby so you can go back.

## Playing it

| SP-1 | Sleepcousonique |
|---|---|
| hold **••** 1.5 s | turn on (as Wakes) |
| hold **••** 3 s, touching nothing else | turn off |
| faders F1–F4 | `lean1`–`lean4` |
| track buttons T1–T4 | `door1`–`door4` |
| FFWD / RWD (the rocker) | `lift` +1 / −1, `lift.up` / `lift.down` pulses |
| PLAY | `wake` |
| **••** | `lucid` (while held) |
| VOL + / − | volume |
| **••** + VOL + / − | next / previous dream slot |
| track row LEDs (above T1–T4) | `lamp` 1–4 |
| play row LEDs | `lamp` 5–8 |

Some things to know:

- The SP-1 reads **one track button at a time**. Its button ladder can't report two held
  together, so don't build a dream that needs chords on the doors.
- Holding a track button sags the rail the faders share, so the faders hold still while a
  button is down. Wakes corrects for this. You'll notice it only as a fader that waits.
- Holding **••** alone for 3 seconds turns the SP-1 off. Touching any other control while
  holding it keeps it on, so `lucid` gestures with a fader or button are safe.
- Tapping **••** shows the battery on the play row for a moment.

## Dream slots

The SP-1 keeps **five** dreams in its own flash memory, up to 16 KB of text each. **••** +
VOL steps through them. An empty slot plays *first night*. Sending from the editor fills the
slot that is playing. The SP-1 also counts the nights it has been turned on, and `drift` uses
that count, so a dream really does wander from night to night.

Saving a dream pauses the processor for a fraction of a second, so expect a short gap in the
sound when one arrives. Flashing new firmware may wipe the slots, so keep your `.dream`
files.

## Building

You need Zephyr 4.3.1 and the Zephyr SDK 0.17.4, the versions Wakes is pinned to.

```
cd ~/sp1-ws                       # a west workspace with Zephyr v4.3.1 (see firmware/README.md)
west build -p always -b stem_player ~/sleepcousonique/firmware/app \
     -- -DBOARD_ROOT=$HOME/sleepcousonique/firmware
# result: build/zephyr/sleepcousonique-sp1.bin
```

Apply the two patches in `firmware/zephyr-patches/` to the Zephyr tree first, as Wakes
requires (`git apply` inside `zephyr/`).

## The USB link

The SP-1 shows up as a USB serial port named *Sleepcousonique*. The editor speaks this
protocol to it, which you can also type in a serial terminal:

```
SCQ HELLO                    -> SLEEPCOUSONIQUE 0.1.0
SCQ LOAD <bytes> <slot>      then exactly <bytes> of the .dream text
                             -> OK 0      or  ERR <why>
SCQ SLOT <1-5>               -> plays that slot
```

The same port also carries Wakes' diagnostic console (battery, audio load and so on).

---

# 4. Credits

- **Peter Blasser**: the Nabra (and *Stores at the Mall*), Shbobo and Fish, whose ideas run
  all through this.
- **Wakes** by Worldwave, MIT licensed: the SP-1 hardware layer this firmware is built on. Its
  licence and notices are in `firmware/LICENSE-wakes`, `firmware/NOTICE` and
  `firmware/LICENSES/`, which also credit the SP-1-dev documentation (Tim Knapen),
  sp1-tape-looper and feldd's sp1-midi, all MIT.
- **Teenage Engineering**: the SP-1.
- Don Buchla, Serge Tcherepnin and Clavia, for the instruments this borrows its thinking
  from.
