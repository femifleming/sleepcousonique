## 1. Flashing (and going back to stock)

### What you need

| | |
|---|---|
| the image | `firmware-wingie/dist/sleepcousonique-wingie.bin`: bootloader, partition table and the program in one file, written at address 0 |
| a cable | USB-C with data in it (a charge-only cable shows no port) |
| esptool | comes with the Arduino ESP32 core; `pip install esptool` works too |
| nothing else on the port | quit the editor (it keeps the Wingie's port open), Meng Qi's configuration page, serial monitors and MIDI tools |

### Building it yourself (optional)

`tools/build_wingie.sh` builds the image from the engine in this repository. It needs
`arduino-cli`, the ESP32 core **2.0.4** (`esp32:esp32@2.0.4`) and the *Adafruit AW9523*
library. It copies the engine into the sketch, turns `examples/wingie-first-light.dream`
into the dream a new Wingie starts with, compiles, and merges everything into
`firmware-wingie/dist/sleepcousonique-wingie.bin`.

### Flashing

1. Plug the Wingie into the computer.
2. In the repository: `tools/flash_wingie.sh` finds the first `/dev/cu.usbserial*` (or
   `wchusbserial*`) port by itself; give it the port if you have several:
   `tools/flash_wingie.sh /dev/cu.usbserial-0001`.
3. It writes the whole image at `0x0` at 460800 baud with esptool. It takes under a minute.
   Don't unplug while it writes.
4. The Wingie restarts and fades in (the volume climbs for about two seconds) playing
   **first light**. It doesn't need a button press to enter the bootloader: esptool resets it.

Without the repository, the same thing by hand:

    esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 \
      write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 sleepcousonique-wingie.bin

If esptool can't connect, hold **BOOT**, tap **RESET/EN**, let go of BOOT when it starts
connecting (the same advice as Meng Qi's flasher).

> **Your stock settings.** The image is written whole from address 0, so it also covers the
> place where the stock firmware keeps its settings (custom caves, alternate tunings, ratios,
> MIDI channels). If you've changed those, note them down on Meng Qi's configuration page
> before you flash: expect factory settings when you go back.

### First light

A new Wingie (or an empty slot) plays **first light**, two scenes:

| scene | what it does |
|---|---|
| first light | the left microphone rings sympathetic strings tuned by the upper keyboard, the right microphone strings an octave up tuned by the lower one; **Mix** is the small room's size, **Decay** how long the strings ring, **Volume** how much goes into the room. The left LED follows the microphones, the right one the room |
| the dark | the microphones ring a comb tuned by the upper keyboard, through a filter bank whose second and fifth bands are **Mix** and **Volume**; **Decay** is the comb's ring |

### Twelve slots

Dreams live on the Wingie in **12 slots** (in its flash: they stay through power-off).
The editor sends to a slot; the panel picks one (chapter 2). Each slot keeps up to 16 KB of
text, far more than most dreams. The Wingie remembers which slot was playing and starts there.

To clear slot 1 back to first light, hold **both Mode buttons while you power up**.

### Going back to stock

Meng Qi's **web flasher** puts the stock Wingie firmware back:

1. Open it in desktop **Chrome** or **Edge**: `Tools/wingie_flasher.html` in
   [github.com/mengqimusic/Wingie2](https://github.com/mengqimusic/Wingie2) (also served as the
   *Firmware Installer* at wingie.wingpinger.com).
2. Quit the Sleepcousonique editor and anything else that might hold the port.
3. **Connect Wingie2**, pick its serial port, then **Install firmware**. Don't unplug until it
   says *Installation complete*.

The stock firmware doesn't know about dreams: keep your `.dream` files on the computer, and
send them again when you come back. Sleepcousonique goes back on the same way it went on the
first time. Nothing can be broken by going back and forth: the ESP32's ROM bootloader is
never written.
