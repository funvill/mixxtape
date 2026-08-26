# Bill of materials — v4

> Generated from `hardware/mixxtape.kicad_sch` by
> `hardware/scripts/gen_bom.py`, with prices and stock read live from
> JLCPCB at the time shown. **Do not hand-edit** — change the
> schematic and re-run, so the two can never disagree again (v3 did,
> in seven places).

Quantities are for a run of **20 boards**, and unit prices are
taken from the tier covering the whole order rather than a single
piece.

---

## Fitted parts

| Ref | Value | LCSC | Package | /bd | Order qty | Stock | Lib | Unit | Ext |
|---|---|---|---|---|---|---|---|---|---|
| C1, C2 | 22uF | C45783 | C0805 | 2 | 40 | 4,718,162 | basic | $0.3251 | $13.00 |
| C5 | 10uF | C15525 | C0402 | 1 | 20 | 10,659,536 | basic | $0.0255 | $0.51 |
| C6–C27 (13) | 100nF | C1525 | C0402 | 13 | 260 | 37,557,517 | basic | $0.0055 | $1.43 |
| D1–D29 (29) | WS2812B-2020 | C5349955 | LED-SMD_4P-L2.0-W2.0-TL_WS2812B-2020 | 29 | 580 | 180,034 | ext | $0.0603 | $34.97 |
| J1 | USB-C-16P | C165948 | USB-C_SMD-TYPE-C-31-M-12_1 | 1 | 20 | 390,037 | ext | $0.1855 | $3.71 |
| J5 | PROG-JIG | — |  | 1 | 20 | — | ? | — | — |
| R1, R2 | 5.1k | C25905 | R0402 | 2 | 40 | 8,910,424 | basic | $0.0043 | $0.17 |
| R3, R4, R5, R6, R7, R10, R11 | 10k | C25804 | R0603 | 7 | 140 | 26,581,544 | basic | $0.0053 | $0.74 |
| SW1, SW2, SW3, SW4 | REC / PLAY / TRACK / MODE | C318884 | SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5 | 4 | 80 | 978,596 | basic | $0.0205 | $1.64 |
| TAB1 | WRITE-PROTECT | — |  | 1 | 20 | — | ? | — | — |
| U1 | ESP32-WROOM-32E-N4 | C701341 | WIFI-SMD_ESP32-WROOM-32E | 1 | 20 | 24,224 | ext | $3.2528 | $65.06 |
| U2 | MSM261S4030H0R | C2840615 | MIC-SMD_8P-L4.0-W3.0-P1.00-BR | 1 | 20 | 0 ⚠ | ext | $1.6143 | $32.29 |
| U3 | W25Q128JVSIQ | C97521 | SOIC-8_L5.3-W5.3-P1.27-LS8.0-BL | 1 | 20 | 70,918 | basic | $2.2557 | $45.11 |
| U4 | AMS1117-3.3 | C6186 | SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR | 1 | 20 | 1,174,771 | basic | $0.2019 | $4.04 |
| U8 | SN74AHCT1G125 | C7484 | SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR | 1 | 20 | 27,012 | ext | $0.1264 | $2.53 |

⚠ marks a part whose current JLCPCB stock is below what this run
needs — check before ordering, or pick the second source.

## Not fitted (DNP)

Footprints on the board, deliberately unpopulated. They cost nothing
on the default build and let a variant be built without a respin.

| Ref | Value | LCSC | Package | Purpose |
|---|---|---|---|---|
| J2 | JST-PH-2 (DNP) | — |  | LiPo connector |
| J4 | microSD (DNP) | C91145 | TF-SMD_TF-01A | microSD — the "studio edition" option |
| R8 | 10k (DNP) | C25804 | R0603 | Charge-current programming resistor |
| R9 | 10k (DNP) | C25804 | R0603 | microSD chip-select pull-up |
| U5 | TP4056 (DNP) | C16581 | ESOP-8_L4.9-W3.9-P1.27-LS6.0-BL-EP | Battery charger — USB-C-only is a locked decision |

## Cost

