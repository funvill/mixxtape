/* Block-framed IMA ADPCM — the format actually stored on the tape.
 *
 * Why blocks rather than one continuous nibble stream:
 *
 *  - Each block carries the codec state in its header, so decoding can
 *    start at any block. That makes seeking free, which the track-skip,
 *    pause/resume and reel-position display all need.
 *  - The header makes a block self-checking: the step index bounds it
 *    (erased flash reads 0xFF, which is > 88) and a payload checksum
 *    detects a block whose header landed but whose data was cut short by
 *    a power loss mid-write. Crash recovery relies on both.
 *  - 512 B is one block per two flash pages, 23.04 ms of audio, and only
 *    0.8 % header overhead.
 *
 * Layout of one block:
 *   [0..1] predictor at block start, int16 little-endian
 *   [2]    step table index at block start, 0..88
 *   [3]    checksum of the payload (truncation detection, not integrity)
 *   [4..]  508 bytes of 4-bit nibbles = 1016 samples
 */
#ifndef ADPCM_BLOCK_H
#define ADPCM_BLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "adpcm.h"

#define ADPCM_BLOCK_BYTES   512u
#define ADPCM_BLOCK_HEADER  4u
#define ADPCM_BLOCK_DATA    (ADPCM_BLOCK_BYTES - ADPCM_BLOCK_HEADER)
#define ADPCM_BLOCK_SAMPLES (ADPCM_BLOCK_DATA * 2u)

/* Encodes exactly ADPCM_BLOCK_SAMPLES samples into a whole block, stamping
 * the state as it stood before this block so the block can be decoded on
 * its own. `st` continues across blocks. Returns bytes written (or 0). */
uint32_t adpcm_block_encode(adpcm_state_t *st, const int16_t *pcm,
                            uint8_t *block);

/* Decodes one block, ignoring any running state: the block's own header is
 * the starting point. Returns samples written (or 0 if malformed). */
uint32_t adpcm_block_decode(const uint8_t *block, int16_t *pcm);

/* True if `block` is a structurally plausible, complete block. Given a
 * whole block the payload checksum is verified too, so a block truncated
 * by a power loss is rejected. */
bool adpcm_block_looks_valid(const uint8_t *block, uint32_t len);

/* The checksum stored in header byte 3. Exposed so tests can build blocks. */
uint8_t adpcm_block_checksum(const uint8_t *payload, uint32_t len);

/* Audio duration held in `blocks` whole blocks, in milliseconds. */
uint32_t adpcm_blocks_to_ms(uint32_t blocks, uint32_t sample_rate);

/* Whole blocks needed to hold `ms` of audio. */
uint32_t adpcm_ms_to_blocks(uint32_t ms, uint32_t sample_rate);

/* Stored bytes per second of audio at this sample rate. */
uint32_t adpcm_bytes_per_sec(uint32_t sample_rate);

#endif /* ADPCM_BLOCK_H */
