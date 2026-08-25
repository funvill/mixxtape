/* Abstract NOR flash interface.
 *
 * Everything above this line is portable C and host-testable; the ESP-IDF
 * SPI driver and the unit-test mock both implement it.  Semantics follow
 * the W25Q128JV: erase sets bits to 1, program may only clear bits to 0,
 * programs never straddle a page boundary.
 */
#ifndef FLASH_HAL_H
#define FLASH_HAL_H

#include <stdint.h>

#define FLASH_OK              0
#define FLASH_ERR_ARG        (-1)
#define FLASH_ERR_IO         (-2)
#define FLASH_ERR_POWER_LOSS (-3) /* injected by tests; real HAL never returns it */

typedef struct flash_hal flash_hal_t;

struct flash_hal {
    uint32_t size;        /* total bytes                        */
    uint32_t page_size;   /* program granularity (256)          */
    uint32_t sector_size; /* small erase unit (4096)            */
    uint32_t block_size;  /* large erase unit (65536)           */

    int (*read)(const flash_hal_t *f, uint32_t addr, void *dst, uint32_t len);
    /* Programs within a single page; caller must not straddle pages. */
    int (*program)(const flash_hal_t *f, uint32_t addr, const void *src, uint32_t len);
    int (*erase_sector)(const flash_hal_t *f, uint32_t addr);
    int (*erase_block)(const flash_hal_t *f, uint32_t addr);

    void *ctx;
};

#endif /* FLASH_HAL_H */
