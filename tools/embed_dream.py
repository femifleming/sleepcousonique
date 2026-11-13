#!/usr/bin/env python3
"""embed_dream.py in.dream out.h — the dream as a C string (SCQ_DEFAULT_DREAM)."""
import sys, os
text = open(sys.argv[1], encoding="utf-8").read()
# keep it small on the device: drop the tapestry positions
text = "\n".join(l for l in text.splitlines() if not l.startswith(";@")) + "\n"
os.makedirs(os.path.dirname(sys.argv[2]), exist_ok=True)
with open(sys.argv[2], "w") as f:
    f.write("/* generated from %s by tools/embed_dream.py */\n" % os.path.basename(sys.argv[1]))
    f.write("#define SCQ_DEFAULT_DREAM \\\n")
    for line in text.splitlines(True):
        esc = line.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
        f.write('\t"%s" \\\n' % esc)
    f.write('\t""\n')
