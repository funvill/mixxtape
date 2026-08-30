# Before you order boards

Nothing here is optional, and most of it is cheap. The expensive mistakes on
a project like this are all made in the ten minutes before clicking order.

Work top-down: the physical test gates the layout, the layout gates the
prototypes, and the prototypes gate the run of 20.

---

## 0. Blockers from the six-agent review (2026-08-30) ⬜

Six independent reviews ran against this board — schematic/electrical,
PCB/DFM, BOM/footprints, firmware-vs-hardware, bring-up/rework, and analog/RF.
Everything they found that could be fixed deterministically **has been fixed**
(see the CHANGELOG). What is left below needs either a physical object or your
judgement, and **each one can still cost the run.**

### 0a. Test-fit a USB-C plug against a 1:1 print — do this first ⬜

J1's mating face sits at x = 129.568; the board edge is at x = 130.330. That
leaves **0.762 mm of bare FR4 protruding in front of the receptacle mouth**,
with no relief notch in the outline.

A USB-C plug's overmould shoulder seats flush against the receptacle at full
insertion, and the overmould extends below the plug axis — into exactly that
0.762 mm lip. Add the cassette shell wall in front of it and the plug may
bottom out before it latches. **A board that cannot be powered or flashed is
20 dead units.**

The setback is measured and certain. The *consequence* is inferred from
standard plug geometry — nobody has put a real plug against a real board.
**Print `case-fit-test-1to1.pdf` at 1:1 again, cut it, and offer a real USB-C
cable up to where J1 sits.** If it fouls, either move J1 +0.762 mm in x or cut
a relief notch across y 76.0–85.5.

### 0b. Re-derive the four mounting holes ⬜

All four are 2.200 mm and the old 0.54 mm regression is long gone — but they
**are not a rectangle**:

| | Position | Board-relative |
|---|---|---|
| 1 | 33.580, 32.750 | 2.750, 60.750 |
| 2 | 33.580, 90.750 | 2.750, 2.750 |
| 3 | 127.250, 33.000 | 96.420, 60.500 |
| 4 | 127.600, 90.750 | 96.770, 2.750 |

Bottom row spans 94.020 mm, top row 93.670 — **0.350 mm of skew**. Left column
pitch 58.000, right 57.750 — **0.250 mm**. A 2.2 mm hole over a ~2.0 mm shell
boss has about 0.20 mm of total slack, so the skew eats more than all of it:
the board will seat on three bosses and stand off the fourth.

Only the *internal* inconsistency is proven. Which pair is right needs the
shell. **Measure your case and put all four on an exact rectangle.**

### 0c. Step through D1–D29 in JLCPCB's previewer ⬜

**The LED part changed on 2026-08-30** — see the CHANGELOG. The old footprint
was drawn for a different manufacturer's pad-1 corner and would have
reverse-powered all 29. The new part (C5349954) has been verified against
LCSC's own library data and the netlist is correct, but:

- 20 parts sit at rotations that are not multiples of 90°, 16 of them LEDs.
- **JLCPCB's library sometimes defines a different zero-rotation than KiCad.**
- The previewer renders a *package*, not a netlist. **It cannot catch a pin-1
  error** — a wrong-corner part looks perfect in the render.

So the previewer is necessary and not sufficient. Look at it anyway, and give
D1–D29 and J1 their own minute.

### 0d. Microphone port direction — RESOLVED ✅

