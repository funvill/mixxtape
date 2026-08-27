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
| C6–C27 (12) | 100nF | C1525 | C0402 | 12 | 240 | 37,557,517 | basic | $0.0055 | $1.32 |
| C9 | 1uF | C29266 | C0402 | 1 | 20 | 3,950,406 | ext | $0.0081 | $0.16 |
| D1–D29 (29) | WS2812B-2020 | C5349955 | LED-SMD_4P-L2.0-W2.0-TL_WS2812B-2020 | 29 | 580 | 180,034 | ext | $0.0603 | $34.97 |
| J1 | USB-C-16P | C165948 | USB-C_SMD-TYPE-C-31-M-12_1 | 1 | 20 | 390,037 | ext | $0.1855 | $3.71 |
| J5 | PROG-JIG | — | PROG_JIG_6PAD | 1 | 20 | — | ? | — | — |
| R1, R2 | 5.1k | C25905 | R0402 | 2 | 40 | 8,910,424 | basic | $0.0043 | $0.17 |
| R3, R4, R5, R6, R7, R10, R11 | 10k | C25804 | R0603 | 7 | 140 | 26,581,544 | basic | $0.0053 | $0.74 |
| R12 | 100R | C25076 | R0402 | 1 | 20 | 4,589,188 | basic | $0.0078 | $0.16 |
| SW1, SW2, SW3, SW4 | PLAY / TRACK / MODE / REC | C318884 | SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5 | 4 | 80 | 978,596 | basic | $0.0205 | $1.64 |
| TAB1 | WRITE-PROTECT | — | TAB_BREAKOFF_LINK | 1 | 20 | — | ? | — | — |
| U1 | ESP32-WROOM-32E-N4 | C701341 | WIFI-SMD_ESP32-WROOM-32E | 1 | 20 | 24,224 | ext | $3.2528 | $65.06 |
| U2 | MSM261DHT006 | C51928215 | LGA-8_L4.0-W3.0-R_WMM7040DT0 | 1 | 20 | 3,485 | ext | $0.4500 | $9.00 |
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
| J4 | microSD (DNP) | C91145 | TF-SMD_TF-01A | microSD — the "studio edition" option |
| R9 | 10k (DNP) | C25804 | R0603 | microSD chip-select pull-up |

## Cost

| | Per board | Run of 20 |
|---|---|---|
| Components | $9.11 | $182.13 |
| Extended-part setup fees (6 × $3.00) | $0.90 | $18.00 |
| **Parts subtotal** | **$10.01** | **$200.13** |

67 placements per board across 17 distinct fitted
parts, 6 of them extended.

PCB fabrication and assembly labour are quoted separately and are not
included above — at this quantity the setup fees above dominate the
component cost, so adding a distinct extended part is expensive and
adding more of one you already use is nearly free.

## Supply-chain rule: every fitted part needs 5,000+ in stock

- `C51928215` MSM261DHT006 — 3,485 in stock, needs 20 — accepted exception

  No digital MEMS microphone at JLCPCB meets the 5,000 rule - the whole category is thin, and the best-stocked digital part is only ~3.5k. The one microphone that does clear the bar (ZTS6216, C481302, 29k) is ANALOG: it needs a DC-blocking cap and a gain stage into the ESP32's ADC, which the brief already describes as a lo-fi path with ~9 effective bits. Meeting the rule there would wreck the product's only input. A digital part at ~3.5k is the lesser risk: the run needs 20 pieces, so that is 174x coverage, and three alternates are qualified.

## Needs attention before ordering

**C1, C2 — 22uF (`C45783`)**

Basic part, so no setup fee, but $0.33 each is dear for a 22 uF 0805. C5674 is $0.11 but extended: 40 pieces costs $13.00 as-is versus $4.47 + $3.00 fee. Worth ~$5.50 if you want it.

**D1, D2, D3… — WS2812B-2020 (`C5349955`)**

Fitted in place of the genuine Worldsemi WS2812B-2020 (C965555), which is end-of-life at JLC with single-digit stock. Same protocol and land pattern; Steven has used these before.

**U1 — ESP32-WROOM-32E-N4 (`C701341`)**

N4 (4 MB). N16 is C701343 — take it if the delta is small, though the firmware fits in 4 MB comfortably. Stock is the thinnest of any fitted part after the microphone, but this module is a locked decision: it is the only ESP32 with Classic Bluetooth, and without that there is no A2DP and no product.

**U2 — MSM261DHT006 (`C51928215`)**

Replaces MSM261S4030H0R (C2840615), which went to zero stock. Acoustically an equal swap, not a downgrade: identical -26 dBFS sensitivity, same 1.6-3.6 V range, same 4x3 mm LGA-8 outline, 1 dB less SNR (60 vs 61 dB) and 2 dB better acoustic overload (122 vs 120 dB SPL), which is the more useful end for a recorder that will meet a door slam. Cheaper too, $0.45 against $1.61, and lower current. Its one real cost is the interface: PDM rather than I2S, so firmware runs the ESP32's I2S peripheral in PDM RX mode - supported in hardware, and it frees GPIO26 because PDM needs two wires instead of three. Note the pinout is COMPLETELY different from the old part (pin 1 is VDD, not GND; pins 5-8 are all GND), so this was a schematic change, not a part-number swap. Like its predecessor it is TOP-ported: no hole through the board, keep its top face clear. Upgrade path if prototypes show the mic is the weak link: ICS-43434 (C5656610, ~3.7k, $3.33) keeps I2S and gains 5 dB of SNR, at 7x the price and a different 3.5x2.65 mm footprint.

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
