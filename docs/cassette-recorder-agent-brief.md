# Cassette Mixtape Recorder — Agent Handoff Brief

**Owner:** Steven
**Status:** Design locked, pre-layout. No schematic drawn yet.
**Run size:** 20 boards, one-off. Not a production program.

---

## 0. Read this first

Several decisions in this document look wrong at first glance and are not.
Each one is marked **[LOCKED]** with its rationale. Do not "optimize" a locked
decision — if a change looks compelling, raise it with Steven rather than
acting. Section 2 lists the specific wrong turns an agent is most likely to
take.

Everything not marked LOCKED is open, and §7 splits it into what you may
decide yourself versus what needs Steven.

---

## 1. What this is

A 2-layer PCB in the exact form factor of a compact cassette (100.0 × 63.5 mm),
which slides into a real Norelco cassette case. The bare board *is* the object —
no enclosure, no bezel.

An onboard MEMS microphone records three tracks. Hold REC to record, release to
stop. Then pair to any Bluetooth earbuds or speaker and press PLAY.

Reel windows are routed out of the board with 12 RGB LEDs ringing each one. As a
track plays, the left ring empties and the right ring fills, mimicking tape
transferring between reels.

**Design ethos:** this descends from Open Music Labs' Mixtape Alpha (2012, with
Jie Qi / MIT Media Lab High-Low Tech) — bare PCB as a thing you touch, trace
routing as visual design, aesthetics as a first-class constraint. Not a
functional-first board that happens to be cassette-shaped.

**Prior art to read before starting:**
- Mixtape Alpha wiki: `wiki.openmusiclabs.com/wiki/MixtapeAlpha` — Eagle files,
  gerbers, BOM, and a candid "limitations of the design" section. Pull
  `mixtape2.brd` for the cassette outline rather than redrawing it.
- Mixxtape (Mixxim, `paulthings.com`) — the commercial cassette-shaped player.
  Player only, no recording. This is the thing we are *not* building.

---

## 2. Traps — the five mistakes most likely to be made here

1. **Do not substitute a newer ESP32.** The ESP32-S3, C3, C6, C5, and H2 are all
   BLE-only. Classic Bluetooth — and therefore A2DP — exists only on the
   original Xtensa LX6 ESP32. No A2DP means it cannot talk to ordinary earbuds,
   which is the entire product. This is the single most important constraint in
   the document.
2. **Do not switch to an nRF part.** Nordic has never shipped Classic Bluetooth.
   Same failure.
3. **Do not add a Seeed XIAO.** No XIAO in the lineup has Classic Bluetooth. The
   XIAO footprint may still be useful as a *secondary* prototyping footprint, but
   it cannot be the production MCU.
4. **Do not increase flash capacity "for headroom."** NOR flash prices rose
   100–120% in 1H2026, with high-density parts hit hardest. Headroom is the most
   expensive thing on this BOM. Track length was set to 4 minutes specifically to
   fit 16 MB.
5. **Do not add a battery to the default build.** See §3, Power.

---

## 3. Locked decisions

