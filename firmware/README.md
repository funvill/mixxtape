# Mixxtape firmware

ESP-IDF firmware for the cassette recorder. Read
[`docs/firmware-plan.md`](../docs/firmware-plan.md) first — it holds the
architecture, the hardware interface, and the milestone order. Storage
arithmetic lives in [`docs/storage-budget.md`](../docs/storage-budget.md).

**Framework is ESP-IDF and that is [LOCKED]** — Classic Bluetooth / A2DP
source exists only in Bluedroid. See the firmware plan for why Zephyr was
rejected.

## Layout

```
components/tape/   slot manager, ADPCM codec, tape-side player (portable C)
components/ui/     button grammar + device state machine     (portable C)
components/leds/   reel display rendering                    (portable C)
components/factory/ factory-test harness                     (portable C)
components/btaudio/ pairing, bonds, A2DP source                (mixed)
                   NB: must NOT be called `bt` — that is ESP-IDF's own
                   component name, and ours would shadow it
components/audio/  record-path look-ahead limiter            (portable C)
main/              ESP32 entry point and pin map
test/host/         host unit tests + NOR flash mock
```

Everything with real logic in it is portable C behind `flash_hal.h`, so it
compiles and is tested on a PC. There is no devkit for this project, so the
target-specific layer is kept as thin as possible and the logic that can
be proven without hardware is proven without hardware.

## Host tests

No ESP-IDF needed — any C compiler and CMake:

```bash
cmake -S firmware/test/host -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

The centrepiece is the **power-yank sweep** in `test_tape_store.c`. The mock
flash models a NOR part honestly (erase sets bits, program only clears them,
torn writes land half the data) and can tear any chosen operation. The sweep
tears *every* flash operation of a recording in turn, "re-plugs" the device,
and re-checks the invariants:

- no slot is left mid-recording
- an untouched take is byte-identical
- the interrupted slot is either a whole-frame prefix of the new take, or
  the previous take complete and intact — never stale audio spliced on
- nothing is ever programmed over non-erased flash
- the device still records afterwards

This matters because there is no battery: a yanked USB cable is the normal
way this device powers off.

## Factory test (M5)

Twenty boards get assembled by hand, so each one is probed on the jig
before production firmware goes on:

```bash
idf.py -C firmware -DMIXXTAPE_FACTORY_TEST=ON build flash monitor
```

It probes every subsystem, prints an operator table, and emits one
greppable line:

```
FT|1|flash_id=PASS:0xEF4018|mic=PASS:1832|tab=PASS:1|bt=PASS:3|VERDICT=PASS
```

Log the jig's serial output to a file and that file is the build record for
the run — when board 14 turns out flaky three months later, you can look it
up instead of guessing.

Two properties matter and are enforced by tests:

- **Every step runs, even after one fails.** A board with a dead mic *and*
  a dead flash reports both faults on one pass, not one fault per reflash.
- **Results carry measured values, not just verdicts.** `mic=PASS:1832`
  lets you spot the one board reading 40 before it ships.

Interactive steps (LED walk, button presses) are skipped automatically on
an unattended run rather than blocking the line.

**Probes are not implemented yet.** The harness is host-tested; the probes
that need drivers M4 has still to write report `SKIP` with a `TODO M4`
detail, and the runner prints `SUITE INCOMPLETE` rather than a pass banner
while any remain. `main/factory_main.c` is the bring-up checklist.

## Target build

Needs ESP-IDF v5.x:

```bash
idf.py -C firmware build
```

Flashing is via the **J5 pogo pads and a jig** — there is no USB-UART on the
board (USB-C is power only). The jig carries the bridge and the EN/IO0
auto-program transistors.

## State

M0-M3 of the plan are done: project skeleton, storage budget resolved,
the host-tested core, and the reference study that settled the stored
format (ADPCM, not SBC — ESP-IDF's A2DP source takes PCM only). Playback
sequencing and the reel display are written and tested too.

Pairing *policy* and the bond table are written and tested too — what is
missing there is only the Bluedroid plumbing that feeds them.

Not yet written, all gated on prototype boards (M4): the SPI flash HAL,
PDM capture, the A2DP source and discovery callbacks, WS2812 output via
RMT, and CC sensing. Every one of those is a driver — the logic they feed
is already written and tested.
