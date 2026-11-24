#!/usr/bin/env python3
"""Builds docs/wingie-manual/Sleepcousonique-Wingie-Manual.pdf from the chapters in this folder
(NN-*.md, in order). Modelled on the Cirrostratus manual's make_manual.py: markdown -> HTML ->
headless Chrome. Needs `markdown` (pip install markdown) and Google Chrome.

On the way it:
  - builds scqtool from the current engine (into a temp folder) and reads the place catalog;
  - expands every `<!-- place NAME -->` into that place's inlets, outlets, switches and its cost
    on the Wingie (measured, from engine/scq_costs_wingie.h, where there is a measurement);
  - checks every ```dream block with `scqtool check` (an `instrument wingie` line is added when a
    block has none): one mistake anywhere and the build stops. Under each block it prints what the
    check said: mistakes, memory of the Wingie's pool, and the load estimate.

  python3 docs/wingie-manual/make_manual.py [--html-only]
"""
import glob, html, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT_PDF = os.path.join(HERE, "Sleepcousonique-Wingie-Manual.pdf")
WINGIE_CYCLES = 5000.0     # cycles a sample has on the Wingie (240 MHz / 48 kHz)


def build_scqtool(tmp):
    exe = os.path.join(tmp, "scqtool")
    subprocess.run(["cc", "-std=c99", "-O2", "-DSCQ_HOST", "-I" + os.path.join(ROOT, "engine"),
                    os.path.join(ROOT, "tools", "scqtool.c"), os.path.join(ROOT, "engine", "scq_core.c"),
                    os.path.join(ROOT, "engine", "scq_places.c"), "-o", exe, "-lm"], check=True)
    return exe


def catalog(exe):
    """{name: dict(family, cost, nature, ins, outs, help, opts)} from `scqtool catalog`."""
    out = subprocess.run([exe, "catalog"], check=True, capture_output=True, text=True).stdout
    places = {}
    for ln in out.splitlines():
        if ln.startswith("P "):
            head, ins_outs = ln[2:].split(" ", 4)[:4], ln[2:].split(" ", 4)[4]
            name, fam, cost, nat = head
            ins, outs, hlp = (x.strip() for x in ins_outs.split("|", 2))
            places[name] = dict(family=fam, cost=float(cost), nature=nat,
                                ins=[i for i in ins.split(",") if i], outs=[o for o in outs.split(",") if o],
                                help=hlp, opts=[])
        elif ln.startswith("O "):
            parts = ln[2:].split()
            if parts and parts[0] in places:
                places[parts[0]]["opts"] = parts[1:]
    return places


def wingie_costs():
    text = open(os.path.join(ROOT, "engine", "scq_costs_wingie.h")).read()
    fixed = int(re.search(r"SCQ_WINGIE_FIXED (\d+)", text).group(1))
    scale = float(re.search(r"SCQ_WINGIE_SCALE_MEASURED ([\d.]+)", text).group(1))
    table = {m.group(1): int(m.group(2)) for m in re.finditer(r'\{ "(\w+)", (\d+) \}', text)}
    return fixed, scale, table


def place_block(name, places, costs):
    if name not in places:
        raise SystemExit("the manual names a place the engine doesn't have: " + name)
    p = places[name]
    fixed, scale, table = costs
    opts = [o for o in p["opts"] if not o.startswith("samplerate=") and not o.startswith("resolution=")]
    opts = ", ".join("<code>%s</code>" % html.escape(o.replace("|", " / ")) for o in opts) or "none"
    if name in table:
        cost = "%.0f %% of the Wingie (measured)" % (100 * table[name] / WINGIE_CYCLES)
    else:
        cost = "about %.0f %% of the Wingie (estimated)" % (100 * p["cost"] * scale / WINGIE_CYCLES)
    ins = ", ".join("<code>%s</code>" % i for i in p["ins"]) or "none"
    outs = ", ".join("<code>%s</code>" % o for o in p["outs"]) or "none"
    return ('<p class="facts"><b>nature</b> %s &nbsp;·&nbsp; <b>in</b> %s &nbsp;·&nbsp; <b>out</b> %s'
            ' &nbsp;·&nbsp; <b>switches</b> %s &nbsp;·&nbsp; <b>cost</b> %s</p>\n' % (p["nature"], ins, outs, opts, cost))


