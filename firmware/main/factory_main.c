/* Factory test runner (plan milestone M5).
 *
 * Twenty boards get built by hand. This is what the jig runs on each one
 * before production firmware goes on: probe every subsystem, print one
 * greppable line, move to the next board.
 *
 *   idf.py -DMIXXTAPE_FACTORY_TEST=ON build flash monitor
 *
 * Log the jig's serial output to a file and that file is the build record
 * for the run — one `FT|1|...` line per board, with measured values, so a
 * board that comes back flaky later can be looked up rather than guessed
 * about.
 *
 * ── Status ────────────────────────────────────────────────────────────
 * The harness (components/factory) is host-tested. The probes below are
 * NOT — there is no board yet, and no ESP-IDF install here to compile
 * against. Treat this file as the bring-up checklist it is: probes that
 * depend on drivers M4 still has to write report SKIP with a "TODO M4"
 * detail, and the runner refuses to print a clean PASS banner while any
 * of them remain. Fill them in as each driver lands.
 */

#ifdef MIXXTAPE_FACTORY_TEST

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "factory_test.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tape_layout.h"

static const char *TAG = "factory";

/* W25Q128JV: manufacturer 0xEF (Winbond), type 0x40, capacity 0x18 (16 MiB). */
#define EXPECT_JEDEC_ID 0xEF4018

typedef struct {
    spi_device_handle_t flash;
    bool flash_ready;
} probe_ctx_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* --- flash ----------------------------------------------------------- */

static int probe_flash_id(void *user, int32_t *measured, char *detail)
{
    probe_ctx_t *ctx = (probe_ctx_t *)user;
    if (!ctx->flash_ready) {
        snprintf(detail, FT_DETAIL_LEN, "spi bus not up");
        return -1;
    }
    uint8_t rx[3] = {0};
    spi_transaction_t t = {
        .cmd = 0,
        .addr = 0,
        .length = 8 * 4,
        .rxlength = 8 * 4,
        .flags = 0,
    };
    uint8_t tx[4] = {0x9Fu, 0, 0, 0}; /* Read JEDEC ID */
    uint8_t buf[4] = {0};
    t.tx_buffer = tx;
    t.rx_buffer = buf;

    if (spi_device_polling_transmit(ctx->flash, &t) != ESP_OK) {
        snprintf(detail, FT_DETAIL_LEN, "spi transfer failed");
        return -1;
    }
    rx[0] = buf[1];
    rx[1] = buf[2];
    rx[2] = buf[3];

    int32_t id = ((int32_t)rx[0] << 16) | ((int32_t)rx[1] << 8) | rx[2];
    *measured = id;

    if (id == 0 || id == 0xFFFFFF) {
        snprintf(detail, FT_DETAIL_LEN, "no flash responding");
        return -1;
    }
    if (id != EXPECT_JEDEC_ID) {
        snprintf(detail, FT_DETAIL_LEN, "wrong part, expected 0x%X",
                 EXPECT_JEDEC_ID);
        return -1;
    }
    return 0;
}

static int probe_flash_rw(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 0;
    /* Exercises the spare region only — never a slot, so running the
     * factory test can never eat a recording. Needs the SPI flash HAL. */
    snprintf(detail, FT_DETAIL_LEN, "TODO M4: needs flash HAL");
    return 1;
}

/* --- microphone ------------------------------------------------------ */

static int probe_mic(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 0;
    /* Capture ~100 ms, compute RMS, and require it to be neither stuck at
     * zero (dead mic or dead clock) nor pinned at the rail (bad bias).
     * The pass window has to be set against real boards in a quiet room. */
    snprintf(detail, FT_DETAIL_LEN, "TODO M4: needs I2S capture");
    return 1;
}

/* --- write-protect tab ----------------------------------------------- */

static int probe_tab(void *user, int32_t *measured, char *detail)
{
    (void)user;
    int level = gpio_get_level(PIN_TAB_SENSE);
    *measured = level;

    /* A board fresh off the line has its tab intact, so this reads high.
     * Reading low means the trace is broken, the pull-down is wrong, or
     * someone has already snapped the tab. */
    if (level != 1) {
        snprintf(detail, FT_DETAIL_LEN, "tab reads snapped");
        return -1;
    }
    return 0;
}

/* --- USB-C CC sense --------------------------------------------------- */

static int probe_cc(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 0;
    /* Read CC1 and CC2 on ADC1 and take the higher: one line is the live
     * one, the other sits near ground. A plausible reading proves the CC
     * pull-downs and the divider are fitted. */
    snprintf(detail, FT_DETAIL_LEN, "TODO M4: needs ADC oneshot");
    return 1;
}

/* --- radio ------------------------------------------------------------ */

static int probe_bt(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 0;
    /* Bring up the controller and Bluedroid, run a short inquiry, and
     * report how many devices answered. Any non-zero count proves the
     * radio transmits and receives and the antenna keepout is sane —
     * which is the single most important thing to know about a board. */
    snprintf(detail, FT_DETAIL_LEN, "TODO M4: needs BT init");
    return 1;
}

