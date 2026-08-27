# Firmware plan — agent handoff

**Audience:** the agent (or person) implementing firmware in `firmware/`.
**Read first:** `docs/cassette-recorder-agent-brief.md` (product spec, locked
decisions, traps). This document adds the firmware architecture, the current
hardware interface (which has evolved from the brief — this doc wins where
they differ), and a milestone order designed around one hard constraint:
**there is no devkit.** The first prototype PCBs are the bring-up vehicle,
so everything that can be de-risked on a host PC must be done before boards
arrive.

---

## 1. Constraints (do not violate)

- **Target:** ESP32-WROOM-32E (original Xtensa ESP32 — the only ESP32 with
  Classic Bluetooth / A2DP). Framework: **ESP-IDF** (A2DP source and SBC
  need IDF-level APIs; Arduino layer optional on top, not required).
  **[LOCKED — Steven, 2026-08-25]** Zephyr was evaluated and rejected: its
  BT stack is BLE-first with only partial BR/EDR host support, upstream has
  proposed deprecating BR/EDR entirely (zephyr#67145), and there is no
  supported Classic-BT/A2DP path on ESP32 under Zephyr — the proven A2DP
  source + SBC encoder live in Bluedroid, which is ESP-IDF-only. Do not
  revisit. Keep everything outside the BT/audio core behind the mockable
  HAL (M2) so the framework dependency stays contained.
- **Playback must stay cheap.** ESP-IDF's A2DP source takes **PCM** and
  encodes SBC inside Bluedroid; there is no API for pre-encoded frames
  (verified in M3 — see `docs/storage-budget.md`). The tape therefore
  stores block-framed IMA ADPCM and playback is read-block, decode with a
  few table lookups per sample, hand over PCM. No resampling, no filter
  banks, no transforms. If you find yourself doing real DSP at playback,
  something upstream is wrong.
- **Encode at record time**, one pass: PDM → PCM → ADPCM block → flash.
  44.1 kHz mono throughout — matches A2DP negotiation, eliminates
  resampling. The PDM-to-PCM decimation happens inside the ESP32's I²S
  peripheral, so it costs no CPU.
- **PDM downsample ratio must be 64, not 128.** PDM clock = sample rate x
  ratio, and 44100 x 128 = 5.64 MHz is above the microphone's 4.8 MHz
  maximum. 64 gives 2.8224 MHz, comfortably inside its window.
- **Recording must survive a USB yank at any moment.** No battery, no
  graceful-shutdown window. Only the slot being written may be corrupted.
  Sector erases (~40 ms) never happen on the record critical path —
  background pre-erase at idle; record issues page programs only (~0.7 ms).
- **Recording is erasing.** Hold REC always overwrites the current slot
  from zero. No delete function. Snapped write-protect tab (GPIO21 low) →
  refuse all recording, permanently.
- **PLAY plays all three tracks in order** — it's a tape side.
- Storage: 3 fixed slots + header region on the W25Q128 (16 MB). No
  filesystem, no dynamic allocation, no compaction.

## 2. Hardware interface (rev 0.1 schematic — supersedes brief §5)

| Signal | GPIO | Notes |
|---|---|---|
| PDM CLK → mic | 25 | MSM261DHT006, mono, L/R tied low, top-ported |
| PDM DATA ← mic | 33 | |
| VSPI SCK / MISO / MOSI | 18 / 19 / 23 | Shared: audio flash + (DNP) microSD |
| Audio flash CS | 5 | W25Q128JVSIQ; 10k pullup |
| microSD CS *(DNP)* | 22 | 10k pullup (DNP); SPI mode |
| microSD card-detect *(DNP)* | 32 | Switch to GND; **enable internal pullup** |
| WS2812 data | 27 | Via SN74AHCT1G125 to 5 V; drive with **RMT peripheral** |
| SW1 REC | 0 | Boot strap — safe as active-low button |
| SW2 PLAY | 4 | Short = play/pause, long = sleep |
| SW3 TRACK | 16 | Cycle 1→2→3 |
| SW4 MODE | 17 | 2 s pair · 5 s dub · 10 s dev WiFi |
| Write-protect tab | 21 | External 10k pulldown; intact = high = record OK |
| CC1 sense | 36 (SENSOR_VP) | ADC1; see §6 power management |
| CC2 sense | 39 (SENSOR_VN) | ADC1 |
| UART0 TX / RX | 1 / 3 | To J5 jig pads only — **no USB-UART on board** |
| Line-in *(DNP)* | 35 | Unpopulated |
| Free | 12*, 13, 14, 15, 2, 26 | *IO12 is MTDI strap — must be low at boot, avoid. IO26 freed by the move from I²S to PDM |

**Changes vs the brief:** CH340N and USB data path deleted (USB-C is power
only); castellated header deleted; programming is via the 6-pad pogo jig
(J5: 3V3, GND, EN, IO0, TX, RX); SENSOR_VP/VN now sense USB-C CC; IO22/IO32
now assigned to the DNP microSD.

## 3. Programming & the jig

- The **jig carries the USB-UART bridge** (CH340C/CP2102 or similar) and the
  classic two-transistor EN/IO0 auto-program circuit, driving the J5 pads
  with pogo pins. The board itself has none of this.
- Flash with `esptool` / `idf.py flash` through the jig. Design a **factory
  test firmware** early (§7 milestone M5): exercises mic, flash, LEDs,
  buttons, tab-sense, BT inquiry — pass/fail over UART so 20 boards take
  minutes each.
- Post-assembly reflash for end users without a jig: none by default —
  consider the 8BitMixtape-style audio bootloader as a stretch goal only.

## 4. Architecture

### Tasks (FreeRTOS, suggested)

| Task | Core | Prio | Job |
|---|---|---|---|
| `audio_capture` | 1 | high | I²S-PDM DMA drain → ring buffer (record only) |
| `encoder` | 1 | med | Limiter + ADPCM block encode, straight to the slot |
| `a2dp_stream` | 0 | high | Decode blocks ahead of the A2DP PCM callback (playback) |
| `storage` | 1 | med | Slot manager, page programs, background pre-erase queue |
| `ui` | 0 | low | Buttons (debounce, short/long), state machine |
| `leds` | 0 | low | RMT frames ~30 fps from a state+progress struct |

BT stack (Bluedroid) lives on core 0; keep the audio/flash pipeline on
core 1. All inter-task communication via queues; no shared mutable state
without a mutex.

### Record pipeline (one pass, as built)

Nothing clever happens in real time.

1. REC held → I²S-PDM DMA → PCM ring → look-ahead limiter → **ADPCM block**
   encode → block ring → `tape_store_write()`. Page programs only; the
   erase frontier runs one 64 KiB block ahead of the write pointer, so a
   slot erase never sits on the critical path.
2. REC released → `tape_store_end_record()` truncates to whole blocks and
   commits a single header record. No pass 2, no wait, no scratch region.
3. A yank at any point leaves the take truncated to its last complete
   block and still playable. Recovery is `tape_store_mount()`'s job and is
   covered by the power-yank sweep in `test/host`.

Level control is a look-ahead limiter (~100 ms in RAM) rather than
whole-take peak normalisation: one pass cannot know the peak in advance,
and a limiter is the better tool for a mic capturing a room anyway.

The M1 storage conflict this replaced, and the M3 finding that settled the
stored format, are both written up in `docs/storage-budget.md`.

### Playback pipeline

`tape_store_read()` a block → `adpcm_block_decode()` → PCM into a small
ring → the A2DP source data callback drains it. Bluedroid encodes SBC.

The callback runs on the BT task and **must not block**: it is handed a
buffer and a length, returns how many bytes it filled, and a length of -1
means flush. So the decode must always be running ahead of it, never
inside it. Blocks are independently decodable, so seeking (track skip,
resume after pause) is a block-index calculation, not a re-scan.

### Pairing / reconnect (brief §6 has the full flow)

A2DP **source** role. MODE 2 s → `esp_bt_gap_start_discovery()` → filter by
Class-of-Device (Audio/Video major; headphone/speaker minor) → filter RSSI
> ~−50 dBm (proximity = selection) → connect, Just Works SSP, bond. Store
4–8 bonds; on power-up try last-used then others, with retry + backoff
(the sink is usually busy reconnecting to a phone — honest LED feedback
after ~15 s). **Measure plug-in→audio latency from day one**; if it's ~8 s
the UX needs rethinking and Steven needs to know.

### Dub (MODE 5 s) — design later, stub now

Board-to-board transfer. Candidate transports: BT (SPP/A2DP between two
boards) or audio itself (8BitMixtape's TinyAudioBoot precedent). Leave a
stub state; don't architect around it yet.

### Dev WiFi (MODE 10 s)

WiFi + Classic BT coexistence on the original ESP32 is poor during A2DP.
Dev mode must **stop streaming first**, then bring up WiFi (logs/OTA).
Never run both.

## 5. LED language (drive via RMT, global brightness cap)

| State | Reels |
|---|---|
| Searching | Amber sweep |
| Connected | Green lock |
| Playing | White: left ring empties, right fills (position = tape metaphor) |
| Recording | Red pulse, brightness = VU level |
| Background erase | Slow reverse spin, dim |
| Dubbing | Violet |
| Sink busy (>15 s) | Slow red pulse |

Plus D25–D27 track indicators, D28 REC, D29 BT status.

## 6. Power management (new — CC sense)

Default assumption: **500 mA budget** (USB 2.0 default). Firmware reads CC1
and CC2 via ADC1 (GPIO36/39, through 10 k). With our Rd = 5.1 k, the live CC
line sits at ≈0.41 V (default 500 mA), ≈0.92 V (1.5 A), ≈1.68 V (3 A). One
line carries the voltage, the other is near 0 — read both, use the higher.

- Budget 500 mA → LED global brightness cap ~15–20 %, never all-white.
- Budget ≥1.5 A → cap may rise to ~25–30 %.
- Re-read on boot only (cable doesn't change mid-session; keep it simple).
- ESP32 ADC is noisy — average 16+ samples; thresholds mid-band (~0.66 V,
  ~1.23 V).

## 7. Milestones — ordered for no-devkit de-risking

**Phase A: host-only (before any hardware exists)**

- **M0 — Skeleton ✅ done.** ESP-IDF project in `firmware/`, pin map in
  `main/include/board.h`, `sdkconfig.defaults` configured for Classic-BT /
  A2DP source with BLE off, CI in `.github/workflows/firmware.yml` (host
  tests + IDF build).
- **M1 — Paper math ✅ done.** `docs/storage-budget.md`. Resolved the §4
  conflict: the two-pass ADPCM scratch cannot coexist with three 5 MiB
  slots, so the record path is now **one pass**. M3 then settled the stored
  format as ADPCM (see below). **Needs Steven's sign-off** — one pass means
  a look-ahead limiter instead of whole-take peak normalisation, and slots
  grew to 5.125 MiB to keep the four-minute take.
- **M2 — Host-testable core ✅ done.** `components/tape` (slot manager,
  append-only header log with CRC + generation, ADPCM block codec,
  tape-side player), `components/ui` (button grammar and device state
  machine) and `components/leds` (reel rendering), all behind
  `flash_hal.h`. `test/host` has a NOR mock with power-loss tearing and a
  sweep that tears every flash operation of a recording in turn, plus a
  power-budget sweep proving no LED mode exceeds its share of the USB
  budget (119 mA worst case at the default cap). 7 suites, ~58,000
  assertions, green.
- **M3 — Reference study ✅ done.** The decisive finding: ESP-IDF's A2DP
  source callback is fed **PCM**, and Bluedroid encodes SBC internally with
  no API for pre-encoded frames. Stored format changed to block-framed IMA
  ADPCM; see `docs/storage-budget.md`. Still to mine from the reference
  code: the callback's buffer-depth and flush (`len == -1`) semantics, which
  shape the playback prefetch task.

**Phase B: first prototype boards (order 2–3 early, before the run of 20 —
at ~$14/board they are the devkit)**

- **M4 — Bring-up:** power rails, UART over jig, flash JEDEC ID, mic I²S
  capture to UART dump, LED chain, buttons, tab, CC sense readings.
- **M5 — Factory test firmware — harness ✅ done, probes gated on M4.**
  `components/factory` sequences the suite, aggregates results and emits
  both an operator table and a one-line machine record; host-tested with
  mock probes (every step runs after a failure, skips do not fail a board,
  interactive steps skip on unattended runs, reports never overflow their
  buffer). `main/factory_main.c` holds the board-side probes: tab sense,
  buttons and the SPI JEDEC ID read are written; mic, LED walk, CC sense,
  flash read/write and BT inquiry are stubs that report SKIP until the M4
  drivers exist, and the runner prints SUITE INCOMPLETE while any remain.
  Build with `idf.py -DMIXXTAPE_FACTORY_TEST=ON`.
- **M6 — The risk spikes**, in order: (1) *retired by M3* — we no longer
  encode SBC ourselves, and ADPCM encode is a handful of ops per sample;
  what remains is confirming PDM capture keeps up with flash writes;
  (2) A2DP source vs a basket of real sinks
  (AirPods, cheap TWS, a BT speaker) — connect rate, CoD filtering, RSSI
  threshold tuning; (3) reconnect latency, measured. Each answers a
  question the brief says can kill the design.
- **M7 — Integration:** full record→normalize→encode→play loop, LED
  language, sleep, bonds. Then the run of 20.

**Anything M6 breaks goes back to Steven before the 20-board order.**

## 8. Out of scope for now

- Dub transport implementation (stub only)
- Audio bootloader (stretch)
- microSD "studio edition" (hardware is DNP; leave a mount point in the
  storage HAL so it can slot in later)
- Line-in, battery operation (DNP)

## 9. References

- ESP-IDF A2DP source example: `examples/bluetooth/bluedroid/classic_bt/a2dp_source`
- pschatzmann ESP32-A2DP: <https://github.com/pschatzmann/ESP32-A2DP>
- 8BitMixtape TinyAudioBoot (audio bootloader precedent):
  <https://github.com/8BitMixtape/8Bit-Mixtape-NEO>
- W25Q128JV datasheet (page program 0.7 ms typ, sector erase 45 ms typ)
- USB Type-C spec §4.5.2 (Rp/Rd CC voltages)
