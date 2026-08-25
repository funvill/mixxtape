#include "adpcm.h"

static const int16_t k_step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t k_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int16_t clamp_index(int32_t idx)
{
    if (idx < 0) return 0;
    if (idx > 88) return 88;
    return (int16_t)idx;
}

static int32_t clamp_sample(int32_t s)
{
    if (s > 32767) return 32767;
    if (s < -32768) return -32768;
    return s;
}

void adpcm_state_init(adpcm_state_t *st)
{
    if (st) {
        st->predictor = 0;
        st->index = 0;
    }
}

static uint8_t encode_sample(adpcm_state_t *st, int16_t sample)
{
    int32_t step = k_step_table[st->index];
    int32_t diff = sample - st->predictor;

    uint8_t code = 0;
    if (diff < 0) {
        code = 8;
        diff = -diff;
    }

    /* Three magnitude bits: diff/step in halving steps. */
    int32_t delta = step >> 3;
    if (diff >= step) {
        code |= 4;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        code |= 2;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        code |= 1;
        delta += step;
    }

    st->predictor = clamp_sample((code & 8) ? st->predictor - delta
                                            : st->predictor + delta);
    st->index = clamp_index(st->index + k_index_table[code]);
    return code;
}

static int16_t decode_sample(adpcm_state_t *st, uint8_t code)
{
    int32_t step = k_step_table[st->index];
    int32_t delta = step >> 3;

    if (code & 4) delta += step;
    if (code & 2) delta += step >> 1;
    if (code & 1) delta += step >> 2;

    st->predictor = clamp_sample((code & 8) ? st->predictor - delta
                                            : st->predictor + delta);
    st->index = clamp_index(st->index + k_index_table[code]);
    return (int16_t)st->predictor;
}

uint32_t adpcm_encode(adpcm_state_t *st, const int16_t *pcm, uint32_t count,
                      uint8_t *out)
{
    if (!st || !pcm || !out || (count & 1u)) {
        return 0;
    }
    uint32_t written = 0;
    for (uint32_t i = 0; i < count; i += 2) {
        uint8_t lo = encode_sample(st, pcm[i]);
        uint8_t hi = encode_sample(st, pcm[i + 1]);
        out[written++] = (uint8_t)(lo | (hi << 4));
    }
    return written;
}

uint32_t adpcm_decode(adpcm_state_t *st, const uint8_t *in, uint32_t bytes,
                      int16_t *pcm)
{
    if (!st || !in || !pcm) {
        return 0;
    }
    uint32_t written = 0;
    for (uint32_t i = 0; i < bytes; i++) {
        pcm[written++] = decode_sample(st, (uint8_t)(in[i] & 0x0Fu));
        pcm[written++] = decode_sample(st, (uint8_t)(in[i] >> 4));
    }
    return written;
}
