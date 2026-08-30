# -*- coding: utf-8 -*-
"""Produce the JLCPCB fabrication package from the board.

Settings follow JLCPCB's own KiCad guide: Protel extensions, soldermask
subtracted from the silkscreen, zone fills checked, and Excellon drills in
millimetres with a decimal zeros format and an absolute origin. X2 is turned
off, which their guide does not require but which keeps the widest CAM
compatibility.

Two exclusions matter and are easy to get wrong:

  * DNP is stored per footprint on the *board*, not only in the schematic.
    J4 and R9 were marked DNP in the schematic but not on the board, so an
    earlier export happily told JLCPCB to fit a microSD socket.
  * J5 and TAB1 are bare copper, not parts. Both carry
    exclude_from_pos_files so they stay out of the placement file.

The BOM and CPL are cross-checked against each other before the zip is
written; a designator in one and not the other stops the export.
"""
import csv
import glob
import io
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PCB = os.path.join(ROOT, "hardware", "mixxtape.kicad_pcb")
OUT = os.path.join(ROOT, "fab", "jlcpcb")
GERB = os.path.join(OUT, "gerbers")
CLI = r"C:/Program Files/KiCad/10.0/bin/kicad-cli.exe"

LAYERS = ("F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts")


def run(args):
    r = subprocess.run([CLI] + args, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"kicad-cli failed:\n{r.stdout}\n{r.stderr}")
    return r.stdout


os.makedirs(GERB, exist_ok=True)

# ---- refuse to export a board that is not clean --------------------------
rpt = os.path.join(OUT, "drc.rpt")
out = run(["pcb", "drc", "--output", rpt, "--severity-error", PCB])
if "Found 0 unconnected items" not in out:
    print(out)
    sys.exit("DRC is not clean - fix it before exporting")
errs = [l for l in io.open(rpt, encoding="utf-8") if l.startswith("[")]
if errs:
    sys.exit(f"DRC reported {len(errs)} errors - fix them before exporting")
print("  DRC clean")

# ---- gerbers and drills --------------------------------------------------
run(["pcb", "export", "gerbers", "--output", GERB, "--layers", LAYERS,
     "--subtract-soldermask", "--check-zones", "--no-x2", PCB])
run(["pcb", "export", "drill", "--output", GERB, "--format", "excellon",
     "--drill-origin", "absolute", "--excellon-zeros-format", "decimal",
     "--excellon-oval-format", "alternate", "--excellon-units", "mm",
     "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
     PCB])
print("  gerbers and drill files written")

# ---- placement file, remapped to JLCPCB's column names -------------------
raw = os.path.join(OUT, "_cpl-raw.csv")
run(["pcb", "export", "pos", "--output", raw, "--format", "csv",
     "--units", "mm", "--side", "both", "--exclude-dnp", PCB])
rows = list(csv.DictReader(io.open(raw, encoding="utf-8")))
cpl = os.path.join(OUT, "mixxtape-cpl.csv")
with io.open(cpl, "w", encoding="utf-8", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["Designator", "Val", "Package", "Mid X", "Mid Y",
                "Rotation", "Layer"])
    for r in rows:
        w.writerow([r["Ref"], r["Val"], r["Package"],
                    f'{float(r["PosX"]):.4f}', f'{float(r["PosY"]):.4f}',
                    f'{float(r["Rot"]) % 360:.1f}',
                    "Top" if r["Side"] == "top" else "Bottom"])
os.remove(raw)
print(f"  CPL: {len(rows)} placements")

# ---- BOM, straight from the generated one --------------------------------
bom_src = os.path.join(ROOT, "docs", "cassette-recorder-bom-v4.csv")
bom_dst = os.path.join(OUT, "mixxtape-bom.csv")
if not os.path.exists(bom_src):
    sys.exit("run gen_bom.py first - the BOM is missing")
io.open(bom_dst, "w", encoding="utf-8", newline="").write(
    io.open(bom_src, encoding="utf-8").read())

# ---- the two must agree ---------------------------------------------------
bom_refs = set()
for r in csv.DictReader(io.open(bom_dst, encoding="utf-8")):
    bom_refs |= set(r["Designator"].split())
    if not r.get("JLCPCB Part #"):
        sys.exit(f"BOM line without a part number: {r['Designator']}")
cpl_refs = {r["Ref"] for r in rows}
if bom_refs != cpl_refs:
    sys.exit(f"BOM and CPL disagree.\n  BOM only: {sorted(bom_refs - cpl_refs)}"
             f"\n  CPL only: {sorted(cpl_refs - bom_refs)}")
print(f"  BOM and CPL agree on all {len(cpl_refs)} designators")

# ---- zip ------------------------------------------------------------------
# Drill map gerbers are left out: they are documentation, and a stray .gbr can
# be mistaken for a copper layer by a layer auto-detector.
KEEP = (".gtl", ".gbl", ".gts", ".gbs", ".gto", ".gbo",
        ".gtp", ".gbp", ".gm1", ".drl", ".gbrjob")
files = [f for f in sorted(glob.glob(GERB + "/*"))
         if f.lower().endswith(KEEP) and "drl_map" not in f]
zpath = os.path.join(OUT, "mixxtape-gerbers.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for f in files:
        z.write(f, os.path.basename(f))
os.remove(rpt)
print(f"  {os.path.basename(zpath)}: {len(files)} files, "
      f"{os.path.getsize(zpath)/1e6:.2f} MB")
print("\nupload mixxtape-gerbers.zip, then mixxtape-bom.csv and "
      "mixxtape-cpl.csv for assembly")
