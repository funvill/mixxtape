/* W25Q128JV driver behind flash_hal.h.
 *
 * Deliberately plain single-SPI: no quad mode, no DMA cleverness. The tape
 * needs 22 KB/s sustained to record and a little more to play, against a
 * bus doing 40 Mbit/s. Reaching for QSPI here would buy nothing and cost
 * the two extra pins the microSD option wants.
 *
 * NOT YET RUN ON HARDWARE.
 */

#include "spi_flash_hal.h"

#include <string.h>

#include "board.h"
#include "driver/spi_master.h"
#include "esp_attr.h"   /* WORD_ALIGNED_ATTR */
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tape_layout.h"

static const char *TAG = "flash";

/* W25Q128JV command set (datasheet section 8.1). */
#define CMD_WRITE_ENABLE 0x06u
#define CMD_READ_STATUS1 0x05u
#define CMD_READ_DATA    0x03u
#define CMD_PAGE_PROGRAM 0x02u
#define CMD_SECTOR_ERASE 0x20u   /* 4 KiB  */
#define CMD_BLOCK_ERASE  0xD8u   /* 64 KiB */
#define CMD_JEDEC_ID     0x9Fu

#define STATUS_BUSY 0x01u
#define STATUS_WEL  0x02u

/* Either 128 Mbit part may be fitted; they differ only in the manufacturer
 * byte of the JEDEC id. Every command this driver uses - 0x06, 0x05, 0x03,
 * 0x02, 0x20, 0xD8, 0x9F - and the 256 B page / 4 KiB sector / 64 KiB block
 * geometry are identical between them. */
#define JEDEC_WINBOND_W25Q128     0xEF4018u
#define JEDEC_GIGADEVICE_GD25Q128 0xC84018u

static const char *jedec_name(uint32_t id)
{
    switch (id) {
    case JEDEC_WINBOND_W25Q128:     return "W25Q128 (Winbond)";
    case JEDEC_GIGADEVICE_GD25Q128: return "GD25Q128 (GigaDevice)";
    default:                        return NULL;
    }
}

/* Erases are slow: a 64 KiB block is 150 ms typical but up to 1 s on a
 * tired part, and a chip erase far longer. The record path never waits on
 * one (the erase frontier runs ahead of the writer), so a generous ceiling
 * costs nothing and a tight one would brick a slow chip. */
#define BUSY_TIMEOUT_MS 3000

static spi_device_handle_t s_dev;
static bool s_ready;

/* --- primitives ------------------------------------------------------- */

static esp_err_t cmd_only(uint8_t cmd)
{
    spi_transaction_t t = {0};
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = cmd;
    return spi_device_polling_transmit(s_dev, &t);
}

static esp_err_t read_status(uint8_t *status)
{
    spi_transaction_t t = {0};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16;
    t.rxlength = 16;
    t.tx_data[0] = CMD_READ_STATUS1;
    esp_err_t err = spi_device_polling_transmit(s_dev, &t);
    if (err == ESP_OK) {
        *status = t.rx_data[1];
    }
    return err;
}

