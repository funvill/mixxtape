/* Playback: turns the three slots into one tape side.
 *
 * PLAY plays every recorded track in order — it is a side, not three voice
 * memos — so this owns track sequencing, position, and the decode that
 * feeds Bluedroid.
 *
 * Pull-based on purpose. The A2DP source callback runs on the Bluetooth
 * task and must never block, so in firmware a prefetch task drains this
 * into a PCM ring and the callback only ever copies out of that ring.
 * Keeping the logic pull-shaped means the whole sequencer is host-testable
 * with no RTOS in sight.
 */
#ifndef TAPE_PLAYER_H
#define TAPE_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "adpcm_block.h"
#include "tape_store.h"

typedef enum {
    TAPE_PLAY_STOPPED = 0,
    TAPE_PLAY_PLAYING,
    TAPE_PLAY_PAUSED,
    TAPE_PLAY_FINISHED, /* reached the end of the side */
} tape_play_state_t;

typedef struct {
    tape_store_t *store;
    tape_play_state_t state;

    int      track;   /* slot being played, -1 when none       */
    uint32_t block;   /* next block index within that slot     */
    uint32_t cursor;  /* samples already handed out of pcm[]   */
    uint32_t have;    /* valid samples in pcm[]                */
    uint32_t elapsed_ms_before; /* duration of finished tracks */

    int16_t pcm[ADPCM_BLOCK_SAMPLES];
} tape_player_t;

/* Starts the side at `track`, skipping forward over empty slots. Returns
 * TAPE_OK, or TAPE_ERR_STATE if the tape holds no audio at all. */
int tape_player_start(tape_player_t *p, tape_store_t *store, int track);

void tape_player_stop(tape_player_t *p);
void tape_player_pause(tape_player_t *p);
void tape_player_resume(tape_player_t *p);

/* Jumps to `track` (or the next recorded one after it) and plays from its
 * start. Returns TAPE_OK, or TAPE_ERR_STATE if nothing is recorded. */
int tape_player_seek_track(tape_player_t *p, int track);

/* Skips to the next recorded track, wrapping. */
int tape_player_next_track(tape_player_t *p);

/* Fills up to `samples` of mono PCM, crossing track boundaries as needed.
 * Returns how many samples were produced; fewer than asked means the side
 * ended (state becomes TAPE_PLAY_FINISHED). Returns 0 when not playing. */
uint32_t tape_player_read(tape_player_t *p, int16_t *dst, uint32_t samples);

/* Position within the whole side, and the side's total length. */
uint32_t tape_player_position_ms(const tape_player_t *p);
uint32_t tape_player_side_ms(const tape_store_t *store);

/* Progress across the side in parts per thousand — what the reel rings
 * render: the left ring empties as this rises, the right ring fills. */
uint32_t tape_player_progress_permille(const tape_player_t *p);

#endif /* TAPE_PLAYER_H */
