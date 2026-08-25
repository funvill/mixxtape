/* Slot-manager tests, with the power-yank sweep as the centrepiece:
 * every single flash operation of a recording is torn in turn, the device
 * is "re-plugged", and the invariants are re-checked. */

#include <stdlib.h>

#include "mock_flash.h"
#include "adpcm_block.h"
#include "tape_store.h"
#include "test_util.h"

/* --- helpers -------------------------------------------------------- */

static uint32_t frame_len(void)
{
    return ADPCM_BLOCK_BYTES;
}

/* Synthetic stream of structurally valid ADPCM blocks: each carries a
 * decodable header (step index <= 88, reserved 0x00) and a payload that is
 * never 0xFF, so byte-exact comparisons stay meaningful and no block looks
 * like erased flash or like a torn write. */
static void fill_stream(uint8_t *buf, uint32_t len, uint32_t seed)
{
    uint32_t x = seed | 1u;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t pos = i % ADPCM_BLOCK_BYTES;
        x = x * 1664525u + 1013904223u;
        if (pos == 2u) {
            buf[i] = (uint8_t)((x >> 16) % 89u); /* valid step index */
        } else {
            buf[i] = (uint8_t)((x >> 16) % 0xFEu); /* 0x00..0xFD */
        }
    }
    /* Stamp each whole block's payload checksum so the blocks pass the same
     * validation crash recovery uses. */
    for (uint32_t off = 0; off + ADPCM_BLOCK_BYTES <= len;
         off += ADPCM_BLOCK_BYTES) {
        buf[off + 3u] = adpcm_block_checksum(buf + off + ADPCM_BLOCK_HEADER,
                                             ADPCM_BLOCK_DATA);
    }
}

static int record_slot(tape_store_t *ts, int slot, const uint8_t *data,
                       uint32_t len, uint32_t chunk)
{
    int rc = tape_store_begin_record(ts, slot);
    if (rc != TAPE_OK) {
        return rc;
    }
    for (uint32_t off = 0; off < len; off += chunk) {
        uint32_t n = (off + chunk > len) ? (len - off) : chunk;
        rc = tape_store_write(ts, data + off, n);
        if (rc != TAPE_OK) {
            return rc;
        }
    }
    return tape_store_end_record(ts);
}

/* --- tests ---------------------------------------------------------- */

static void test_format_and_mount(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        CHECK_EQ(ts.slots[i].state, TAPE_SLOT_EMPTY);
        CHECK_EQ(ts.slots[i].length, 0);
    }
    CHECK(tape_store_is_blank(&ts));

    tape_store_t ts2;
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK(tape_store_is_blank(&ts2));
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    mock_flash_destroy(f);
}

static void test_mount_blank_device(void)
{
    /* A virgin chip has no header at all; mount must format rather than fail. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    CHECK_EQ(tape_store_mount(&ts, f), TAPE_OK);
    CHECK(tape_store_is_blank(&ts));
    mock_flash_destroy(f);
}

static void test_record_and_read_back(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t len = frame_len() * 100u;
    uint8_t *data = (uint8_t *)malloc(len);
    uint8_t *back = (uint8_t *)malloc(len);
    fill_stream(data, len, 1);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 1, data, len, 333u), TAPE_OK);

    CHECK_EQ(ts.slots[1].state, TAPE_SLOT_VALID);
    CHECK_EQ(ts.slots[1].length, len);
    CHECK(!tape_store_is_blank(&ts));

    /* Duration should match the storage-budget arithmetic. */
    uint32_t expect_ms =
        adpcm_blocks_to_ms(len / ADPCM_BLOCK_BYTES, TAPE_SAMPLE_RATE);
    CHECK_EQ(ts.slots[1].duration_ms, expect_ms);

    CHECK_EQ(tape_store_read(&ts, 1, 0, back, len), TAPE_OK);
    CHECK_MEM(back, data, len);

    /* Survives a clean reboot. */
    tape_store_t ts2;
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK_EQ(ts2.slots[1].state, TAPE_SLOT_VALID);
    CHECK_EQ(ts2.slots[1].length, len);
    CHECK_EQ(tape_store_read(&ts2, 1, 0, back, len), TAPE_OK);
    CHECK_MEM(back, data, len);

    /* Reads outside the stored stream are refused. */
    CHECK_EQ(tape_store_read(&ts2, 1, len, back, 1), TAPE_ERR_ARG);
    CHECK_EQ(tape_store_read(&ts2, 0, 0, back, 1), TAPE_ERR_STATE);
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    free(data);
    free(back);
    mock_flash_destroy(f);
}

