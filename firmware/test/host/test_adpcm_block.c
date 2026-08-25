/* Tests for the stored audio format: block-framed IMA ADPCM. */

#include <stdlib.h>
#include <string.h>

#include "adpcm_block.h"
#include "tape_layout.h"
#include "test_util.h"

static void make_triangle(int16_t *pcm, uint32_t n, int32_t amplitude,
                          uint32_t period)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t phase = i % period;
        int32_t half = (int32_t)(period / 2u);
        int32_t v = (phase < (uint32_t)half)
                        ? (int32_t)phase * amplitude / half
                        : (2 * half - (int32_t)phase) * amplitude / half;
        pcm[i] = (int16_t)(v - amplitude / 2);
    }
}

static void test_block_geometry(void)
{
    CHECK_EQ(ADPCM_BLOCK_BYTES, 512u);
    CHECK_EQ(ADPCM_BLOCK_DATA, 508u);
    CHECK_EQ(ADPCM_BLOCK_SAMPLES, 1016u);

    /* One block is a whole number of flash pages — writes never straddle. */
    CHECK_EQ(ADPCM_BLOCK_BYTES % TAPE_PAGE_SIZE, 0);
    /* And a slot is a whole number of blocks, so no runt at the end. */
    CHECK_EQ(TAPE_SLOT_SIZE % ADPCM_BLOCK_BYTES, 0);
}

static void test_rates_and_capacity(void)
{
    /* 22,223 B/s at 44.1 kHz mono, per docs/storage-budget.md
     * (44100 * 512 / 1016, truncated). */
    CHECK_EQ(adpcm_bytes_per_sec(TAPE_SAMPLE_RATE), 22223u);

    /* 23.04 ms per block. */
    CHECK_EQ(adpcm_blocks_to_ms(1u, TAPE_SAMPLE_RATE), 23u);
    CHECK_EQ(adpcm_blocks_to_ms(100u, TAPE_SAMPLE_RATE), 2303u);

    /* A slot must hold a full four-minute take — that is the product
     * promise the flash size was chosen around. */
    uint32_t blocks = TAPE_SLOT_SIZE / ADPCM_BLOCK_BYTES;
    uint32_t ms = adpcm_blocks_to_ms(blocks, TAPE_SAMPLE_RATE);
    printf("   slot holds %u blocks = %u ms (%u:%02u)\n", blocks, ms,
           ms / 60000u, (ms / 1000u) % 60u);
    CHECK(ms >= 240000u);

    /* Round trip. */
    uint32_t need = adpcm_ms_to_blocks(240000u, TAPE_SAMPLE_RATE);
    CHECK(need <= blocks);
    CHECK(adpcm_blocks_to_ms(need, TAPE_SAMPLE_RATE) <= 240000u);
}

static void test_encode_decode_round_trip(void)
{
    int16_t *pcm = (int16_t *)malloc(ADPCM_BLOCK_SAMPLES * sizeof(int16_t));
    int16_t *out = (int16_t *)malloc(ADPCM_BLOCK_SAMPLES * sizeof(int16_t));
    uint8_t block[ADPCM_BLOCK_BYTES];
    adpcm_state_t st;

    make_triangle(pcm, ADPCM_BLOCK_SAMPLES, 18000, 300u);
    adpcm_state_init(&st);

    CHECK_EQ(adpcm_block_encode(&st, pcm, block), ADPCM_BLOCK_BYTES);
    CHECK(adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));
    CHECK_EQ(adpcm_block_decode(block, out), ADPCM_BLOCK_SAMPLES);

    long long total = 0;
    for (uint32_t i = 64; i < ADPCM_BLOCK_SAMPLES; i++) {
        int32_t err = (int32_t)out[i] - (int32_t)pcm[i];
        total += (err < 0) ? -err : err;
    }
    int32_t mean = (int32_t)(total / (ADPCM_BLOCK_SAMPLES - 64));
    printf("   mean abs error %d (full scale 32768)\n", mean);
    CHECK(mean < 400);

    free(pcm);
    free(out);
}

