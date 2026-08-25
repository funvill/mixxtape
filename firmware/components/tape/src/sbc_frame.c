#include "sbc_frame.h"

uint32_t sbc_frame_bytes(uint32_t subbands, uint32_t blocks, uint32_t channels,
                         uint32_t bitpool)
{
    if (!subbands || !blocks || !channels) {
        return 0;
    }
    /* Spec form for MONO / DUAL_CHANNEL. Joint/stereo modes add a join
     * bitmap; the tape only ever stores mono, so they are out of scope. */
    uint32_t bits = blocks * channels * bitpool;
    return 4u + (4u * subbands * channels) / 8u + (bits + 7u) / 8u;
}

uint32_t sbc_frames_per_sec_milli(uint32_t sample_rate, uint32_t subbands,
                                  uint32_t blocks)
{
    if (!subbands || !blocks) {
        return 0;
    }
    /* x1000 to keep a fraction without floating point: 44100/128 = 344.531 */
    return (sample_rate * 1000u) / (subbands * blocks);
}

uint32_t sbc_bytes_per_sec(uint32_t sample_rate, uint32_t subbands,
                           uint32_t blocks, uint32_t channels, uint32_t bitpool)
{
    uint32_t frame = sbc_frame_bytes(subbands, blocks, channels, bitpool);
    uint32_t fps_milli = sbc_frames_per_sec_milli(sample_rate, subbands, blocks);
    return (frame * fps_milli) / 1000u;
}

uint32_t sbc_bytes_to_ms(uint32_t bytes, uint32_t sample_rate, uint32_t subbands,
                         uint32_t blocks, uint32_t channels, uint32_t bitpool)
{
    uint32_t frame = sbc_frame_bytes(subbands, blocks, channels, bitpool);
    if (!frame || !sample_rate) {
        return 0;
    }
    uint32_t frames = bytes / frame;
    uint64_t samples = (uint64_t)frames * subbands * blocks;
    return (uint32_t)((samples * 1000u) / sample_rate);
}

uint32_t sbc_ms_to_bytes(uint32_t ms, uint32_t sample_rate, uint32_t subbands,
                         uint32_t blocks, uint32_t channels, uint32_t bitpool)
{
    uint32_t frame = sbc_frame_bytes(subbands, blocks, channels, bitpool);
    if (!frame || !subbands || !blocks) {
        return 0;
    }
    uint64_t samples = ((uint64_t)ms * sample_rate) / 1000u;
    uint32_t frames = (uint32_t)(samples / (subbands * blocks));
    return frames * frame;
}

bool sbc_frame_looks_valid(const uint8_t *buf, uint32_t len)
{
    if (!buf || len < 4u) {
        return false;
    }
    if (buf[0] != SBC_SYNCWORD) {
        return false;
    }
    /* byte1: sampling_frequency(2) block_mode(2) channel_mode(2)
     *        allocation_method(1) subbands(1)
     * byte2: bitpool. Reject the all-ones pattern of erased flash. */
    if (buf[1] == 0xFFu || buf[2] == 0x00u || buf[2] > 250u) {
        return false;
    }
    return true;
}
