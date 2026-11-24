#!/bin/bash
# Every place, at several natures, under clang's alignment checker. The SP-1's Cortex-M4 faults on a
# misaligned multi-word or float load (shimmer did, 2026-11-24: a hard reset on the SP-1, while a Mac
# and the Wingie played on). Must print "0 misaligned".
cd "$(dirname "$0")/.."
T=$(mktemp -d)
clang -std=c99 -O1 -g -fsanitize=alignment -DSCQ_HOST -Iengine tools/scqtool.c engine/scq_core.c engine/scq_places.c -o "$T/scqtool" -lm || exit 1
bad=0
for p in $("$T/scqtool" catalog | awk '$1=="P" && $3!="hands" && $3!="ears" {print $2}'); do
  for nat in 0.5 1 1.5 2 3 5 9; do
    printf 'the clk is clock\nthe src is vco 3\nthe x is %s %s\nsrc -> x\nclk.eighth -> x\nx -> out\n' "$p" "$nat" > "$T/n.dream"
    if "$T/scqtool" stats "$T/n.dream" 2>&1 | grep -q "runtime error"; then echo "misaligned: $p $nat"; bad=$((bad+1)); fi
  done
done
clang -std=c99 -O1 -g -fsanitize=alignment -DSCQ_HOST -Iengine tests/sweep.c engine/scq_core.c engine/scq_places.c -o "$T/sweep" -lm && \
  n=$("$T/sweep" 2>&1 | grep -c "runtime error") && bad=$((bad+n))
rm -rf "$T"
echo "$bad misaligned"
[ "$bad" = 0 ]
