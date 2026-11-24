#!/bin/sh
# Flash Sleepcousonique onto a Wingie2 over USB (any port: tools/flash_wingie.sh /dev/cu.usbserial-X).
# The Wingie's own firmware goes back with Meng Qi's web flasher (github.com/mengqimusic/Wingie2).
set -e
cd "$(dirname "$0")/.."
img=firmware-wingie/dist/sleepcousonique-wingie.bin
port=${1:-$(ls /dev/cu.usbserial* /dev/cu.wchusbserial* 2>/dev/null | head -1)}
[ -n "$port" ] || { echo "no Wingie on USB"; exit 1; }
ESPTOOL=$(ls ~/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | tail -1)
"$ESPTOOL" --chip esp32 --port "$port" --baud 460800 write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 "$img"
