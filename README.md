# Mixxtape — a cassette you can actually record on

A 2-layer PCB in the exact form factor of a compact cassette (100.0 × 63.5 mm)
that slides into a real Norelco cassette case. The bare board **is** the object —
no enclosure, no bezel.

An onboard MEMS microphone records three 4-minute tracks. Hold REC to record,
release to stop. Pair it to any Bluetooth earbuds or speaker and press PLAY.

Cassette-shaped Bluetooth *players* exist. Nobody ships a cassette-shaped
**recorder** — and recording is the part that made a mixtape a mixtape.

![The Mixxtape board turning, front and back. The front carries the ESP32, two
rings of RGB LEDs around the routed-out reel windows, four buttons and a USB-C
port; the back is covered edge to edge in a peony line-art silkscreen.](artwork/mixxtape.gif)

<sub>3D render from KiCad. The mask reads green here because that is the
viewer's default — the board ships **matte white**, which is what makes the
sharpie label work.</sub>

![A cassette and its paper insert, both carrying the same black peony line-art
on an off-white ground, the insert showing a blank ruled label
box.](artwork/case-mockup.png)

<sub>Artwork mockup of the B-side design and the J-card insert. The board is
the object; this shows where the artwork is going.</sub>

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

Every fitted part is stocked at JLCPCB. The project rule is a minimum of 5,000
in stock, so a run never stalls on one line item — with a single documented
exception: **the microphone sits at ~3,500**, because no digital MEMS
microphone at JLCPCB clears the bar, and the one that does is analogue and
would wreck the audio path.

| Block | Part | JLCPCB | Qty | Notes |
|---|---|---|---|---|
| MCU | ESP32-WROOM-32E-N4 | [C701341](https://jlcpcb.com/partdetail/C701341) | 1 | The original Xtensa ESP32 — the only one with Classic Bluetooth, so the only one that can be an A2DP *source* |
| Microphone | MSM261DHT006 | [C51928215](https://jlcpcb.com/partdetail/C51928215) | 1 | **PDM** digital MEMS, mono, **top-ported** — sound enters from the component side, so there is no hole through the board |
| Storage | W25Q128JVSIQ | [C97521](https://jlcpcb.com/partdetail/C97521) | 1 | 16 MB NOR flash, separate from the module's own flash. Three fixed slots, ~4 min each |
| Indicators | XL-2020RGBC-2812B | [C5349955](https://jlcpcb.com/partdetail/C5349955) | 29 | WS2812-compatible RGB. Two 12-LED reel rings, three track lamps, REC, BT |
| LED data buffer | SN74AHCT1G125 | [C7484](https://jlcpcb.com/partdetail/C7484) | 1 | Lifts the ESP32's 3.3 V data line to the LEDs' 5 V rail |
| Regulator | AMS1117-3.3 | [C6186](https://jlcpcb.com/partdetail/C6186) | 1 | 5 V USB down to 3.3 V |
| USB-C | TYPE-C-31-M-12 | [C165948](https://jlcpcb.com/partdetail/C165948) | 1 | Power and CC sensing only — no data lines |
| Buttons | TS-1187A-B-A-B | [C318884](https://jlcpcb.com/partdetail/C318884) | 4 | REC, PLAY, TRACK, MODE |
| Write-protect tab | — | — | 1 | Not a part. A mouse-bitten PCB tongue carrying a net tie; snap it and recording is disabled for good |
| Programming pads | — | — | 1 | Six bare pogo targets. No USB-UART on the board |

Passives are all JLCPCB basic-library parts:
[22 µF](https://jlcpcb.com/partdetail/C45783) ×2,
[10 µF](https://jlcpcb.com/partdetail/C15525),
[1 µF](https://jlcpcb.com/partdetail/C29266) (the ESP32's EN delay),
[100 nF](https://jlcpcb.com/partdetail/C1525) ×12,
[10 k](https://jlcpcb.com/partdetail/C25804) ×7,
[5.1 k](https://jlcpcb.com/partdetail/C25905) ×2 (the USB-C CC pull-downs) and
[100 Ω](https://jlcpcb.com/partdetail/C25076) (microphone supply isolation).

| | |
|---|---|
| Audio | 44.1 kHz mono, ADPCM-encoded at record time; playback decodes with table lookups and hands PCM to the Bluetooth stack |
| Power | USB-C only, power and CC sense. No battery, no charger |
| Board | 2-layer, 100.33 × 63.50 mm, matte white solder mask with black silkscreen |
| Cost | **$10.01/board** at a run of 20 — $182.13 in parts plus $18.00 of setup fees for six extended-library parts |

Full detail, with live stock and pricing, is in the
[generated BOM](docs/cassette-recorder-bom-v4.md) — it is derived from the
schematic, so it cannot drift out of step with it.

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

- [Cassette insert template](docs/jcard-template-1to1.pdf) — double-sided
  J-card for the case, 1:1, print at 100% ([side 1](docs/jcard-template-side1.svg), [side 2](docs/jcard-template-side2.svg))
- [B-side silkscreen brief](docs/b-side-silkscreen-brief.html) — the design
  brief for the blank back face, with dimensioned keepouts and a 1:1
  [template](hardware/mixxtape-back-silkscreen-template.svg)
- [Documentation index](docs/README.md) — everything below, organised
- [Manual](docs/MANUAL.md) — how to record, play, pair, and what the lights mean
- [FAQ](docs/FAQ.md) — including the honest limitations

**For people building or hacking one:**

- [Project write-up](docs/cassette-project-writeup.md) — the pitch, design
  rationale, and open questions
- [Agent handoff brief](docs/cassette-recorder-agent-brief.md) — the
  authoritative spec: locked decisions, GPIO map, behaviour spec, phase order
- [Bill of materials](docs/cassette-recorder-bom-v4.md) — generated from the
  schematic with live JLCPCB prices and stock, so it cannot drift
- [Prior art](docs/prior-art.md) — research on cassette-form-factor projects
- [Firmware plan](docs/firmware-plan.md) — architecture, milestones, and
  hardware interface for the firmware agent

## Prior art

- **Mixtape Alpha** (Open Music Labs with Jie Qi / MIT Media Lab, 2012) — a
  cassette-shaped ATmega328p synth; the bare-PCB-as-object philosophy comes
  straight from it.
- **Mixxtape** (Mixxim) — the commercial cassette-shaped Bluetooth *player*.
  The thing this project is not.
