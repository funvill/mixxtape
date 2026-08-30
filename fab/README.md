# Fabrication export — JLCPCB

Generated from `hardware/mixxtape.kicad_pcb`. Rebuild with:

```
python hardware/scripts/export_fab.py
```

It refuses to run on a board that is not DRC clean, and it cross-checks the BOM
against the placement file — a designator in one and not the other stops the
export.

**Board state:** DRC 0 errors, 0 unconnected · ERC 0 violations · board and
schematic agree on every pad.

## What to upload

| File | Where it goes |
|---|---|
| `jlcpcb/mixxtape-gerbers.zip` | the main upload box |
| `jlcpcb/mixxtape-bom.csv` | BOM, once assembly is switched on |
| `jlcpcb/mixxtape-cpl.csv` | CPL / pick-and-place |

## Order settings

| | |
|---|---|
| Layers | 2 |
| Dimensions | **99.50 × 63.50 mm** |
| Thickness | 1.6 mm |
| **Solder mask** | **Matte white** — adds about two days, and the silkscreen is then black as a fixed pairing, not a choice |
| Silkscreen | Black (follows from white mask) |
| Surface finish | HASL is fine; ENIG if you want the break-off tab's link to stay bright |
| Assembly | Top side only |
| Parts | 66 placements across 15 BOM lines |

## Things worth checking in their previewer

**Rotations.** Twenty parts sit at angles that are not multiples of 90° — the
two twelve-LED reel rings, plus four capacitors tucked between them. That is
deliberate, not an export fault. JLCPCB handles arbitrary angles, but their
part library sometimes defines a different zero-rotation than KiCad does,
particularly for LEDs and connectors. **Step through the placement previewer
before paying**, especially D1–D29 and J1.

**One part is deliberately not fitted.** J4, the microSD socket, is marked DNP
and is absent from both the BOM and the CPL — the microSD is a footprint-only
option. **If the previewer shows a microSD socket, stop.**

R9 *is* fitted. It is the pull-up that keeps the SD chip-select pin defined
even with the socket unpopulated.

## What is in the zip

Nine Gerber layers, two Excellon drill files (PTH and NPTH separately) and the
`.gbrjob` file, which helps their parser map the layers.

Drill map Gerbers are generated into `jlcpcb/gerbers/` but deliberately left
**out** of the zip — they are documentation, and a stray `.gbr` can be mistaken
for a copper layer by an auto-detector. Send them only if DFM asks.

## Export settings used

Per JLCPCB's own KiCad guide:

- Layers: F.Cu, B.Cu, F.Paste, B.Paste, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
- Protel filename extensions (their recommendation)
- Subtract soldermask from silkscreen
- Zone fills checked before plotting
- X2 format **off**, for the widest CAM compatibility
- Drill: Excellon, millimetres, decimal, absolute origin, alternate oval mode,
  PTH and NPTH in separate files

The Gerbers and the CPL both use the absolute origin, so they align.
