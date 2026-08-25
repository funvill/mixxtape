/* SBC frame arithmetic.  Pure math, no state — see docs/storage-budget.md.
 *
 * Only the mono/8-subband/16-block configuration the tape stores is
 * supported; the helpers still take parameters so the unit tests can walk
 * the spec's formula across configurations.
 */
#ifndef SBC_FRAME_H
#define SBC_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#define SBC_SYNCWORD 0x9Cu

/* frame_bytes = 4 + (4*subbands*channels)/8 + ceil(blocks*channels*bitpool/8) */
uint32_t sbc_frame_bytes(uint32_t subbands, uint32_t blocks,
                         uint32_t channels, uint32_t bitpool);

/* Frames per second = sample_rate / (blocks * subbands). Returned x1000. */
uint32_t sbc_frames_per_sec_milli(uint32_t sample_rate, uint32_t subbands,
                                  uint32_t blocks);

/* Encoded bytes per second for the given configuration. */
uint32_t sbc_bytes_per_sec(uint32_t sample_rate, uint32_t subbands,
                           uint32_t blocks, uint32_t channels,
                           uint32_t bitpool);

/* Milliseconds of audio held in `bytes` of encoded stream. */
uint32_t sbc_bytes_to_ms(uint32_t bytes, uint32_t sample_rate,
                         uint32_t subbands, uint32_t blocks,
                         uint32_t channels, uint32_t bitpool);

/* Bytes needed to store `ms` of audio, rounded down to a whole frame. */
uint32_t sbc_ms_to_bytes(uint32_t ms, uint32_t sample_rate, uint32_t subbands,
                         uint32_t blocks, uint32_t channels, uint32_t bitpool);

/* True if `buf` starts with a plausible SBC frame header for our config. */
bool sbc_frame_looks_valid(const uint8_t *buf, uint32_t len);

#endif /* SBC_FRAME_H */
