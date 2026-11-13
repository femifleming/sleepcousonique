#!/bin/bash
# Measures every place on a connected SP-1 (engine load, %), one place at a time.
#   tools/bench_places.sh /dev/cu.usbmodemX > bench.txt
# Each test goes to slot 5; slot 1 is put back at the end. About 12 s per place.
cd "$(dirname "$0")/.."
PORT=${1:-$(ls /dev/cu.usbmodem* | head -1)}
T=$(mktemp -d)
printf '; nothing\n' > "$T/_nothing.dream"
printf 'the q is value 0\nq -> out\n' > "$T/_bottle.dream"
printf 'the q is value 0\nq -> out\nq ~> out\nq ~> out\nq ~> out\nq ~> out\n' > "$T/_fog4.dream"
for p in $(./tools/scqtool catalog | awk '$1=="P" && $3!="hands" && $3!="ears" && $2!="value" {print $2}'); do
  printf 'the x is %s\nx -> out\n' "$p" > "$T/$p.dream"
done
./tools/sp1bench "$PORT" "$T"/_*.dream $(ls "$T"/*.dream | grep -v '/_')
