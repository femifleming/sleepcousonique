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

## Installing (flashing the .bin)

You flash the firmware once. After that the editor sends your dreams over USB; changing a
patch never needs a reflash.

You need the SP-1, a USB-C data cable and **Chrome or Edge** (the updater uses WebUSB).

1. Take `sleepcousonique-sp1.bin` from the release zip (or `firmware/dist/`, or build it,
   see *Building*).
2. **Turn the SP-1 off** and unplug it.
3. **Boot mode:** hold **T1 + T4** and plug in USB. Let go when T1 lights.
4. Open **solderless.engineering** in Chrome or Edge, choose the custom firmware (.bin),
   pick `sleepcousonique-sp1.bin`, connect to the SP-1 when the browser asks, and flash.
5. The SP-1 restarts into Sleepcousonique and plays the first night. In the editor the scene
   bar now says **SP-1 connected**, with its slot and scene.

On Windows the SP-1 appears as a USB serial (COM) port using Windows' own driver; on macOS as
`/dev/cu.usbmodem…`. Nothing to install on either.

This firmware is unofficial: not affiliated with or endorsed by Teenage Engineering.

## If something goes wrong

The bootloader is never touched, so the SP-1 can always be brought back:

1. Turn the SP-1 off and unplug it.
2. Hold **T1 + T4** and plug in USB. Let go when T1 lights: this is boot mode.
3. Flash any `.bin` with the Solderless updater: this one again, a newer one, Wakes,
   the tape looper or anything else.

| part of the SP-1's memory | who writes it |
|---|---|
| 0x00000–0x1FFFF, the bootloader (T1 + T4 lives here) | **never** this firmware |
| 0x20000–0xE6FFF, the firmware | only the bootloader, when you flash a `.bin` |
| 0xE7000–0xFEFFF, the five dream slots | Sleepcousonique, and nothing outside it |
| 0xFF000, the bootloader's settings page | **never** this firmware |

On battery, a crash or unexpected restart turns the SP-1 off instead of looping. Press
**••** to start it again. A dream that misbehaves doesn't need a reflash: send a different
one, or step to another slot with **••** + VOL. Reflashing the firmware may clear the slots,
so keep your `.dream` files on the computer.

## Power and battery

| to | do this |
|---|---|
| turn on | hold **••** for 1.5 s (the track row fills, then it starts) |
| turn off | hold **••** for 3 s, touching nothing else |
| check the battery | tap **••**: the play row shows the charge as a bar for 1.5 s |
| see it charging (off) | plug in USB: the play row shows the charge bar, breathing while it charges, solid when full |
| see it charging (on) | plugged in, the play row shows the same charge bar whenever your dream isn't lighting those four LEDs; tapping **••** shows it any time |

The SP-1 charges whenever it's plugged in, from a charger or a computer, on or off. (Wakes
stops charging from a computer while on, to spare battery-powered hosts. Sleepcousonique
keeps charging, because you're usually plugged in to send dreams.) On battery, the SP-1 only
starts when you hold **••**. A crash turns it off rather than looping.

## Bluetooth

Sleepcousonique doesn't send audio over Bluetooth yet. Sound comes out of the speaker and
the headphone jack. The SP-1's Bluetooth is a separate Infineon (Cypress) chip that needs its
own radio program, built with Infineon's SDK, which isn't public, and no released SP-1
firmware sends audio to Bluetooth headphones or speakers so far. Tape Looper's author says
an SP-1 Bluetooth SDK is in development. When it's out, Bluetooth output can be added.

Sleepcousonique never touches the Bluetooth chip. Whatever is on it now stays there, and
flashing back to another firmware brings its Bluetooth back exactly as it was.

## Playing it

| SP-1 | Sleepcousonique |
|---|---|
| hold **••** 1.5 s / 3 s | on / off (see *Power and battery*) |
| tap **••** | show the battery |
| faders F1–F4 | `fader1`–`fader4` |
| track buttons T1–T4 | `button1`–`button4` |
| FFWD / RWD (the rocker) | `rocker` +1 / −1, `rocker.up` / `rocker.down` pulses, yours to patch |
| **••** + FFWD / RWD | next / previous scene (the track row shows which, 1–4); the rocker doesn't reach `rocker` while •• is held |
| PLAY | `play` |
| **••** | `dots` (while held) |
| VOL + / − | volume |
| **••** + VOL + / − | next / previous dream slot |
| track row LEDs (above T1–T4) | `leds` 1–4 |
| play row LEDs | `leds` 5–8 |

Some things to know:

- The SP-1 reads **one track button at a time**. Its button ladder can't report two held
  together, so don't build a dream that needs chords on the doors.
- Holding a track button sags the rail the faders share, so the faders hold still while a
  button is down. Wakes corrects for this. You'll notice it only as a fader that waits.
- Holding **••** alone for 3 seconds turns the SP-1 off. Touching any other control while
  holding it keeps it on, so `dots` gestures with a fader or button are safe.
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
SCQ WATCH 1                  -> about 30 times a second, until WATCH 0 or the port closes:
                                SCQ HANDS f1 f2 f3 f4 buttons scene slot leds
```

The editor keeps this link open while the SP-1 is plugged in, so its hands panel follows the
SP-1 live, and **send to SP-1** goes over the same connection.

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