| Decision | Value | Rationale |
|---|---|---|
| **MCU** | ESP32-WROOM-32E, module not bare chip | **[LOCKED]** Classic BT for A2DP source. Module carries FCC/IC/CE modular approval; a bare chip would put full intentional-radiator testing on us. |
| **Flash size** | 16 MB external | **[LOCKED]** 3 × 4 min mono SBC ≈ 11.5 MB. |
| **Separate audio flash** | Yes, not module flash | **[LOCKED]** No battery means no graceful-shutdown window. A yank mid-write must not be able to corrupt the bootloader. Also avoids XIP cache stalls during record. |
| **Audio path** | Mono, encode to **block-framed IMA ADPCM** at record time | **[LOCKED]** Playback must be read-frames-and-transmit. No real-time decode, no resample, no DSP during streaming. |
| **Sample rate** | 44.1 kHz | **[LOCKED]** Matches A2DP negotiation, eliminates resampling entirely. |
| **Power** | USB-C only; no battery or charger on the board | **[LOCKED]** A LiPo in a drawer for years is dead and possibly hazardous. The object's premise is that it still works when rediscovered. Also avoids UN38.3 shipping paperwork. |
| **Controls** | 4 tactile buttons + 1 break-off write-protect tab | **[LOCKED]** Physical buttons, not cap touch — explicitly reversed after consideration. |
| **Indicators** | 29 × WS2812B-2020 RGB, single data line | **[LOCKED]** Replaced charlieplexing. Quiescent draw is irrelevant on USB. |
| **Recording UX** | Hold REC to record; always overwrites current slot from zero | **[LOCKED]** No delete function needed — recording *is* erasing, same as tape. |
| **Playback UX** | PLAY plays all three tracks in order | **[LOCKED]** It's a tape side, not three voice memos. |
| **Reconnect** | Auto-reconnect to last bond on power-up | **[LOCKED]** |
| **Storage layout** | 3 fixed 5 MB slots + header region | **[LOCKED]** No dynamic allocation, no compaction. |
| **Solder mask** | Matte white | **[LOCKED]** Glossy mask rejects permanent marker. Users sharpie the label. |
| **Sides** | One side, three tracks. No accelerometer. | **[LOCKED]** Rejected on BOM cost. |
| **Case** | Thrifted Norelco cassette cases | **[LOCKED]** |

---

## 4. Bill of materials

The authoritative BOM is **generated from the schematic** — see
[`cassette-recorder-bom-v4.md`](cassette-recorder-bom-v4.md), rebuilt with
`hardware/scripts/gen_bom.py`. It carries live JLCPCB stock and pricing and
cannot drift away from the design. Summary:

