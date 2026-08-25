/* Slot manager for the audio flash.
 *
 * Crash-safety contract (there is no battery, so a USB yank can land
 * between any two flash operations):
 *
 *  - The header is an append-only log of fixed-size records across two
 *    ping-pong sectors. A record is committed by a single page program;
 *    a torn write fails CRC and the previous record still wins.
 *  - Only the slot being recorded may be damaged by a yank. A slot left
 *    in RECORDING state is repaired at mount by scanning for the last
 *    complete SBC frame — you keep what was recorded, like real tape.
 *  - Recording never erases on the critical path: slots are pre-erased in
 *    the background, or erased one block ahead of the write pointer.
 *
 * See docs/storage-budget.md.
 */
#ifndef TAPE_STORE_H
#define TAPE_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "flash_hal.h"
#include "tape_layout.h"

#define TAPE_OK             0
#define TAPE_ERR_ARG       (-1)
#define TAPE_ERR_IO        (-2)
#define TAPE_ERR_STATE     (-3)
#define TAPE_ERR_FULL      (-4)
#define TAPE_ERR_NO_HEADER (-5)

typedef enum {
    TAPE_SLOT_EMPTY     = 0,
    TAPE_SLOT_VALID     = 1,
    TAPE_SLOT_RECORDING = 2,
} tape_slot_state_t;

typedef struct {
    tape_slot_state_t state;
    uint32_t length;      /* bytes of SBC stream held      */
    uint32_t duration_ms; /* playing time                  */
    bool     erased;      /* pre-erased, ready to record   */
} tape_slot_t;

typedef struct {
    const flash_hal_t *flash;
    tape_slot_t slots[TAPE_SLOT_COUNT];
    uint32_t generation;

    /* header log cursor */
    uint32_t hdr_sector;  /* 0 or 1                                    */
    uint32_t hdr_offset;  /* byte offset of next free record in sector */

    /* live recording state */
    int      rec_slot;      /* -1 when idle              */
    uint32_t rec_written;   /* bytes committed to flash  */
    uint32_t rec_erased_to; /* bytes erased ahead        */
    uint32_t frame_bytes;   /* cached SBC frame size     */
} tape_store_t;

/* Wipes the header region and marks every slot empty. Does not erase audio. */
int tape_store_format(tape_store_t *ts, const flash_hal_t *flash);

/* Reads the header log, picks the newest valid record, and repairs a slot
 * left mid-recording. Safe to call on a blank device (formats it). */
int tape_store_mount(tape_store_t *ts, const flash_hal_t *flash);

/* Begins recording into `slot`, discarding whatever it held. Erases lazily:
 * only enough leading blocks to start writing. */
int tape_store_begin_record(tape_store_t *ts, int slot);

/* Appends encoded audio to the open slot, erasing ahead as needed. */
int tape_store_write(tape_store_t *ts, const void *data, uint32_t len);

/* Closes the open slot and commits it as playable. */
int tape_store_end_record(tape_store_t *ts);

/* Reads from a slot's stored stream. */
int tape_store_read(tape_store_t *ts, int slot, uint32_t offset, void *dst,
                    uint32_t len);

/* Erases a slot completely — background/idle work, never during record. */
int tape_store_erase_slot(tape_store_t *ts, int slot);

/* Index of the next slot that would benefit from background pre-erase,
 * or -1 if none. */
int tape_store_next_preerase(const tape_store_t *ts);

/* True when the tape holds no playable audio. */
bool tape_store_is_blank(const tape_store_t *ts);

uint32_t tape_crc32(const void *data, uint32_t len);

#endif /* TAPE_STORE_H */
