"""Place every component onto the mechanical board, with nets attached.

Runs under KiCad's bundled Python (it needs the pcbnew module):

    "C:/Program Files/KiCad/9.0/bin/python.exe" gen_placement.py

Reads the netlist exported from the schematic, loads each footprint, puts
it where the floorplan below says, assigns pad nets, and then checks its
own work: overlapping parts, parts hanging off the board, and parts
sitting in a reel window, on the tab or over the lanyard hole are all
reported rather than left for DRC to find later.

FLOORPLAN
=========
The fixed features do the deciding. Two reel windows eat the middle, the
label block owns the upper left, and the tab owns the top-left corner.

The thing that surprised: the ESP32 module footprint is 26.7 x 20.1 mm
once its courtyard counts, which does *not* fit the 18 mm-tall strip above
the reels. It lives along the bottom instead, antenna pointing at the
right-hand edge with ~8 mm of clear board beyond it.

    bottom  y 0.5-23    ESP32 + antenna clearance, buttons, indicator
                        LEDs, jig pads
    centre  x 40-60     mic (dead centre, so neither gripped edge covers
                        it), audio flash, level shifter
    left    x 0.6-18    USB-C and the regulator: power in, one corner
    right   x 82-99.7   microSD (DNP), which needs edge access
    top     y 45-63     battery section (DNP) and LED-rail decoupling,
                        kept clear of the label block and the tab

Reel LEDs are not hand-placed: D1-D12 and D13-D24 sit on the same 18 mm
rings gen_layout.py draws, in chain order, so daisy-chain hops are short.
"""

import math
import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
HW = HERE.parent
BOARD_IN = HW / "mixxtape.kicad_pcb"
NETLIST = HERE / "mixxtape.net"
PRETTY = {
    "mixxtape_parts": HW / "parts" / "mixxtape_parts.pretty",
    "mixxtape_local": HW / "parts" / "mixxtape_local.pretty",
}

# Must match gen_layout.py.
ORIGIN_X, ORIGIN_Y = 30.0, 30.0
BOARD_W, BOARD_H = 100.33, 63.5
HUB_SPACING, HUB_Y = 42.0, 34.3
LED_RING_DIA = 18.0
EDGE_KEEPOUT = 0.6      # copper to board edge
REEL_CLEAR_R = 10.9     # ring radius + LED half-size + margin

LEFT_HUB = (BOARD_W / 2.0 - HUB_SPACING / 2.0, HUB_Y)
RIGHT_HUB = (BOARD_W / 2.0 + HUB_SPACING / 2.0, HUB_Y)

TAB_BOX = (9.0, 57.5, 21.5, BOARD_H)      # routed slots + break line
LANYARD = (6.5, 6.5, 2.6)                 # x, y, radius
MOUNT_HOLES = [(6.5, 14.0, 2.2), (95.0, 47.0, 2.2)]  # x, y, radius


def kx(bx):
    return ORIGIN_X + bx


def ky(by):
    """Board coords measure up from the bottom edge; KiCad measures down."""
    return ORIGIN_Y + (BOARD_H - by)


def ring(hub, count, start_deg):
    cx, cy = hub
    out = []
    for i in range(count):
        deg = start_deg - i * (360.0 / count)   # clockwise
        a = math.radians(deg)
        out.append((cx + (LED_RING_DIA / 2.0) * math.cos(a),
                    cy + (LED_RING_DIA / 2.0) * math.sin(a),
                    deg % 360.0))
    return out


PLACEMENT = {}
for i, (x, y, a) in enumerate(ring(LEFT_HUB, 12, 90)):
    PLACEMENT["D%d" % (i + 1)] = (x, y, a - 90.0)
for i, (x, y, a) in enumerate(ring(RIGHT_HUB, 12, 90)):
    PLACEMENT["D%d" % (i + 13)] = (x, y, a - 90.0)

