# Line-out option — a 3.5 mm jack

> **Status: REJECTED, 2026-08-29.** Not being built. Kept as a decision record
> so the ground does not get gone over again — the parts, costs and pin budget
> below were all verified and remain accurate.
>
> Rejected on scope, not on feasibility. Everything below still stands, and
> **IO26, IO13 and IO14 are free again** — capacitive touch pads briefly took
> two of them and were removed. If this is ever revisited, the pin budget is
> intact.

The board's only output today is Bluetooth. This is the assessment of adding a
3.5 mm jack so it can also drive a powered amp, a pair of desk speakers, or
headphones on a wire.

The intended use is **the board out of its case** — you slide it out of the
Norelco shell, plug in, and it plays. That matters more than it sounds; see
[What it costs the design](#what-it-costs-the-design).

---

## The short version

| | |
|---|---|
| **Feasible?** | Yes, comfortably. Pins, board space and firmware structure are all already there. |
| **Cost** | About **$1/board**, on a board that currently costs $10.01 |
| **Effort** | ~1 hour schematic · half a day to a day of layout · ~1 day firmware |
| **Hard part** | Layout. The board is fully routed with zero DRC errors; adding a part means re-routing a region and re-checking the back artwork. |
| **Decision needed** | Whether headphones are a first-class use, which decides between shape A and shape B below. |

---

## Two shapes

Line-out and headphones look like the same jack, but they are different
electrical jobs. A line output wants ~2 V RMS into a high-impedance input. A
headphone output wants tens of milliwatts into 16–32 Ω. One part does not do
both well.

### Shape A — line-out only

```
tape_player_read() → I²S1 → PCM5102A → 3.5 mm jack
```

**Perfect for a powered amp or powered speakers.** The PCM5102A is a line
driver: 2.1 V RMS, designed for kΩ loads.

**Headphones will work, but quietly**, and will distort as you push them. Its
output stage is not specified to drive 32 Ω. That may be an acceptable
trade — it is the simplest possible version, one IC and one connector.

### Shape B — line-out and real headphone drive

```
tape_player_read() → I²S1 → PCM5102A → PAM8908 → 3.5 mm jack
```

Adds a 35 mW stereo Class-AB headphone amplifier. Drives 16–32 Ω properly, and
at low volume its output is a perfectly good line-out as well — which is how
every phone has ever solved this.

Costs one more IC, roughly four more passives, and a little more board area.

---

## Parts

All verified in JLCPCB stock. The project rule is ≥5,000.

| Role | Part | JLCPCB | Stock | Note |
|---|---|---|---|---|
| DAC | PCM5102APWR | [C107671](https://jlcpcb.com/partdetail/C107671) | 12,561 | $0.8872 @500. TSSOP-20. Config pins (FLT, DEMP, XSMT, FMT) all strap high or low — no I²C |
| Jack | PJ-320D | [C431535](https://jlcpcb.com/partdetail/C431535) | **79,274** | $0.037. **Surface mount**, 4 pads, 11.4 × 8.5 mm pad field |
| Headphone amp *(shape B only)* | PAM8908JER | [C33233](https://jlcpcb.com/partdetail/C33233) | 5,865 | 35 mW stereo Class-AB. Only just clears the stock rule — see the risk note below |

### Jack alternatives, all surface mount

| Part | JLCPCB | Stock | Pads |
|---|---|---|---|
| PJ-342 141 | [C7507452](https://jlcpcb.com/partdetail/C7507452) | 21,038 | 6 |
| PJ-327A 5JJ | [C668605](https://jlcpcb.com/partdetail/C668605) | 14,552 | 5 |
| PJ-320B | [C22355831](https://jlcpcb.com/partdetail/C22355831) | 5,413 | 3 |

These were confirmed surface-mount by reading their actual footprint data —
zero drilled holes, plain rectangular pads — not by trusting JLCPCB's category
field, which is unreliable for connectors. Watch for parts tagged 手工焊接
(*hand soldering*): JLCPCB will not machine-place those, which would break the
board's single-sided-SMT constraint.

### The part to avoid

**MAX98357A.** It is the default answer for ESP32 audio and it is wrong here.
Its output is **bridged** — both terminals swing, neither is ground. Wire that
to a 3.5 mm jack and the plug's sleeve shorts half the bridge. It is for a
soldered speaker, not a socket.

---

## Why this fits the board as it stands

Four things happen to already be true, which is most of why this is cheap.

**The GPIOs are free and unstrapped.** IO26, IO13 and IO14 — exactly the three
an I²S transmit needs. IO26 was freed when the microphone moved from I²S to
PDM, which needs two wires instead of three.

**The I²S peripheral is free.** The microphone runs on `I2S_NUM_0`, because PDM
receive is I2S0-only on this chip. **I2S1 has never been touched.** Recording
and playback are modal anyway, so they would not contend even if they shared.

**The firmware already has the right shape.** Playback is pull-based:

```c
uint32_t tape_player_read(tape_player_t *p, int16_t *dst, uint32_t samples);
```

and `bt_audio_set_source()` consumes exactly that signature. An I²S output task
calls *the same function*. This is not a coincidence to rely on twice — it is
worth keeping that way.

**Mono into both ears is already solved.** `bt_audio.c`'s data callback
duplicates every mono sample into interleaved stereo for A2DP. The I²S path
does the identical thing, so both channels of the DAC get signal.

---

## Where it goes

A 12.4 × 9.5 mm pocket (the PJ-320D plus a millimetre of clearance) was scanned
against every placed part, courtyard and board cutout:

| Edge | Free positions | Span |
|---|---|---|
| **Left** | **89** | y 6 → 50 |
| Bottom | 26 | x 4 → 16.5 |
| Top | 5 | x 22.5 → 24.5 |
| Right | **0** | blocked by J4, R12, C2 |

**The left edge is the natural home** and is wide open. The right edge is full —
that is the USB-C, regulator and microphone cluster.

The signal run from U1 to the left edge is roughly 40 mm, crossing the left reel
ring's LED chain. That is unremarkable for I²S at a few MHz, but it is real
routing work on a board that is currently finished.

---

## A freebie worth taking

The PJ-320D has a fourth pad. On jacks of this style it is normally a **switch
contact that opens when a plug is inserted** — which would let the firmware
auto-switch from Bluetooth to wire the moment you plug in, with no gesture.

**IO34 and IO35 are free and input-only.** No strapping risk, and input-only is
exactly what a detect line wants.

*Unverified:* that the fourth pad on this specific part is a detect switch
rather than a second sleeve or a TRRS ring. Confirm against the datasheet before
designing it in.

---

## Effort

| | |
|---|---|
| **Schematic** | ~1 hour. One TSSOP-20, one jack, ~8 passives. No I²C, no register configuration. |
| **Layout** | **Half a day to a day — this is the real cost.** The board is fully routed: 469 segments, 98 vias, zero DRC errors. Three new signals must cross ~40 mm, then zones re-pour, DRC re-runs, and the placed B-side artwork needs re-checking against any new vias. |
| **Firmware** | ~1 day. An I²S TX driver, output routing (Bluetooth / jack / both), a gesture to switch, volume. The look-ahead limiter already exists. Plus host tests for the routing logic. |
| **BOM and docs** | Free. The BOM is generated from the schematic. |

### Cost

| | Shape A | Shape B |
|---|---|---|
| DAC | $0.89 | $0.89 |
| Jack | $0.04 | $0.04 |
| Headphone amp | — | not priced |
| Extended-part setup fee | $3.00 one-off | $6.00 one-off |
| **Per board at qty 20** | **~$1.08** | **~$1.4 + amp** |

Against a current $10.01/board, shape A is roughly a **11 % increase**.

---

## What it costs the design

**The silhouette.** The premise is the exact cassette form factor — *the bare
board is the object*. A jack body is 11.4 × 8.5 mm and about 5 mm tall, and its
opening has to be reachable. Because the board comes out of the case to be
used, this only shows while it is in your hand rather than while it is being a
cassette on a shelf. That is a much smaller cost than it first appears, and it
is why the out-of-case decision matters.

**A second port with no case answer.** USB-C already has this open question — a
closed Norelco shell has walls all round. A jack makes it two. If the answer for
both is "take the board out", that is coherent; it should just be stated
somewhere a user will read it.

**It competes with the thing that already works.** "Pair it to any Bluetooth
earbuds or speaker" is in the first line of the README. The jack buys lower
latency, no pairing, and compatibility with anything that has a 3.5 mm input.
Those are real, but they are an addition to a solved problem, not a fix for a
broken one.

---

## Risks

**PAM8908 stock is thin.** 5,865 against a rule of 5,000. One supply wobble and
shape B needs a documented exception, the way the microphone already does. If
headphones matter, it is worth finding a second source before committing.

**Current budget.** The DAC is about 10 mA — irrelevant. A headphone amp at
volume is tens of milliamps — still small beside 29 LEDs. Neither threatens the
USB budget, which the firmware already caps by CC sensing.

---

## What was verified, and what was not

Verified against the design files and live JLCPCB data:

- IO26 / IO13 / IO14 free and unstrapped; IO34 / IO35 free and input-only
- Microphone on `I2S_NUM_0`, I2S1 unused
- `tape_player_read` / `bt_audio_set_source` signatures, and the mono-to-stereo
  duplication in the A2DP callback
- Board space, by scanning a real 12.4 × 9.5 mm envelope against every placed
  courtyard, pad and cutout
- Every part number, stock figure and price above
- That the listed jacks are surface mount, by reading their footprint pad data

Not verified:

- **That the PCM5102A is inadequate for 32 Ω headphones.** This follows from its
  role as a line driver and its datasheet load conditions, not from measurement.
  If someone has one on the bench, it is worth ten minutes.
- The PAM8908's price
- Whether the PJ-320D's fourth pad is a detect switch
- Footprint figures are pad-field extents, not body size or plug clearance

---

## The decision

**Is wired headphone listening a first-class use, or is this a line-out to
powered speakers that headphones happen to fit?**

Answer that and the shape follows. Everything else here is settled.


