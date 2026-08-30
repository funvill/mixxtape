"""Add ground pours to both copper layers, and keep them out of the antenna.

Run under KiCad's Python:

    "C:/Program Files/KiCad/9.0/bin/python.exe" add_zones.py

Two layers is not much for 83 nets, so the bottom layer is a solid ground
plane and the top gets a filled pour around the signal routing. That gives
every return current a short path home, which matters here for two
reasons: a 2.8 MHz PDM clock running to the microphone, and an LED chain
that can swing over a hundred milliamps in a few microseconds.

The antenna region is punched out of both pours. Copper beside a WROOM's
antenna detunes it, and the brief calls that out specifically — a board
that pairs at 30 cm instead of 3 m is a board nobody can use.
"""

import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
BOARD = HERE.parent / "mixxtape.kicad_pcb"

ORIGIN_X, ORIGIN_Y = 30.0, 30.0
BOARD_W, BOARD_H = 99.5, 63.5

# Must match gen_layout.py's ANTENNA_KEEPOUT (x, y, w, h in board coords).
ANTENNA_KEEPOUT = (92.5, 0.6, 7.8, 22.0)

ZONE_INSET = 0.3        # pull the pour in from the board edge
EDGE_CLEARANCE = 0.4    # copper to any board edge, cutouts included
ZONE_CLEARANCE = 0.25
ZONE_MIN_WIDTH = 0.2
THERMAL_GAP = 0.3
THERMAL_BRIDGE = 0.35


def kx(bx):
    return pcbnew.FromMM(ORIGIN_X + bx)


def ky(by):
    return pcbnew.FromMM(ORIGIN_Y + (BOARD_H - by))


def add_rect_zone(board, layer, net, pts, name):
    zone = pcbnew.ZONE(board)
    zone.SetLayer(layer)
    zone.SetNet(net)
    zone.SetIsFilled(False)
    zone.SetZoneName(name)
    zone.SetLocalClearance(pcbnew.FromMM(ZONE_CLEARANCE))
    zone.SetMinThickness(pcbnew.FromMM(ZONE_MIN_WIDTH))
    zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    zone.SetThermalReliefGap(pcbnew.FromMM(THERMAL_GAP))
    zone.SetThermalReliefSpokeWidth(pcbnew.FromMM(THERMAL_BRIDGE))

    outline = zone.Outline()
    outline.NewOutline()
    for bx, by in pts:
        outline.Append(kx(bx), ky(by))
    board.Add(zone)
    return zone


def add_keepout(board, pts, name):
    """A rule area that excludes copper, pours and tracks."""
    zone = pcbnew.ZONE(board)
    layers = pcbnew.LSET()
    layers.addLayer(pcbnew.F_Cu)
    layers.addLayer(pcbnew.B_Cu)
    zone.SetLayerSet(layers)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowCopperPour(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowPads(True)
    zone.SetZoneName(name)

    outline = zone.Outline()
    outline.NewOutline()
    for bx, by in pts:
        outline.Append(kx(bx), ky(by))
    board.Add(zone)
    return zone


def main():
    board = pcbnew.LoadBoard(str(BOARD))

    # Embed the manufacturing rules in the board itself. Loading a board
    # outside a project does not pick up .kicad_pro, so without this the
    # pours fill right up to the reel windows and the mouse bites, and DRC
    # then reports dozens of edge-clearance errors that are really one
    # missing setting.
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = pcbnew.FromMM(EDGE_CLEARANCE)
    ds.m_MinClearance = pcbnew.FromMM(0.2)
    ds.m_TrackMinWidth = pcbnew.FromMM(0.15)
    ds.m_ViasMinSize = pcbnew.FromMM(0.45)
    ds.m_ViasMinAnnularWidth = pcbnew.FromMM(0.1)
    ds.m_MinThroughDrill = pcbnew.FromMM(0.3)

    gnd = board.FindNet("GND")
    if gnd is None:
        print("no GND net - run gen_placement.py first")
        return 1

    # Remove any pours from a previous run so this stays re-runnable.
    for z in list(board.Zones()):
        board.Remove(z)

    i = ZONE_INSET
    body = [(i, i), (BOARD_W - i, i),
            (BOARD_W - i, BOARD_H - i), (i, BOARD_H - i)]

    add_rect_zone(board, pcbnew.B_Cu, gnd, body, "GND plane (bottom)")
    add_rect_zone(board, pcbnew.F_Cu, gnd, body, "GND pour (top)")

    ax, ay, aw, ah = ANTENNA_KEEPOUT
    add_keepout(board, [(ax, ay), (ax + aw, ay),
                        (ax + aw, ay + ah), (ax, ay + ah)],
                "ESP32 antenna keepout")

    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())

    pcbnew.SaveBoard(str(BOARD), board)

    filled = sum(1 for z in board.Zones() if not z.GetIsRuleArea())
    rules = sum(1 for z in board.Zones() if z.GetIsRuleArea())
    print("added %d ground pours and %d keepout area(s), filled"
          % (filled, rules))
    return 0


if __name__ == "__main__":
    sys.exit(main())
