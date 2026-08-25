/* In-memory NOR flash model with power-loss fault injection.
 *
 * Models the parts of a W25Q128JV that can bite us:
 *   - erase sets bits to 1, program may only clear them to 0
 *   - programming over non-erased flash is silently wrong on real hardware,
 *     so it is counted here and tests assert the count stays zero
 *   - a yank mid-operation tears the write: part of the data lands, the
 *     rest does not, and every later operation fails until "power" returns
 */
#ifndef MOCK_FLASH_H
#define MOCK_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "flash_hal.h"

typedef struct {
    uint8_t *mem;
    uint32_t size;

    int32_t  fail_at_op;  /* -1 = never; else the op index that tears     */
    uint32_t op_count;    /* program + erase operations attempted         */
    bool     powered;

    uint32_t programs;
    uint32_t erases;
    uint32_t dirty_programs; /* programs that needed a 0 -> 1 bit change  */
} mock_flash_t;

flash_hal_t *mock_flash_create(uint32_t size);
void mock_flash_destroy(flash_hal_t *f);
mock_flash_t *mock_flash_ctx(const flash_hal_t *f);

/* Tear the op with this index (0-based, counted since the last reset). */
void mock_flash_fail_at(flash_hal_t *f, int32_t op_index);

/* Simulates the cable being plugged back in: memory persists, power returns,
 * op counter resets. */
void mock_flash_repower(flash_hal_t *f);

#endif /* MOCK_FLASH_H */
