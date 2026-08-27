# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project is pre-release; entries are dated rather than versioned until the
first prototype run.

## [Unreleased]

### Fixed (2026-08-27, CI: the `bt` component name collided with ESP-IDF's)

- **`firmware/components/bt` is renamed to `components/btaudio`.** ESP-IDF
  ships its own component called `bt`, and project components override
  IDF's by directory name, so ours silently shadowed the real Bluetooth
  component. `esp_hid` then lost the include paths it inherits through
  `bt` and the build died on `esp_timer.h: No such file or directory` —
  an error inside IDF's own source, which is why it read as an IDF problem
  rather than ours.
- **The IDF build has been red since 3dcf692** ("pairing policy and
  persistent bond table"), the commit that created the component. Four
  consecutive failures, all the same cause. Host tests were green
  throughout, which is why it went unnoticed.
- The constraint is now written down in `firmware/README.md`, because the
  next person to add a component called `console`, `driver` or `log` will
  hit exactly this.

### Changed (2026-08-27, CI runs on pull requests only)

- The firmware workflow no longer runs on push; it runs on `pull_request`
  and `workflow_dispatch`. **This repo commits straight to main, and a push
  to main opens no pull request** — so unless the habit changes to raising
  PRs, CI now only runs when triggered by hand from the Actions tab.
- `actions/checkout` bumped v4 → v5, clearing the Node 20 deprecation
  warning that was annotating every run.

### Removed (2026-08-27, battery section)

- **The TP4056 charger, its JST battery connector and PROG resistor are
  gone** (U5, J2, R8, plus the VBAT and PROG nets). They were always DNP,
  and they could never have worked: VBAT dead-ended at the connector and
  the charger, and even wired through, a lithium cell at 3.0–4.2 V cannot
  feed an AMS1117 that needs 1.1–1.3 V of headroom to make 3.3 V. The LEDs
  and the level shifter sit on the 5 V USB rail besides. Running from a
  cell means a different regulator and a different LED rail — a redesign,
  not a populated footprint. Removing it frees about 40 mm² on a board
  that is about to be routed.
- Docs no longer promise a battery footprint you can solder to. "USB-C
  only" was already the locked decision; now the board matches it.

### Fixed (2026-08-27, schematic review before routing)

- **The board outline was self-intersecting and would have failed CAM.**
  The write-protect tab's two slots were drawn as closed rectangles lying
  *on top of* the top edge instead of cut into it, so the edge ran straight
  through both — four real crossings. Split the top edge into three spans
  and removed the slot mouths. The outline is now a single closed loop with
  no crossings and no dangling endpoints, and the stale zone fills that
  came with it are refilled.
- **Every symbol pin now has a real electrical type.** easyeda2kicad types
  every pin `unspecified`, which silently disables the checks that matter.
  109 pins retyped from the datasheets, in the schematic *and* in
  `mixxtape_parts.kicad_sym` so a library update cannot undo it. Turning
  the checks on immediately found three genuine faults: U4's two VOUT pins
  and J1's paired VBUS and GND pins each read as two power outputs fighting
  over one rail. They are duplicate pins of one physical node, so one of
  each pair now carries the `power_out` and its twin is `passive`.
  **ERC: 191 warnings → 0 violations.**
- **TAB1 had no footprint at all**, so TAB_SENSE would have read
  permanently low. Added `mixxtape_local:TAB_BREAKOFF_LINK`, a two-pad net
  tie placed on the break-off tongue: snap the tongue and the link goes
  with it. Note for routing — both nets must cross the perforation through
  the gaps between the bite holes, and freerouting does not understand net
  ties, so hand-route those two first.
- **C9 was 100 nF on EN; the ESP32-WROOM-32E datasheet asks for 1 µF**
  (§ "it is advised to add an RC delay circuit at the EN pin… R = 10 kΩ and
  C = 1 µF"). Changed to C29266.
- **Added R12, 100 Ω (C25076), in the microphone's supply**, per figure 1
  of the MSM261DHT006 datasheet. C7 moves onto the new MIC_VDD net so the
  two form an RC filter — left on 3V3 it would just be another rail cap.
  A PWR_FLAG marks MIC_VDD as driven, and `gen_bom.py` now skips
  `in_bom no` symbols so the flag is not a phantom BOM line.
- **Four through-hole pads had zero annular ring** (J1 ×2, J4 ×2) — the
  connectors' moulded locating pegs, where pad size equalled drill size.
  Now `np_thru_hole`, fixed in the board and in the library footprints.
- **Datasheet links corrected.** D1–D29 pointed at Worldsemi C965555 while
  the fitted part is XINGLIGHT C5349955; the two symbols cloned for R12 and
  C9 had inherited their donors' links.

### Added (2026-08-27, mounting holes)

- **Three Ø2.2 mm mounting holes** at the top-left (32.75, 32.75),
  bottom-left (32.75, 90.75) and top-right (122.50, 32.75), each with the
  Ø4.2 mm annotation ring the existing holes use. The top-right one sits
  8.3 mm inboard rather than in the corner because the microphone is there
  — there is no room between it and the board edge. Moving the mic ~3 mm
  left would free (127.58, 32.75) for a symmetric pattern.
- **`mixxtape.kicad_dru`**, scoping a 0.1 mm clearance exception to J1. The
  USB-C receptacle's own pads sit 0.10 mm apart against the board's 0.20 mm
  rule; that is the connector's geometry, not our routing.
- Verified: **ERC 0, DRC clean apart from the 130 unrouted nets, every
  footprint's pads reconcile with its symbol's pins, and every board pad
  agrees with the schematic netlist.**

### Changed (2026-08-25, microphone: MSM261DHT006, PDM)

- **Microphone changed to MSM261DHT006 (C51928215)** across schematic,
  layout, firmware and docs, replacing MSM261S4030H0R (C2840615) which is
  at zero stock. Acoustically an equal swap — identical −26 dBFS
  sensitivity, same 1.6–3.6 V range, same 4×3 mm LGA-8 outline, 1 dB less
  SNR and 2 dB *better* acoustic overload — at $0.45 against $1.61.
- **This was a schematic change, not a part-number swap.** The pinout is
  completely different: pin 1 is VDD where the old part had GND, L/R moves
  to pin 2, and pins 5–8 are all GND. Footprint changed to
  `LGA-8_L4.0-W3.0-R_WMM7040DT0`.
- **Interface moves from I²S to PDM.** Two wires instead of three, so
  **GPIO26 is freed**. GPIO25 becomes the PDM clock, GPIO33 the data input.
  The ESP32's I²S peripheral does PDM receive and decimation in hardware,
  so nothing downstream of the capture task changes.
- **PDM downsample ratio must be 64.** PDM clock = sample rate × ratio, and
  44100 × 128 = 5.64 MHz exceeds the microphone's 4.8 MHz maximum. 64 gives
  2.8224 MHz, inside its window. Recorded in `board.h` because it is the
  obvious mistake to make.
- Board cost drops to **$9.85/board** at 20 boards, from $11.01.
- ERC 0 errors, DRC 0 violations, 11 host suites still green.

### Fixed (2026-08-25, microphone datasheet review)

- **The brief's "bottom-ported" microphone is actually top-ported.** The
  specified MSM261S4030H0R datasheet says so on page 2 ("omnidirectional,
  Top-ported, I²S digital output"), and so does every in-stock alternative
  in the family. The 0.5–0.8 mm acoustic hole the brief called for through
  the board would have done nothing except weaken it — sound enters from
  the component side. Hole removed from the mechanical layout and replaced
  with a keep-clear annotation; the brief is corrected at source.

### Findings (2026-08-25, microphone replacement)

- **MSM261DHT006 (C51928215) is acoustically an equal swap**, not a
  downgrade: identical −26 dBFS sensitivity, same 1.6–3.6 V range, same
  4×3 mm LGA-8 footprint, 1 dB less SNR (60 vs 61 dB) and 2 dB *better*
  acoustic overload point (122 vs 120 dB SPL) — which matters for a
  recorder that will meet the occasional door slam. Cheaper too ($0.45 vs
  $1.61) and lower current.
- **Its one real cost is the interface: PDM, not I²S.** Firmware would
  initialise the ESP32's I²S peripheral in PDM RX mode, which the original
  ESP32 supports in hardware. This is a firmware change, not a hardware
  one — the pin count and footprint are unchanged.
- **Alternative if the firmware change is unwelcome:** ICS-43434
  (C5656610, ~3.7k, $3.33) keeps I²S and gains 5 dB of SNR (65 dB) on the
  project's riskiest subsystem — but costs 7× more (+$58 across 20 boards)
  and has a different 3.5×2.65 mm footprint, so symbol and land pattern
  both change.

### Added (2026-08-25, supply-chain rule and the case-fit test)

- **Supply-chain rule (Steven):** every fitted part must have 5,000+ in
  stock at JLCPCB, and a common part beats a capable one. Enforced by
  `gen_bom.py`, which now prints a rule section; `find_common_parts.py`
  searches for compliant alternatives. Exceptions must be argued in
  `STOCK_EXCEPTIONS` rather than waved through.
- **Audit result: every fitted part clears 5,000 except the microphone.**
  The nearest thing to a problem elsewhere is the ESP32 module at 24k,
  which is a locked decision anyway (only ESP32 with Classic Bluetooth).
- **⚠ No digital MEMS microphone at JLCPCB meets the rule.** The whole
  category is thin — the best-stocked digital part is ~3.5k. The only mic
  that clears 5,000 (ZTS6216, C481302, 29k) is **analog**: it needs a
  DC-blocking cap and a gain stage into the ESP32's ADC, which the brief
  already calls a lo-fi path with ~9 effective bits. Meeting the rule there
  would wreck the product's only input, so a digital part at ~3.5k is
  recorded as an accepted exception with three qualified alternates.
- `docs/case-fit-test-1to1.pdf` — printable board outline at 1:1, with a
  50 mm calibration bar so a silently scaled print is caught before
  anything is measured against it.
- `docs/PRE-PRODUCTION-CHECKLIST.md` — the case-fit procedure, every open
  decision, what to re-quote, and the three prototype questions that could
  still change the hardware. Linked from the README.

### Added (2026-08-25, PCB mechanical layer)

- `hardware/mixxtape.kicad_pcb` — board outline, reel windows, break-off
  write-protect tab, lanyard hole and microphone port. Generated by
  `hardware/scripts/gen_layout.py`; DRC clean. **Mechanical only** — no
  components placed and no routing, which is the brief's phase 2 and the
  part that has to be checked against a real case before any copper.
- **The outline is not redrawn from a standard.** Per the brief, it is
  traced from Open Music Labs' Mixtape Alpha released gerbers
  (`hardware/scripts/cassette_outline.json`, 104 segments,
  100.33 × 63.50 mm) — a cassette shape that has actually been fabricated
  and fits real cases, which beats a dimension copied out of a datasheet.
- Reel hubs at the compact-cassette standard 42.0 mm spacing, 34.3 mm from
  the bottom edge (measured off the Mixtape Alpha board render — they sit
  slightly above centre, which is what makes it read as a cassette). Ø14 mm
  windows with the twelve LED positions marked on a Ø18 mm ring.
- Tab is 9 × 5.5 mm at the top-left, held by five 0.6 mm mouse bites, with
  routed slots either side.

### Needs Steven (2026-08-25)

- **⚠ Licence of the borrowed outline is unstated.** The Mixtape Alpha wiki
  publishes the board files but names no licence for them (the "GPL
  licensed" on that page refers to the wiki software). Confirm with Open
  Music Labs before distributing derived artwork. Mitigation if they
  decline: the shape is the compact-cassette form factor, so redrawing it
  from a measured cassette is straightforward.
- **Print the outline at 1:1 and put it in a real case.** Reel window
  diameter versus hub-clamp ridges, tab position versus internal ribs, and
  the lanyard hole versus the hinge are all still assumptions. Case brands
  are not dimensionally consistent — the brief says buy several.
- The five decorative guide holes along the bottom edge are deferred: they
  never enter a deck, so they are artwork rather than mechanism, and
  guessed positions would constrain component placement for nothing. Add
  them with the silkscreen artwork.

### Added (2026-08-25, BOM v4 — generated, with live pricing)

- `hardware/scripts/gen_bom.py` — derives the BOM from
  `hardware/mixxtape.kicad_sch` and reads prices, stock and library type
  live from JLCPCB, emitting both `docs/cassette-recorder-bom-v4.md` and a
  JLCPCB-format assembly CSV. Requests are paced and cached because JLC
  throttles bursts.
- **BOM v3 had drifted from the schematic in seven places** and is now
  marked superseded: it still listed the CH340N, USBLC6 and S8050 pair
  (deleted when USB-C became power-only) and the ME6211 LDO (replaced by
  the AMS1117), and was missing the fitted WS2812 substitute, the
  SN74AHCT1G125 level shifter and the DNP microSD footprint. Generating it
  removes that whole class of error.

### Findings from live JLCPCB data (2026-08-25)

- **⚠ The microphone is out of stock.** MSM261S4030H0R (C2840615) shows
  zero at JLC — and it is the one part the product cannot ship without.
  Same-family alternatives are in stock at roughly a third of the price
  (C51928215, C22390138, C51928210), but the part-number suffix encodes
  package and port direction, and this design needs a **bottom-ported**
  mic because there is an acoustic hole through the board beneath it. Each
  candidate needs its datasheet checked before substituting. Needs Steven.
- NOR flash is up as the brief predicted: W25Q128 is $2.26 against v3's
  $1.65.
- Parts now cost **$11.01/board** at 20 boards ($205.20 components +
  $15.00 in setup fees for 5 extended parts), against v3's ~$8.50 estimate
  for components. PCB and assembly labour are quoted separately.
- At this quantity the per-extended-part setup fee dominates: adding a new
  extended part costs $3, while using more of one already on the board is
  nearly free.

### Added (2026-08-25, user documentation)

- `docs/MANUAL.md` — the manual as it would ship with a board: getting
  started, the controls, recording and playback, the light language, the
  write-protect tab and why it is permanent, power, labelling, care,
  troubleshooting, specifications, and a plain list of what the device
  deliberately does not do.
- `docs/FAQ.md` — the questions people actually ask, answered against the
  design as built, with a closing section of honest limitations (no way to
  get recordings onto a computer, twelve minutes total, mono, handling
  noise still unproven, needs mains power, will not play in a deck).
- Both carry a draft banner and mark unverified claims as such — sound
  quality, handling noise, AirPods compatibility and reconnect latency are
  design intent until prototypes exist, and the docs say so rather than
  implying they are settled.
- Neither document uses the project's current name, since it collides with
  Mixxim's commercial product and a rename is planned; they say "your
  tape" throughout, which reads better anyway and makes the rename a
  non-event.

### Added (2026-08-25, firmware — record-path limiter)

- `components/audio/limiter` — look-ahead peak limiter with bounded makeup
  gain, replacing the whole-take peak normalisation that one-pass recording
  gave up. One gain does both jobs:
  `target = min(max_gain, ceiling / envelope)` — loud rooms are pulled down
  and cannot clip, quiet rooms are lifted (up to +18 dB) so an ambient take
  is actually audible. It is also the better tool than normalisation, which
  a single door slam defeats.
- The envelope is a true **sliding maximum** over a window that includes
  the sample about to leave the delay line, not a decaying peak follower.
  That makes `|output| <= LIMITER_CEILING` a hard guarantee for any input
  rather than a fast attack that usually keeps up — a decaying signal
  defeats the cheap version, and there is a test for exactly that case.
- Gain falls immediately and rises slowly: the immediate fall lands on
  audio still inside the 10 ms delay line, so it is inaudible, while the
  slow rise (~371 ms) stops the noise floor pumping between words. A gate
  freezes the gain below -44 dBFS so a silent room is not amplified into
  hiss. Integer throughout, so host and target results are identical.
- Also feeds the REC lamp and reel brightness via `limiter_level()`.
- Host tests: the ceiling invariant is checked against full-scale squares,
  near-Nyquist content, rail-to-rail alternation, noise and a decaying
  tone; look-ahead is proven by a silence-to-full-scale burst whose *first*
  cycle already sits at the ceiling; plus delay accounting, gain bounds,
  gate behaviour, smooth recovery, in-place aliasing, and invariance to
  chunk size. 11 suites, ~62,000 assertions, green in ~1.8 s.

### Fixed (2026-08-25)

- Level meter decay stalled at 3/255 because the shift-based decay
  underflows below 2^9, which would have left the REC lamp faintly lit
  forever after a loud take. Caught by `test_limiter`.

### Added (2026-08-25, firmware — pairing policy and bond storage)

- `components/bt/pairing` — the selection policy that makes "hold the board
  against the speaker" the entire interaction. Class-of-Device filtering
  drops phones, laptops, peripherals and wearables so the user never sees a
  list of their own hardware, and rejects the non-sink members of the
  Audio/Video class (microphones, set-top boxes, VCRs, cameras, monitors).
  RSSI then makes proximity the selection mechanism. Ties keep the earlier
  entry so repeated inquiries do not make the board look like it is
  dithering. Speakers that omit the Audio service bit are still accepted —
  plenty of cheap ones do.
- Reconnect backoff with the "sink busy" signal: the sink is usually busy
  reconnecting to its owner's phone at power-up, so attempts back off
  exponentially to a cap, and after ~15 s the user is told honestly rather
  than left watching a hopeful blink. Correct across the millisecond
  counter wrapping (~49 days).
- `components/bt/bonds` — persistent bond table in the spare flash region,
  reusing the tape header's append-only ping-pong log: 8 bonds, ordered
  most-recently-used first (which is the order power-up tries them),
  least-recently-used eviction when full, and crash-safe commits.
- Host tests: a power-yank sweep over the bond log proves an established
  bond survives a cable pull during a new pairing, the new bond either
  lands completely or not at all, and the table is still usable
  afterwards. 10 suites, ~60,000 assertions, green in ~1.9 s.

### Added (2026-08-25, firmware M5 — factory test harness)

- `components/factory` — the harness that gets 20 hand-built boards
  checked identically: sequences probes, aggregates results, and emits an
  operator table plus a one-line machine record
  (`FT|1|flash_id=PASS:0xEF4018|...|VERDICT=PASS`) so the jig's serial log
  becomes the build record for the run.
- Design properties, all host-tested with mock probes: every step runs even
  after one fails (a board with three faults reports three, not one per
  reflash); each result carries a measured value, not just a verdict;
  skips never fail a board; interactive steps are skipped automatically on
  unattended runs; both report formats truncate safely at every buffer size
  (swept 1..200 bytes).
- `main/factory_main.c` — board-side probes and the runner, built with
  `idf.py -DMIXXTAPE_FACTORY_TEST=ON`. Tab sense, button walk and the SPI
  JEDEC ID read are written; mic, LED walk, CC sense, flash read/write and
  BT inquiry are stubs reporting SKIP until the M4 drivers exist. The
  runner prints `SUITE INCOMPLETE` rather than a pass banner while any
  probe is unimplemented — a green light nobody earned is worse than none.
- 8 host suites, ~59,000 assertions, green in ~1.6 s.

### Added (2026-08-25, firmware — playback and the reel display)

- `components/tape/tape_player` — the tape-side sequencer. PLAY runs every
  recorded track in order as one side, skipping empty slots rather than
  playing silence; pause/resume, track skip with wrap, and position
  measured across the whole side (which is what the reels show). Pull-based
  so the A2DP callback, which must never block, only ever copies out of a
  ring a prefetch task fills.
- `components/leds/reels` — the reel display as a pure function of (mode,
  progress, level, clock). Left ring empties while the right fills; red
  pulse riding the input level while recording; amber sweep hunting for a
  sink; green lock; dim reverse spin during background erase; violet while
  dubbing; slow red pulse when the sink is busy.
- Host tests: `test_tape_player` (side sequencing verified sample-exact
  against independently decoded audio, empty-slot skipping, pause/resume,
  skip wrap, monotonic position, playback after remount) and `test_reels`.
  7 suites, ~58,000 assertions, green in ~1.3 s.

### Verified (2026-08-25)

- **LED power budget holds.** `test_reels` sweeps every mode across time at
  worst-case inputs and measures the frame: **119 mA worst case** at the
  default 500 mA-source brightness cap, 227 mA at the 1.5 A cap — against a
  200 mA slice of the USB budget (the ESP32 alone bursts to ~240 mA on BT
  transmit). No mode ever renders all-white. The constraint is now enforced
  by a test rather than by hoping.
- Reel conservation: the two rings together always hold the same amount of
  "tape" across a whole side, to within integer rounding.

### Changed (2026-08-25, firmware M3 — stored format is ADPCM, not SBC)

- **ESP-IDF's A2DP source accepts PCM only.** `esp_a2d_source_data_cb_t` is
  handed a "buffer to be filled with PCM data stream"; Bluedroid encodes SBC
  internally and exposes no API for pre-encoded frames. The brief's
  "playback is read-frames-and-transmit" is therefore unbuildable as
  written — storing SBC would mean decoding it back to PCM at playback just
  so the stack could re-encode it.
- **Tape now stores block-framed IMA ADPCM.** 512-byte blocks: 4-byte header
  (predictor, step index, payload checksum) + 508 B of nibbles = 1016
  samples = 23.04 ms, 22,223 B/s. Playback is read-block → table-lookup
  decode → PCM to Bluedroid, which is far cheaper than SBC decode and keeps
  the spirit of the locked "no DSP at playback" decision.
- Blocks are self-contained, so **seeking is free** — track skip,
  pause/resume and the reel position display all depend on it.
- **Slot size 5 MiB → 5.125 MiB** (82 erase blocks) to preserve the full
  four-minute take at ADPCM's higher bitrate. Flagged deviation from the
  locked "3 x 5 MB slots"; keeps that decision's substance (three fixed
  slots, no dynamic allocation) and the 4-minute promise. 576 KiB spare.
- Retires M6 risk #1 (SBC-encode-while-capturing timing) — we no longer
  encode SBC at all.
- Crash recovery now validates a **payload checksum** per block. Without it
  a torn write whose header landed but whose data was cut short was
  wrongly accepted, keeping 23 ms of noise — a real bug the yank sweep
  caught once the format changed.
- Host tests: added `test_adpcm_block` (block geometry, capacity,
  independent decodability, truncation rejection). 5 suites, ~4,100
  assertions, green.

### Added (2026-08-25, firmware Phase A — M0/M1/M2)

- `firmware/` — ESP-IDF project skeleton: pin map (`main/include/board.h`),
  `sdkconfig.defaults` (Classic BT + A2DP source, BLE off, BR/EDR-only
  controller), component/CMake wiring, and a UI-bring-up `app_main`.
- `firmware/components/tape` — crash-safe slot manager behind an abstract
  `flash_hal.h`: append-only ping-pong header log (64-byte records, CRC32 +
  generation counter), just-in-time erase ahead of the write pointer, and
  mount-time repair of a slot left mid-recording.
- `firmware/components/ui` — button grammar and device state machine
  (REC hold, PLAY short/long, TRACK cycle, MODE 2/5/10 s), pure logic.
- `firmware/test/host` — NOR flash mock that models erase/program bit
  semantics and tears any chosen operation, plus a **power-yank sweep**
  that tears every flash operation of a recording in turn and re-checks the
  invariants. 4 suites, ~2,300 assertions, all passing in <1 s.
- `.github/workflows/firmware.yml` — CI: host tests via ctest, plus an
  ESP-IDF v5.3 container build.
- `docs/storage-budget.md` — the M1 arithmetic.

### Changed / findings (2026-08-25, firmware)

- **Record pipeline changed to one-pass SBC — needs Steven's sign-off.**
  The brief's two-pass pipeline needs a 5.05 MiB ADPCM scratch region, but
  three 5 MiB slots plus a header leave only 960 KiB — a 4.11 MiB deficit,
  i.e. 44 seconds of scratch instead of 4 minutes. Encoding SBC directly at
  capture removes the scratch entirely, honours the LOCKED "3 x 5 MiB slots"
  layout exactly, raises the affordable bitpool from 18 to 26 (121 -> 165
  kbps), halves flash writes, removes the post-record wait, and makes a
  power yank *keep* the partial take. Only loss is whole-take peak
  normalisation, replaced by a look-ahead limiter. ADPCM is implemented and
  tested regardless. See `docs/storage-budget.md` §4.
- Slot-repair scanning is a linear forward scan, not a binary search: a torn
  erase leaves a half-erased block whose tail still holds the previous take,
  so the slot is not monotonic and a binary search could resurrect stale
  audio. Caught by the block-crossing yank sweep.
- `tape_store_erase_slot` disowns a slot's audio *before* erasing it, so a
  yank mid-erase cannot leave the header claiming playable audio over
  half-erased flash.
- Storage-budget table corrected to whole-frame sizes (4:00 at bitpool 26 is
  4,961,220 B, not the fractional 4,961,250) — caught by `test_sbc_frame`.

### Added (2026-08-25, prior art research)

- `docs/prior-art.md` — eleven cassette-form-factor projects researched:
  Mixtape Alpha and 8Bit Mix Tape (direct ancestors), plus Mixxtape (Mixxim),
  the Larry Schotz car cassette adapter, 8BitMixtape NEO, JamHamster's ZX
  Spectrum cassette, the Cassette Pi IoT scroller, the **Digisette Duo-Aria
  (2000)** — the only known cassette-shaped device with record + playback,
  incl. deck playback via edge transducer, predating the Mixxim patent —
  the ION Bluetooth cassette adapter (mic inside the shell), Milktape /
  Suck UK USB mixtapes, and NFC mixtape tokens. Per-project takeaways and
  cross-cutting patterns.

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
- **USB-C is now power-only; programming moves to a jig (Steven,
  2026-08-25):** CH340N, USBLC6 ESD, and their caps removed; USB data pins
  NC. The castellated hacker header is removed. New J5: six bare pogo pads
  (3V3, GND, EN, IO0, TX, RX) — the jig carries the USB-UART bridge and
  auto-program transistors. Saves ~$0.60/board and the castellated-edge
  surcharge; resolves the CH340N-has-no-DTR question by removing it.
- **USB-C CC sensing added (2026-08-25):** CC1/CC2 wired through 10 k
  (R10/R11) to ADC1 on GPIO36/39 so firmware can read the source's current
  advertisement and raise the LED brightness cap on 1.5 A/3 A supplies.
  Extra resistors alone cannot unlock more current — the source advertises,
  the sink measures.
- **Firmware framework locked: ESP-IDF (Steven, 2026-08-25).** Zephyr was
  considered and rejected — BLE-first stack, partial BR/EDR host, upstream
  deprecation proposal for Classic BT, and no supported A2DP path on ESP32;
  Bluedroid (ESP-IDF) has the proven A2DP source + SBC encoder.
- **Name change planned (Steven, 2026-08-25):** "mixxtape" collides with
  Mixxim's commercial product name; will be renamed before going public.
- `docs/firmware-plan.md` — firmware architecture and milestone plan for
  agent handoff: task layout, two-pass record pipeline, yank-safety rules,
  no-devkit de-risking order (host-tested core + early 2–3 board prototype
  as the bring-up vehicle), factory-test firmware, CC-sense power logic.
  Flags a storage-budget conflict (4-min ADPCM scratch ≈ 5.3 MB vs ~1 MB
  spare) to resolve on paper first.
- **MicroSD footprint added, DNP (Steven, 2026-08-25):** TF-01A push-push
  socket (**C91145**, ~$0.13 @ 500) as J4, the "studio edition" option for
  loading songs. SPI-mode wiring sharing VSPI with the audio flash:
  CS = IO22 (with DNP 10 k pullup R9 so a populated card stays deselected
  during flash access), card-detect = IO32 (needs internal pullup in
  firmware), DAT1/DAT2 unused. Default build unchanged — footprint only.
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