static void test_blocks_are_independently_decodable(void)
{
    /* This is what makes seeking free: decoding block N on its own must
     * give the same audio as decoding blocks 0..N in sequence. Track skip,
     * pause/resume and the reel position display all depend on it. */
    const uint32_t nblocks = 8u;
    uint32_t nsamp = ADPCM_BLOCK_SAMPLES * nblocks;
    int16_t *pcm = (int16_t *)malloc(nsamp * sizeof(int16_t));
    int16_t *seq = (int16_t *)malloc(nsamp * sizeof(int16_t));
    int16_t *one = (int16_t *)malloc(ADPCM_BLOCK_SAMPLES * sizeof(int16_t));
    uint8_t *blocks = (uint8_t *)malloc(nblocks * ADPCM_BLOCK_BYTES);
    adpcm_state_t st;

    make_triangle(pcm, nsamp, 22000, 511u);

    adpcm_state_init(&st);
    for (uint32_t b = 0; b < nblocks; b++) {
        CHECK_EQ(adpcm_block_encode(&st, pcm + b * ADPCM_BLOCK_SAMPLES,
                                    blocks + b * ADPCM_BLOCK_BYTES),
                 ADPCM_BLOCK_BYTES);
    }

    /* Sequential decode of everything. */
    for (uint32_t b = 0; b < nblocks; b++) {
        CHECK_EQ(adpcm_block_decode(blocks + b * ADPCM_BLOCK_BYTES,
                                    seq + b * ADPCM_BLOCK_SAMPLES),
                 ADPCM_BLOCK_SAMPLES);
    }

    /* Now decode each block cold, as a seek would, and require identity. */
    for (uint32_t b = 0; b < nblocks; b++) {
        CHECK_EQ(adpcm_block_decode(blocks + b * ADPCM_BLOCK_BYTES, one),
                 ADPCM_BLOCK_SAMPLES);
        CHECK_MEM(one, seq + b * ADPCM_BLOCK_SAMPLES,
                  ADPCM_BLOCK_SAMPLES * sizeof(int16_t));
    }

    free(pcm);
    free(seq);
    free(one);
    free(blocks);
}

static void test_erased_flash_is_never_valid(void)
{
    /* Crash recovery scans for the first structurally invalid block, so
     * erased flash must never pass. */
    uint8_t erased[ADPCM_BLOCK_BYTES];
    memset(erased, 0xFF, sizeof(erased));
    CHECK(!adpcm_block_looks_valid(erased, ADPCM_BLOCK_BYTES));

    uint8_t block[ADPCM_BLOCK_BYTES];
    memset(block, 0x00, sizeof(block));
    block[3] = adpcm_block_checksum(block + ADPCM_BLOCK_HEADER,
                                    ADPCM_BLOCK_DATA);
    CHECK(adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));

    block[2] = 88u; /* highest legal step index */
    CHECK(adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));
    block[2] = 89u; /* one past */
    CHECK(!adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));

    block[2] = 10u;
    block[3] ^= 0xFFu; /* wrong checksum */
    CHECK(!adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));

    CHECK(!adpcm_block_looks_valid(block, 2u)); /* too short */
    CHECK(!adpcm_block_looks_valid(0, ADPCM_BLOCK_BYTES));

    /* A malformed block decodes to nothing rather than garbage. */
    int16_t out[ADPCM_BLOCK_SAMPLES];
    CHECK_EQ(adpcm_block_decode(erased, out), 0u);
}

static void test_truncated_block_is_rejected(void)
{
    /* The exact power-loss shape: a block whose header and part of its
     * payload landed, with the tail left erased. Validation must reject it,
     * otherwise crash recovery would keep 23 ms of noise. */
    int16_t pcm[ADPCM_BLOCK_SAMPLES];
    uint8_t block[ADPCM_BLOCK_BYTES];
    adpcm_state_t st;

    make_triangle(pcm, ADPCM_BLOCK_SAMPLES, 15000, 401u);
    adpcm_state_init(&st);
    CHECK_EQ(adpcm_block_encode(&st, pcm, block), ADPCM_BLOCK_BYTES);
    CHECK(adpcm_block_looks_valid(block, ADPCM_BLOCK_BYTES));

    /* Erase progressively more of the tail; every truncation is caught. */
    for (uint32_t cut = 1u; cut <= 256u; cut *= 2u) {
        uint8_t torn[ADPCM_BLOCK_BYTES];
        memcpy(torn, block, sizeof(torn));
        memset(torn + ADPCM_BLOCK_BYTES - cut, 0xFF, cut);
        CHECK(!adpcm_block_looks_valid(torn, ADPCM_BLOCK_BYTES));
    }
}

static void test_guards(void)
{
    uint8_t block[ADPCM_BLOCK_BYTES];
    int16_t pcm[ADPCM_BLOCK_SAMPLES];
    adpcm_state_t st;
    adpcm_state_init(&st);

    CHECK_EQ(adpcm_block_encode(0, pcm, block), 0u);
    CHECK_EQ(adpcm_block_encode(&st, 0, block), 0u);
    CHECK_EQ(adpcm_block_encode(&st, pcm, 0), 0u);
    CHECK_EQ(adpcm_block_decode(block, 0), 0u);
    CHECK_EQ(adpcm_blocks_to_ms(10u, 0u), 0u);
}

TEST_MAIN_BEGIN()

RUN(test_block_geometry);
RUN(test_rates_and_capacity);
RUN(test_encode_decode_round_trip);
RUN(test_blocks_are_independently_decodable);
RUN(test_erased_flash_is_never_valid);
RUN(test_truncated_block_is_rejected);
RUN(test_guards);

TEST_MAIN_END()