def cost_table(names, places, costs):
    """Measured Wingie costs beside the SP-1's, for the limits chapter."""
    fixed, scale, table = costs
    rows = ["| place | Wingie, cycles a sample | of the Wingie | SP-1, cycles a sample | of the SP-1 |",
            "|---|---|---|---|---|",
            "| an empty dream | %d | %.1f %% | | |" % (fixed, 100 * fixed / WINGIE_CYCLES)]
    for n in names:
        if n not in table or n not in places:
            raise SystemExit("no measured Wingie cost for " + n)
        sp = places[n]["cost"]
        rows.append("| `%s` | %d | %.0f %% | %.0f | %.0f %% |" % (n, table[n], 100 * table[n] / WINGIE_CYCLES, sp, 100 * sp / 1333.0))
    return "\n".join(rows) + "\n"


def check_dream(code, exe, tmp, n):
    text = code if re.search(r"^\s*instrument\s+wingie", code, re.M) else "instrument wingie\n" + code
    path = os.path.join(tmp, "ex%03d.dream" % n)
    open(path, "w").write(text)
    r = subprocess.run([exe, "check", path], capture_output=True, text=True)
    errs = [l for l in r.stdout.splitlines() if l.startswith("E ")]
    warns = [l for l in r.stdout.splitlines() if l.startswith("W ")]
    if errs or r.returncode:
        raise SystemExit("example %d has mistakes:\n%s\n%s" % (n, text, "\n".join(errs)))
    scenes = len(re.findall(r"^-{3,}\s*$", text, re.M))
    mems = []
    if scenes > 1:   # every scene: render a moment of each and read its pool
        st = subprocess.run([exe, "stats", path], capture_output=True, text=True).stdout
        for ln in st.splitlines():
            mm = re.search(r"M (\d+) (\d+) ([\d.]+)", ln)
            if mm:
                mems.append((int(mm.group(1)), int(mm.group(2)), float(mm.group(3))))
    else:
        mm = re.search(r"^M (\d+) (\d+) ([\d.]+)", r.stdout, re.M)
        mems.append((int(mm.group(1)), int(mm.group(2)), float(mm.group(3))))
    for used, total, load in mems:
        if used > total:
            raise SystemExit("example %d doesn't fit the Wingie's pool" % n)
    if len(mems) == 1:
        used, total, load = mems[0]
        meta = "checked: 0 mistakes · memory %d of %d KB · load %.0f %%" % (round(used / 1024), round(total / 1024), 100 * load)
    else:
        meta = "checked: 0 mistakes · %d scenes · memory %s KB of %d · load %s %%" % (
            len(mems), " / ".join(str(round(u / 1024)) for u, _, _ in mems), round(mems[0][1] / 1024),
            " / ".join("%.0f" % (100 * l) for _, _, l in mems))
    heavy = max(l for _, _, l in mems)
    if heavy > 1.0:
        print("warning: example %d is too heavy for the Wingie (load %.0f %%)" % (n, 100 * heavy))
        meta += " · TOO HEAVY for the Wingie: try samplerate=half"
    if warns:
        meta += " · " + "; ".join(w.split(" ", 2)[2] for w in warns)
    return meta