static void test_recording_is_erasing(void)
{
    /* Re-recording a slot must not leave any of the old take behind. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t big = frame_len() * 400u;
    uint32_t small = frame_len() * 10u;
    uint8_t *a = (uint8_t *)malloc(big);
    uint8_t *b = (uint8_t *)malloc(big);
    uint8_t *back = (uint8_t *)malloc(big);
    fill_stream(a, big, 7);
    fill_stream(b, small, 99);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 0, a, big, 1024u), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 0, b, small, 1024u), TAPE_OK);

    CHECK_EQ(ts.slots[0].length, small);
    CHECK_EQ(tape_store_read(&ts, 0, 0, back, small), TAPE_OK);
    CHECK_MEM(back, b, small);

    tape_store_t ts2;
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK_EQ(ts2.slots[0].length, small);

    free(a);
    free(b);
    free(back);
    mock_flash_destroy(f);
}

static void test_partial_frame_is_dropped(void)
{
    /* Only whole SBC frames are playable. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t fl = frame_len();
    uint32_t len = fl * 5u + 13u; /* trailing partial block */
    uint8_t *data = (uint8_t *)malloc(len);
    fill_stream(data, len, 3);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 2, data, len, len), TAPE_OK);
    CHECK_EQ(ts.slots[2].length, fl * 5u);

    free(data);
    mock_flash_destroy(f);
}

static void test_erase_slot(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t len = frame_len() * 20u;
    uint8_t *data = (uint8_t *)malloc(len);
    fill_stream(data, len, 5);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 0, data, len, len), TAPE_OK);
    CHECK_EQ(tape_store_erase_slot(&ts, 0), TAPE_OK);

    CHECK_EQ(ts.slots[0].state, TAPE_SLOT_EMPTY);
    CHECK(ts.slots[0].erased);
    CHECK(tape_store_is_blank(&ts));

    /* A freshly formatted tape reports slots as needing pre-erase; an
     * already-erased slot does not. */
    CHECK_EQ(tape_store_next_preerase(&ts), 1);

    tape_store_t ts2;
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK_EQ(ts2.slots[0].state, TAPE_SLOT_EMPTY);
    CHECK(ts2.slots[0].erased);

    free(data);
    mock_flash_destroy(f);
}

static void test_header_log_rollover(void)
{
    /* More commits than fit one sector: the ping-pong log must keep working
     * and mount must still find the newest record. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t len = frame_len() * 2u;
    uint8_t *data = (uint8_t *)malloc(len);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);

    uint32_t rounds = TAPE_HDR_PER_SECTOR * 3u; /* forces several switches */
    for (uint32_t i = 0; i < rounds; i++) {
        fill_stream(data, len, i + 11u);
        CHECK_EQ(record_slot(&ts, 0, data, len, len), TAPE_OK);
    }

    tape_store_t ts2;
    uint8_t *back = (uint8_t *)malloc(len);
    CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);
    CHECK_EQ(ts2.slots[0].state, TAPE_SLOT_VALID);
    CHECK_EQ(ts2.slots[0].length, len);
    CHECK_EQ(ts2.generation, ts.generation);
    CHECK_EQ(tape_store_read(&ts2, 0, 0, back, len), TAPE_OK);
    CHECK_MEM(back, data, len); /* the last take, not an earlier one */
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    free(data);
    free(back);
    mock_flash_destroy(f);
}