**MSM261DHT006 (C51928215) is TOP-PORTED.** Confirmed 2026-08-30 from LCSC's
own product page, which states: *"The MSM261DHT006 is an omnidirectional,
top-port MEMS microphone with PDM digital output."*
[C51928215](https://www.lcsc.com/product-detail/C51928215.html) ·
[datasheet](https://www.lcsc.com/datasheet/C51928215.pdf)

So the board is correct as drawn: **no through-hole under U2**, and the top
face is the acoustic path. This was flagged because `gen_layout.py` argues the
case using the name of the part it *replaced* (MSM261S4030H0R), and two
reviewers could not close it from the repo. It is closed now.

**What still applies:** keep U2's top face clear. Nothing tall beside it — the
only neighbours within 14 mm are C7 at 2.70 mm and R12 at 3.79 mm, both 0402,
so that is fine. But check during the case-fit test that no shell rib lands
over it, and remember a written label on the B-side artwork must not cover it.

### 0e. Decide on the protective components that were NOT added ⬜

These need schematic changes plus placement and routing. They were identified
but deliberately not applied unattended, because each adds a part and a net:

| Add | Where | Why |
|---|---|---|
| 10 k to 3V3 | `BTN_REC` (GPIO0) | Only the ESP32's internal ~45 k holds the boot strap. `BTN_REC` is **94.8 mm** — the longest signal net on the board — runs 16 mm alongside SPI_CLK, and lands on a bare pogo pad people touch. Leakage or noise at reset drops the board into download mode: dark, unresponsive, looks dead. |
| 10 k to GND | `LED_DATA` | GPIO27 is high-Z from reset, so U8's AHCT input floats near threshold, drawing crowbar current and possibly latching 29 LEDs to random colours at power-on. |
| 330 R series | U1.12 → U8.2 | J5 supplies **3V3, not VBUS**, so on jig power U8's VCC is 0 V. Driving GPIO27 high then forward-biases U8's input clamp into a dead rail, on **every flash cycle**. |
| 330 R series | U8.4 → D1 DIN | Damps the first hop of the chain. |
| 22 µF | U4 output, ≤2 mm | The AMS1117 datasheet asks for it and `cassette-recorder-agent-brief.md:103` says so; it never reached the schematic. Effective output C is realistically 5–6 µF. |
| 100 nF | U1 EN pin, ≤2 mm | The EN de-glitch RC (R3/C9) is **52 mm of routed trace** from the pin, alongside SPI_MOSI. |
| 1 k series | `TAB_SENSE` at U1 | The user snaps this tab **with their fingers** — a guaranteed human contact event on exposed copper wired straight to a GPIO with no clamp. |

Also consider **inverting the TAB1 divider** (3V3–R7–TAB_SENSE–TAB1–GND). As
built, GPIO21 is tied to 3V3 through a zero-ohm net tie; one firmware line
configuring it as output-low shorts the rail through the ESP32's driver.

### 0f. Fill in the factory-test probes ⬜

**With no prototype, the factory test is your only instrument** — and five of
its eight probes are stubs returning SKIP, so it can never report PASS. The
three unimplemented ones that matter most are exactly what you cannot check by
eye: **the microphone works, the radio transmits, and the flash accepts a
write.**

None of these need a board change:

- **`mic` — make it acoustic.** RMS of room noise cannot tell a working mic
  from a clock picking up crosstalk. Put a cheap piezo on the jig at a fixed
  distance, play a fixed tone, run a Goertzel at that frequency, and report the
  bin magnitude *and* its ratio to two neighbours.
- **`bt` — report RSSI, not a device count.** An inquiry count is
  environment-dependent: a quiet bench gives 0 and fails a good board. Pin one
  discoverable device at a fixed distance. An antenna or shield-solder defect
  shows as a 15 dB shift a pass/fail count would never see.
- **`flash_rw`** — pattern-write the spare region, read back, report CRC and
  throughput in KB/s. The JEDEC read barely exercises MISO.
- **`cc`** is nearly free: `cc_sense.c` already exists and works, it just is not
  wired into `probe_cc`.
- Add `esp_reset_reason()`, and **prefix the log line with the MAC** —
  `factory_test.h` calls the log "the build record for the run" and there is
  currently no way to tell board 14 from board 3.

**Also add a boot-time LED flash to `app_main`.** Today it never calls
`ws2812_init`, so a working board and a dead board are visually identical: 29
dark LEDs and UART on the jig pads. One line of "the CPU is running and the
RMT/level-shifter/LED-chain path works" is worth more during a 20-board
bring-up than any amount of UART.

Beware: **the `buttons` probe can pass spuriously.** REC is GPIO0, which the
jig's auto-reset also drives. The jig must tri-state IO0 before that step, and
the probe should verify IO0 reads high first.

### 0g. Build five, not twenty ⬜

Order 20 bare PCBs, **assemble 5**. Against the failure modes that remain
unrecoverable on this board — the USB-C fit, the mic port direction, the
mounting-hole skew — a pilot costs roughly one extra setup fee (~$20–30) and
one lead time, against ~$280 of scrapped assemblies.

Note **JLC will not assemble boards you already hold**, so a second run means
paying for boards again. Do not plan to hand-solder the microphone later: it is
a bottom-terminated LGA and hand-placing it is *harder* than letting JLC reflow
it.

### 0h. Accepted residuals — known, quantified, not fixed ⬜

These were judged not worth the risk of an unattended change. Decide whether
you agree.

- **Antenna clearance is 8.5 mm, not the 15 mm Espressif recommend.** Improved
  from 1.85 mm. Reaching 15 mm means rerouting 3V3/TAB_SENSE and EN/SPI_MOSI,
  or notching the outline so the antenna overhangs — which would invalidate
  the case fit you just verified against real plastic.
- **The write-protect tab is ~51 % solid FR4** across its 7.4 mm break line.
  It is now genuinely drilled (it was milled, at a size JLC could not cut), and
  the pitch was widened to 1.05 mm so the 0.45 mm hole-to-hole floor is met -
  but widening the pitch adds web as fast as it adds gap, so the solid fraction
  did not improve. The central unperforated span went from 2.30 mm to 1.60 mm.
  **It will need pliers or a score-and-snap, not a thumb.**
  The limit is geometric: 3V3 crosses at x = 45.150 and TAB_SENSE at 45.550,
  forcing a hole-free zone through the middle, and with a mandatory 0.45 mm
  between holes there is no arrangement that gets below ~45 %. Making it
  properly snappable means rerouting those two traces to cross at the edge of
  the tab rather than its centre. That is a real routing change and was not
  attempted unattended.
- **PDM_DATA runs 0.61 mm from SPI_CLK** for 4.8 mm on B.Cu, and the flash is
  written while the mic is recording. Expect flash-write-correlated clicks in
  recordings. Moving U2 next to U1 would fix this and the 67 mm PDM route at
  once — a placement change too large to make unattended.
- **U4 is at ~67 mm² of attached copper**, up from 29. Fine at a realistic
  150–250 mA load; marginal at a sustained 500 mA.
- ~~J1's CC pad mapping is unverified.~~ **RESOLVED 2026-08-30.** Read in
  physical order along the signal row, J1's pads come out as `B1A12, B4A9, B5,
  A8, B6, A7, A6, B7, A5, B8, A4B9, A1B12` — exactly the USB Type-C standard
  12-pin sequence, reversed by the connector's 90° rotation. **B5 lands on CC2
  and A5 on CC1**, both in their standard positions, each with its own 5.1 k
  pull-down (R2, R1). VBUS is on both A4B9 and B4A9, GND on both A1B12 and
  B1A12, and SBU1/SBU2/D+/D− are correctly left open. Cross-checked against
  LCSC's own library data for C165948 (TYPE-C-31-M-12).
- **The board draws more than a default USB port guarantees.** 29 LEDs plus an
  ESP32 TX burst exceeds 500 mA. R10/R11 correctly route CC to GPIO36/39 so
  firmware *can* read the source's advertised current — **it must**, and must
  cap brightness when the source is default-USB.

---

## 1. The case-fit test — mostly done ✅

**Steven ran the 1:1 printout against a real plastic case on 2026-08-30 and
it fits.** That retires the single largest mechanical unknown: the outline
had been borrowed from a board that fits *some* case, and the reel windows,
tab position and mounting holes are ours and had never touched plastic.

One item below is still open, and it is the one that decides whether the fit
generalises or was luck.

⬜ **Buy several cassette cases, from different brands.** The brief is
   emphatic: internal ribs and hub-clamp ridges vary, and thrifted cases are
   not dimensionally consistent. **One case proves the design is not wrong;
   it does not prove it is right.** Still worth doing before a 20-board run,
   because the failure it catches is one that scraps every board at once.

✅ **Print [`case-fit-test-1to1.pdf`](case-fit-test-1to1.pdf) at 100%.**
   Done 2026-08-30. Turn off "fit to page" / "scale to fit" if reprinting —
   printers shrink pages by a few percent without telling you.

✅ **Measure the calibration bar on the printout.** Must read exactly
   50.0 mm. Treated as satisfied by the successful fit — a scaled print
   would not have seated correctly — but measure it explicitly on any
   reprint, because a print scaled by a percent or two can still *feel*
   right in the hand.

✅ **Cut it out and put it in a case.** Done 2026-08-30 — it fits. The
   points that were checked, kept here as the record of what "it fits"
   covered:

   - Does the outline sit in the case without forcing?
   - Do the **reel windows** clear the hub-clamp ridges, or do the ridges
     land on board material where LEDs will be?
   - Does the **write-protect tab** (top-left) clear the internal ribs? The
     mouse-bite nibs must not sit where a rib presses.
   - Do the four **mounting holes** clear the case's internal ribs?
   - Does the **microphone** land somewhere a fingertip, a case rib or
     a written label will not cover? It is top-ported, so its top face
     is the acoustic path — there is no hole through the board.

⬜ **Write the measurements down.** Record them in this file rather than
   re-running `hardware/scripts/gen_layout.py`. **Do not re-run that
   script** — it rebuilds the original 100.33 mm outline and would undo the
   trim to 99.50 mm, along with every placement and route since. It carries
   a warning header saying so. If a measurement forces a mechanical change,
   edit the board directly and re-run `hardware/scripts/export_fab.py`.

---

## 2. Decisions still open ⬜

These are all flagged in the repo but need an actual answer before copper.

✅ **The microphone — decided.** Moved to MSM261DHT006 (C51928215), the
   best-stocked digital MEMS mic at JLC. Acoustically an equal swap; the
   cost was the interface (PDM instead of I²S, firmware updated) and a
   completely different pinout, so the schematic changed rather than just
   the part number. Two things came out of the datasheet review and are
   worth remembering:

   - It is **top-ported**, and so was the part the brief specified. The
     acoustic hole the brief called for through the board was a mistake
     and has been removed. Keep the mic's top face clear instead.
   - **3,485 in stock is below the 5,000 rule** — a documented exception,
     because no digital MEMS mic at JLC clears it and the only one that
     does is analog. Re-check stock before ordering; if it has fallen,
     alternates are MP34DT06JTR (C503097) and ICS-43434 (C5656610).

⬜ **Record path: one pass, not two.** The brief's two-pass pipeline needs a
   scratch region that does not fit. Firmware now encodes ADPCM straight to
   the slot. Consequence: a look-ahead limiter instead of whole-take peak
   normalisation. See `storage-budget.md` §4.

⬜ **Slot size 5 MiB → 5.125 MiB** (82 erase blocks), to keep the full
   four-minute take. A deliberate deviation from the locked "3 × 5 MB
   slots"; it preserves three fixed slots and the 4-minute promise.

⬜ **Licence of the borrowed outline.** Open Music Labs publish the Mixtape
   Alpha board files but state no licence for them. Confirm with them
   before distributing derived artwork. If they decline, redraw the outline
   from a measured cassette.

⬜ **The name.** The repo is named after Mixxim's commercial product. Fix
   before anything goes public.

⬜ **Tab count** — one for the whole tape (recommended, faithful) or three,
   one per track (more useful).

⬜ **Silkscreen artwork.** On a bare-PCB product the art *is* the industrial
   design. Nobody should generate this but you. The five decorative guide
   holes along the bottom edge are deferred until this happens.

---

## 2b. Manufacturability, audited 2026-08-29 ⬜

Checked against JLCPCB's published 2-layer limits. Everything below either
passed or was fixed, but three items want a human eye before the order goes in.

- [ ] **Smallest plated annular ring is 0.200 mm on J1** — above JLCPCB's
  0.18 mm floor but under their 0.25 mm recommendation. Fine in all likelihood;
  worth a glance at the fab's DFM report rather than a surprise.
- [ ] **Closest hole-to-hole is 0.500 mm against a 0.45 mm limit**, inside the
  DNP microSD footprint. Only bites if J4 is ever fitted.
- [ ] **The B-side artwork has 32 shapes thinner than 0.15 mm** and 52 more
  between 0.15 and 0.25 mm. The first group will not print. Get the artwork
  thickened before committing to a run, or accept that the fine detail vanishes.
- [x] Footprint attributes corrected — the microphone was tagged `through_hole`
  with eight SMD pads, which can drop it from the pick-and-place file entirely.
- [x] Silkscreen line widths raised to JLCPCB's 0.15 mm minimum.
- [x] Eleven DRC rules had been set to `ignore`, hiding 145 findings including
  the one that would have caught the microphone. The eight that matter are back
  on as warnings.
- [x] **USB-C needs no board notch.** The TYPE-C-31-M-12 is a top-mount
  right-angle receptacle; its only board penetrations are four plated shell legs
  and two non-plated locating pegs, all already in the footprint. Its mouth sits
  0.76 mm inboard of the board edge, and the strip in front of it is clear of
  copper and parts. A notch is only needed for *mid-mount* receptacles, which
  this is not.
- [ ] **Confirm the case does not block the USB-C.** The port is on the right
  edge; a closed Norelco case has a wall there. Decide whether the board is
  meant to come out of the case to charge, or whether the case gets modified.

---

## 3. Re-quote, don't trust this repo ⬜

⬜ **Re-run `hardware/scripts/gen_bom.py`.** Prices and stock in the BOM are
   a snapshot. NOR flash has already moved once (W25Q128 went from $1.65 to
   $2.26 during this project).

⬜ **Check the supply-chain rule section.** Every fitted part should clear
   5,000 in stock, or be a documented exception.

⬜ **Confirm matte white solder mask** is available and what it costs.

⬜ **Count the extended parts.** At 20 boards the per-part setup fees
   dominate the component cost — adding one new extended part costs about
   as much as 50 LEDs.

---

## 4. Prototypes before the run ⬜

⬜ **Order 3–5 boards first, not 20.** At ~$14 each they are cheaper than a
   devkit, and they are the only way to answer the questions below.

⬜ **Build the programming jig.** There is no USB-UART on the board, so
   nothing can be flashed without it.

⬜ **Fill in the factory-test probes.** `firmware/main/factory_main.c` is
   the checklist; the harness is written and tested, the probes are stubs.
   The runner prints `SUITE INCOMPLETE` until they are real.

⬜ **Answer the three questions that could still change the hardware:**

   - **Handling noise.** A bare board with a mic on it, held in a hand.
     This is the project's biggest technical risk and it cannot be
     answered on paper.
   - **A2DP against real sinks.** Test a basket of them — AirPods, cheap
     TWS earbuds, a Bluetooth speaker. Confirm the class-of-device filter
     and tune the RSSI threshold, which is currently a guess.
   - **Reconnect latency on power-up.** If it is ~8 seconds, the ritual is
     dead and the interaction needs rethinking. Measure it early.

---

## 5. Only then ⬜

⬜ Fix whatever the prototypes taught you.
⬜ Re-quote again.
⬜ Order the 20.