CSS = """
 @page { size: A4; margin: 16mm 15mm; }
 body { font: 10pt/1.45 -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif; color: #1c1c1c; }
 h1 { font-size: 26pt; margin: 0 0 4pt; letter-spacing: -0.5pt; }
 h2 { font-size: 15pt; margin: 22pt 0 6pt; border-bottom: 1.5pt solid #1c1c1c; padding-bottom: 2pt; page-break-after: avoid; }
 h2.chapter { page-break-before: always; }
 h3 { font-size: 12pt; margin: 16pt 0 4pt; page-break-after: avoid; }
 h4 { font-size: 11pt; margin: 14pt 0 3pt; page-break-after: avoid; }
 table { border-collapse: collapse; width: 100%; margin: 4pt 0 8pt; page-break-inside: avoid; }
 th, td { border-bottom: 0.5pt solid #bbb; padding: 2.5pt 5pt; text-align: left; vertical-align: top; }
 th { background: #eee; }
 th:empty { display: none; }
 td:first-child { white-space: nowrap; font-weight: 600; }
 code, pre { font: 8.5pt "SF Mono", Menlo, monospace; }
 pre { background: #f3f3f3; padding: 6pt 8pt; page-break-inside: avoid; margin-bottom: 1pt; }
 p.meta { font: 7.5pt "SF Mono", Menlo, monospace; color: #6b6b6b; margin: 0 0 8pt; }
 hr { border: 0; border-top: 0.5pt solid #ccc; margin: 14pt 0; }
 figure { margin: 8pt 0 10pt; page-break-inside: avoid; }
 figure img { max-width: 100%; max-height: 105mm; border: 0.5pt solid #bbb; display: block; margin: 0 auto; }
 figure.crop img { max-height: 48mm; }
 p.facts { font-size: 8.5pt; color: #333; margin: 3pt 0 4pt; padding: 3pt 6pt; border-left: 2pt solid #1c1c1c; background: #fafafa; }
 figcaption { font-size: 8.5pt; color: #555; margin-top: 3pt; font-style: italic; }
 .place { page-break-inside: avoid; }
 blockquote { margin: 6pt 0 8pt 0; padding: 0 0 0 10pt; border-left: 2pt solid #1c1c1c; }
"""


def build(html_only=False):
    import markdown
    tmp = tempfile.mkdtemp(prefix="scq_wingie_manual_")
    exe = build_scqtool(tmp)
    places = catalog(exe)
    costs = wingie_costs()
    chapters = sorted(glob.glob(os.path.join(HERE, "[0-9][0-9]-*.md")))
    text = "\n\n".join(open(f, encoding="utf-8").read() for f in chapters)

    text = re.sub(r"<!-- place (\w+) -->", lambda m: place_block(m.group(1), places, costs), text)
    text = text.replace("<!-- scale -->", "%.2f" % costs[1])
    text = re.sub(r"<!-- costs ([\w ]+) -->", lambda m: cost_table(m.group(1).split(), places, costs), text)
    n = [0]
    metas = []

    def dream(m):
        n[0] += 1
        metas.append(check_dream(m.group(1), exe, tmp, n[0]))
        return "```\n%s```\n\n@@META%d@@\n" % (m.group(1), n[0] - 1)
    text = re.sub(r"```dream\n(.*?)```", dream, text, flags=re.S)
    body = markdown.markdown(text, extensions=["tables", "fenced_code"])
    body = re.sub(r"<p>@@META(\d+)@@</p>", lambda m: '<p class="meta">%s</p>' % html.escape(metas[int(m.group(1))]), body)
    body = re.sub(r'<p><img alt="([^"]*)" src="([^"]*)" /></p>',
                  lambda m: '<figure%s><img src="%s"><figcaption>%s</figcaption></figure>'
                  % (' class="crop"' if "crop-" in m.group(2) else "", m.group(2), m.group(1)), body)
    first = [True]

    def h2(m):
        if first[0]:
            first[0] = False
            return m.group(0)
        return '<h2 class="chapter">'
    body = re.sub(r"<h2>(?=\d+\. )", h2, body)
    page = ('<!doctype html><html><head><meta charset="utf-8"><title>Sleepcousonique on the Wingie</title>'
            "<style>" + CSS + "</style></head><body>" + body + "</body></html>")
    html_path = os.path.join(HERE, "manual.html")
    open(html_path, "w", encoding="utf-8").write(page)
    print("%d examples checked, 0 mistakes" % n[0])
    if html_only:
        print(html_path)
        return
    if os.path.exists(OUT_PDF):
        os.remove(OUT_PDF)
    chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    subprocess.run([chrome, "--headless", "--disable-gpu", "--no-pdf-header-footer",
                    "--user-data-dir=" + tempfile.mkdtemp(prefix="scq_chrome_"),
                    "--print-to-pdf=" + OUT_PDF, "file://" + html_path],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # headless Chrome sometimes crashes on shutdown after writing: judge by the PDF
    if not os.path.exists(OUT_PDF) or os.path.getsize(OUT_PDF) < 10000:
        raise SystemExit("PDF was not written")
    os.remove(html_path)
    print(OUT_PDF)


if __name__ == "__main__":
    build("--html-only" in sys.argv)
