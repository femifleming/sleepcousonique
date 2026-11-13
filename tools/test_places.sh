#!/bin/bash
# Every place alone, fed a clock and a sound, every outlet listened to.
# Fails on NaN, silence where there should be sound, or runaway values.
cd "$(dirname "$0")/.."
T=$(mktemp -d)
fails=0
for p in $(./tools/scqtool catalog | awk '$3!="hands" && $3!="ears" && $2!="bottle" {print $2}'); do
  line=$(./tools/scqtool catalog | awk -v n="$p" '$2==n')
  ins=$(echo "$line" | awk '{print $6}' | tr ',' ' ')
  [ "$ins" = "|" ] && ins=""
  outs=$(echo "$line" | awk -F' [|] ' '{print $2}' | tr ',' ' ')
  {
    echo "the x is $p"; echo "the clk is tram"; echo "the src is turntable 3"
    for i in $ins; do case "$p.$i" in
      waitingroom.in|crossroads.in) echo "clk.eighth -> x.$i";;
      *.in|*.a|*.carrier|*.voice|*.in1|*.in2) echo "src -> x.$i";;
      *.strike|*.pluck|*.trig|*.clock|*.gate|*.next|*.start|*.learn|*.flick|*.sync|*.hold|*.wake) echo "clk.eighth -> x.$i";;
    esac; done
    for i in ; do case $i in
      in|a|carrier|voice|in1|in2) echo "src -> x.$i";;
      strike|pluck|trig|clock|gate|next|start|learn|flick|sync|hold|wake) echo "clk.eighth -> x.$i";;
    esac; done
    first=1; for o in $outs; do
      if [ $first = 1 ]; then echo "x -> ears"; first=0; else echo "x.$o -60> ears"; fi
    done
  } > "$T/$p.dream"
  stats=$(./tools/scqtool stats "$T/$p.dream" 2>&1)
  ./tools/scqtool probe "$T/$p.dream" > "$T/$p.probe" 2>&1; probe=$?
  errs=$(grep -c '^E ' <<<"$stats")
  sline=$(grep '^scene' <<<"$stats" | head -1)
  peak=$(sed -n 's/.*peak \([0-9.]*\).*/\1/p' <<<"$sline")
  nan=$(sed -n 's/.*nan \([0-9]*\).*/\1/p' <<<"$sline")
  mem=$(sed -n 's/.*M \([0-9]*\) .*/\1/p' <<<"$sline")
  load=$(sed -n 's/.*M [0-9]* [0-9]* \([0-9.]*\).*/\1/p' <<<"$sline")
  ok=ok
  if [ "$errs" != 0 ] || [ "$nan" != 0 ] || [ $probe != 0 ] || awk "BEGIN{exit !($peak < 0.0005)}"; then ok=FAIL; fails=$((fails+1)); fi
  printf "%-12s peak %-6s mem %-6s load %-6s %s\n" "$p" "$peak" "$mem" "$load" "$ok"
  [ $ok = FAIL ] && { grep '^E ' <<<"$stats"; cat "$T/$p.probe"; }
done
echo "$fails failed"
exit $fails