| | Per board | Run of 20 |
|---|---|---|
| Components | $10.26 | $205.20 |
| Extended-part setup fees (5 × $3.00) | $0.75 | $15.00 |
| **Parts subtotal** | **$11.01** | **$220.20** |

66 placements per board across 15 distinct fitted
parts, 5 of them extended.

PCB fabrication and assembly labour are quoted separately and are not
included above — at this quantity the setup fees above dominate the
component cost, so adding a distinct extended part is expensive and
adding more of one you already use is nearly free.

## Supply-chain rule: every fitted part needs 5,000+ in stock

- `C2840615` MSM261S4030H0R — 0 in stock, needs 20 — accepted exception

  No digital MEMS microphone at JLCPCB meets the 5,000 rule — the whole category is thin, and the best stocked part in it is only ~3.5k. The one microphone that does clear the bar (ZTS6216, C481302, 29k) is **analog**: it needs a DC-blocking cap and a gain stage into the ESP32's ADC, which the brief already describes as a lo-fi path with ~9 effective bits. Taking it would meet the rule and wreck the product's only input. Holding a digital part at ~3.5k is the lesser risk: the run needs 20 pieces, so that is 174x coverage, and three alternates are qualified below.

## Needs attention before ordering

**Insufficient stock for this run:**

- `C2840615` MSM261S4030H0R — needs 20, JLC shows 0

**C1, C2 — 22uF (`C45783`)**

Basic part, so no setup fee, but $0.33 each is dear for a 22 uF 0805. C5674 is $0.11 but extended: 40 pieces costs $13.00 as-is versus $4.47 + $3.00 fee. Worth ~$5.50 if you want it.

**D1, D2, D3… — WS2812B-2020 (`C5349955`)**

Fitted in place of the genuine Worldsemi WS2812B-2020 (C965555), which is end-of-life at JLC with single-digit stock. Same protocol and land pattern; Steven has used these before.

**U1 — ESP32-WROOM-32E-N4 (`C701341`)**

N4 (4 MB). N16 is C701343 — take it if the delta is small, though the firmware fits in 4 MB comfortably. Stock is the thinnest of any fitted part after the microphone, but this module is a locked decision: it is the only ESP32 with Classic Bluetooth, and without that there is no A2DP and no product.

**U2 — MSM261S4030H0R (`C2840615`)**

**BLOCKER — zero stock, and the board cannot be built without a microphone.** Not swapped in the schematic yet on purpose: the footprint has to be confirmed first. Best candidate is MSM261DHT006 (C51928215, ~3.5k, $0.45). Same family, a third of the price, best-stocked digital MEMS mic at JLCPCB. **Datasheet check required first:** the suffix encodes package size and port direction, and this design needs a **bottom-ported** part because there is an acoustic hole through the board beneath it. If it is top-ported, take one of the alternates. Qualified alternates, all digital: MP34DT06JTR (C503097, ~3.8k, $1.80, ST, PDM) and ICS-43434 (C5656610, ~3.7k, $3.33, TDK, I2S, very well documented). PDM parts are fine — the ESP32's I2S peripheral reads PDM directly — but the firmware would initialise in PDM RX mode instead of standard I2S.

**U3 — W25Q128JVSIQ (`C97521`)**

$2.26 here against $1.65 in BOM v3 — the NOR price rise the brief warned about is real. Second source C113767.

**U8 — SN74AHCT1G125 (`C7484`)**

TI part, 26k stock. UMW second-source C20617903 has 40k and costs a third as much if stock ever tightens. Both clear the rule easily.

## Second sources

| Part | Primary | Alternate | Note |
|---|---|---|---|
| Audio flash | C97521 | C113767 | Same W25Q128JV, different vendor |
| ESP32 module | C701341 (N4) | C701343 (N16) | 16 MB module if the delta is small; firmware fits in 4 MB |
| Reel LEDs | C5349955 | C5349956 (SK6812 protocol) | Genuine WS2812B-2020 (C965555) is end-of-life at JLC |
| Level shifter | C7484 | C20617900 (UMW) | Cheaper, less known |
