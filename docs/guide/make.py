#!/usr/bin/env python3
"""Builds docs/guide/Building-Sleepcousonique.pdf from guide.md (needs: markdown; Chrome)."""
import os, re, subprocess, markdown
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, "guide.md"), encoding="utf-8").read()
body = markdown.markdown(src, extensions=["tables", "fenced_code", "md_in_html"])
css = """
@page { size: A4; margin: 18mm 17mm 20mm 17mm; }
body { font-family: Georgia, serif; color: #1d1b19; font-size: 10.6pt; line-height: 1.55; margin: 0; }
h1 { font-family: 'Helvetica Neue', Helvetica, sans-serif; font-weight: 800; font-size: 24pt; margin: 1.2em 0 .3em; page-break-before: always; }
h1:first-of-type { page-break-before: avoid; font-size: 36pt; margin-top: 0; }
h2 { font-family: 'Helvetica Neue', sans-serif; font-size: 14pt; color: #7a4a2a; border-bottom: 1px solid #d8cfb8; padding-bottom: 3px; margin-top: 1.5em; }
h3 { font-family: 'Helvetica Neue', sans-serif; font-size: 12.5pt; color: #1f3f8f; margin-top: .2em; }
code, pre { font-family: Menlo, monospace; font-size: 8.8pt; }
pre { background: #f2ede0; border-left: 3px solid #d2a531; padding: 10px 12px; white-space: pre-wrap; page-break-inside: avoid; }
p code, td code, li code { background: #f2ede0; padding: 0 3px; border-radius: 2px; }
table { border-collapse: collapse; width: 100%; margin: .8em 0; font-size: 9.4pt; }
th { text-align: left; background: #1d1b19; color: #fbf8f0; font-family: 'Helvetica Neue', sans-serif; font-weight: 600; }
th, td { padding: 4px 7px; border-bottom: 1px solid #e3dccb; vertical-align: top; }
tr { page-break-inside: avoid; }
.diagram { margin: 1em 0; page-break-inside: avoid; }
.diagram svg { width: 100%; height: auto; }
hr { border: 0; border-top: 1px solid #d8cfb8; margin: 1.6em 0; }
strong { color: #1d1b19; }
"""
html = f"<!doctype html><html><head><meta charset='utf-8'><title>Building Sleepcousonique</title><style>{css}</style></head><body>{body}</body></html>"
out = os.path.join(here, "Building-Sleepcousonique.html")
open(out, "w", encoding="utf-8").write(html)
pdf = os.path.join(here, "Building-Sleepcousonique.pdf")
subprocess.run(["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", "--headless=new", "--disable-gpu",
                "--no-pdf-header-footer", f"--print-to-pdf={pdf}", "file://" + out], check=True, capture_output=True)
print("wrote", pdf)