PLACEMENT.update({
    # --- bottom strip -----------------------------------------------------
    # Pushed hard right so the antenna fires off the board edge and its
    # keepout costs no usable board (Steven's review). Rotated 180 because
    # the antenna is at this footprint's left end.
    "U1":  (83.1, 11.6, 180),

    # Buttons centred on the board rather than hugging the left edge, so
    # the row reads as deliberate against the cassette silhouette.
    "SW1": (32.2, 6.0, 0),
    "SW2": (44.2, 6.0, 0),
    "SW3": (56.2, 6.0, 0),
    "SW4": (68.2, 6.0, 0),

    "D25": (36.0, 15.0, 0),      # track 1
    "D26": (42.0, 15.0, 0),      # track 2
    "D27": (48.0, 15.0, 0),      # track 3
    "D28": (56.0, 15.0, 0),      # REC
    "D29": (64.0, 15.0, 0),      # BT

    "J5":  (16.0, 18.0, 0),      # jig pogo pads, bottom-left

    # ESP32 support, in the narrow column just left of the module
    "C5":  (71.5, 20.0, 0),
    "C6":  (71.5, 17.0, 0),
    "C9":  (71.5, 14.0, 0),
    "R3":  (71.5, 11.0, 0),
    "C20": (22.0, 22.0, 0),
    "C21": (28.0, 22.0, 0),

    # --- centre gap: audio ------------------------------------------------
    "U3":  (50.0, 32.0, 0),      # audio flash
    "C8":  (55.5, 32.0, 0),
    "R4":  (45.0, 38.0, 0),
    "R5":  (45.0, 35.5, 0),
    "R6":  (45.0, 28.5, 0),
    "U8":  (50.0, 24.8, 0),      # WS2812 level shifter
    "C19": (45.0, 24.8, 0),

    # --- left edge: power in -----------------------------------------------
    "J1":  (6.2, 38.0, 0),       # USB-C, hard against the left edge
    "U4":  (6.2, 27.5, 0),       # 3V3 regulator, stacked below it
    "C1":  (14.0, 30.0, 0),
    "C2":  (14.0, 38.0, 0),
    "R1":  (3.5, 45.0, 0),
    "R2":  (3.5, 47.0, 0),
    "R10": (14.0, 25.0, 0),
    "R11": (14.0, 43.0, 0),

    # --- right edge: microSD (DNP) ------------------------------------------
    "J4":  (91.0, 34.0, 0),
    "R9":  (84.0, 41.0, 0),

    # --- top strip ----------------------------------------------------------
    "R7":  (3.5, 50.0, 0),       # tab pull-down, left of the label block

    # Mic to the top-right corner (Steven's review). Top-ported, so its
    # face is the acoustic path; up here it is clear of both gripped edges
    # and of the sharpie label.
    "U2":  (95.0, 57.0, 0),
    "C7":  (89.0, 57.0, 0),

    # Battery section, DNP. The JST sits on the top edge with its opening
    # outwards so a cell can be plugged in without routing wire over the
    # board (Steven's review).
    "U5":  (70.0, 52.0, 0),
    "R8":  (78.0, 52.0, 0),
    "J2":  (70.0, 60.0, 0),

    # LED-rail decoupling, spread along the top clear of the label
    "C22": (64.0, 46.0, 0),
    "C23": (69.0, 46.0, 0),
    "C24": (74.0, 46.0, 0),
    "C25": (79.0, 46.0, 0),
    "C26": (84.0, 46.0, 0),
    "C27": (89.0, 46.0, 0),
})


def parse_netlist(path):
    """Returns (components, nets): ref -> footprint, and net -> [(ref, pad)].

    Split-based rather than regex: several components legitimately have no
    footprint field, and a greedy pattern quietly attributes the *next*
    component's footprint to them.
    """
    text = path.read_text(encoding="utf-8")

    comps = {}
    for blk in text.split('(comp (ref "')[1:]:
        ref = blk.split('"', 1)[0]
        key = '(footprint "'
        i = blk.find(key)
        comps[ref] = blk[i + len(key):].split('"', 1)[0] if i >= 0 else ""

    nets = {}
    for blk in text.split('(net (code "')[1:]:
        k = blk.find('(name "')
        if k < 0:
            continue
        name = blk[k + 7:].split('"', 1)[0]
        nodes = []
        for nb in blk.split('(node (ref "')[1:]:
            ref = nb.split('"', 1)[0]
            pk = nb.find('(pin "')
            if pk < 0:
                continue
            nodes.append((ref, nb[pk + 6:].split('"', 1)[0]))
        if nodes:
            nets[name] = nodes
    return comps, nets


def board_boxes(board):
    """Each footprint's bounding box in board coords: ref -> (x0,y0,x1,y1)."""
    out = {}
    for fp in board.GetFootprints():
        bb = fp.GetBoundingBox(False, False)
        x0 = bb.GetLeft() / 1e6 - ORIGIN_X
        x1 = bb.GetRight() / 1e6 - ORIGIN_X
        # KiCad Y grows downward; flip back to board coords.
        y0 = BOARD_H - (bb.GetBottom() / 1e6 - ORIGIN_Y)
        y1 = BOARD_H - (bb.GetTop() / 1e6 - ORIGIN_Y)
        out[fp.GetReference()] = (x0, y0, x1, y1)
    return out


