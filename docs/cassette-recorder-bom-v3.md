# Cassette Mixtape Recorder — BOM v3

Single-sided SMT, 2-layer, 100.0 × 63.5 mm. USB-C only (battery circuit DNP).
Mono MEMS mic → SBC encode at record → A2DP playback. 3 × 4-minute tracks.
**Target run: 20 boards.** Ships in a thrifted Norelco cassette case.

Confidence: **[V]** price from a current listing · **[C]** part confirmed in JLC
library, price estimated · **[E]** estimated from category norms

---

## 1. Core

| Ref | Part | LCSC | Pkg | Qty | Unit | Ext | Conf |
|---|---|---|---|---|---|---|---|
| U1 | ESP32-WROOM-32E-N4 | C701341 | SMD 18×25.5 | 1 | ~$2.90 | 2.90 | [C] |
| U2 | MSM261S4030H0R | C2840615 | 3.76×2.95 | 1 | ~$0.75 | 0.75 | [C] |
| U3 | W25Q128JVSIQ | C97521 | SOIC-8 | 1 | **$1.65** | 1.65 | [V] |
| U4 | ME6211C33M5G | C82942 | SOT-23-5 | 1 | ~$0.06 | 0.06 | [E] |
| U6 | CH340N | C506813 | SOP-8 | 1 | ~$0.45 | 0.45 | [E] |
| U7 | USBLC6-2SC6 | C7519 | SOT-23-6 | 1 | ~$0.10 | 0.10 | [E] |
| U5 | TP4056 *(DNP)* | C16581 | SOP-8 | 1 | — | — | [E] |

N16 reference is C701343 at **$3.5161** [V]. Verify the N4 delta; if under ~$0.40
take the N16 for firmware slack.

**LDO sizing changed.** 29 WS2812s at 20% brightness draw ~150 mA on top of the
ESP32's ~140 mA average and 240 mA TX bursts. ME6211 is a 500 mA part — that's
now uncomfortably close. **Move to a 1 A LDO** (e.g. AMS1117-3.3 is fine here
since VBUS is 5 V, not battery — dropout is no longer a constraint once the
battery is DNP). Re-check this before layout.

---

## 2. Indicators — 29 × WS2812B-2020 RGB

| Ref | Function | Count | Ext | Conf |
|---|---|---|---|---|
| D1–D12 | Left reel ring | 12 | 0.72 | [E] |
| D13–D24 | Right reel ring | 12 | 0.72 | [E] |
| D25–D27 | Track 1/2/3 | 3 | 0.18 | [E] |
| D28 | REC | 1 | 0.06 | [E] |
| D29 | BT status | 1 | 0.06 | [E] |
| C20–C27 | 100 nF, 1 per ~4 LEDs | 8 | 0.04 | [E] |

**2020 package** (2.0 × 2.0 mm) — a 12-LED ring on an 18 mm circle gives 4.7 mm
pitch, which fits inside a cassette reel window with room to spare.

Single data line off one GPIO, driven by the ESP32's RMT peripheral in hardware
— no CPU cost, no timing conflict with the I²S mic DMA.

**Brightness cap is mandatory.** All 29 at full white is ~1.7 A. Clamp global
brightness to ~25% in firmware and never allow all-white; you're on a USB-C
budget, not a bench supply.

**Quiescent draw stopped mattering** when the battery went DNP. ~25 mA of idle
WS2812 current is irrelevant on USB, which is what makes RGB affordable here.
If anyone populates the battery option, that's ~15% of their runtime gone.

**What RGB buys, in colour:**

| State | Reels | Meaning |
|---|---|---|
| Amber sweep | Searching for a sink | |
| Green lock | Connected | |
| White transfer | Playing — left empties, right fills | |
| Red pulse | Recording, brightness = VU level | |
| Slow reverse, dim | Background slot erase | |
| Violet | Dubbing to another board | |

---

## 3. Controls — 4 buttons + 1 tab

| Ref | Function | LCSC | Qty | Ext |
|---|---|---|---|---|
| SW1 | REC (hold) — also GPIO0 boot | C318884 | 1 | 0.05 |
| SW2 | PLAY / pause · long = sleep | C318884 | 1 | 0.05 |
| SW3 | TRACK cycle 1→2→3 | C318884 | 1 | 0.05 |
| SW4 | MODE — 2 s pair · 5 s dub · 10 s dev WiFi | C318884 | 1 | 0.05 |
| TAB1 | **Write-protect tab** | — | 1 | $0 |

