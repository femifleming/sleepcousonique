#!/usr/bin/env python3
"""Writes docs/places.md from the engine's own place table (scqtool catalog)."""
import subprocess, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cat = subprocess.run([os.path.join(root, "tools", "scqtool"), "catalog"], capture_output=True, text=True).stdout
fam_title = {"source": "Sources", "shape": "Shapes", "space": "Spaces", "current": "Currents",
             "hands": "Hands (the SP-1's controls)", "ears": "Ears (where things leave)"}
rows = {}
for line in cat.splitlines():
    if not line.startswith("P "): continue
    head, outs, help_ = line.split(" | ", 2)
    t = head.split(" ")
    name, fam, cost, nature = t[1], t[2], t[3], t[4]
    ins = t[5] if len(t) > 5 else ""
    rows.setdefault(fam, []).append((name, nature, ins.replace(",", ", "), outs.replace(",", ", "), help_, cost))

guide = open(os.path.join(root, "docs", "language.md")).read()
out = [guide, "\n# The places\n",
       "Every place has a fixed *nature* (the number after its name). Nothing has knobs: "
       "you change a place only by running something into its inlets. The first inlet and "
       "the first outlet need no name: `voice -> window` means `voice.out -> window.in`.\n"]
for fam in ["source", "shape", "space", "current", "hands", "ears"]:
    out.append(f"\n## {fam_title[fam]}\n")
    out.append("| place | nature | in | out | what it is |\n|---|---|---|---|---|")
    for name, nature, ins, outs, help_, cost in rows.get(fam, []):
        out.append(f"| **{name}** | {nature} | {ins or '—'} | {outs or '—'} | {help_} |")
open(os.path.join(root, "docs", "places.md"), "w").write("\n".join(out) + "\n")
print("wrote docs/places.md")
