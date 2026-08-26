# Before you order boards

Nothing here is optional, and most of it is cheap. The expensive mistakes on
a project like this are all made in the ten minutes before clicking order.

Work top-down: the physical test gates the layout, the layout gates the
prototypes, and the prototypes gate the run of 20.

---

## 1. The case-fit test — do this first ⬜

**Everything mechanical is an assumption until this is done.** The outline
is borrowed from a board that fits *some* case; the reel windows, tab
position and lanyard hole are ours and have never touched plastic.

⬜ **Buy several cassette cases, from different brands.** The brief is
   emphatic: internal ribs and hub-clamp ridges vary, and thrifted cases are
   not dimensionally consistent. One case proves nothing.

⬜ **Print [`case-fit-test-1to1.pdf`](case-fit-test-1to1.pdf) at 100%.**
   Turn off "fit to page" / "scale to fit". Printers shrink pages by a few
   percent without telling you.

⬜ **Measure the calibration bar on the printout.** It must read exactly
   50.0 mm. If it does not, the print is scaled and nothing else on the
   sheet means anything — reprint before going further.

⬜ **Cut it out and put it in a case.** Then check, in order:

   - Does the outline sit in the case without forcing?
   - Do the **reel windows** clear the hub-clamp ridges, or do the ridges
     land on board material where LEDs will be?
   - Does the **write-protect tab** (top-left) clear the internal ribs? The
     mouse-bite nibs must not sit where a rib presses.
   - Does the **lanyard hole** (bottom-left) clear the hinge?
   - Does the **mic port** land somewhere a fingertip will not cover?

⬜ **Write the measurements down** and update the constants at the top of
   `hardware/scripts/gen_layout.py`, then re-run it. Every value there is
   named and commented for exactly this.

---

## 2. Decisions still open ⬜

These are all flagged in the repo but need an actual answer before copper.

⬜ **The microphone.** MSM261S4030H0R (C2840615) is at **zero stock** and
   the board cannot be built without a mic. Best candidate is MSM261DHT006
   (C51928215, ~3.5k, $0.45) — but check the datasheet for **package size
   and port direction** first: the design needs a *bottom-ported* part
   because there is an acoustic hole through the board beneath it.
   Alternates: MP34DT06JTR (C503097, PDM) or ICS-43434 (C5656610, I2S).
   See the BOM's "needs attention" section.

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
