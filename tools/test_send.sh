#!/bin/bash
# The editor's own "send to SP-1" (Sleepcousonique --send, the same Link the window uses), run
# headless against a pretend SP-1 behind a pseudo-terminal (build/fakesp1: the firmware's
# protocol, the real engine). Nothing real is opened: SCQ_PORTS points the editor at the pty.
# macOS / Linux.   usage: tools/test_send.sh [build dir]
cd "$(dirname "$0")/.."
B="${1:-build}"
APP="$(find "$B" -type f \( -path '*Sleepcousonique.app/Contents/MacOS/Sleepcousonique' -o -path '*Sleepcousonique_artefacts*/Sleepcousonique' \) | head -n 1)"
FAKE="$B/fakesp1"
[ -x "$APP" ] && [ -x "$FAKE" ] || { echo "build Sleepcousonique and fakesp1 first ($APP / $FAKE)"; exit 1; }
T=$(mktemp -d)
trap 'kill $FP 2>/dev/null; rm -rf "$T"' EXIT
fails=0
pass() { echo "ok   $1"; }
fail() { echo "FAIL $1"; fails=$((fails+1)); }

start() {   # start <mode> [args]: a pretend SP-1; its port in $PORT
    kill $FP 2>/dev/null; wait $FP 2>/dev/null
    "$FAKE" "$@" --save "$T/got.dream" > "$T/port" 2> "$T/fake.log" &
    FP=$!
    for i in $(seq 50); do [ -s "$T/port" ] && break; sleep 0.1; done
    PORT=$(head -1 "$T/port")
    rm -f "$T/got.dream"
}
send() { SCQ_PORTS="$PORT" perl -e 'alarm 60; exec @ARGV' "$APP" --send "$1" > "$T/out" 2>&1; echo $? > "$T/rc"; }

for f in examples/*.dream; do
    if grep -q "^instrument wingie" "$f"; then start on --wingie; else start on; fi   # each dream to its own instrument
    send "$f"
    if [ "$(cat "$T/rc")" = 0 ] && grep -q "^sent to $PORT" "$T/out" && cmp -s "$f" "$T/got.dream"; then
        pass "$(basename "$f"): sent, and the instrument got it byte for byte ($(wc -c < "$f" | tr -d ' ') bytes)"
    else
        fail "$(basename "$f"): rc $(cat "$T/rc"): $(cat "$T/out")"
    fi
done

start standby
send examples/first-night.dream
if [ "$(cat "$T/rc")" = 1 ] && grep -q "switched off" "$T/out" && ! grep -q "Flash" "$T/out"; then
    pass "an SP-1 in standby (as after flashing): told to switch it on, not to flash it"
else fail "standby: $(cat "$T/out")"; fi

start other
send examples/first-night.dream
if [ "$(cat "$T/rc")" = 1 ] && grep -q "Flash" "$T/out"; then pass "an SP-1 with other firmware: told to flash it"
else fail "other firmware: $(cat "$T/out")"; fi

start stall
python3 -c "import sys; sys.stdout.write(open('examples/a-history.dream').read() + ('; pad ' + 'x'*70 + '\n')*60)" > "$T/big.dream"
t0=$(date +%s)
send "$T/big.dream"
t1=$(date +%s)
if [ "$(cat "$T/rc")" = 1 ] && grep -q "stopped taking" "$T/out" && [ $((t1-t0)) -lt 40 ]; then
    pass "an SP-1 that stops reading: reported after $((t1-t0)) s, no hang"
else fail "stall (rc $(cat "$T/rc"), $((t1-t0)) s): $(cat "$T/out")"; fi

start on 1.2.0 --old 2
send examples/first-night.dream
if [ "$(cat "$T/rc")" = 1 ] && grep -q "1.2.0" "$T/out" && grep -q "Flash" "$T/out"; then
    pass "an SP-1 on older firmware that can't read the dream: told to update its firmware"
else fail "old firmware: $(cat "$T/out")"; fi

kill $FP 2>/dev/null; wait $FP 2>/dev/null
PORT="/dev/nonexistent-sp1"
send examples/first-night.dream
if [ "$(cat "$T/rc")" = 1 ] && grep -qi "has gone" "$T/out"; then pass "no SP-1 there: says so ($(head -c 60 "$T/out")...)"
else fail "nothing there: $(cat "$T/out")"; fi

echo; [ $fails = 0 ] && echo "all good" || echo "$fails FAILED"
exit $fails
