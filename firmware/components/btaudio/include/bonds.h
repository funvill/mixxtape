/* Persistent A2DP bond table.
 *
 * "Plug it in and it auto-reconnects to the last thing it paired with" is
 * a locked decision, and it is most of what makes the object feel finished
 * rather than fiddly. That needs bonds to survive a power cut, which on
 * this board is the normal way of switching off.
 *
 * So the table lives in the spare flash region and uses the same
 * append-only ping-pong log as the tape header: a record is committed by
 * one page program, a torn write fails CRC and the previous record still
 * wins. Losing a bond is not catastrophic — you re-pair — but silently
 * losing *all* of them because a record was half-written would be.
 *
 * Ordering is most-recently-used first, because power-up tries the last
 * sink first and then the others.
 */
#ifndef BONDS_H
#define BONDS_H

#include <stdbool.h>
#include <stdint.h>

#include "flash_hal.h"
#include "tape_layout.h"

#define BONDS_MAX       8u
#define BONDS_NAME_LEN  20u
#define BONDS_RECORD_SZ 256u /* one flash page; 16 records per sector */
#define BONDS_SECTORS   2u

#define BONDS_OK          0
#define BONDS_ERR_ARG    (-1)
#define BONDS_ERR_IO     (-2)
#define BONDS_ERR_FULL   (-4)

typedef struct {
    uint8_t  bda[6];
    uint32_t last_used; /* counter, not a clock: there is no RTC */
    char     name[BONDS_NAME_LEN];
} bond_t;

typedef struct {
    const flash_hal_t *flash;
    uint32_t base; /* byte offset of the bond log in flash */

    bond_t   list[BONDS_MAX]; /* most recently used first */
    uint32_t count;
    uint32_t generation;
    uint32_t use_counter;

    uint32_t sector; /* 0 or 1 */
    uint32_t offset; /* next free record in that sector */
} bonds_t;

/* Wipes the log. */
int bonds_format(bonds_t *b, const flash_hal_t *flash, uint32_t base);

/* Loads the newest valid record; formats a blank region rather than
 * failing. `base` must be sector-aligned — TAPE_SPARE_BASE in firmware. */
int bonds_mount(bonds_t *b, const flash_hal_t *flash, uint32_t base);

/* Records a bond, or refreshes one already known. When the table is full
 * the least recently used entry is dropped. */
int bonds_add(bonds_t *b, const uint8_t bda[6], const char *name);

/* Marks an existing bond as the most recent. Returns BONDS_ERR_ARG if it
 * is not known. */
int bonds_touch(bonds_t *b, const uint8_t bda[6]);

int bonds_forget(bonds_t *b, const uint8_t bda[6]);
int bonds_forget_all(bonds_t *b);

/* Index of `bda`, or -1. */
int bonds_find(const bonds_t *b, const uint8_t bda[6]);

/* Bond at `index` in most-recently-used order, or NULL. Index 0 is the one
 * to try first on power-up. */
const bond_t *bonds_get(const bonds_t *b, uint32_t index);

uint32_t bonds_count(const bonds_t *b);

#endif /* BONDS_H */
