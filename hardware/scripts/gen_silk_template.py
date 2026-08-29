# -*- coding: utf-8 -*-
"""Build a 1:1 SVG template of the board's BACK face for the silkscreen artist.

Everything is mirrored (x' = W - x) so the file matches what you see holding
the board with the back towards you. Draw in this orientation; the flip back
into board coordinates happens on import.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sexp import parse, kids, one   # hardware/scripts/sexp.py

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(HERE, "..", "mixxtape.kicad_pcb")
OUT = os.path.join(HERE, "..", "mixxtape-back-silkscreen-template.svg")
OX = OY = 30.0
W, H = 100.33, 63.50

root = parse(open(PCB, encoding="utf-8").read())


def mx(x):
    return W - (x - OX)          # mirror into back-view coordinates


def my(y):
    return y - OY


lines, circles = [], []
for n in kids(root, "gr_line"):
    if one(n, "layer") != "Edge.Cuts":
        continue
    a, b = kids(n, "start")[0], kids(n, "end")[0]
    lines.append((mx(float(a[1])), my(float(a[2])),
                  mx(float(b[1])), my(float(b[2]))))
for n in kids(root, "gr_circle"):
    if one(n, "layer") != "Edge.Cuts":
        continue
    c, e = kids(n, "center")[0], kids(n, "end")[0]
    cx, cy = float(c[1]), float(c[2])
    r = math.hypot(float(e[1]) - cx, float(e[2]) - cy)
    circles.append((mx(cx), my(cy), r))

parts = [
    f'<?xml version="1.0" encoding="UTF-8"?>',
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" '
    f'viewBox="0 0 {W} {H}">',
    '<title>Mixxtape back-face silkscreen template (1:1, viewed from the back)</title>',
    '<g id="board-outline" fill="none" stroke="#111111" stroke-width="0.12">',
]
for x1, y1, x2, y2 in lines:
    parts.append(f'  <line x1="{x1:.3f}" y1="{y1:.3f}" '
                 f'x2="{x2:.3f}" y2="{y2:.3f}"/>')
for cx, cy, r in circles:
    parts.append(f'  <circle cx="{cx:.3f}" cy="{cy:.3f}" r="{r:.3f}"/>')
parts.append('</g>')

# --- things the art must stay clear of -----------------------------------
parts.append('<g id="keepout" fill="#ff0000" fill-opacity="0.10" '
             'stroke="#ff0000" stroke-width="0.15" stroke-dasharray="0.8 0.6">')
# microSD socket: on the back face, not fitted, but its pads are bare copper
parts.append(f'  <rect x="{W - 95.4:.3f}" y="21.7" '
             f'width="{95.4 - 80.5:.3f}" height="{37.3 - 21.7:.3f}"/>')
parts.append('</g>')

parts.append('<g id="keepout-labels" font-family="sans-serif" font-size="1.6" '
             'fill="#cc0000">')
parts.append(f'  <text x="{W - 94.5:.3f}" y="30.2">microSD pads</text>')
parts.append('  <text x="72.6" y="8.6" fill="#cc7700">snap-off tab '
             '&#8212; art here is lost</text>')
parts.append('</g>')

# --- the break-off tongue: art here is destroyed when the tab is snapped --
parts.append('<g id="breakoff-tab" fill="#ff9900" fill-opacity="0.14" '
             'stroke="#ff9900" stroke-width="0.15">')
parts.append(f'  <rect x="{W - 18.9:.3f}" y="0" '
             f'width="{18.9 - 11.5:.3f}" height="5.5"/>')
parts.append('</g>')

parts.append('</svg>')
open(OUT, "w", encoding="utf-8").write("\n".join(parts) + "\n")
print(f"wrote {OUT}")
print(f"  {len(lines)} outline segments, {len(circles)} cutouts, mirrored for back view")