### Write-protect tab

Break-off PCB tab on the top edge, held by mouse-bites, carrying a trace from
3V3 to a GPIO with a pull-down. Tab intact → pin high → recording allowed. Tab
snapped → pin floats low → **firmware refuses to record, permanently.**

One tab locks the whole tape, matching real cassette semantics and the gift
ritual: you record it, you hand it over, they snap the tab. Three tabs (one per
track) is the alternative — more useful, less faithful. Recommend one.

Route the trace so the snap line crosses it cleanly, and keep the mouse-bite
nibs away from the case ribs.

---

## 4. Hacker header — 10 castellated pads

Bottom edge, half-holes, silkscreened pin names.

| Pad | Signal |
|---|---|
| 1–2 | 3V3, GND |
| 3–5 | I²S BCLK, WS, DIN |
| 6–7 | UART TX, RX |
| 8 | EN |
| 9–10 | Spare GPIO ×2 |

**Cost note:** JLCPCB charges an extra fee for castellated / half-hole edges.
Small, but confirm it in the quote — it's the one board feature here that isn't
free.

---

## 5. Line-in — footprint only, DNP

3.5 mm SMD jack (PJ-320D or similar) + AC coupling and divider into ADC1, read
via the ESP32's I²S-ADC mode. All DNP.

Caveats, so future-you isn't surprised: the ESP32 ADC is 12-bit with ~9 effective
bits and is noisy — this is a lo-fi path, not a line input in the hi-fi sense.
And it can't run concurrently with the digital I²S mic; firmware picks one source
at boot. Good enough for a "studio edition" variant without a respin.

---

## 6. Connector, passives, mechanical

| Ref | Part | LCSC | Qty | Ext |
|---|---|---|---|---|
| J1 | USB-C 16P SMD | C165948 | 1 | 0.12 |
| R1,R2 | 5.1 kΩ CC pulldowns | C25905 | 2 | 0.01 |
| C1,C2 | 22 µF 6.3 V | C45783 | 2 | 0.04 |
| C3–C14 | 100 nF / 10 µF assorted | C1525/C15525 | 12 | 0.07 |
| Q1,Q2 | S8050 auto-reset pair | C2150 | 2 | 0.02 |
| R10–R22 | 10 kΩ pullups, tab pulldown | C25804 | 13 | 0.02 |
| — | **Lanyard hole**, 3 mm | — | 1 | $0 |
| J2/BT1 | JST-PH + LiPo *(DNP)* | — | — | — |

**Lanyard hole placement:** corner, clear of the WROOM antenna keepout and clear
of the castellated edge. Check it doesn't foul the case hinge.

---

## 7. Cost at qty 20

| | Per board | Run of 20 |
|---|---|---|
| Components | ~$8.50 | ~$170 |
| PCB (2-layer, matte white, slots, castellated) | ~$1.50 | ~$30 |
| Assembly | ~$3.25 | ~$65 |
| **Total** | **~$13.25** | **~$265** |

Assembly at this quantity is dominated by ~7 extended-part reel fees (~$3 each,
one-time per order), not placement count. The 29 WS2812s add ~$1.44 in parts —
the flash chip still costs more than every LED combined.

Thrifted cases: ~$0.50 each, and they make the object.

---

## 8. Case fit — verify before finalizing the outline

A standard Norelco case interior runs roughly 103 × 66 mm, so a 100 × 63.5 mm
board clears. But: **get a real case in hand and check it against a paper
mock-up** before committing. Specifically —

- Internal ribs and hub-clamp ridges vary between case brands
- The castellated edge pads must not sit where a rib presses
- The lanyard hole must clear the hinge
- Thrifted cases are not dimensionally consistent; buy several brands

---

## 9. Open items

- N4 vs N16 price delta [unverified]
- LDO uprate to 1 A [decision pending]
- Castellated edge surcharge at JLC [unverified]
- Tab count: 1 (recommended) vs 3
- Mic mechanical isolation — needs a prototype, not a decision
