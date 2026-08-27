# Mixxtape — a cassette you can actually record on

A 2-layer PCB in the exact form factor of a compact cassette (100.0 × 63.5 mm)
that slides into a real Norelco cassette case. The bare board **is** the object —
no enclosure, no bezel.

An onboard MEMS microphone records three 4-minute tracks. Hold REC to record,
release to stop. Pair it to any Bluetooth earbuds or speaker and press PLAY.

Cassette-shaped Bluetooth *players* exist. Nobody ships a cassette-shaped
**recorder** — and recording is the part that made a mixtape a mixtape.

## How you use it

- **Hold REC** — records. Release stops. Recording always overwrites the current
  track from zero — recording *is* erasing, same as tape.
- **PLAY** — plays all three tracks in order. It's a tape side, not three voice
  memos.
- **TRACK** — cycles track 1 → 2 → 3.
- **MODE (hold 2 s)** — pairing. Hold the board against your speaker; it picks
  the closest device by signal strength. No phone, no app, no list.
- Plug it in and it auto-reconnects to the last thing it paired with.

## Notable design features

- **Snap-off write-protect tab.** A mouse-bitten break-off PCB tab carries a
  trace to a GPIO. Snap it and the firmware refuses to record — permanently.
  Handing someone a tape becomes a ritual with a physical, irreversible step.
- **The reels show tape position.** Twelve RGB LEDs ring each routed-out reel
  window. During playback the left ring empties and the right ring fills, like
  tape transferring between reels. Red pulse tracks recording level.
- **Matte white solder mask** so you can sharpie the label on, with a ruled
  label block silkscreened where a cassette's paper label sits.
- **No battery.** USB-C only, and there is no charging circuit on the board
  at all. A cassette's promise is that you find it years later and it still
  works.
- **Jig-programmed.** No USB-UART on the board — USB-C is power only.
  Firmware loads through six pogo-pin pads via a programming jig.

## Hardware summary

| | |
|---|---|
| MCU | ESP32-WROOM-32E (original Xtensa ESP32 — the only ESP32 with Classic Bluetooth / A2DP) |
| Mic | MSM261S4030H0R I²S MEMS, mono, bottom-ported |
| Storage | W25Q128 16 MB external NOR flash, separate from module flash; 3 fixed slots, ~4 min each |
| Audio | 44.1 kHz mono, ADPCM-encoded at record time; playback decodes with table lookups and hands PCM to the Bluetooth stack |
| Indicators | 29 × WS2812B-2020 RGB (2 × 12 reel rings + track/REC/BT) |
| Controls | 4 tactile buttons + break-off write-protect tab |
| Power | USB-C only (power + CC sense, no data). No battery, no charger |
| Cost | ~$13.25/board at a run of 20 (~$265 total) |

## Project status

**Design locked, schematic and firmware core underway.** `hardware/` holds a
KiCad 9 project with a first-pass schematic (ERC-clean) and JLCPCB part
libraries. `firmware/` has the ESP-IDF skeleton plus the host-tested core —
crash-safe slot manager, ADPCM codec, playback sequencer, reel display,
pairing policy, bond table and record limiter — all proven without hardware,
since there is no devkit. Target: a prototype run of 3–5 boards, then a full
run of 20. What is left needs a board in hand: the SPI flash, I²S capture,
the A2DP plumbing, LED output, and A2DP against real earbuds.

## Documentation

**Before ordering boards:**

- [Pre-production checklist](docs/PRE-PRODUCTION-CHECKLIST.md) — the
  case-fit test, open decisions, and what to re-quote
- [Case-fit test sheet](docs/case-fit-test-1to1.pdf) — print at 100%, cut
  out, put in a real cassette case

**For people using one:**

- [Manual](docs/MANUAL.md) — how to record, play, pair, and what the lights mean
- [FAQ](docs/FAQ.md) — including the honest limitations

**For people building or hacking one:**

- [Project write-up](docs/cassette-project-writeup.md) — the pitch, design
  rationale, and open questions
- [Agent handoff brief](docs/cassette-recorder-agent-brief.md) — the
  authoritative spec: locked decisions, GPIO map, behaviour spec, phase order
- [BOM v4](docs/cassette-recorder-bom-v4.md) — generated from the schematic
  with live JLCPCB prices and stock ([v3](docs/cassette-recorder-bom-v3.md)
  is superseded)
- [Prior art](docs/prior-art.md) — research on cassette-form-factor projects
- [Firmware plan](docs/firmware-plan.md) — architecture, milestones, and
  hardware interface for the firmware agent

## Prior art

- **Mixtape Alpha** (Open Music Labs with Jie Qi / MIT Media Lab, 2012) — a
  cassette-shaped ATmega328p synth; the bare-PCB-as-object philosophy comes
  straight from it.
- **Mixxtape** (Mixxim) — the commercial cassette-shaped Bluetooth *player*.
  The thing this project is not.