static esp_err_t wait_ready(void)
{
    /* Poll fast at first: a page program is ~0.7 ms, so yielding a whole
     * tick straight away would triple the cost of every write. */
    for (int spins = 0; spins < 200; spins++) {
        uint8_t st = 0;
        esp_err_t err = read_status(&st);
        if (err != ESP_OK) {
            return err;
        }
        if (!(st & STATUS_BUSY)) {
            return ESP_OK;
        }
    }
    for (int ms = 0; ms < BUSY_TIMEOUT_MS; ms++) {
        uint8_t st = 0;
        esp_err_t err = read_status(&st);
        if (err != ESP_OK) {
            return err;
        }
        if (!(st & STATUS_BUSY)) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGE(TAG, "still busy after %d ms", BUSY_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t write_enable(void)
{
    esp_err_t err = wait_ready();
    if (err != ESP_OK) {
        return err;
    }
    err = cmd_only(CMD_WRITE_ENABLE);
    if (err != ESP_OK) {
        return err;
    }
    /* Confirm the latch actually set. If it did not, the erase or program
     * about to be issued would be silently ignored, which on a recorder
     * looks like a take that vanished. */
    uint8_t st = 0;
    err = read_status(&st);
    if (err != ESP_OK) {
        return err;
    }
    return (st & STATUS_WEL) ? ESP_OK : ESP_FAIL;
}

static esp_err_t addr_cmd(uint8_t cmd, uint32_t addr)
{
    spi_transaction_t t = {0};
    uint8_t tx[4] = {cmd,
                     (uint8_t)(addr >> 16),
                     (uint8_t)(addr >> 8),
                     (uint8_t)addr};
    t.length = 32;
    t.tx_buffer = tx;
    return spi_device_polling_transmit(s_dev, &t);
}

/* --- flash_hal_t implementation ---------------------------------------- */

static int hal_read(const flash_hal_t *f, uint32_t addr, void *dst,
                    uint32_t len)
{
    (void)f;
    if (!s_ready || !dst) {
        return FLASH_ERR_ARG;
    }
    if (addr + len > TAPE_FLASH_SIZE) {
        return FLASH_ERR_ARG;
    }
    if (wait_ready() != ESP_OK) {
        return FLASH_ERR_IO;
    }

    /* One transaction: 4 command/address bytes then the payload. The
     * driver's per-transaction limit is set by max_transfer_sz at bus
     * init, so long reads are chunked here rather than assumed. */
    uint8_t *out = (uint8_t *)dst;
    while (len) {
        uint32_t chunk = len > 2048u ? 2048u : len;
        spi_transaction_t t = {0};
        /* rx_buffer must cover the whole transaction, command bytes
         * included, so read into a scratch and copy past the header.
         *
         * tx_buffer must ALSO cover the whole transaction: t.length governs
         * how much the driver clocks out, and with DMA enabled it builds a
         * descriptor over tx_buffer for that full length. A 4-byte stack
         * array here meant a 2048-byte DMA read off the end of the stack.
         * Both buffers are word-aligned and static, as the DMA path
         * requires. */
        static WORD_ALIGNED_ATTR uint8_t scratch[4 + 2048];
        static WORD_ALIGNED_ATTR uint8_t txbuf[4 + 2048];
        txbuf[0] = CMD_READ_DATA;
        txbuf[1] = (uint8_t)(addr >> 16);
        txbuf[2] = (uint8_t)(addr >> 8);
        txbuf[3] = (uint8_t)addr;
        t.length = 8 * (4 + chunk);
        t.rxlength = t.length;
        t.tx_buffer = txbuf;
        memset(scratch, 0xFF, 4);
        t.rx_buffer = scratch;

        if (spi_device_polling_transmit(s_dev, &t) != ESP_OK) {
            return FLASH_ERR_IO;
        }
        memcpy(out, scratch + 4, chunk);
        out += chunk;
        addr += chunk;
        len -= chunk;
    }
    return FLASH_OK;
}

static int hal_program(const flash_hal_t *f, uint32_t addr, const void *src,
                       uint32_t len)
{
    if (!s_ready || !src || len == 0) {
        return FLASH_ERR_ARG;
    }
    /* The caller (tape_store) already splits at page boundaries; check it
     * anyway, because a straddling program wraps within the page on this
     * part rather than erroring, and silently corrupts audio. */
    if ((addr / f->page_size) != ((addr + len - 1u) / f->page_size)) {
        return FLASH_ERR_ARG;
    }
    if (write_enable() != ESP_OK) {
        return FLASH_ERR_IO;
    }

    spi_transaction_t t = {0};
    static uint8_t buf[4 + 256];
    buf[0] = CMD_PAGE_PROGRAM;
    buf[1] = (uint8_t)(addr >> 16);
    buf[2] = (uint8_t)(addr >> 8);
    buf[3] = (uint8_t)addr;
    memcpy(buf + 4, src, len);
    t.length = 8 * (4 + len);
    t.tx_buffer = buf;

    if (spi_device_polling_transmit(s_dev, &t) != ESP_OK) {
        return FLASH_ERR_IO;
    }
    return wait_ready() == ESP_OK ? FLASH_OK : FLASH_ERR_IO;
}

static int erase(uint8_t cmd, uint32_t addr, uint32_t unit)
{
    if (!s_ready) {
        return FLASH_ERR_ARG;
    }
    if (addr + unit > TAPE_FLASH_SIZE) {
        return FLASH_ERR_ARG;
    }
    if (write_enable() != ESP_OK) {
        return FLASH_ERR_IO;
    }
    if (addr_cmd(cmd, (addr / unit) * unit) != ESP_OK) {
        return FLASH_ERR_IO;
    }
    return wait_ready() == ESP_OK ? FLASH_OK : FLASH_ERR_IO;
}

static int hal_erase_sector(const flash_hal_t *f, uint32_t addr)
{
    return erase(CMD_SECTOR_ERASE, addr, f->sector_size);
}

static int hal_erase_block(const flash_hal_t *f, uint32_t addr)
{
    return erase(CMD_BLOCK_ERASE, addr, f->block_size);
}

/* --- bring-up ---------------------------------------------------------- */

int spi_flash_hal_read_id(uint32_t *id_out)
{
    if (!s_ready || !id_out) {
        return ESP_ERR_INVALID_STATE;
    }
    spi_transaction_t t = {0};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 32;
    t.rxlength = 32;
    t.tx_data[0] = CMD_JEDEC_ID;

    esp_err_t err = spi_device_polling_transmit(s_dev, &t);
    if (err != ESP_OK) {
        return err;
    }
    *id_out = ((uint32_t)t.rx_data[1] << 16) |
              ((uint32_t)t.rx_data[2] << 8) | t.rx_data[3];
    return ESP_OK;
}

int spi_flash_hal_init(flash_hal_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4 + 2048,
    };
    esp_err_t err = spi_bus_initialize(VSPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t dev = {
        /* 20 MHz, and NOT the 40 MHz the flash itself allows for plain
         * read (0x03). Every transaction here is FULL DUPLEX - hal_read
         * sets both tx_buffer and rx_buffer - and ESP-IDF cannot insert
         * the dummy bits that compensate for MISO input delay in full
         * duplex, which caps the bus at SPI_MASTER_FREQ_26M. At 40 MHz the
         * sample point lands past the valid window and the JEDEC ID reads
         * back as garbage, so init fails and the tape never mounts.
         *
         * 20 MHz leaves margin over that 26 MHz ceiling for a slow part or
         * a warm board. The record path needs ~22 KB/s; this is four
         * orders of magnitude more than that, so the margin is free. */
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_FLASH_CS,
        .queue_size = 2,
    };
    err = spi_bus_add_device(VSPI_HOST, &dev, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device: %s", esp_err_to_name(err));
        return err;
    }
    s_ready = true;

    uint32_t id = 0;
    err = spi_flash_hal_read_id(&id);
    if (err != ESP_OK) {
        s_ready = false;
        return err;
    }
    const char *part = jedec_name(id);
    if (!part) {
        ESP_LOGE(TAG, "unexpected JEDEC id 0x%06X (expected 0x%06X or 0x%06X)",
                 (unsigned)id, (unsigned)JEDEC_WINBOND_W25Q128,
                 (unsigned)JEDEC_GIGADEVICE_GD25Q128);
        s_ready = false;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "%s found, id 0x%06X", part, (unsigned)id);

    memset(out, 0, sizeof(*out));
    out->size = TAPE_FLASH_SIZE;
    out->page_size = TAPE_PAGE_SIZE;
    out->sector_size = TAPE_SECTOR_SIZE;
    out->block_size = TAPE_BLOCK_SIZE;
    out->read = hal_read;
    out->program = hal_program;
    out->erase_sector = hal_erase_sector;
    out->erase_block = hal_erase_block;
    return ESP_OK;
}

void spi_flash_hal_deinit(void)
{
    if (s_ready) {
        spi_bus_remove_device(s_dev);
        s_ready = false;
    }
}
