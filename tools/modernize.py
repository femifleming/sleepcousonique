#!/usr/bin/env python3
"""modernize.py file.dream [...] — rewrite dreams written before the 2026-11-13 rename with
the new place, inlet and outlet names (in place). Notes after ';' are left alone, except
the SP-1's controls (lean1 -> fader1 and so on). Use --stdout to print instead."""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from renames import R

WAS = {}  # old name -> (new, old ins, old outs)
for line in open(os.path.join(HERE, "..", "engine", "scq_was.inc")):
    m = re.match(r'\{ "(\w+)", "(\w+)", "([^"]*)", "([^"]*)" \}', line)
    if m:
        new, old, oi, oo = m.groups()
        WAS[old] = (new, oi.split() or R[old][1].split(), oo.split() or R[old][2].split())
NEW = {o: (n, ins.split(), outs.split()) for o, (n, ins, outs, _) in R.items()}
NEWNAMES = {n for n, *_ in NEW.values()}
HANDS = {"lean": "fader", "door": "button", "lift": "rocker", "wake": "play"}
WORD = re.compile(r"[A-Za-z][A-Za-z0-9.']*")


def old_place(word):
    """old place name (and digits) a word names, or (None, '')"""
    if word in WAS:
        return word, ""
    m = re.match(r"([A-Za-z]+)(\d[\d.]*)$", word)
    if m and m.group(1) in WAS:
        return m.group(1), m.group(2)
    return None, ""


def map_port(old, port, role):
    _, oi, oo = WAS[old]
    _, ni, no = NEW[old]
    order = [(oi, ni), (oo, no)] if role == 1 else [(oo, no), (oi, ni)]
    for olds, news in order:
        if port in olds:
            return news[olds.index(port)]
    return port


def rewrite(text):
    out, names, prelude = [], {}, None
    lines = text.split("\n")
    for i, line in enumerate(lines):
        nxt = lines[i + 1] if i + 1 < len(lines) else ""
        if re.match(r"^-{3,}\s*$", nxt) and line.strip():          # a scene starts
            if prelude is None:
                prelude = dict(names)
            names = dict(prelude)
            out.append(line)
            continue
        if line.startswith(";@"):
            out.append(line)
            continue
        code, sep, note = line.partition(";")
        for o, n in HANDS.items():
            note = re.sub(r"\b%s(\d?)\b" % o, n + r"\1", note) if sep else note
        note = note.replace("lucid", "dots") if "the rocker" in note else note
        tokens = WORD.finditer(code)
        m = re.match(r"\s*(the|The)\s+(\w+)\s+(is|are)\s+([A-Za-z]+)", code)
        role = 0
        pieces, last = [], 0
        if m:
            place = m.group(4)
            if place in WAS:
                names[m.group(2)] = place
                code = code[:m.start(4)] + NEW[place][0] + code[m.end(4):]
            else:
                names.pop(m.group(2), None)
            out.append(code + sep + note)
            continue
        lucid = re.match(r"\s*lucid\s", code)
        for t in WORD.finditer(code):
            w = t.group(0)
            between = code[last:t.start()]
            if re.search(r"[-~=][\d.]*>|>\|", between) or (lucid and ":" in code[:t.start()]):
                role = 1
            pieces.append(between)
            last = t.end()
            if lucid and w == "lucid" and not code[:t.start()].strip():
                pieces.append(w); continue
            base, dot, port = w.partition(".")
            if w == "bottle":
                pieces.append("value"); continue
            if base in names:
                place, newbase = names[base], base
            else:
                place, digits = old_place(base)
                newbase = NEW[place][0] + digits if place else base
            if place and dot:
                port = map_port(place, port, role)
            pieces.append(newbase + (dot + port if dot else ""))
        pieces.append(code[last:])
        out.append("".join(pieces) + sep + note)
    return "\n".join(out)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--stdout"]
    for path in args:
        new = rewrite(open(path, encoding="utf-8").read())
        if "--stdout" in sys.argv:
            sys.stdout.write(new)
        else:
            open(path, "w", encoding="utf-8").write(new)
