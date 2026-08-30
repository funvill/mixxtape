/* flash_hal_t implemented against the 16 MiB NOR flash on VSPI.
 *
 * This is the one piece that turns the host-tested slot manager into
 * something that runs on the board. Everything above flash_hal.h has been
 * written and tested against a mock; this fills in the real chip.
 *
 * NOT YET RUN ON HARDWARE — there are no boards. Bring-up notes are in the
 * .c file next to the code they concern.
 */
#ifndef SPI_FLASH_HAL_H
#define SPI_FLASH_HAL_H

#include "flash_hal.h"

/* Brings up the SPI bus and the flash device, verifies the JEDEC ID, and
 * fills in `out`. Returns ESP_OK, or an error if the part does not answer
 * or answers with the wrong ID. */
int spi_flash_hal_init(flash_hal_t *out);

/* Reads the JEDEC ID (0xC84018 for the fitted GD25Q128E, 0xEF4018 for a
 * W25Q128JV - both are accepted). Useful on its own for the
 * factory test, which wants the number even when it does not match. */
int spi_flash_hal_read_id(uint32_t *id_out);

void spi_flash_hal_deinit(void);

#endif /* SPI_FLASH_HAL_H */
