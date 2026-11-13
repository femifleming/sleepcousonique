# Sleepcousonique firmware for the SP-1

The Sleepcousonique engine (`../engine`) running on **Wakes**' SP-1 hardware layer: Worldwave's
power, standby, audio, controls, LED, battery and USB code, MIT licensed, copied here with
its notices (`LICENSE-wakes`, `NOTICE`, `LICENSES/`). The changes to Wakes are:

- `app/src/sp1_scq.c` / `.h` (new): the engine, its hands and lamp, the USB serial link, and five dream slots in flash.
- `app/src/main.c`, `app/src/sp1_audio.c`: `CONFIG_SP1_SCQ` hooks next to the existing tone build.
- `app/Kconfig`, `app/prj.conf`, `app/CMakeLists.txt`: the SCQ option. Plaits off; the engine is built at -O2.
- `boards/.../stem_player.dts`: the application partition ends at 0xE7000; `scq_partition` (96 KB) holds the dreams.

## Build

Zephyr **v4.3.1** and Zephyr SDK **0.17.4**, as Wakes pins them:

```
mkdir ~/sp1-ws && cd ~/sp1-ws
python3 -m venv .venv && . .venv/bin/activate && pip install west
git clone https://github.com/Worldwave/Wakes wakes-sp1      # only for its west.yml
west init -l wakes-sp1 && west update --narrow -o=--depth=1
pip install -r zephyr/scripts/requirements-base.txt
(cd zephyr && git apply ~/sleepcousonique/firmware/zephyr-patches/*.patch)
# Zephyr SDK 0.17.4 (arm-zephyr-eabi) from github.com/zephyrproject-rtos/sdk-ng/releases

west build -p always -b stem_player ~/sleepcousonique/firmware/app -- -DBOARD_ROOT=$HOME/sleepcousonique/firmware
# -> build/zephyr/sleepcousonique-sp1.bin
```

Flash with the Solderless updater (solderless.engineering).

**Not yet run on hardware.** Keep a known-good `.bin` (Wakes, tape-looper) nearby. T1 + T4
while plugging in USB enters the bootloader's recovery.
