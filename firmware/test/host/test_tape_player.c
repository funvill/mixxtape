/* Playback tests: the three slots must behave as one tape side. */

#include <stdlib.h>
#include <string.h>

#include "adpcm_block.h"
#include "mock_flash.h"
#include "tape_player.h"
#include "tape_store.h"
#include "test_util.h"

#define BLOCKS_PER_TRACK 6u

static void make_ramp(int16_t *pcm, uint32_t n, int32_t base, uint32_t period)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t phase = i % period;
        int32_t half = (int32_t)(period / 2u);
        int32_t v = (phase < (uint32_t)half)
                        ? (int32_t)phase * 12000 / half
                        : (2 * half - (int32_t)phase) * 12000 / half;
        pcm[i] = (int16_t)(v - 6000 + base);
    }
}

/* Records `blocks` blocks of synthetic audio into `slot`, and appends what
 * playback should hand back (the decoded blocks) to `expect`. */
static void record_track(tape_store_t *ts, int slot, uint32_t blocks,
                         int32_t base, int16_t *expect, uint32_t *expect_len)
{
    int16_t *pcm = (int16_t *)malloc(ADPCM_BLOCK_SAMPLES * sizeof(int16_t));
    uint8_t block[ADPCM_BLOCK_BYTES];
    adpcm_state_t st;
    adpcm_state_init(&st);

    CHECK_EQ(tape_store_begin_record(ts, slot), TAPE_OK);
    for (uint32_t b = 0; b < blocks; b++) {
        make_ramp(pcm, ADPCM_BLOCK_SAMPLES, base + (int32_t)b * 100, 257u);
        CHECK_EQ(adpcm_block_encode(&st, pcm, block), ADPCM_BLOCK_BYTES);
        CHECK_EQ(tape_store_write(ts, block, ADPCM_BLOCK_BYTES), TAPE_OK);
        if (expect) {
            uint32_t n = adpcm_block_decode(block, expect + *expect_len);
            CHECK_EQ(n, ADPCM_BLOCK_SAMPLES);
            *expect_len += n;
        }
    }
    CHECK_EQ(tape_store_end_record(ts), TAPE_OK);
    free(pcm);
}

static void test_plays_all_tracks_in_order(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;

    uint32_t cap = ADPCM_BLOCK_SAMPLES * BLOCKS_PER_TRACK * TAPE_SLOT_COUNT;
    int16_t *expect = (int16_t *)malloc(cap * sizeof(int16_t));
    int16_t *got = (int16_t *)malloc(cap * sizeof(int16_t));
    uint32_t expect_len = 0;

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    for (int s = 0; s < TAPE_SLOT_COUNT; s++) {
        record_track(&ts, s, BLOCKS_PER_TRACK, s * 1000, expect, &expect_len);
    }

    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_OK);
    CHECK_EQ(p.state, TAPE_PLAY_PLAYING);

    /* Drain in awkward chunk sizes so block and track boundaries never line
     * up with the reads — the callback will not be tidy either. */
    uint32_t total = 0;
    for (;;) {
        uint32_t want = 777u;
        if (total + want > cap) {
            want = cap - total;
        }
        if (!want) {
            break;
        }
        uint32_t n = tape_player_read(&p, got + total, want);
        total += n;
        if (n < want) {
            break;
        }
    }
    /* One more read: end of side is only discovered by asking for audio
     * that is not there, which is exactly how the prefetch task behaves. */
    int16_t tail[16];
    CHECK_EQ(tape_player_read(&p, tail, 16u), 0u);

    CHECK_EQ(total, expect_len);
    CHECK_MEM(got, expect, expect_len * sizeof(int16_t));
    CHECK_EQ(p.state, TAPE_PLAY_FINISHED);
    CHECK_EQ(tape_player_progress_permille(&p), 1000u);

    /* Once finished, it stays finished and produces nothing more. */
    CHECK_EQ(tape_player_read(&p, got, 128u), 0u);

    free(expect);
    free(got);
    mock_flash_destroy(f);
}

static void test_skips_empty_slots(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;
    uint32_t cap = ADPCM_BLOCK_SAMPLES * BLOCKS_PER_TRACK * 2u;
    int16_t *expect = (int16_t *)malloc(cap * sizeof(int16_t));
    int16_t *got = (int16_t *)malloc(cap * sizeof(int16_t));
    uint32_t expect_len = 0;

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    /* Only slots 0 and 2 hold audio: an empty middle track is skipped, not
     * played as silence. */
    record_track(&ts, 0, BLOCKS_PER_TRACK, 0, expect, &expect_len);
    record_track(&ts, 2, BLOCKS_PER_TRACK, 5000, expect, &expect_len);

    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_OK);
    uint32_t total = 0;
    for (;;) {
        uint32_t want = (cap - total < 512u) ? (cap - total) : 512u;
        if (!want) {
            break;
        }
        uint32_t n = tape_player_read(&p, got + total, want);
        total += n;
        if (n == 0u) {
            break;
        }
    }
    CHECK_EQ(total, expect_len);
    CHECK_MEM(got, expect, expect_len * sizeof(int16_t));

    free(expect);
    free(got);
    mock_flash_destroy(f);
}

static void test_blank_tape_refuses_to_play(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;
    int16_t buf[64];

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_ERR_STATE);
    CHECK_EQ(p.state, TAPE_PLAY_STOPPED);
    CHECK_EQ(tape_player_read(&p, buf, 64u), 0u);
    CHECK_EQ(tape_player_side_ms(&ts), 0u);

    mock_flash_destroy(f);
}