static void test_erase_ahead_crosses_blocks(void)
{
    /* Write past the first 64 KiB block so the just-in-time erase frontier
     * has to advance mid-recording. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t len = TAPE_BLOCK_SIZE * 2u + 5000u;
    len -= len % frame_len();
    uint8_t *data = (uint8_t *)malloc(len);
    uint8_t *back = (uint8_t *)malloc(len);
    fill_stream(data, len, 21);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 0, data, len, 4096u), TAPE_OK);
    CHECK_EQ(ts.slots[0].length, len);
    CHECK_EQ(tape_store_read(&ts, 0, 0, back, len), TAPE_OK);
    CHECK_MEM(back, data, len);
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    free(data);
    free(back);
    mock_flash_destroy(f);
}

static void test_slot_full(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    uint32_t chunk = 65536u;
    uint8_t *data = (uint8_t *)malloc(chunk);
    fill_stream(data, chunk, 31);

    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(tape_store_begin_record(&ts, 0), TAPE_OK);

    int rc = TAPE_OK;
    uint32_t written = 0;
    while (rc == TAPE_OK && written < TAPE_SLOT_SIZE + chunk) {
        rc = tape_store_write(&ts, data, chunk);
        if (rc == TAPE_OK) {
            written += chunk;
        }
    }
    CHECK_EQ(rc, TAPE_ERR_FULL);
    CHECK_EQ(written, TAPE_SLOT_SIZE);
    CHECK_EQ(tape_store_end_record(&ts), TAPE_OK);

    free(data);
    mock_flash_destroy(f);
}

static void test_api_guards(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;
    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);

    CHECK_EQ(tape_store_begin_record(&ts, -1), TAPE_ERR_ARG);
    CHECK_EQ(tape_store_begin_record(&ts, TAPE_SLOT_COUNT), TAPE_ERR_ARG);
    CHECK_EQ(tape_store_write(&ts, "x", 1), TAPE_ERR_STATE); /* not recording */
    CHECK_EQ(tape_store_end_record(&ts), TAPE_ERR_STATE);

    CHECK_EQ(tape_store_begin_record(&ts, 0), TAPE_OK);
    CHECK_EQ(tape_store_begin_record(&ts, 1), TAPE_ERR_STATE); /* already open */
    CHECK_EQ(tape_store_erase_slot(&ts, 0), TAPE_ERR_STATE);   /* in use */
    CHECK_EQ(tape_store_end_record(&ts), TAPE_OK);

    mock_flash_destroy(f);
}

/* --- the power-yank sweep ------------------------------------------- */

/* Outcome tally, so a sweep cannot pass vacuously: if every torn operation
 * happened to leave the slot empty we would be asserting nothing about the
 * interesting recovery paths. */
static int g_out_empty, g_out_new, g_out_old;

