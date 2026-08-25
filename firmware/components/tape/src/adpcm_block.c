#include "adpcm_block.h"

#include <string.h>

#define STEP_INDEX_MAX 88u

/* Cheap 8-bit mixing checksum. Its job is to notice a block whose payload
 * was cut short by a power loss (the tail reads back as 0xFF), not to
 * guarantee integrity — the flash itself has no bit-rot problem here.
 * Multiplying by an odd constant keeps the mapping invertible, so trailing
 * runs of 0xFF reliably change the result. */
uint8_t adpcm_block_checksum(const uint8_t *payload, uint32_t len)
{
    uint8_t c = 0xA5u;
    for (uint32_t i = 0; i < len; i++) {
        c = (uint8_t)(((uint8_t)(c ^ payload[i]) * 31u) + 7u);
    }
    return c;
}

uint32_t adpcm_block_encode(adpcm_state_t *st, const int16_t *pcm,
                            uint8_t *block)
{
    if (!st || !pcm || !block) {
        return 0;
    }
    /* Stamp the state as it is *before* encoding, so a decoder starting at
     * this block reproduces the same predictor trajectory. */
    int32_t predictor = st->predictor;
    block[0] = (uint8_t)((uint32_t)predictor & 0xFFu);
    block[1] = (uint8_t)(((uint32_t)predictor >> 8) & 0xFFu);
    block[2] = (uint8_t)st->index;

    uint32_t n = adpcm_encode(st, pcm, ADPCM_BLOCK_SAMPLES,
                              block + ADPCM_BLOCK_HEADER);
    if (n != ADPCM_BLOCK_DATA) {
        return 0u;
    }
    block[3] = adpcm_block_checksum(block + ADPCM_BLOCK_HEADER,
                                    ADPCM_BLOCK_DATA);
    return ADPCM_BLOCK_BYTES;
}

uint32_t adpcm_block_decode(const uint8_t *block, int16_t *pcm)
{
    if (!block || !pcm || !adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES)) {
        return 0;
    }
    adpcm_state_t st;
    st.predictor = (int16_t)((uint16_t)block[0] | ((uint16_t)block[1] << 8));
    st.index = (int16_t)block[2];

    return adpcm_decode(&st, block + ADPCM_BLOCK_HEADER, ADPCM_BLOCK_DATA, pcm);
}

bool adpcm_block_looks_valid(const uint8_t *block, uint32_t len)
{
    if (!block || len < ADPCM_BLOCK_HEADER) {
        return false;
    }
    /* Erased flash reads 0xFF, well past the highest legal step index. */
    if ((uint8_t)block[2] > (uint8_t)STEP_INDEX_MAX) {
        return false;
    }
    /* With a whole block in hand, verify it is not a torn write: a program
     * interrupted by a power loss lands only part of the payload and the
     * rest stays erased, which the checksum catches. */
    if (len >= ADPCM_BLOCK_BYTES) {
        if (block[3] != adpcm_block_checksum(block + ADPCM_BLOCK_HEADER,
                                             ADPCM_BLOCK_DATA)) {
            return false;
        }
    }
    return true;
}

uint32_t adpcm_blocks_to_ms(uint32_t blocks, uint32_t sample_rate)
{
    if (!sample_rate) {
        return 0;
    }
    uint64_t samples = (uint64_t)blocks * ADPCM_BLOCK_SAMPLES;
    return (uint32_t)((samples * 1000u) / sample_rate);
}

uint32_t adpcm_ms_to_blocks(uint32_t ms, uint32_t sample_rate)
{
    uint64_t samples = ((uint64_t)ms * sample_rate) / 1000u;
    return (uint32_t)(samples / ADPCM_BLOCK_SAMPLES);
}

uint32_t adpcm_bytes_per_sec(uint32_t sample_rate)
{
    return (sample_rate * ADPCM_BLOCK_BYTES) / ADPCM_BLOCK_SAMPLES;
}
