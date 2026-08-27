"""Import a routed Specctra session back onto the board and refill pours.

Run under KiCad's Python, after freerouting has produced a .ses:

    "C:/Program Files/KiCad/9.0/bin/python.exe" import_routes.py <file.ses>

Refilling the ground pours afterwards is not optional: the imported tracks
carve into them, and a stale fill would hide clearance problems from DRC.
"""

import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
BOARD = HERE.parent / "mixxtape.kicad_pcb"


def main():
    if len(sys.argv) < 2:
        print("usage: import_routes.py <file.ses>")
        return 1
    ses = Path(sys.argv[1]).resolve()
    if not ses.exists():
        print("no such session file: %s" % ses)
        return 1

    board = pcbnew.LoadBoard(str(BOARD))
    before = len(board.GetTracks())

    if not pcbnew.ImportSpecctraSES(board, str(ses)):
        print("ImportSpecctraSES failed")
        return 1

    after = len(board.GetTracks())
    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())
    pcbnew.SaveBoard(str(BOARD), board)

    vias = sum(1 for t in board.GetTracks() if isinstance(t, pcbnew.PCB_VIA))
    length = sum(t.GetLength() for t in board.GetTracks()
                 if not isinstance(t, pcbnew.PCB_VIA)) / 1e6

    print("imported %d track segments (%d before), %d vias"
          % (after - before, before, vias))
    print("total track length %.1f mm" % length)
    print("zones refilled, board saved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