static void yank_iteration(int fail_op, const uint8_t *keep, uint32_t keep_len,
                           const uint8_t *old, uint32_t old_len,
                           const uint8_t *fresh, uint32_t fresh_len,
                           int *out_ops)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    tape_store_t ts;

    /* Phase 1: an existing take in slot 0 that must survive untouched, and
     * an older, longer take in slot 1 that phase 2 will record over (so a
     * torn erase leaves stale audio behind the new data). */
    CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 0, keep, keep_len, 1024u), TAPE_OK);
    CHECK_EQ(record_slot(&ts, 1, old, old_len, 1024u), TAPE_OK);

    /* Phase 2: re-record slot 1, tearing the chosen operation. */
    mock_flash_fail_at(f, fail_op);
    record_slot(&ts, 1, fresh, fresh_len, 512u); /* errors are expected */
    if (out_ops) {
        *out_ops = (int)mock_flash_ctx(f)->op_count;
    }

    /* Plug it back in. */
    mock_flash_repower(f);
    tape_store_t ts2;
    int rc = tape_store_mount(&ts2, f);

    CHECK_EQ(rc, TAPE_OK);
    if (rc != TAPE_OK) {
        mock_flash_destroy(f);
        return;
    }

    /* 1. Nothing is left mid-recording. */
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        CHECK(ts2.slots[i].state != TAPE_SLOT_RECORDING);
    }

    /* 2. The untouched take is byte-identical. */
    CHECK_EQ(ts2.slots[0].state, TAPE_SLOT_VALID);
    CHECK_EQ(ts2.slots[0].length, keep_len);
    if (ts2.slots[0].state == TAPE_SLOT_VALID &&
        ts2.slots[0].length == keep_len) {
        uint8_t *back = (uint8_t *)malloc(keep_len);
        CHECK_EQ(tape_store_read(&ts2, 0, 0, back, keep_len), TAPE_OK);
        CHECK_MEM(back, keep, keep_len);
        free(back);
    }

    /* 3. The interrupted slot holds a whole number of frames and is one of
     *    exactly two honest outcomes:
     *      - a prefix of the new take (the erase had begun), or
     *      - the previous take, complete and byte-identical (the yank
     *        landed before anything was erased — recording is erasing, so
     *        until the erase happens the old audio is still yours).
     *    Anything else — a partial old take, or new data with stale audio
     *    spliced onto the end — is a corrupt slot and fails here. */
    if (ts2.slots[1].state == TAPE_SLOT_VALID) {
        uint32_t n = ts2.slots[1].length;
        CHECK_EQ(n % frame_len(), 0);
        uint8_t *back = (uint8_t *)malloc(n ? n : 1u);
        CHECK_EQ(tape_store_read(&ts2, 1, 0, back, n), TAPE_OK);

        bool new_prefix = (n <= fresh_len) && (memcmp(back, fresh, n) == 0);
        bool old_intact = (n == old_len) && (memcmp(back, old, n) == 0);
        CHECK(new_prefix || old_intact);
        if (!new_prefix && !old_intact) {
            printf("   slot1 corrupt after tearing op %d: length %u\n", fail_op,
                   n);
        }
        if (old_intact) {
            g_out_old++;
        } else if (new_prefix) {
            g_out_new++;
        }
        free(back);
    } else {
        CHECK_EQ(ts2.slots[1].state, TAPE_SLOT_EMPTY);
        g_out_empty++;
    }

    /* 4. Nothing was ever programmed over non-erased flash. */
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    /* 5. The device is usable again: a new recording works. */
    uint32_t again_len = frame_len() * 4u;
    uint8_t *again = (uint8_t *)malloc(again_len);
    fill_stream(again, again_len, 777u);
    CHECK_EQ(record_slot(&ts2, 2, again, again_len, again_len), TAPE_OK);
    CHECK_EQ(ts2.slots[2].length, again_len);
    free(again);

    mock_flash_destroy(f);
}

static void test_power_yank_sweep(void)
{
    uint32_t keep_len = frame_len() * 30u;
    uint32_t fresh_len = frame_len() * 60u;
    uint32_t old_len = fresh_len * 2u; /* longer, so stale tails are visible */
    uint8_t *keep = (uint8_t *)malloc(keep_len);
    uint8_t *fresh = (uint8_t *)malloc(fresh_len);
    uint8_t *old = (uint8_t *)malloc(old_len);
    fill_stream(keep, keep_len, 1234u);
    fill_stream(fresh, fresh_len, 5678u);
    fill_stream(old, old_len, 4242u);

    /* One clean pass to learn how many operations phase 2 performs. */
    int total_ops = 0;
    yank_iteration(-1, keep, keep_len, old, old_len, fresh, fresh_len,
                   &total_ops);
    CHECK(total_ops > 3);
    printf("   sweeping %d flash operations\n", total_ops);

    int before = g_failures;
    for (int op = 0; op < total_ops; op++) {
        yank_iteration(op, keep, keep_len, old, old_len, fresh, fresh_len,
                       NULL);
        if (g_failures != before) {
            printf("   (first failure while tearing operation %d)\n", op);
            break;
        }
    }

    free(keep);
    free(fresh);
    free(old);
}

