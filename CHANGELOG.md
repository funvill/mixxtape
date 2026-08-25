# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project is pre-release; entries are dated rather than versioned until the
first prototype run.

## [Unreleased]

### Added (2026-08-25, KiCad project setup)

- KiCad 9 project in `hardware/` (`mixxtape.kicad_pro`, `mixxtape.kicad_sch`,
  lib tables) with first-pass schematic: USB-C + ESD + CH340N, AMS1117 power,
  ESP32-WROOM-32E wired per the brief's GPIO map, I²S mic, W25Q128 flash,
  4 buttons, write-protect tab, castellated header, 29-LED WS2812 chain,
  and the DNP battery section. ERC passes with 0 errors.
- Symbols/footprints/3D models for all BOM parts fetched from JLCPCB/EasyEDA
  into `hardware/parts/mixxtape_parts.*` via `hardware/parts/fetch_parts.py`
  (uses curl_cffi Chrome TLS impersonation — EasyEDA's API blocks plain
  clients, and rate-limits bursts).
- Hand-authored `mixxtape_local.kicad_sym` (break-off tab, castellated
  header, JST connector symbols).
- `hardware/scripts/gen_schematic.py` — one-shot generator that produced the
  initial schematic (do not re-run after hand-editing in KiCad).

### Changed / findings (2026-08-25)

- **WS2812B-2020 substituted:** genuine Worldsemi part (C965555) is EOL at
  JLC (stock 3). Fitted part is XINGLIGHT XL-2020RGBC-2812B (**C5349955**,
  ~$0.06 @ 500, 180k stock) — same protocol and land pattern; schematic uses
  the WS2812B-2020 symbol/footprint with LCSC field C5349955.
- **CH340N has no DTR** (RTS# only) — the BOM's S8050 auto-reset pair needs
  both signals. Schematic fits a cap-coupled RTS reset instead; Q1/Q2 omitted
  until Steven decides (alternative: CH340C/CH343). Flagged on the schematic.
- LDO uprated to 1 A as the BOM required: AMS1117-3.3 (C6186), SOT-223.
- **LED level shifter added (Steven, 2026-08-25):** XL-2020RGBC-2812B
  confirmed working from Steven's past projects, including at 3.3 V data —
  but a TI SN74AHCT1G125 (**C7484**, SOT-23-5, ~$0.08) is fitted anyway as
  U8: ESP32 IO27 (`LED_DATA`) → buffer → `LED_DATA_5V` → D1, OE̅ tied to GND,
  powered from VBUS.

### Added

- Project write-up covering concept, rationale, and open design questions
  (`docs/cassette-project-writeup.md`)
- Agent handoff brief with locked decisions, GPIO map, behaviour spec, and
  suggested phase order (`docs/cassette-recorder-agent-brief.md`)
- Bill of materials v3 with LCSC part numbers and qty-20 costing
  (`docs/cassette-recorder-bom-v3.md`)
- Empty `firmware/` and `hardware/` directory placeholders
- README, changelog, and AGENTS.md

### Design decisions (as of 2026-08-25)

- MCU locked to ESP32-WROOM-32E for Classic Bluetooth / A2DP source
- USB-C only power; battery circuit on board but unpopulated
- 29 × WS2812B-2020 indicators replacing earlier charlieplex approach
- LDO to be uprated from 500 mA to 1 A (part selection pending)
- Write-protect tab count (one vs three) still open