| Ref | Part | JLCPCB | Note |
|---|---|---|---|
| U1 | ESP32-WROOM-32E-N4 | [C701341](https://jlcpcb.com/partdetail/C701341) | **MSL 3, X-ray required.** N16 is C701343 |
| U2 | MSM261DHT006 | [C51928215](https://jlcpcb.com/partdetail/C51928215) | **PDM** MEMS mic, top-ported. Replaced MSM261S4030H0R (C2840615) at zero stock — completely different pinout, so a schematic change rather than a swap |
| U3 | GD25Q128ESIG | [C2758105](https://jlcpcb.com/partdetail/C2758105) | Second sources: C97521 (Winbond W25Q128JVSIQ), C113767 |
| U4 | AMS1117-3.3 | [C6186](https://jlcpcb.com/partdetail/C6186) | SOT-223. 22 µF on the output for stability, per its datasheet |
| U8 | SN74AHCT1G125 | [C7484](https://jlcpcb.com/partdetail/C7484) | 3.3 V → 5 V buffer for the LED data line |
| D1–D29 | XL-2020RGBC-2812B | [C5349955](https://jlcpcb.com/partdetail/C5349955) | 12 + 12 reel rings, 3 track, REC, BT |
| SW1–4 | TS-1187A-B-A-B | [C318884](https://jlcpcb.com/partdetail/C318884) | |
| J1 | USB-C 16P | [C165948](https://jlcpcb.com/partdetail/C165948) | + 2 × 5.1 kΩ CC pulldowns — mandatory |
| R12 | 100 Ω | [C25076](https://jlcpcb.com/partdetail/C25076) | Microphone supply isolation, per the MSM261DHT006 datasheet |
| C9 | 1 µF | [C29266](https://jlcpcb.com/partdetail/C29266) | The ESP32's EN delay. The datasheet asks for 1 µF, not 100 nF |

**$10.01/board at qty 20** — $182.13 in parts plus $18.00 of setup fees for six
extended-library parts.

**U6 (CH340N) and U7 (USBLC6-2SC6) are gone.** USB-C is power only; programming
is via the six-pad pogo jig, so there is no USB data path to protect.

---

## 5. GPIO map

Avoid GPIO 6–11 (module flash). GPIO 12 is MTDI strapping and must be low at
boot — leave unused. GPIO 34/35/36/39 are input-only.

| Signal | GPIO | Note |
|---|---|---|
| PDM CLK → mic | 25 | **Changed from I²S.** MSM261DHT006 is PDM |
| PDM DATA ← mic | 33 | |
| *(free)* | 26 | Was I²S BCLK; PDM needs only two wires |
| VSPI SCK → flash | 18 | |
| VSPI MISO ← flash | 19 | |
| VSPI MOSI → flash | 23 | |
| VSPI CS → flash | 5 | Strapping, defaults high — OK |
| WS2812 data | 27 | Drive via **RMT peripheral** |
| SW1 REC | 0 | Also BOOT for flashing |
| SW2 PLAY | 4 | |
| SW3 TRACK | 16 | |
| SW4 MODE | 17 | |
| Write-protect tab | 21 | Pull-down; tab intact = high |
| Line-in ADC *(DNP)* | 35 | ADC1 |
| UART0 TX / RX | 1, 3 | To the J5 pogo pads only — there is no USB-UART on the board |

Free for expansion: 13, 14, 22, 32.

---

## 6. Behaviour spec

### Buttons

| Button | Short | Long |
|---|---|---|
| REC | — | **Hold to record.** Release stops. Overwrites current slot. |
| PLAY | Play / pause | Sleep |
| TRACK | Cycle 1 → 2 → 3 | — |
| MODE | — | 2 s pair · 5 s dub · 10 s dev WiFi |

### Write-protect tab

Break-off PCB tab, top edge, mouse-bites, trace from 3V3 to GPIO21 with
pull-down. Intact → high → recording permitted. Snapped → low → **firmware
refuses all recording, permanently.** One tab locks the whole tape.

Route the trace so the snap line crosses it cleanly. Keep mouse-bite nibs clear
of case ribs.

### Pairing

The mixtape is the A2DP **source** — it initiates. Bluetooth sinks have no
source-selection UI and don't need one.

1. User puts speaker/earbuds into their own pairing mode.
2. User holds MODE 2 s. Reels sweep amber.
3. `esp_bt_gap_start_discovery()` → inquiry.
4. Filter results by **Class of Device**: major class Audio/Video, minor class
   headphones or loudspeaker. Drops phones, laptops, keyboards automatically.
5. Filter by **RSSI better than ~−50 dBm** (≈30 cm). This makes proximity the
   selection mechanism — the user holds the board against the speaker. Threshold
   needs empirical tuning.
6. `esp_a2d_source_connect()`, Just Works SSP, bond, stream.

Store 4–8 bonds. On power-up try last-used first, then others.

**Known race:** the sink is often simultaneously trying to reconnect to its own
last source (usually a phone). Handle with retry + backoff, and give honest LED
feedback — slow red pulse after ~15 s of failure means "the speaker is busy."

### LED states

| State | Reels |
|---|---|
| Searching | Amber sweep |
| Connected | Green lock |
| Playing | White — left ring empties, right fills |
| Recording | Red pulse, brightness = VU level |
| Background erase | Slow reverse, dim |
| Dubbing | Violet |

**Clamp global brightness to ~25%.** All 29 at full white is ~1.7 A.

### Record pipeline

Two-pass. Nothing clever happens in real time.

1. REC held → I²S DMA into ring buffer → write **ADPCM** to a scratch region.
2. REC released → device idle → analyze peak, normalize, encode to **SBC**,
   write to the final slot. Reels spin during this, so the wait reads as
   rewinding rather than hanging.
3. Slot invalidation triggers **background pre-erase** at idle. Sector erase is
   ~40 ms and must never happen on the record critical path; recording issues
   page programs only (~0.7 ms).

### Playback

Read SBC frames from flash → `esp_a2d_source_audio_data_send()`. That is the
entire playback path. If you find yourself decoding or resampling during
playback, something upstream went wrong.

---

## 7. Open questions

### Agent may decide

- LDO part selection (1 A, 5 V in, 3.3 V out, SOT-23-5 or SOT-89)
- WS2812B-2020 specific LCSC part and second source
- Exact reel ring diameter and LED pitch
- Decoupling strategy for the LED chain (~1 cap per 4 LEDs)
- Hacker header pinout refinement — currently 3V3, GND, I²S ×3, UART ×2, EN,
  spare GPIO ×2
- Firmware task structure and RTOS priorities
- ADPCM variant

### Needs Steven

- **Tab count:** one for the whole tape (recommended, faithful) vs three
  (per-track, more useful)
- **N4 vs N16** once the live price delta is known — if under ~$0.40, take N16
- **Silkscreen artwork.** On a bare-PCB product the art *is* the industrial
  design. Nobody should generate this without him.
- **Project name**
- **Line-in DNP:** confirm the 3.5 mm jack footprint choice
- **Tape-head playback in a real deck** — deliberately deferred. Mixxim holds a
  patent covering a cassette-shaped player that plays in real decks. Do not
  design toward this without a legal read.

### Must verify before ordering

- [ ] ESP32-WROOM-32E-N4 live price vs N16
- [ ] GD25Q128ESIG live price and stock, and on the second sources C97521 and C113767
- [ ] JLCPCB surcharge for matte **white** solder mask (adds ~2 days; the
  silkscreen is then black, which is fixed rather than a choice)
- [ ] Matte white solder mask availability and any surcharge
- [ ] Extended-part reel fees — ~7 extended parts × ~$3, dominates cost at qty 20
- [ ] Norelco case internal dimensions **against a physical case in hand** — rib
      and hub-ridge geometry varies by brand; buy several

### Unanswerable on paper — needs a prototype

- **Mic mechanical noise.** A bare PCB transmits fingernail contact and board
  flex straight into a MEMS mic. This is the top technical risk. Mitigate with
  relief slots around the mic and placement away from the natural grip, but it
  cannot be confirmed without building one.
- Whether SBC-at-record fits the real-time budget alongside I²S DMA.
- A2DP reconnect latency on power-up. If it's ~8 s, the ritual is dead and the
  UX needs rethinking.

---

## 8. Mechanical

- Outline 100.0 × 63.5 mm, 1.6 mm FR4 (real cassette: 100.5 × 64 × 10 mm)
- Two reel window slots + five guide holes along the bottom edge
- **WROOM antenna keepout:** no copper, no pour, nothing underneath. Overhang
  the module past the board edge or cut a 6 mm clear region. Easy to get wrong
  on a board this dense.
- **Mic acoustic port: ~~0.5–0.8 mm non-plated hole through the board~~ —
  WRONG, removed.** The specified MSM261S4030H0R is *top*-ported (its
  datasheet, p.2: "omnidirectional, Top-ported"), as is every in-stock
  alternative in that family. Sound enters from the component side, so a
  hole beneath the part does nothing but weaken the board. What matters
  instead is that nothing covers the mic's top face — no case rib, no
  label, no fingertip. Marked as a keep-clear zone in the layout.
- **Sharpie label block:** upper-left third, where a cassette's paper label sits.
  Faint light-grey ruled lines on silkscreen.
- **Mounting holes:** four Ø2.2 mm, one near each corner.
- **No lanyard hole and no castellated header.** Both were dropped; the
  programming pads replaced the header, and the lanyard hole was removed when
  the mounting holes went in.
- All components top-side. Single-sided SMT is a hard constraint.

---

## 9. Suggested phase order

1. **Verification pass** — clear the §7 checklist. Cheap, and it gates BOM
   decisions.
2. **Outline + mechanical** — pull the Mixtape Alpha board file, adapt the
   outline, place reel windows, the break-off tab and four mounting holes. Check against
   a physical case.
3. **Schematic** — power, MCU, mic, flash, LED chain, buttons, tab, USB, DNP
   sections clearly marked.
4. **Firmware bring-up on a devkit** — before layout is final. Prove A2DP source
   connects to real earbuds, prove PDM mic capture, prove ADPCM encode timing.
   These are the risky parts and they don't need the real board.
5. **Layout** — aesthetics are a design requirement here, not an afterthought.
6. **Prototype run of 3–5** before committing all 20.

Steps 1 and 4 are the highest-value early work. Layout before firmware bring-up
would be a mistake — the timing unknowns could change the hardware.