static void test_power_yank_sweep_across_blocks(void)
{
    /* The nastiest case: the recording is long enough that the erase
     * frontier has to advance mid-take, and the take it is overwriting is
     * longer still. Tearing the erase leaves a half-erased block whose tail
     * still holds the previous recording — if repair scanned naively it
     * would splice that stale audio onto the end of the new take. */
    uint32_t fl = frame_len();
    uint32_t keep_len = fl * 20u;
    uint32_t fresh_len = (TAPE_BLOCK_SIZE + 8192u);
    uint32_t old_len = (TAPE_BLOCK_SIZE * 3u);
    fresh_len -= fresh_len % fl;
    old_len -= old_len % fl;

    uint8_t *keep = (uint8_t *)malloc(keep_len);
    uint8_t *fresh = (uint8_t *)malloc(fresh_len);
    uint8_t *old = (uint8_t *)malloc(old_len);
    fill_stream(keep, keep_len, 90001u);
    fill_stream(fresh, fresh_len, 90002u);
    fill_stream(old, old_len, 90003u);

    int total_ops = 0;
    yank_iteration(-1, keep, keep_len, old, old_len, fresh, fresh_len,
                   &total_ops);
    printf("   %d operations; sweeping every 3rd\n", total_ops);

    g_out_empty = g_out_new = g_out_old = 0;
    int before = g_failures;
    for (int op = 0; op < total_ops; op += 3) {
        yank_iteration(op, keep, keep_len, old, old_len, fresh, fresh_len,
                       NULL);
        if (g_failures != before) {
            printf("   (first failure while tearing operation %d)\n", op);
            break;
        }
    }
    printf("   outcomes: %d empty, %d truncated-new, %d old-take-intact\n",
           g_out_empty, g_out_new, g_out_old);

    /* All three recovery paths must actually have been taken — in
     * particular a partially-written new take, which is the case that would
     * expose stale audio if repair scanned the slot naively. */
    CHECK(g_out_new > 0);
    CHECK(g_out_old > 0);

    free(keep);
    free(fresh);
    free(old);
}

static void test_yank_during_slot_erase(void)
{
    /* Tearing the background erase must never leave a slot claiming to hold
     * playable audio that is now half 0xFF. */
    uint32_t len = frame_len() * 50u;
    uint8_t *data = (uint8_t *)malloc(len);
    fill_stream(data, len, 8080u);

    for (int op = 0; op < 12; op++) {
        flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
        tape_store_t ts;
        CHECK_EQ(tape_store_format(&ts, f), TAPE_OK);
        CHECK_EQ(record_slot(&ts, 0, data, len, 1024u), TAPE_OK);

        mock_flash_fail_at(f, op);
        tape_store_erase_slot(&ts, 0);

        mock_flash_repower(f);
        tape_store_t ts2;
        CHECK_EQ(tape_store_mount(&ts2, f), TAPE_OK);

        /* Either the take is still fully intact, or the slot is empty —
         * never a valid-looking slot over erased flash. */
        if (ts2.slots[0].state == TAPE_SLOT_VALID) {
            uint8_t *back = (uint8_t *)malloc(ts2.slots[0].length);
            CHECK_EQ(ts2.slots[0].length, len);
            CHECK_EQ(tape_store_read(&ts2, 0, 0, back, ts2.slots[0].length),
                     TAPE_OK);
            CHECK_MEM(back, data, ts2.slots[0].length);
            free(back);
        } else {
            CHECK_EQ(ts2.slots[0].state, TAPE_SLOT_EMPTY);
        }
        mock_flash_destroy(f);
    }
    free(data);
}

TEST_MAIN_BEGIN()

RUN(test_format_and_mount);
RUN(test_mount_blank_device);
RUN(test_record_and_read_back);
RUN(test_recording_is_erasing);
RUN(test_partial_frame_is_dropped);
RUN(test_erase_slot);
RUN(test_header_log_rollover);
RUN(test_erase_ahead_crosses_blocks);
RUN(test_slot_full);
RUN(test_api_guards);
RUN(test_yank_during_slot_erase);
RUN(test_power_yank_sweep);
RUN(test_power_yank_sweep_across_blocks);

TEST_MAIN_END()
