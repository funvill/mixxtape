/* Checks the SBC arithmetic against the table in docs/storage-budget.md.
 * If these numbers move, that document and the slot sizing are both wrong. */

#include "sbc_frame.h"
#include "tape_layout.h"
#include "test_util.h"

#define MONO_FRAME(bp) sbc_frame_bytes(8u, 16u, 1u, (bp))

static void test_frame_length_formula(void)
{
    /* frame = 8 + 2*bitpool for the mono/8-subband/16-block configuration. */
    CHECK_EQ(MONO_FRAME(16u), 40u);
    CHECK_EQ(MONO_FRAME(19u), 46u);
    CHECK_EQ(MONO_FRAME(20u), 48u);
    CHECK_EQ(MONO_FRAME(26u), 60u);
    CHECK_EQ(MONO_FRAME(31u), 70u);

    /* Odd bitpools round the payload up to a whole byte. */
    CHECK_EQ(sbc_frame_bytes(8u, 16u, 1u, 1u), 8u + 2u);
    CHECK_EQ(sbc_frame_bytes(4u, 16u, 1u, 10u), 4u + 2u + 20u);

    CHECK_EQ(sbc_frame_bytes(0u, 16u, 1u, 26u), 0u); /* guards */
    CHECK_EQ(sbc_frame_bytes(8u, 0u, 1u, 26u), 0u);
}

static void test_rates(void)
{
    /* 44100 / (8*16) = 344.531 frames/s */
    CHECK_EQ(sbc_frames_per_sec_milli(44100u, 8u, 16u), 344531u);

    /* Bytes per second per the doc's table. */
    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 16u), 13781u);
    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 19u), 15848u);
    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 26u), 20671u);
}

static void test_duration_round_trip(void)
{
    uint32_t four_min = 240u * 1000u;
    uint32_t bytes = sbc_ms_to_bytes(four_min, 44100u, 8u, 16u, 1u, 26u);

    /* 4:00 at bitpool 26 is 82,687 whole frames = 4,961,220 B (the exact
     * fractional value, 4,961,250, is not a whole number of frames) and
     * must fit a 5 MiB slot. */
    CHECK_EQ(bytes, 4961220u);
    CHECK(bytes < TAPE_SLOT_SIZE);

    /* Round-tripping back to milliseconds lands within one frame (23 ms). */
    uint32_t ms = sbc_bytes_to_ms(bytes, 44100u, 8u, 16u, 1u, 26u);
    CHECK(ms <= four_min);
    CHECK(four_min - ms < 30u);
}

static void test_slot_capacity(void)
{
    /* The maximum take a slot can hold, quoted as 4:13 in the doc. */
    uint32_t ms = sbc_bytes_to_ms(TAPE_SLOT_SIZE, TAPE_SBC_SAMPLE_RATE,
                                  TAPE_SBC_SUBBANDS, TAPE_SBC_BLOCKS,
                                  TAPE_SBC_CHANNELS, TAPE_SBC_BITPOOL);
    CHECK(ms >= 253000u);
    CHECK(ms <= 254000u);

    /* The configured layout must actually fit the part. */
    CHECK(TAPE_SLOT_BASE + TAPE_SLOT_COUNT * TAPE_SLOT_SIZE <= TAPE_FLASH_SIZE);
    CHECK_EQ(TAPE_SLOT_SIZE % TAPE_BLOCK_SIZE, 0); /* whole-block erases */
    CHECK_EQ(TAPE_SLOT_ADDR(0) % TAPE_BLOCK_SIZE, 0);
    CHECK_EQ(TAPE_SECTOR_SIZE % TAPE_HDR_RECORD_SZ, 0);
}

static void test_frame_validity(void)
{
    uint8_t good[4] = {SBC_SYNCWORD, 0x35u, 26u, 0x00u};
    uint8_t erased[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    uint8_t bad_sync[4] = {0x00u, 0x35u, 26u, 0x00u};

    CHECK(sbc_frame_looks_valid(good, 4u));
    CHECK(!sbc_frame_looks_valid(erased, 4u));
    CHECK(!sbc_frame_looks_valid(bad_sync, 4u));
    CHECK(!sbc_frame_looks_valid(good, 2u)); /* too short */
    CHECK(!sbc_frame_looks_valid(0, 4u));
}

TEST_MAIN_BEGIN()

RUN(test_frame_length_formula);
RUN(test_rates);
RUN(test_duration_round_trip);
RUN(test_slot_capacity);
RUN(test_frame_validity);

TEST_MAIN_END()
