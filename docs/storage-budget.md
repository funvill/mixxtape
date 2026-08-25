# Storage & buffer budget (firmware M1)

Resolves the conflict flagged in `docs/firmware-plan.md` §4: the brief's
two-pass record pipeline needs an ADPCM scratch region that does not fit
alongside three 5 MiB slots in a 16 MiB flash.

> ## ⚠ Revised after the M3 reference study: the tape stores ADPCM, not SBC
>
> **ESP-IDF's A2DP source accepts PCM only.** The API reference is explicit
> — `esp_a2d_source_data_cb_t` receives a "buffer to be filled with PCM data
> stream from higher layer", and "for now, the input should be PCM data
> stream". Bluedroid performs the SBC encoding itself, and exposes **no API
> for handing it pre-encoded SBC frames**.
>
> So the brief's "playback is read-frames-and-transmit" cannot be built as
> written: storing SBC would force us to *decode* SBC to PCM at playback
> purely so the stack could re-encode it — the exact real-time DSP the
> design forbids, plus a generation of extra loss.
>
> **The tape therefore stores block-framed IMA ADPCM.** Playback reads a
> block, decodes it with a few table lookups per sample, and hands PCM to
> Bluedroid. That is far cheaper than SBC decode and honours the intent of
> the locked decision better than SBC ever could. Recording is symmetric:
> I²S PCM → ADPCM → flash, and ADPCM encode is trivially cheap, which also
> retires the "does SBC encode fit the real-time budget" risk (M6 #1).
>
> Consequences: sections 1–4 below are the *original* SBC analysis, kept
> because the air-format numbers still govern what pairing negotiates.
> Sections 5 onward describe what is actually built. Slot size grew from
> 5 MiB to 5.125 MiB (82 erase blocks) to keep the full four-minute take —
> a deliberate, flagged deviation from the locked "3 x 5 MB slots", which
> preserves that decision's substance (three fixed slots, no dynamic
> allocation) and the 4-minute promise the flash size was chosen around.

All sizes are **binary** (MiB = 1,048,576 B). Flash is a W25Q128JV:
16,777,216 B, 256 B program page, 4 KiB sector erase, 64 KiB block erase.

---

## 1. Audio rates

Source: 44,100 Hz, mono, 16-bit → **88,200 B/s** raw PCM.

**SBC frame length** (A2DP/SBC spec, MONO channel mode):

```
frame_bytes = 4 + (4 x subbands x channels)/8 + ceil(blocks x channels x bitpool / 8)
```

With the standard config — 8 subbands, 16 blocks, mono, Loudness allocation —
this reduces to `frame_bytes = 8 + 2 x bitpool`, at
`44100 / (16 x 8) = 344.531` frames/s.

4:00 is 82,687 whole frames (`floor(240 x 44100 / 128)` — the exact value is
82,687.5, and a partial frame is not playable), so stored sizes are that
count times the frame length:

| bitpool | frame B | B/s | kbps | 4:00 (B) | 4:00 (MiB) |
|---|---|---|---|---|---|
| 16 | 40 | 13,781 | 110 | 3,307,480 | 3.15 |
| 19 | 46 | 15,848 | 127 | 3,803,602 | 3.63 |
| 20 | 48 | 16,537 | 132 | 3,968,976 | 3.79 |
| **26** | **60** | **20,671** | **165** | **4,961,220** | **4.73** |
| 31 | 70 | 24,117 | 193 | 5,788,090 | 5.52 |

These are the numbers `sbc_frame.c` produces and `test_sbc_frame.c` asserts;
integer truncation is deliberate so firmware and this table cannot drift.

IMA-ADPCM scratch (4 bits/sample) = 22,050 B/s → **4:00 = 5,292,000 B
(5.05 MiB)**.

## 2. The conflict, quantified

Brief's locked layout: 3 x 5 MiB slots + header region.

```
header (64 KiB block)            65,536 B
3 x 5 MiB slots             15,728,640 B
                            -----------
subtotal                    15,794,176 B
flash total                 16,777,216 B
remaining for scratch          983,040 B   (960 KiB)
ADPCM scratch needed         5,292,000 B   (5.05 MiB)
DEFICIT                      4,308,960 B   (4.11 MiB)
```

960 KiB of scratch buys **44 seconds** of ADPCM, not 4 minutes.

## 3. Options considered

**A — Scratch overlays the target slot.** Recording already invalidates the
slot, so pass 1 could write ADPCM into slot N itself. Fails twice: ADPCM
(5,292,000 B) overflows a 5 MiB slot by 49,120 B, and pass 2 would have to
read ADPCM from and write SBC into the same region — a yank mid-encode
destroys both the scratch and the take.

**B — Dedicated scratch, smaller slots.** 5.25 MiB scratch + 64 KiB header
leaves 3.56 MiB per slot, which caps SBC at bitpool 18 (121 kbps). Violates
the locked "3 x 5 MiB slots" layout and costs audio quality.

**C — Keep 5 MiB slots, scratch in the 960 KiB remainder.** Caps recording
at 44 s. Not viable.

**D — One-pass: encode SBC directly during capture. ✅ RECOMMENDED**
No ADPCM scratch at all. The conflict disappears, and every locked decision
is satisfied *more* directly.

## 4. Recommendation: one-pass (Option D)