def is_ring_led(ref):
    return (ref.startswith("D") and ref[1:].isdigit()
            and 1 <= int(ref[1:]) <= 24)


def check(board):
    """Reports placement problems. Returns a list of complaints."""
    boxes = board_boxes(board)
    bad = []

    for ref in sorted(boxes):
        x0, y0, x1, y1 = boxes[ref]

        if (x0 < EDGE_KEEPOUT or y0 < EDGE_KEEPOUT
                or x1 > BOARD_W - EDGE_KEEPOUT
                or y1 > BOARD_H - EDGE_KEEPOUT):
            bad.append("%s: off board or inside the %.1f mm edge keepout "
                       "(%.1f,%.1f)-(%.1f,%.1f)"
                       % (ref, EDGE_KEEPOUT, x0, y0, x1, y1))

        if not is_ring_led(ref):
            for name, hub in (("left reel", LEFT_HUB),
                              ("right reel", RIGHT_HUB)):
                cx, cy = hub
                nx = max(x0, min(cx, x1))
                ny = max(y0, min(cy, y1))
                if math.hypot(nx - cx, ny - cy) < REEL_CLEAR_R:
                    bad.append("%s: inside the %s" % (ref, name))

        tx0, ty0, tx1, ty1 = TAB_BOX
        if x0 < tx1 and x1 > tx0 and y0 < ty1 and y1 > ty0:
            bad.append("%s: inside the write-protect tab" % ref)

        for hx, hy, hr in [LANYARD] + MOUNT_HOLES:
            nx = max(x0, min(hx, x1))
            ny = max(y0, min(hy, y1))
            if math.hypot(nx - hx, ny - hy) < hr:
                what = ("lanyard hole" if (hx, hy, hr) == LANYARD
                        else "mounting hole at (%.0f,%.0f)" % (hx, hy))
                bad.append("%s: over the %s" % (ref, what))

    refs = sorted(boxes)
    for i, a in enumerate(refs):
        ax0, ay0, ax1, ay1 = boxes[a]
        for b in refs[i + 1:]:
            bx0, by0, bx1, by1 = boxes[b]
            if ax0 < bx1 and ax1 > bx0 and ay0 < by1 and ay1 > by0:
                bad.append("%s overlaps %s" % (a, b))
    return bad


def main():
    board = pcbnew.LoadBoard(str(BOARD_IN))
    comps, nets = parse_netlist(NETLIST)
    print("netlist: %d components, %d nets" % (len(comps), len(nets)))

    for name in nets:
        if not board.FindNet(name):
            board.Add(pcbnew.NETINFO_ITEM(board, name))

    placed = 0
    missing_fp, missing_pos = [], []
    for ref, fpid in sorted(comps.items()):
        if not fpid:
            # TAB1 is the break-off tab: drawn in the mechanical layer and
            # deliberately has no footprint.
            continue
        lib, fpname = fpid.split(":", 1)
        libdir = PRETTY.get(lib)
        if libdir is None:
            missing_fp.append((ref, fpid))
            continue
        fp = pcbnew.FootprintLoad(str(libdir), fpname)
        if fp is None:
            missing_fp.append((ref, fpid))
            continue
        if ref not in PLACEMENT:
            missing_pos.append(ref)
            continue

        bx, by, rot = PLACEMENT[ref]
        fp.SetReference(ref)
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(kx(bx)),
                                       pcbnew.FromMM(ky(by))))
        fp.SetOrientationDegrees(rot)
        board.Add(fp)
        placed += 1

    attached = 0
    for name, nodes in nets.items():
        net = board.FindNet(name)
        if net is None:
            continue
        for ref, padname in nodes:
            fp = board.FindFootprintByReference(ref)
            if fp is None:
                continue
            for pad in fp.Pads():
                if pad.GetNumber() == padname:
                    pad.SetNet(net)
                    attached += 1

    board.BuildListOfNets()
    pcbnew.SaveBoard(str(BOARD_IN), board)
    print("placed %d footprints, attached %d pads to nets" % (placed, attached))

    if missing_fp:
        print("MISSING FOOTPRINTS:")
        for ref, fpid in missing_fp:
            print("  %s: %s" % (ref, fpid))
    if missing_pos:
        print("NO PLACEMENT for: %s" % ", ".join(sorted(missing_pos)))

    problems = check(board)
    if problems:
        print("\n%d placement problems:" % len(problems))
        for p in problems:
            print("  %s" % p)
    else:
        print("\nplacement clean: nothing overlapping, off-board, in a reel "
              "window, on the tab, or over the lanyard hole")
    return 1 if (missing_fp or missing_pos or problems) else 0


if __name__ == "__main__":
    sys.exit(main())
