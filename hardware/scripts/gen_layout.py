"""Generate the mechanical layer of the PCB: outline, reel windows, the
break-off write-protect tab, the lanyard hole and the microphone port.

This is phase 2 of the brief's order ("outline + mechanical"), deliberately
separate from component placement and routing. Getting the shape right is
what everything else hangs off, and it is the part that has to be checked
against a physical cassette case before any copper is drawn.

The outline is **not redrawn from a datasheet**. The brief says to take it
from Open Music Labs' Mixtape Alpha rather than guess, so it comes from
that project's released gerbers (`cassette_outline.json`, extracted from
the board-outline layer of mixtape2.zip). It is a cassette shape that has
been fabricated and is known to fit real cases — which is worth much more
than a dimension copied out of a standard.

Every value below that is *ours* rather than inherited is marked NEEDS
CHECK, and the brief is emphatic about why: internal ribs and hub-clamp
ridges vary between case brands, and thrifted cases are not dimensionally
consistent. Print this at 1:1, cut it out, and put it in a real case.

    python gen_layout.py            # writes ../mixxtape.kicad_pcb
"""

import argparse
import json
import math
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUTLINE_JSON = HERE / "cassette_outline.json"
OUT_PCB = HERE.parent / "mixxtape.kicad_pcb"

# --- geometry ---------------------------------------------------------
# Board comes out 100.33 x 63.50 mm. The brief says 100.0 x 63.5; the
# reference is 0.33 mm wider and has actually been built, so it wins.

# Reel hubs. Spacing is the compact-cassette standard 42 mm; the height was
# measured off the Mixtape Alpha board render (34.3 mm from the bottom
# edge, i.e. sitting slightly above centre, which is what makes a cassette
# look like a cassette rather than a rectangle with two holes).
HUB_SPACING = 42.0
HUB_Y = 34.3
REEL_WINDOW_DIA = 14.0  # NEEDS CHECK against a case's hub-clamp ridges
LED_RING_DIA = 18.0     # 12 LEDs at 4.71 mm pitch, per the BOM

# Break-off write-protect tab, top edge. Snapping it severs a trace and
# disables recording permanently.
TAB_W = 9.0
TAB_DEPTH = 5.5
TAB_CENTRE_X = 16.0     # NEEDS CHECK: must clear the case's internal ribs
SLOT_W = 1.6            # routed slot either side of the tab
BITE_DIA = 0.6          # mouse-bite holes along the break line
BITE_PITCH = 1.4
BITE_COUNT = 5

LANYARD_DIA = 3.0
LANYARD_POS = (6.5, 6.5)  # bottom-left; NEEDS CHECK against the case hinge

# Bottom-ported mic needs a hole through the board beneath it. Placed away
# from both reel windows and from where fingers naturally land.
MIC_PORT_DIA = 0.7
MIC_PORT_POS = (86.0, 52.0)  # NEEDS CHECK once U2 placement is fixed

# The WROOM antenna must have no copper under or beside it.
ANTENNA_KEEPOUT = (74.0, 44.0, 26.0, 17.0)  # x, y, w, h — proposed

# Sharpie label block, where a cassette's paper label sits.
LABEL_BOX = (7.0, 45.0, 55.0, 14.0)
LABEL_RULES = 3

# Where the board sits on the sheet. Keeps the outline and the notes clear
# of the title block instead of running off the corner of the page.
ORIGIN_X = 30.0
ORIGIN_Y = 30.0

EDGE_W = 0.1
SILK_W = 0.15


def u():
    return str(uuid.uuid4())


def seg(x1, y1, x2, y2, layer="Edge.Cuts", width=EDGE_W):
    x1 += ORIGIN_X
    x2 += ORIGIN_X
    return (f'\t(gr_line (start {x1:.3f} {y1:.3f}) (end {x2:.3f} {y2:.3f})\n'
            f'\t\t(stroke (width {width}) (type solid)) (layer "{layer}")\n'
            f'\t\t(uuid "{u()}")\n\t)')


def circle(cx, cy, dia, layer="Edge.Cuts", width=EDGE_W, fill="no"):
    cx += ORIGIN_X
    return (f'\t(gr_circle (center {cx:.3f} {cy:.3f}) '
            f'(end {cx + dia / 2.0:.3f} {cy:.3f})\n'
            f'\t\t(stroke (width {width}) (type solid)) (fill {fill}) '
            f'(layer "{layer}")\n\t\t(uuid "{u()}")\n\t)')


def rect(x, y, w, h, layer, width=SILK_W):
    x += ORIGIN_X
    return (f'\t(gr_rect (start {x:.3f} {y:.3f}) (end {x + w:.3f} {y + h:.3f})\n'
            f'\t\t(stroke (width {width}) (type solid)) (fill no) '
            f'(layer "{layer}")\n\t\t(uuid "{u()}")\n\t)')


