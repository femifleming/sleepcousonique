#!/usr/bin/env python3
"""Builds docs/manual/Sleepcousonique-Manual.pdf (and .html) from the manual's markdown,
the language guide and the generated place reference. Needs: pip install markdown; Chrome."""
import os, re, subprocess, sys, markdown
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))
subprocess.run([sys.executable, os.path.join(root, "tools", "make_places.py")], check=True)

def read(p): return open(os.path.join(root, p), encoding="utf-8").read()
lang = read("docs/places.md")
lang = lang.replace("# Sleepcousonique\n\n*A patch language for the SP-1, written like a dream journal.*",
                    "# 2. Writing dreams", 1)
lang = lang.replace("\n# The places\n", "\n## The places\n").replace("\n## Sources", "\n### Sources")
for h in ["Shapes", "Spaces", "Currents", "Hands (the SP-1's controls)", "Ears (where things leave)"]:
    lang = lang.replace("\n## " + h, "\n### " + h)
src = read("docs/manual/manual.md") + "\n\n" + lang + "\n\n---\n\n" + read("docs/manual/firmware.md")
body = markdown.markdown(src, extensions=["tables", "fenced_code"])
body = re.sub(r'<hr />\s*(<h1)', r'\1', body)   # a chapter starts a page anyway: a rule before it can land alone on a page of its own
body = re.sub(r'<p><img alt="([^"]*)" src="([^"]*)" /></p>',
              r'<figure><img src="\2"><figcaption>\1</figcaption></figure>', body)
css = """
@page { size: A4; margin: 18mm 17mm 20mm 17mm; }
body { font-family: Georgia, 'Times New Roman', serif; color: #1d1b19; background: #fff;
       font-size: 10.6pt; line-height: 1.5; margin: 0; padding: 0; }
h1 { font-family: 'Helvetica Neue', Helvetica, sans-serif; font-weight: 800; letter-spacing: .02em;
     font-size: 25pt; margin: 1.4em 0 .3em; color: #1d1b19; page-break-before: always; }
h1:first-of-type { page-break-before: avoid; font-size: 38pt; margin-top: .2em; }
h2 { font-family: 'Helvetica Neue', Helvetica, sans-serif; font-size: 14.5pt; color: #7a4a2a;
     border-bottom: 1px solid #d8cfb8; padding-bottom: 3px; margin-top: 1.6em; }
h3 { font-family: 'Helvetica Neue', Helvetica, sans-serif; font-size: 12pt; color: #1f3f8f; margin-top: 1.3em; }
code, pre { font-family: Menlo, monospace; font-size: 9pt; }
pre { background: #f2ede0; border-left: 3px solid #d2a531; padding: 10px 12px; white-space: pre-wrap; page-break-inside: avoid; }
p code, td code, li code { background: #f2ede0; padding: 0 3px; border-radius: 2px; }
table { border-collapse: collapse; width: 100%; margin: .8em 0; font-size: 9.4pt; page-break-inside: auto; }
th { text-align: left; background: #1d1b19; color: #fbf8f0; font-family: 'Helvetica Neue', sans-serif; font-weight: 600; }
th, td { padding: 4px 7px; border-bottom: 1px solid #e3dccb; vertical-align: top; }
tr { page-break-inside: avoid; }
figure { margin: 1em 0; page-break-inside: avoid; }
figure img { width: 100%; border: 1px solid #d8cfb8; }
figcaption { font-style: italic; font-size: 9pt; color: #7a6f5c; margin-top: 4px; }
blockquote { border-left: 4px solid #b0301f; margin: 1em 0; padding: 4px 14px; background: #f7ece4; }
hr { border: 0; border-top: 1px solid #d8cfb8; margin: 2em 0; }
h3 + p em { color: #7a4a2a; }
"""
html = f"<!doctype html><html><head><meta charset='utf-8'><title>Sleepcousonique — Manual</title><style>{css}</style></head><body>{body}</body></html>"
out_html = os.path.join(here, "Sleepcousonique-Manual.html")
open(out_html, "w", encoding="utf-8").write(html)
pdf = os.path.join(here, "Sleepcousonique-Manual.pdf")
chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
import tempfile
for attempt in range(4):      # headless Chrome gives up now and then on a busy machine: its own profile, and try again
    with tempfile.TemporaryDirectory() as profile:
        done = subprocess.run([chrome, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", f"--user-data-dir={profile}",
                               f"--print-to-pdf={pdf}", "file://" + out_html], capture_output=True)
    if done.returncode == 0: break
else:
    sys.exit("Chrome could not print the manual: " + done.stderr.decode(errors="replace")[-400:])
print("wrote", pdf)