static void test_pause_and_resume(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;
    int16_t buf[1024];

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    record_track(&ts, 0, BLOCKS_PER_TRACK, 0, 0, 0);

    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_OK);
    CHECK_EQ(tape_player_read(&p, buf, 1024u), 1024u);

    uint32_t at_pause = tape_player_position_ms(&p);
    tape_player_pause(&p);
    CHECK_EQ(p.state, TAPE_PLAY_PAUSED);
    CHECK_EQ(tape_player_read(&p, buf, 1024u), 0u); /* silent while paused */
    CHECK_EQ(tape_player_position_ms(&p), at_pause); /* and does not drift */

    tape_player_resume(&p);
    CHECK_EQ(p.state, TAPE_PLAY_PLAYING);
    CHECK_EQ(tape_player_read(&p, buf, 1024u), 1024u);
    CHECK(tape_player_position_ms(&p) > at_pause);

    tape_player_stop(&p);
    CHECK_EQ(tape_player_read(&p, buf, 1024u), 0u);

    mock_flash_destroy(f);
}

static void test_track_skip_wraps(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    for (int s = 0; s < TAPE_SLOT_COUNT; s++) {
        record_track(&ts, s, BLOCKS_PER_TRACK, s * 1000, 0, 0);
    }

    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_OK);
    CHECK_EQ(p.track, 0);
    CHECK_EQ(tape_player_next_track(&p), TAPE_OK);
    CHECK_EQ(p.track, 1);
    CHECK_EQ(tape_player_next_track(&p), TAPE_OK);
    CHECK_EQ(p.track, 2);
    CHECK_EQ(tape_player_next_track(&p), TAPE_OK);
    CHECK_EQ(p.track, 0); /* wraps */

    /* Seeking restarts that track from its beginning. */
    CHECK_EQ(tape_player_seek_track(&p, 2), TAPE_OK);
    CHECK_EQ(p.track, 2);
    CHECK_EQ(p.block, 0u);

    mock_flash_destroy(f);
}

static void test_position_and_progress(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;
    int16_t buf[512];

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    for (int s = 0; s < TAPE_SLOT_COUNT; s++) {
        record_track(&ts, s, BLOCKS_PER_TRACK, 0, 0, 0);
    }

    uint32_t side = tape_player_side_ms(&ts);
    uint32_t one = adpcm_blocks_to_ms(BLOCKS_PER_TRACK, TAPE_SAMPLE_RATE);
    CHECK_EQ(side, one * 3u);

    CHECK_EQ(tape_player_start(&p, &ts, 0), TAPE_OK);
    CHECK_EQ(tape_player_position_ms(&p), 0u);
    CHECK_EQ(tape_player_progress_permille(&p), 0u);

    /* Position must never go backwards as the side plays out. */
    uint32_t last_pos = 0, last_prog = 0;
    uint32_t n;
    while ((n = tape_player_read(&p, buf, 512u)) > 0) {
        uint32_t pos = tape_player_position_ms(&p);
        uint32_t prog = tape_player_progress_permille(&p);
        CHECK(pos >= last_pos);
        CHECK(prog >= last_prog);
        CHECK(prog <= 1000u);
        last_pos = pos;
        last_prog = prog;
        if (n < 512u) {
            break;
        }
    }
    CHECK_EQ(tape_player_progress_permille(&p), 1000u);

    /* Starting mid-side reports position from the side's start, because the
     * reels show where you are on the tape, not within one track. */
    CHECK_EQ(tape_player_start(&p, &ts, 2), TAPE_OK);
    CHECK_EQ(tape_player_position_ms(&p), one * 2u);
    CHECK(tape_player_progress_permille(&p) > 600u);

    mock_flash_destroy(f);
}

static void test_survives_reboot_mid_side(void)
{
    /* Remount and play: what was recorded is still a coherent side. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    tape_player_t p;
    int16_t buf[1024];

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    record_track(&ts, 1, BLOCKS_PER_TRACK, 400, 0, 0);

    tape_store_t ts2;
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK_EQ(tape_player_start(&p, &ts2, 0), TAPE_OK);
    CHECK_EQ(p.track, 1); /* skipped the empty first slot */

    uint32_t total = 0, n;
    while ((n = tape_player_read(&p, buf, 1024u)) > 0) {
        total += n;
    }
    CHECK_EQ(total, ADPCM_BLOCK_SAMPLES * BLOCKS_PER_TRACK);

    mock_flash_destroy(f);
}

static void test_guards(void)
{
    tape_player_t p;
    int16_t buf[16];
    CHECK_EQ(tape_player_start(0, 0, 0), TAPE_ERR_ARG);
    memset(&p, 0, sizeof(p));
    CHECK_EQ(tape_player_read(&p, buf, 16u), 0u);
    CHECK_EQ(tape_player_read(&p, 0, 16u), 0u);
    CHECK_EQ(tape_player_side_ms(0), 0u);
    CHECK_EQ(tape_player_progress_permille(0), 0u);
}

TEST_MAIN_BEGIN()

RUN(test_plays_all_tracks_in_order);
RUN(test_skips_empty_slots);
RUN(test_blank_tape_refuses_to_play);
RUN(test_pause_and_resume);
RUN(test_track_skip_wraps);
RUN(test_position_and_progress);
RUN(test_survives_reboot_mid_side);
RUN(test_guards);

TEST_MAIN_END()