def text(s, x, y, layer, size=1.2, width=0.2):
    x += ORIGIN_X
    return (f'\t(gr_text "{s}" (at {x:.3f} {y:.3f} 0) (layer "{layer}")\n'
            f'\t\t(uuid "{u()}")\n'
            f'\t\t(effects (font (size {size} {size}) (thickness {width})) '
            f'(justify left))\n\t)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(OUT_PCB))
    args = ap.parse_args()

    ref = json.loads(OUTLINE_JSON.read_text(encoding="utf-8"))
    pts = ref["outline"]
    board_w = max(p[0] for p in pts)
    board_h = max(p[1] for p in pts)

    # KiCad's Y axis runs downwards; the gerber's runs up. Flip once here so
    # every constant above can be read as "from the bottom edge", which is
    # how anyone looking at a cassette thinks about it.
    def fy(y):
        return ORIGIN_Y + (board_h - y)

    _fx = lambda x: ORIGIN_X + x  # noqa: E731

    body = []
    body.append(f'\t(gr_text "board outline from Open Music Labs Mixtape '
                f'Alpha (CC) — {board_w:.2f} x {board_h:.2f} mm"\n'
                f'\t\t(at 4.0 {fy(-4.5):.3f} 0) (layer "Cmts.User")\n'
                f'\t\t(uuid "{u()}")\n'
                f'\t\t(effects (font (size 1.2 1.2) (thickness 0.2)) '
                f'(justify left))\n\t)')

    # --- outline -------------------------------------------------------
    for i in range(len(pts) - 1):
        x1, y1 = pts[i]
        x2, y2 = pts[i + 1]
        body.append(seg(x1, fy(y1), x2, fy(y2)))

    # --- reel windows --------------------------------------------------
    hubs = [(board_w / 2.0 - HUB_SPACING / 2.0, HUB_Y),
            (board_w / 2.0 + HUB_SPACING / 2.0, HUB_Y)]
    for cx, cy in hubs:
        body.append(circle(cx, fy(cy), REEL_WINDOW_DIA))
        # LED ring as a placement guide, not a fabricated feature.
        body.append(circle(cx, fy(cy), LED_RING_DIA, layer="Dwgs.User",
                           width=SILK_W))
        for k in range(12):
            a = math.radians(k * 30.0)
            lx = cx + (LED_RING_DIA / 2.0) * math.cos(a)
            ly = cy + (LED_RING_DIA / 2.0) * math.sin(a)
            body.append(circle(lx, fy(ly), 1.0, layer="Dwgs.User",
                               width=0.1))

    # --- write-protect tab ---------------------------------------------
    # Two routed slots down from the top edge; the tab is the piece between
    # them, held by mouse bites along the break line.
    tab_l = TAB_CENTRE_X - TAB_W / 2.0
    tab_r = TAB_CENTRE_X + TAB_W / 2.0
    break_y = board_h - TAB_DEPTH
    for sx in (tab_l, tab_r):
        body.append(seg(sx, fy(board_h), sx, fy(break_y)))
        body.append(seg(sx - SLOT_W, fy(board_h), sx - SLOT_W, fy(break_y)))
        body.append(seg(sx - SLOT_W, fy(break_y), sx, fy(break_y)))
        body.append(seg(sx - SLOT_W, fy(board_h), sx, fy(board_h)))

    span = TAB_W - SLOT_W
    start = tab_l + (span - (BITE_COUNT - 1) * BITE_PITCH) / 2.0
    for k in range(BITE_COUNT):
        body.append(circle(start + k * BITE_PITCH, fy(break_y), BITE_DIA))
    body.append(text("SNAP TO LOCK", tab_l - 1.0, fy(break_y + 7.5),
                     "F.SilkS", size=1.0))

    # --- lanyard hole and mic port --------------------------------------
    body.append(circle(LANYARD_POS[0], fy(LANYARD_POS[1]), LANYARD_DIA))
    body.append(circle(MIC_PORT_POS[0], fy(MIC_PORT_POS[1]), MIC_PORT_DIA))
    body.append(circle(MIC_PORT_POS[0], fy(MIC_PORT_POS[1]), 2.4,
                       layer="Dwgs.User", width=0.1))
    body.append(text("mic port — keep clear", MIC_PORT_POS[0] - 10.0,
                     fy(MIC_PORT_POS[1] - 3.0), "Cmts.User", size=0.9))

    # --- antenna keepout -------------------------------------------------
    ax, ay, aw, ah = ANTENNA_KEEPOUT
    body.append(rect(ax, fy(ay + ah), aw, ah, "Cmts.User"))
    body.append(text("ANTENNA KEEPOUT — no copper, no pour, no traces",
                     ax + 0.5, fy(ay + ah - 2.0), "Cmts.User", size=0.9))

    # --- label block ------------------------------------------------------
    lx, ly, lw, lh = LABEL_BOX
    body.append(rect(lx, fy(ly + lh), lw, lh, "F.SilkS", width=0.12))
    for k in range(1, LABEL_RULES):
        ry = ly + lh * k / LABEL_RULES
        body.append(seg(lx + 1.0, fy(ry), lx + lw - 1.0, fy(ry),
                        layer="F.SilkS", width=0.12))

    # --- print calibration --------------------------------------------
    # Printers default to "fit to page" and will happily shrink this by a
    # few percent without saying so, which would make the whole case-fit
    # test lie. Measure the bar before trusting anything else on the sheet.
    cal_x = 112.0          # clear of the board, which ends at 100.33
    cal_y = 56.0
    cal_len = 50.0
    body.append(seg(cal_x, fy(cal_y), cal_x + cal_len, fy(cal_y),
                    layer="Dwgs.User", width=0.25))
    for k in range(11):
        tick = 2.5 if k % 5 == 0 else 1.2
        body.append(seg(cal_x + k * 5.0, fy(cal_y),
                        cal_x + k * 5.0, fy(cal_y + tick),
                        layer="Dwgs.User", width=0.25))
    body.append(text("50.0 mm", cal_x, fy(cal_y + 5.0), "Dwgs.User", size=1.6))
    for i, line in enumerate([
            "MEASURE THIS BAR FIRST.",
            "If it is not exactly 50.0 mm, the printer scaled the page and",
            "nothing else on this sheet can be trusted. Reprint at 100% /",
            "actual size, with any fit-to-page option turned off."]):
        body.append(text(line, cal_x, fy(cal_y - 3.5 - i * 2.4),
                         "Dwgs.User", size=1.1))

    # --- notes ------------------------------------------------------------
    notes = [
        "MECHANICAL ONLY - no components placed, no routing.",
        "Outline traced from Open Music Labs Mixtape Alpha released",
        "gerbers. Their wiki states no licence for the design files;",
        "confirm with them before distributing derived artwork. The shape",
        "is the compact-cassette form factor, so redrawing it from a",
        "measured cassette is a straightforward fallback.",
        "",
        "NEEDS CHECK against a physical Norelco case before any copper:",
        "  reel window dia vs hub-clamp ridges, tab position vs internal",
        "  ribs, lanyard hole vs the case hinge.",
        "Print at 1:1, cut it out, put it in a case. Brands vary.",
        "",
        "DEFERRED: the five decorative guide holes along the bottom edge.",
        "They never enter a deck, so they are artwork rather than",
        "mechanism, and guessed positions would constrain placement for",
        "no reason. Add them with the silkscreen artwork.",
    ]
    for i, n in enumerate(notes):
        body.append(text(n, 4.0, fy(-9.0 - i * 2.2), "Cmts.User", size=1.0))

    pcb = HEADER.format(uuid=u()) + "\n" + "\n".join(body) + "\n)\n"
    Path(args.out).write_text(pcb, encoding="utf-8", newline="\n")
    print(f"wrote {args.out}")
    print(f"  board {board_w:.2f} x {board_h:.2f} mm, {len(pts) - 1} outline "
          f"segments")
    print(f"  reel windows dia {REEL_WINDOW_DIA} at x="
          f"{hubs[0][0]:.2f}/{hubs[1][0]:.2f}, y={HUB_Y}")
    print(f"  tab {TAB_W} x {TAB_DEPTH} at x={TAB_CENTRE_X}, "
          f"{BITE_COUNT} mouse bites")


HEADER = """(kicad_pcb
\t(version 20241229)
\t(generator "gen_layout.py")
\t(generator_version "9.0")
\t(general
\t\t(thickness 1.6)
\t\t(legacy_teardrops no)
\t)
\t(paper "A4")
\t(title_block
\t\t(title "Mixxtape — mechanical")
\t\t(rev "0.1")
\t\t(comment 1 "Outline from Open Music Labs Mixtape Alpha")
\t)
\t(layers
\t\t(0 "F.Cu" signal)
\t\t(2 "B.Cu" signal)
\t\t(9 "F.Adhes" user "F.Adhesive")
\t\t(11 "B.Adhes" user "B.Adhesive")
\t\t(13 "F.Paste" user)
\t\t(15 "B.Paste" user)
\t\t(5 "F.SilkS" user "F.Silkscreen")
\t\t(7 "B.SilkS" user "B.Silkscreen")
\t\t(1 "F.Mask" user)
\t\t(3 "B.Mask" user)
\t\t(17 "Dwgs.User" user "User.Drawings")
\t\t(19 "Cmts.User" user "User.Comments")
\t\t(21 "Eco1.User" user "User.Eco1")
\t\t(23 "Eco2.User" user "User.Eco2")
\t\t(25 "Edge.Cuts" user)
\t\t(27 "Margin" user)
\t\t(31 "F.CrtYd" user "F.Courtyard")
\t\t(29 "B.CrtYd" user "B.Courtyard")
\t\t(35 "F.Fab" user)
\t\t(33 "B.Fab" user)
\t)
\t(setup
\t\t(pad_to_mask_clearance 0)
\t\t(allow_soldermask_bridges_in_footprints no)
\t)
\t(net 0 "")"""


if __name__ == "__main__":
    main()
