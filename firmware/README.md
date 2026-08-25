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

Not yet written, all gated on prototype boards (M4): the SPI flash HAL,
I²S capture, the record-side limiter, A2DP source and pairing, WS2812
output via RMT, and CC sensing.
