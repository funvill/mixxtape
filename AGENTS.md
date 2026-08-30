# AGENTS.md — working on Mixxtape

Guidance for AI agents (and new human contributors) working in this repository.

## Read first

The authoritative spec is
[docs/cassette-recorder-agent-brief.md](docs/cassette-recorder-agent-brief.md).
Read it in full before doing any design, firmware, or BOM work. Decisions
marked **[LOCKED]** there have rationale behind them and must not be changed —
if a change looks compelling, raise it with Steven rather than acting.

The BOM of record is
[docs/cassette-recorder-bom-v4.md](docs/cassette-recorder-bom-v4.md), and it is
**generated** — rebuild it with `python hardware/scripts/gen_bom.py` rather than
editing it by hand. The hand-maintained version it replaced had drifted from the
board in seven places before anyone noticed.

## The five traps

These are the mistakes an agent is most likely to make. Each one looks like an
improvement and is not:

1. **Do not substitute a newer ESP32.** The S3/C3/C6/C5/H2 are all BLE-only.
   Classic Bluetooth — and therefore A2DP — exists only on the original Xtensa
   ESP32. No A2DP means it cannot talk to ordinary earbuds, which is the entire
   product.
2. **Do not switch to an nRF part.** Nordic has never shipped Classic Bluetooth.
3. **Do not add a Seeed XIAO as the production MCU.** No XIAO has Classic
   Bluetooth.
4. **Do not increase flash capacity "for headroom."** NOR flash prices roughly
   doubled in 1H2026; track length was set to 4 minutes specifically to fit
   16 MB.
5. **Do not add a battery to the default build.** USB-C only is a locked
   decision (longevity of the object, UN38.3 shipping paperwork).

## What agents may decide vs. what needs Steven

**Agent may decide:** LDO part selection (1 A), WS2812B-2020 LCSC part choice,
reel ring diameter and LED pitch, LED decoupling, hacker-header pinout
refinement, firmware task structure and RTOS priorities, ADPCM variant.

**Needs Steven:** tab count (one vs three), N4 vs N16 module choice, silkscreen
artwork (on a bare-PCB product the art *is* the industrial design), project
name, line-in jack footprint choice, and anything touching tape-head playback
in a real deck (patent risk — do not design toward it).

## Repository layout

- `docs/` — design documents
- `hardware/` — KiCad 9 project (`mixxtape.kicad_pro` / `mixxtape.kicad_sch`)
  - `hardware/parts/` — project libraries: `mixxtape_parts.*` fetched from
    JLCPCB/EasyEDA, `mixxtape_local.kicad_sym` hand-authored (tab, header, JST)
  - `hardware/parts/fetch_parts.py` — fetches symbol+footprint+3D for an LCSC
    ID into the project lib (`python fetch_parts.py C12345`). Uses curl_cffi
    because EasyEDA's API rejects plain HTTP clients; bursts of ~15 requests
    trigger a temporary IP block — space out large fetches.
  - `hardware/scripts/gen_schematic.py` — one-shot generator that produced the
    initial schematic. **Do not re-run once the schematic has been hand-edited
    in KiCad; it overwrites the file.**
- `firmware/` — ESP32 firmware (empty; ESP-IDF expected, since A2DP source and
  SBC encode need IDF-level APIs)

## Hardware conventions

- **USB-C is power only** — no USB data path, no USB-UART bridge on the
  board. Programming happens through the J5 pogo pads via a jig. Do not
  re-add a CH340/CP2102 or castellated pads; Steven removed them
  deliberately (2026-08-25).
- Firmware work follows `docs/firmware-plan.md`; where it and the agent
  brief's GPIO map differ, the firmware plan wins.

- Every part carries an `LCSC` field with its JLC part number; keep it correct
  — it drives JLCPCB assembly BOM export.
- The reel/status LEDs are XINGLIGHT **XL-1615RGBC-2812B (C5349954)**, footprint
  `LED-SMD_4P-L1.6-W1.5_XL-1615RGBC-2812B`. Its pad numbering is the
  MANUFACTURER'S — **1=GND 2=DIN 3=VDD 4=DOUT** — not the 1=DO 2=GND 3=DI 4=VDD
  order Worldsemi uses. The schematic symbol was renumbered to match. **Never
  renumber one without the other.**
- **A footprint whose `LCSC Part` differs from the BOM line is a defect, not an
  annotation.** This board previously carried a WS2812B-2020 footprint drawn for
  the genuine Worldsemi C965555 while the BOM shipped the XINGLIGHT C5349955,
  whose pad 1 is on the diagonally opposite corner. All 29 LEDs would have been
  reverse-powered across the 5 V rail. **ERC, DRC, the netlist and the BOM/CPL
  cross-check all passed** — every one of them trusts the footprint. When a part
  is substituted, verify the LAND PATTERN against the new manufacturer's data,
  not just the protocol. "Same protocol" says nothing about pad 1.
- ERC must stay at 0 errors: run
  `kicad-cli sch erc --severity-error --exit-code-violations hardware/mixxtape.kicad_sch`.

## Conventions

- Prices in the docs came from search-index snippets, not live quotes.
  **Re-quote in the JLC BOM tool before ordering** — NOR pricing moves fast.
- Follow the phase order in the brief (§9): verify open items, then mechanical
  outline, then schematic, then firmware bring-up on a devkit, then layout,
  then a 3–5 board prototype run. Layout before firmware bring-up would be a
  mistake — the timing unknowns could change the hardware.
- Aesthetics are a first-class constraint. This is a bare-PCB object in the
  Mixtape Alpha tradition, not a functional-first board that happens to be
  cassette-shaped.
- Keep `CHANGELOG.md` updated when design decisions change or documents are
  added.