/* --- LEDs (operator) --------------------------------------------------- */

static int probe_leds(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 0;
    /* Walk the chain one group at a time — left reel, right reel, track,
     * REC, BT — and have the operator confirm. A single dead WS2812 kills
     * everything downstream of it, so the walk has to be visual. */
    snprintf(detail, FT_DETAIL_LEN, "TODO M4: needs RMT driver");
    return 1;
}

/* --- buttons (operator) ------------------------------------------------ */

static int probe_buttons(void *user, int32_t *measured, char *detail)
{
    (void)user;
    const int pins[4] = {PIN_BTN_REC, PIN_BTN_PLAY, PIN_BTN_TRACK,
                         PIN_BTN_MODE};
    const char *names[4] = {"REC", "PLAY", "TRACK", "MODE"};
    uint32_t seen = 0;
    const uint32_t deadline_ms = 20000u;
    uint32_t start = now_ms();

    printf("  press each button: REC, PLAY, TRACK, MODE\n");

    while (seen != 0x0Fu && (now_ms() - start) < deadline_ms) {
        for (int i = 0; i < 4; i++) {
            if (gpio_get_level(pins[i]) == 0) { /* active low */
                if (!(seen & (1u << i))) {
                    seen |= (1u << i);
                    printf("    %s ok\n", names[i]);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    *measured = (int32_t)seen;

    if (seen != 0x0Fu) {
        char missing[FT_DETAIL_LEN];
        missing[0] = '\0';
        for (int i = 0; i < 4; i++) {
            if (!(seen & (1u << i))) {
                strncat(missing, names[i],
                        sizeof(missing) - strlen(missing) - 2u);
                strncat(missing, " ", sizeof(missing) - strlen(missing) - 1u);
            }
        }
        snprintf(detail, FT_DETAIL_LEN, "no press: %s", missing);
        return -1;
    }
    return 0;
}

/* --- suite ------------------------------------------------------------ */

static const ft_step_def_t k_steps[] = {
    {"flash_id", false, true,  probe_flash_id},
    {"flash_rw", false, false, probe_flash_rw},
    {"mic",      false, false, probe_mic},
    {"tab",      false, false, probe_tab},
    {"cc",       false, false, probe_cc},
    {"bt",       false, false, probe_bt},
    {"leds",     true,  false, probe_leds},
    {"buttons",  true,  false, probe_buttons},
};

#define STEP_COUNT (sizeof(k_steps) / sizeof(k_steps[0]))

static void configure_gpio(void)
{
    uint64_t buttons = (1ULL << PIN_BTN_REC) | (1ULL << PIN_BTN_PLAY) |
                       (1ULL << PIN_BTN_TRACK) | (1ULL << PIN_BTN_MODE);
    gpio_config_t bcfg = {
        .pin_bit_mask = buttons,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bcfg));

    gpio_config_t tab = {
        .pin_bit_mask = (1ULL << PIN_TAB_SENSE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, /* the tab has an external pull-down */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&tab));
}

static bool configure_flash_spi(probe_ctx_t *ctx)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    if (spi_bus_initialize(VSPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 10 * 1000 * 1000, /* gentle for bring-up */
        .mode = 0,
        .spics_io_num = PIN_FLASH_CS,
        .queue_size = 1,
    };
    if (spi_bus_add_device(VSPI_HOST, &dev, &ctx->flash) != ESP_OK) {
        return false;
    }
    ctx->flash_ready = true;
    return true;
}

void factory_test_main(void)
{
    printf("\n=== mixxtape factory test ===\n");

    configure_gpio();

    probe_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    if (!configure_flash_spi(&ctx)) {
        ESP_LOGE(TAG, "SPI bus init failed; flash probes will fail");
    }

    ft_suite_t suite;
    if (ft_init(&suite, k_steps, (uint32_t)STEP_COUNT, true) != 0) {
        ESP_LOGE(TAG, "suite init failed");
        return;
    }

    ft_run(&suite, &ctx, now_ms);

    char report[FT_REPORT_LEN];
    ft_format_human(&suite, report, sizeof(report));
    printf("\n%s", report);

    ft_format_machine(&suite, report, sizeof(report));
    printf("\n%s\n", report);

    /* Any skip here means a probe is still a stub, so the board has not
     * really been tested. Refuse to show a clean banner until M4 has
     * filled them in — a green light nobody earned is worse than none. */
    if (suite.skipped > 0u) {
        printf("\n*** SUITE INCOMPLETE: %u probe(s) unimplemented ***\n",
               (unsigned)suite.skipped);
    } else if (ft_verdict(&suite)) {
        printf("\n*** BOARD PASSED ***\n");
    } else {
        printf("\n*** BOARD FAILED ***\n");
    }
}

#endif /* MIXXTAPE_FACTORY_TEST */
