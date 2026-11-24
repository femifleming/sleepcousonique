#!/bin/sh
# Build the SP-1 firmware and keep a versioned copy: tools/build_sp1.sh
#   -> firmware/dist/sleepcousonique-sp1-v<SCQ_VERSION>.bin   (never overwrites an existing one)
# Needs the Zephyr workspace in ~/sp1-ws (west in its .venv, SDK 0.17.4).
set -e
cd "$(dirname "$0")/.."
REPO=$(pwd)
VER=$(sed -n 's/^#define SCQ_VERSION *"\(.*\)"/\1/p' firmware/app/src/sp1_scq.c)
OUT="firmware/dist/sleepcousonique-sp1-v$VER.bin"
[ -e "$OUT" ] && { echo "$OUT already exists: raise SCQ_VERSION first"; exit 1; }
. "$HOME/sp1-ws/.venv/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr ZEPHYR_SDK_INSTALL_DIR="$HOME/sp1-ws/zephyr-sdk-0.17.4"
cd "$HOME/sp1-ws"
rm -f build-scq/zephyr/sleepcousonique-sp1.bin
west build -b stem_player "$REPO/firmware/app" -d build-scq -- -DBOARD_ROOT="$REPO/firmware" > build-scq.log 2>&1 \
  || { grep -E "error" build-scq.log | head; echo "build failed (see ~/sp1-ws/build-scq.log)"; exit 1; }
grep -E "FLASH:|RAM:" build-scq.log
cp build-scq/zephyr/sleepcousonique-sp1.bin "$REPO/$OUT"
echo "-> $OUT"
