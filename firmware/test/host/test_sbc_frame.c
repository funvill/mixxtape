/* SBC arithmetic for the *air* format only.
 *
 * The tape no longer stores SBC: ESP-IDF's A2DP source takes PCM and
 * encodes SBC inside Bluedroid, so what goes over the air is the stack's
 * business. These numbers still matter because pairing must negotiate a
 * configuration real sinks accept (M6), and because docs/storage-budget.md
 * quotes them.
 */

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

    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 16u), 13781u);
    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 19u), 15848u);
    CHECK_EQ(sbc_bytes_per_sec(44100u, 8u, 16u, 1u, 26u), 20671u);
}

static void test_negotiated_config_is_sane(void)
{
    /* What we will ask a sink for. Bitpool must sit inside the range every
     * common sink advertises (typically 2..53) or stored audio cannot be
     * streamed at all. */
    CHECK(TAPE_SBC_BITPOOL >= 2u);
    CHECK(TAPE_SBC_BITPOOL <= 53u);
    CHECK_EQ(TAPE_SBC_SUBBANDS, 8u);
    CHECK_EQ(TAPE_SBC_BLOCKS, 16u);

    /* A2DP frames must fit a single L2CAP MTU comfortably. */
    uint32_t frame = MONO_FRAME(TAPE_SBC_BITPOOL);
    CHECK(frame > 0u && frame < 512u);
}

static void test_layout_fits_the_part(void)
{
    CHECK(TAPE_SLOT_BASE + TAPE_SLOT_COUNT * TAPE_SLOT_SIZE <= TAPE_FLASH_SIZE);
    CHECK_EQ(TAPE_SLOT_SIZE % TAPE_BLOCK_SIZE, 0); /* whole-block erases   */
    CHECK_EQ(TAPE_SLOT_ADDR(0) % TAPE_BLOCK_SIZE, 0);
    CHECK_EQ(TAPE_SECTOR_SIZE % TAPE_HDR_RECORD_SZ, 0);
    CHECK_EQ(TAPE_SPARE_BASE, TAPE_SLOT_ADDR(TAPE_SLOT_COUNT));
    CHECK(TAPE_FLASH_SIZE - TAPE_SPARE_BASE >= 65536u); /* room to grow */
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
RUN(test_negotiated_config_is_sane);
RUN(test_layout_fits_the_part);
RUN(test_frame_validity);

TEST_MAIN_END()
