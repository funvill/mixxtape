# Documentation

Start with the root [README](../README.md) for what the object is. This folder
holds the working documents behind it.

## Read first

| | |
|---|---|
| [Agent handoff brief](cassette-recorder-agent-brief.md) | The authoritative spec — locked decisions and the reasoning behind them, the GPIO map, the behaviour spec, and the phase order. Read in full before design, firmware or BOM work. |
| [Pre-production checklist](PRE-PRODUCTION-CHECKLIST.md) | Everything that must be true before boards are ordered. The case-fit test comes first, because the outline and hole positions have never touched real plastic. |

## For users

| | |
|---|---|
| [Manual](MANUAL.md) | How to record, play, pair, and what the lights mean. |
| [Quick-start pictograms](quick-start-pictograms.html) | Wordless, IKEA-style instructions for pairing, playing, recording and changing track. Drawn to the board's real proportions. Prints on one sheet. |
| [FAQ](FAQ.md) | Including the honest limitations. |

## Build and manufacture

| | |
|---|---|
| [Bill of materials](cassette-recorder-bom-v4.md) | **Generated** from the schematic with live JLCPCB stock and pricing. Rebuild with `python hardware/scripts/gen_bom.py`; never edit by hand. |
| [BOM as CSV](cassette-recorder-bom-v4.csv) | JLCPCB assembly format, fitted parts only. |
| [Case-fit test sheet](case-fit-test-1to1.pdf) | Print at 100 %, cut it out, put it in a case. |

## Artwork and packaging

| | |
|---|---|
| [B-side silkscreen brief](b-side-silkscreen-brief.html) | The design brief for the back face, with dimensioned keepouts, process limits and the delivery spec. Pairs with the 1:1 [template](../hardware/mixxtape-back-silkscreen-template.svg) (`gen_silk_template.py`). |
| [Cassette insert template](jcard-template-1to1.pdf) | Double-sided J-card for the case, 1:1, two pages for duplex printing. Editable sides: [1](jcard-template-side1.svg), [2](jcard-template-side2.svg). Rebuild with `gen_jcard_template.py`. |

## Design record

| | |
|---|---|
| [Firmware plan](firmware-plan.md) | Architecture, milestones, and what is deliberately out of scope. |
| [Storage budget](storage-budget.md) | Why the tape stores ADPCM rather than SBC, and how the numbers work out. |
| [Line-out option](line-out-option.md) | **Rejected.** Kept as a decision record: what a 3.5 mm jack would have cost, with verified parts and pricing, should it ever come back. |
| [Prior art](prior-art.md) | Cassette-form-factor projects, and what nobody has shipped. |
| [Project write-up](cassette-project-writeup.md) | The pitch and the design reasoning, long form. |

## Generated files

Several documents here are produced by scripts in `hardware/scripts/` and will
be overwritten. Edit the generator, not the output:

| Output | Generator |
|---|---|
| `cassette-recorder-bom-v4.md` / `.csv` | `gen_bom.py` |
| `jcard-template-side{1,2}.svg`, `jcard-template-1to1.pdf` | `gen_jcard_template.py` |
| `../hardware/mixxtape-back-silkscreen-template.svg` | `gen_silk_template.py` |
| `../hardware/mixxtape-mechanical.kicad_pcb` | `gen_layout.py` |
