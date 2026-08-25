#include "tape_player.h"

#include <string.h>

/* Next slot at or after `from` that holds audio, or -1. */
static int next_recorded(const tape_store_t *store, int from)
{
    for (int i = from; i < TAPE_SLOT_COUNT; i++) {
        if (store->slots[i].state == TAPE_SLOT_VALID &&
            store->slots[i].length > 0) {
            return i;
        }
    }
    return -1;
}

static int first_recorded(const tape_store_t *store)
{
    return next_recorded(store, 0);
}

static uint32_t track_blocks(const tape_store_t *store, int track)
{
    return store->slots[track].length / ADPCM_BLOCK_BYTES;
}

/* Loads and decodes the next block of the current track. Returns false when
 * the track is exhausted. */
static bool load_block(tape_player_t *p)
{
    if (p->track < 0 || p->block >= track_blocks(p->store, p->track)) {
        return false;
    }
    uint8_t raw[ADPCM_BLOCK_BYTES];
    if (tape_store_read(p->store, p->track, p->block * ADPCM_BLOCK_BYTES, raw,
                        ADPCM_BLOCK_BYTES) != TAPE_OK) {
        return false;
    }
    uint32_t n = adpcm_block_decode(raw, p->pcm);
    if (n == 0) {
        return false; /* corrupt block: treat as the end of the track */
    }
    p->have = n;
    p->cursor = 0;
    p->block++;
    return true;
}

static void begin_track(tape_player_t *p, int track)
{
    p->track = track;
    p->block = 0;
    p->cursor = 0;
    p->have = 0;
}

/* Duration of every recorded track before `track`. Position is measured
 * across the whole side, not within one track. */
static uint32_t ms_before(const tape_store_t *store, int track)
{
    uint32_t total = 0;
    for (int i = 0; i < track; i++) {
        if (store->slots[i].state == TAPE_SLOT_VALID) {
            total += store->slots[i].duration_ms;
        }
    }
    return total;
}

int tape_player_start(tape_player_t *p, tape_store_t *store, int track)
{
    if (!p || !store) {
        return TAPE_ERR_ARG;
    }
    memset(p, 0, sizeof(*p));
    p->store = store;
    p->track = -1;

    if (track < 0 || track >= TAPE_SLOT_COUNT) {
        track = 0;
    }
    int t = next_recorded(store, track);
    if (t < 0) {
        t = first_recorded(store); /* wrap: the side may start earlier */
    }
    if (t < 0) {
        p->state = TAPE_PLAY_STOPPED;
        return TAPE_ERR_STATE;
    }
    begin_track(p, t);
    p->elapsed_ms_before = ms_before(store, t);
    p->state = TAPE_PLAY_PLAYING;
    return TAPE_OK;
}

void tape_player_stop(tape_player_t *p)
{
    if (p) {
        p->state = TAPE_PLAY_STOPPED;
        p->track = -1;
        p->have = 0;
        p->cursor = 0;
    }
}

void tape_player_pause(tape_player_t *p)
{
    if (p && p->state == TAPE_PLAY_PLAYING) {
        p->state = TAPE_PLAY_PAUSED;
    }
}

void tape_player_resume(tape_player_t *p)
{
    if (p && p->state == TAPE_PLAY_PAUSED) {
        p->state = TAPE_PLAY_PLAYING;
    }
}

int tape_player_seek_track(tape_player_t *p, int track)
{
    if (!p || !p->store) {
        return TAPE_ERR_ARG;
    }
    if (track < 0) {
        track = 0;
    }
    track %= TAPE_SLOT_COUNT;

    int t = next_recorded(p->store, track);
    if (t < 0) {
        t = first_recorded(p->store);
    }
    if (t < 0) {
        tape_player_stop(p);
        return TAPE_ERR_STATE;
    }
    begin_track(p, t);
    p->elapsed_ms_before = ms_before(p->store, t);
    p->state = TAPE_PLAY_PLAYING;
    return TAPE_OK;
}

int tape_player_next_track(tape_player_t *p)
{
    if (!p || !p->store) {
        return TAPE_ERR_ARG;
    }
    int start = (p->track < 0) ? 0 : (p->track + 1) % TAPE_SLOT_COUNT;
    return tape_player_seek_track(p, start);
}

uint32_t tape_player_read(tape_player_t *p, int16_t *dst, uint32_t samples)
{
    if (!p || !dst || p->state != TAPE_PLAY_PLAYING) {
        return 0;
    }
    uint32_t produced = 0;

    while (produced < samples) {
        if (p->cursor >= p->have) {
            if (!load_block(p)) {
                /* Track exhausted: run straight on into the next recorded
                 * one — this is a tape side, not three separate memos. */
                int t = (p->track < 0)
                            ? -1
                            : next_recorded(p->store, p->track + 1);
                if (t < 0) {
                    p->state = TAPE_PLAY_FINISHED;
                    break;
                }
                if (p->track >= 0) {
                    p->elapsed_ms_before +=
                        p->store->slots[p->track].duration_ms;
                }
                begin_track(p, t);
                continue;
            }
        }
        uint32_t avail = p->have - p->cursor;
        uint32_t want = samples - produced;
        uint32_t n = (avail < want) ? avail : want;
        memcpy(dst + produced, p->pcm + p->cursor, n * sizeof(int16_t));
        p->cursor += n;
        produced += n;
    }
    return produced;
}

uint32_t tape_player_position_ms(const tape_player_t *p)
{
    if (!p) {
        return 0;
    }
    if (p->track < 0) {
        return p->elapsed_ms_before;
    }
    /* Whole blocks already handed out of this track, plus however far into
     * the block currently being drained we are. */
    uint32_t whole = (p->block > 0) ? (p->block - 1u) : 0u;
    uint32_t ms =
        p->elapsed_ms_before + adpcm_blocks_to_ms(whole, TAPE_SAMPLE_RATE);
    if (p->have) {
        ms += (uint32_t)(((uint64_t)p->cursor * 1000u) / TAPE_SAMPLE_RATE);
    }
    return ms;
}

uint32_t tape_player_side_ms(const tape_store_t *store)
{
    if (!store) {
        return 0;
    }
    uint32_t total = 0;
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        if (store->slots[i].state == TAPE_SLOT_VALID) {
            total += store->slots[i].duration_ms;
        }
    }
    return total;
}

uint32_t tape_player_progress_permille(const tape_player_t *p)
{
    if (!p || !p->store) {
        return 0;
    }
    if (p->state == TAPE_PLAY_FINISHED) {
        return 1000u;
    }
    uint32_t total = tape_player_side_ms(p->store);
    if (!total) {
        return 0;
    }
    uint64_t pos = tape_player_position_ms(p);
    uint32_t permille = (uint32_t)((pos * 1000u) / total);
    return (permille > 1000u) ? 1000u : permille;
}
