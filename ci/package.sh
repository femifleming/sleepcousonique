#!/usr/bin/env bash
# Collects the editor, the SP-1 firmware, the manual and the flashing guide into
# dist/sleepcousonique-<os>/ and zips it.   usage: ci/package.sh <macOS|Windows> <build dir> [legacy build dir]
# With the Array (macOS) and the Legacy versions when they were built.
set -euo pipefail
OS="$1"; B="$2"; BL="${3:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="sleepcousonique-$OS"
OUT="$ROOT/dist/$NAME"
rm -rf "$OUT"; mkdir -p "$OUT"
A=""
for d in "$B/Sleepcousonique_artefacts/Release" "$B/Sleepcousonique_artefacts"; do
    if [ -e "$d/Sleepcousonique.app" ] || [ -e "$d/Sleepcousonique.exe" ]; then A="$d"; break; fi
done
[ -n "$A" ] || { echo "no Sleepcousonique build in $B" >&2; exit 1; }
case "$OS" in
    macOS)   cp -R "$A/Sleepcousonique.app" "$OUT/"
             codesign --force --deep -s - "$OUT/Sleepcousonique.app" ;;
    Windows) cp "$A/Sleepcousonique.exe" "$OUT/" ;;
esac
# the Array (macOS), and the legacy versions
art () { for d in "$1/$2_artefacts/Release" "$1/$2_artefacts"; do [ -e "$d/$3" ] && { echo "$d/$3"; return; }; done; }
if [ "$OS" = macOS ]; then
    a="$(art "$B" SleepcousoniqueArray "Sleepcousonique Array.app")"
    [ -n "$a" ] && cp -R "$a" "$OUT/" && codesign --force --deep -s - "$OUT/Sleepcousonique Array.app"
fi
if [ -n "$BL" ] && [ -d "$BL" ]; then
    mkdir -p "$OUT/Legacy"
    case "$OS" in
        macOS)   for n in "Sleepcousonique Legacy" "Sleepcousonique Array Legacy"; do
                     t="$( [ "$n" = "Sleepcousonique Legacy" ] && echo Sleepcousonique || echo SleepcousoniqueArray )"
                     a="$(art "$BL" "$t" "$n.app")"
                     [ -n "$a" ] && cp -R "$a" "$OUT/Legacy/" && codesign --force --deep -s - "$OUT/Legacy/$n.app"
                 done ;;
        Windows) a="$(art "$BL" Sleepcousonique "Sleepcousonique Legacy.exe")"; [ -n "$a" ] && cp "$a" "$OUT/Legacy/" ;;
    esac
fi
cp "$ROOT/firmware/dist/sleepcousonique-sp1.bin" "$OUT/"
cp "$ROOT/docs/manual/Sleepcousonique-Manual.pdf" "$OUT/"
cp "$ROOT/FLASHING.md" "$OUT/FLASHING.txt"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$OUT/"
mkdir -p "$OUT/examples" && cp "$ROOT"/examples/*.dream "$OUT/examples/"
cd "$ROOT/dist"
case "$OS" in
    macOS)   ditto -c -k --keepParent "$NAME" "$NAME.zip" ;;
    Windows) 7z a -tzip -r "$NAME.zip" "$NAME" > /dev/null ;;
esac
ls -la "$ROOT/dist"
