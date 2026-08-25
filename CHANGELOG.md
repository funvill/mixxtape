# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project is pre-release; entries are dated rather than versioned until the
first prototype run.

## [Unreleased]

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
