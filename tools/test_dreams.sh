#!/bin/bash
# Every example dream (SP-1 and Wingie), every scene, played through tests/dreamstorm.c: line in
# that sounds like a gig, the hands moved about at random (deterministically), then checked for
# numbers that aren't, loops that run away, places that don't come back, and dreams that change
# when loaded again and again. One line per scene, a summary, non-zero exit on a real failure.
#
#   tools/test_dreams.sh                    all of examples/, 30 s a scene
#   tools/test_dreams.sh -s 12              shorter
#   tools/test_dreams.sh -r 10              ten storms a scene, each different (a deeper look)
#   tools/test_dreams.sh examples/x.dream   just these
cd "$(dirname "$0")/.."
SECONDS_ARG=()
FILES=()
while [ $# -gt 0 ]; do
  case "$1" in
    -s|-n|-r) SECONDS_ARG+=("$1" "$2"); shift 2;;
    -v) SECONDS_ARG+=("$1"); shift;;
    *) FILES+=("$1"); shift;;
  esac
done
[ ${#FILES[@]} -eq 0 ] && FILES=(examples/*.dream)

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cc -std=c99 -O2 -Wall -DSCQ_HOST -Iengine tests/dreamstorm.c engine/scq_core.c engine/scq_places.c -o "$T/dreamstorm" -lm || exit 2

# one process per dream (the engine is one global instrument), as many at once as there are cores
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
i=${#FILES[@]}
for ((k = 0; k < i; k++)); do
  echo "$k ${FILES[$k]}"
done | xargs -P "$JOBS" -n 2 sh -c '"$0" '"${SECONDS_ARG[*]}"' "$2" > "'"$T"'/$1.out" 2>&1; echo $? > "'"$T"'/$1.rc"' "$T/dreamstorm"

fails=0; scenes=0; sfail=0; swarn=0
for ((k = 0; k < i; k++)); do
  grep -v '^dreamstorm:' "$T/$k.out"
  rc=$(cat "$T/$k.rc" 2>/dev/null || echo 1)
  [ "$rc" != 0 ] && fails=$((fails + 1))
  s=$(sed -n 's/^dreamstorm: [0-9]* dreams, \([0-9]*\) scenes: \([0-9]*\) scenes failed, \([0-9]*\) warned.*/\1 \2 \3/p' "$T/$k.out")
  set -- $s
  scenes=$((scenes + ${1:-0})); sfail=$((sfail + ${2:-0})); swarn=$((swarn + ${3:-0}))
done
echo "test_dreams: $i dreams, $scenes scenes: $sfail scenes failed, $swarn warned; $fails dreams failed"
[ $fails -eq 0 ]
