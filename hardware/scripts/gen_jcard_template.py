# -*- coding: utf-8 -*-
"""Build a 1:1, double-sided J-card template for the cassette case insert.

Panel sizes follow the stepped standard: each folded panel is 1/16 inch
narrower than the one before, so the card nests inside its own folds instead
of binding against them.

    front panel      2 9/16"   65.0875 mm   seen through the case front
    spine            1/2"      12.7    mm   seen on the shelf edge
    tracklist flap   1 1/16"   26.9875 mm   wraps behind the cassette
    card width       4"        101.6   mm

The 101.6 mm runs horizontally, the same way the board's 99.5 mm does, so
the front panel is 101.6 x 65.09 and reads upright. Panels are horizontal
bands; the folds are horizontal too.

Two pages, card in the identical position on each, because that is what
duplex printing needs. Print flipped on the LONG edge: that mirrors left to
right, the card is centred, so every panel lands on the back of itself.
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.normpath(os.path.join(HERE, "..", "..", "docs"))

FRONT, SPINE, FLAP = 65.0875, 12.7, 26.9875
CARD_W = 101.6
CARD_H = FRONT + SPINE + FLAP          # 104.775
BLEED, SAFE = 3.0, 3.0

PAGE_W, PAGE_H = 210.0, 297.0
LEFT = (PAGE_W - CARD_W) / 2           # 54.2
TOP = 62.0

INK, FOLD, SAFEC, NOTE = "#111111", "#0F6FA8", "#C0392B", "#7A8288"
MONO = "'IBM Plex Mono','DejaVu Sans Mono',monospace"


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Sheet:
    def __init__(self):
        self.o = []

    def add(self, s):
        self.o.append(s)

    def text(self, x, y, s, size=2.4, fill=INK, anchor="start", weight="400", ls="0"):
        self.add(f'<text x="{x:.3f}" y="{y:.3f}" font-family="{MONO}" '
                 f'font-size="{size}" font-weight="{weight}" fill="{fill}" '
                 f'text-anchor="{anchor}" letter-spacing="{ls}">{esc(s)}</text>')

    def line(self, x1, y1, x2, y2, colour, w=0.25, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" '
                 f'stroke="{colour}" stroke-width="{w}"{d}/>')

    def rect(self, x, y, w, h, stroke, sw=0.3, dash=None, fill="none"):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x:.3f}" y="{y:.3f}" width="{w:.3f}" height="{h:.3f}" '
                 f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}/>')


def build(side):
    s = Sheet()
    s.add('<?xml version="1.0" encoding="UTF-8"?>')
    s.add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_W}mm" '
          f'height="{PAGE_H}mm" viewBox="0 0 {PAGE_W} {PAGE_H}">')
    s.add(f'<title>Mixxtape J-card template — side {side}</title>')
    s.add(f'<rect width="{PAGE_W}" height="{PAGE_H}" fill="#ffffff"/>')

    label = "SIDE 1 — OUTSIDE" if side == 1 else "SIDE 2 — INSIDE"
    s.text(LEFT, 22, "MIXXTAPE — CASSETTE INSERT (J-CARD)", 4.2, INK, "start", "600", "0.08")
    s.text(LEFT, 27.5, "1:1. Print at 100% / actual size, fit-to-page OFF.", 2.4, NOTE)
    s.text(LEFT, 34.5, label, 3.6, INK, "start", "600", "0.16")
    s.text(LEFT + CARD_W, 34.5, f"{CARD_W:.1f} × {CARD_H:.3f} mm", 2.4, NOTE, "end")
    s.text(LEFT, 39.5,
           "Page 1 and 2 sit in the same place. Duplex, flip on the LONG edge.",
           2.2, NOTE)

    # bleed / cut / safe
    s.rect(LEFT - BLEED, TOP - BLEED, CARD_W + 2 * BLEED, CARD_H + 2 * BLEED,
           NOTE, 0.2, "2 1.5")
    s.rect(LEFT, TOP, CARD_W, CARD_H, INK, 0.45)
    s.rect(LEFT + SAFE, TOP + SAFE, CARD_W - 2 * SAFE, CARD_H - 2 * SAFE,
           SAFEC, 0.2, "1.2 1.2")

    bands = [("FRONT PANEL", FRONT), ("SPINE", SPINE), ("TRACKLIST FLAP", FLAP)]
    y = TOP
    edges = {}
    for i, (name, h) in enumerate(bands):
        if i:
            s.line(LEFT - BLEED, y, LEFT + CARD_W + BLEED, y, FOLD, 0.4, "3 2")
        edges[name] = (y, h)
        s.text(LEFT + CARD_W - 2.5, y + (6.0 if h > 20 else 4.6),
               f"{name}  {h:.4g} mm",
               2.1, NOTE, "end", "600", "0.06")
        y += h

    fy, fh = edges["FRONT PANEL"]
    sy, sh = edges["SPINE"]
    ty, th = edges["TRACKLIST FLAP"]
    cx = LEFT + CARD_W / 2

    if side == 1:
        s.text(cx, fy + 16, "ARTWORK", 7.0, INK, "middle", "600", "0.35")
        s.text(cx, fy + 22.5, "the face seen through the case front", 2.4, NOTE, "middle")
        s.text(cx, fy + 40, "name  ·  cover art", 2.6, NOTE, "middle")
        s.text(LEFT + 4, sy + sh / 2 + 1, "NAME  ·  REV", 2.6, NOTE, "start", "600", "0.1")
        s.text(LEFT + CARD_W - 4, sy + sh / 2 + 1, "seen on the shelf edge",
               2.2, NOTE, "end")
        s.text(LEFT + 5, ty + 5.0, "TRACKS", 3.2, INK, "start", "600", "0.2")
        s.text(LEFT + 5, ty + 9.0,
               "leave blank — the owner records these by hand", 1.9, NOTE)
        for i in range(3):
            ly = ty + 14.5 + i * 4.5
            s.text(LEFT + 5, ly - 0.7, f"{i + 1}", 2.2, NOTE)
            s.line(LEFT + 8.5, ly, LEFT + CARD_W - 24, ly, NOTE, 0.3)
            s.line(LEFT + CARD_W - 22, ly, LEFT + CARD_W - 5, ly, NOTE, 0.3)
            s.text(LEFT + CARD_W - 13.5, ly - 1.1, "min:sec", 1.5, NOTE, "middle")
    else:
        s.text(cx, fy + 16, "INSIDE FRONT", 6.0, INK, "middle", "600", "0.25")
        s.text(cx, fy + 22.5, "hidden until the card is lifted out", 2.4, NOTE, "middle")
        s.text(cx, fy + 40, "how it works  ·  credits  ·  URL  ·  licence",
               2.6, NOTE, "middle")
        s.text(LEFT + 4, sy + sh / 2 + 1, "reverse of the spine — usually blank",
               2.2, NOTE)
        s.text(LEFT + 5, ty + 5.0, "NOTES", 3.2, INK, "start", "600", "0.2")
        s.text(LEFT + 5, ty + 9.0,
               "recorded on · where · who it is for", 1.9, NOTE)
        for i in range(3):
            ly = ty + 14.5 + i * 4.5
            s.line(LEFT + 5, ly, LEFT + CARD_W - 5, ly, NOTE, 0.3)

    # calibration bar
    by = TOP + CARD_H + 20
    s.line(LEFT, by, LEFT + 50, by, INK, 0.6)
    for t in (0, 50):
        s.line(LEFT + t, by - 2, LEFT + t, by + 2, INK, 0.6)
    s.text(LEFT + 53, by + 1, "50.0 mm — MEASURE THIS FIRST", 2.8, INK, "start", "600")
    s.text(LEFT, by + 6,
           "If this bar is not exactly 50.0 mm the printer scaled the page,", 2.2, NOTE)
    s.text(LEFT, by + 9.6, "and nothing else here can be trusted.", 2.2, NOTE)

    ly = by + 18
    for name, colour, desc in (
            ("solid black", INK, "cut line — the finished card"),
            ("dashed blue", FOLD, "fold — score, do not cut"),
            ("dotted red", SAFEC, "safe area — keep type inside"),
            ("dashed grey", NOTE, "bleed — run art out to here (3 mm)")):
        s.add(f'<rect x="{LEFT:.3f}" y="{ly - 2.2:.3f}" width="6" height="2.6" '
              f'fill="{colour}" fill-opacity="0.85"/>')
        s.text(LEFT + 8.5, ly, f"{name} — {desc}", 2.3, NOTE)
        ly += 4.4

    s.text(LEFT, ly + 4,
           "Norelco cases vary by brand. Cut one, fold it, try it in a real case",
           2.3, INK, "start", "600")
    s.text(LEFT, ly + 8, "before printing a batch.", 2.3, INK, "start", "600")

    s.add('</svg>')
    return "\n".join(s.o) + "\n"


paths = []
for side in (1, 2):
    p = os.path.join(DOCS, f"jcard-template-side{side}.svg")
    open(p, "w", encoding="utf-8").write(build(side))
    paths.append(p)
    print(f"wrote {p}")

# stitch the two sheets into one duplex-ready PDF
INK_EXE = r"C:/Program Files/Inkscape/bin/inkscape.exe"
if os.path.exists(INK_EXE):
    pdfs = []
    for p in paths:
        q = p[:-4] + ".pdf"
        subprocess.run([INK_EXE, p, "--export-type=pdf", f"--export-filename={q}"],
                       check=True, capture_output=True)
        pdfs.append(q)
    try:
        import pypdf
        w = pypdf.PdfWriter()
        for q in pdfs:
            w.append(q)
        out = os.path.join(DOCS, "jcard-template-1to1.pdf")
        with open(out, "wb") as fh:
            w.write(fh)
        for q in pdfs:
            os.remove(q)
        print(f"wrote {out} (2 pages)")
    except ImportError:
        print("pypdf missing — per-side PDFs left in place")

print(f"  card {CARD_W:.1f} x {CARD_H:.3f} mm "
      f"(front {FRONT}, spine {SPINE}, flap {FLAP})")