| | Two-pass (brief §6) | One-pass (D) |
|---|---|---|
| Scratch region | 5.05 MiB (doesn't fit) | none |
| Locked 3 x 5 MiB layout | violated | **honoured exactly** |
| Max bitpool at 4:00 | 18 (121 kbps) | **26 (165 kbps)** |
| Flash writes per take | 2x (ADPCM + SBC) | **1x** |
| Wait after REC release | ~10-20 s pass-2 | **none** |
| Yank during record | scratch lost, take lost | **take survives, truncated** |
| Level control | peak normalise (whole take) | look-ahead limiter (~100 ms) |

The only real loss is whole-take peak normalisation. A look-ahead limiter in
RAM is standard practice for live recorders and is the right tool for a mic
capturing a room anyway — it also can't be fooled by one loud transient the
way peak normalisation can.

The yank behaviour is a genuine *improvement* and very on-theme: pull the
cable mid-record and you keep what was recorded, exactly like real tape.

> **⚠ Needs Steven's sign-off.** The brief's §6 two-pass pipeline is
> descriptive prose, not a row in the LOCKED table — and the LOCKED row it
> serves ("encode to SBC at record time") is satisfied more literally by
> one-pass. Firmware defaults to D, with the layout and record policy behind
> compile-time constants so reverting is cheap. The ADPCM codec is
> implemented and tested regardless (small, and needed if two-pass returns).

## 5. Chosen layout (`tape_layout.h`) — as built

Stored format: **block-framed IMA ADPCM**, 4 bits/sample, 44.1 kHz mono.

One block is 512 B: a 4-byte header (predictor int16, step index, payload
checksum) plus 508 B of nibbles = 1016 samples = **23.04 ms**. Overhead is
0.8 %, giving **22,223 B/s** (`44100 * 512 / 1016`, truncated).

Blocks earn their keep three times over:

- **Seeking is free.** Each block carries its own codec state, so playback
  can start at any block — which track skip, pause/resume and the reel
  position display all need.
- **Erased flash is unmistakable.** A valid header needs a step index ≤ 88;
  erased flash reads 0xFF. Crash recovery scans for the first invalid block.
- **Torn writes are detectable.** The payload checksum catches a block whose
  header landed but whose data was cut short by a power loss — without it,
  recovery keeps 23 ms of noise. (This was a real bug, caught by the sweep.)

```
offset          size          region
0x000000        64 KiB        header (sectors 0/1 used as ping-pong log)
0x010000        5.125 MiB     slot 0
0x530000        5.125 MiB     slot 1
0xA50000        5.125 MiB     slot 2
0xF70000        576 KiB       spare / reserved
                              (future: bond table, wear relief)
```

Each slot is 82 erase blocks — the smallest whole-block size holding a full
four-minute take. Slot erase is therefore always whole-block, and a slot is
a whole number of ADPCM blocks with no runt at the end.

**Track length:** 5,373,952 / 22,223 = **4:01.8 max**; a nominal 4:00 take
uses 5,333,760 B (99.3 % of a slot).

## 6. Header log & crash safety

- Two 4 KiB sectors used as a **ping-pong append-only log** of 64-byte
  records (64 records/sector). Writing a record is one page program; when a
  sector fills, erase the other and continue there. ~64x fewer erases than a
  naive A/B rewrite scheme.
- Each record: magic, generation counter, per-slot state/length/duration,
  SBC config, CRC32. Mount picks the valid record with the highest
  generation; a yank mid-write leaves the previous record intact.
- **Partial-recording recovery:** a slot left in `RECORDING` state is
  repaired at mount by binary-searching for the first fully-erased page,
  walking back to the last complete SBC frame (syncword 0x9C, fixed frame
  length), and committing it as `VALID` with that length.

## 7. Erase strategy

A 5 MiB slot is 80 x 64 KiB block erases (~150 ms typ, 1 s max each) —
12 s typical, so it can never sit on the record critical path.

1. Preferred: background pre-erase at idle after a slot is invalidated;
   header tracks an `erased` flag per slot.
2. Fallback: just-in-time erase **one block ahead** of the write pointer.
   Writing a 64 KiB block at 20.7 KB/s takes 3.2 s, comfortably longer than
   even a worst-case 1 s erase, and the SBC ring absorbs the stall.

## 8. RAM buffers

Buffer the *encoded* stream, not PCM — SBC is 4.3x smaller, so the same
stall tolerance costs a quarter of the RAM.

| Stage | Size | Covers |
|---|---|---|
| I²S DMA (PCM) | 8.8 KB (100 ms) | DMA/scheduling jitter |
| ADPCM ring (post-encoder) | 44 KB (2 s) | 1 s worst-case block erase, 2x margin |
| Playback prefetch | 8 KB (~360 ms) | flash read latency vs A2DP callbacks |
| PCM out to Bluedroid | 8.8 KB (100 ms) | decoded ahead of the A2DP callback |

Total ≈ 70 KB of the ESP32's ~320 KB DRAM — comfortable alongside Bluedroid.

Buffer the *encoded* stream, not PCM, wherever there is a choice: ADPCM is
4x smaller, so the same stall tolerance costs a quarter of the RAM. The one
unavoidable PCM buffer is the last hop into Bluedroid, which only needs to
cover callback jitter, not flash stalls.

## 9. Open item for M6 (needs real sinks)

**Stored bitpool must be one the sink will accept.** We are the A2DP source
and propose the config, but the sink advertises min/max bitpool, and stored
frames cannot be re-encoded at playback (that would be exactly the
real-time DSP the design forbids).

Typical sinks accept up to bitpool 53, so 26 should be universally safe —
but this is untested. M6 must verify bitpool 26 / mono / 16 blocks /
8 subbands / Loudness against a basket of real sinks. If any common sink
rejects it, drop `TAPE_SBC_BITPOOL` (one constant) to 19 and re-encode;
3.63 MiB still fits a 5 MiB slot with room to spare.
