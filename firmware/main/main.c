/* M0 skeleton: brings up the buttons and the tab sense and drives the
 * interaction state machine, logging every event over the jig UART.
 *
 * This is deliberately the *only* target-specific code so far. Everything
 * with real logic in it — the slot manager, the SBC arithmetic, the button
 * grammar — lives in components/ and is covered by host tests, because
 * there is no devkit to try things on (docs/firmware-plan.md sec.7).
 *
 * Still to come, and all gated on the first prototype boards (M4):
 *   - SPI flash HAL implementing flash_hal.h against the W25Q128
 *   - I2S capture and the SBC encoder feeding tape_store
 *   - A2DP source, pairing by RSSI, bond storage
 *   - WS2812 output via the RMT peripheral
 *   - CC sensing to raise the LED brightness cap
 */

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "ui_state.h"

static const char *TAG = "mixxtape";

static const int k_button_pins[4] = {PIN_BTN_REC, PIN_BTN_PLAY, PIN_BTN_TRACK,
                                     PIN_BTN_MODE};
static const uint32_t k_button_bits[4] = {UI_BTN_REC, UI_BTN_PLAY,
                                          UI_BTN_TRACK, UI_BTN_MODE};

static void configure_inputs(void)
{
    uint64_t button_mask = 0;
    for (int i = 0; i < 4; i++) {
        button_mask |= (1ULL << k_button_pins[i]);
    }
    gpio_config_t buttons = {
        .pin_bit_mask = button_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, /* buttons short to GND */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));

    /* The tab has an external 10k pull-down; intact reads high. No internal
     * pull, so a snapped tab cannot be masked by one. */
    gpio_config_t tab = {
        .pin_bit_mask = (1ULL << PIN_TAB_SENSE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&tab));
}

static uint32_t sample_buttons(void)
{
    uint32_t mask = 0;
    for (int i = 0; i < 4; i++) {
        if (gpio_get_level(k_button_pins[i]) == 0) { /* active low */
            mask |= k_button_bits[i];
        }
    }
    return mask;
}

static bool tab_intact(void)
{
    return gpio_get_level(PIN_TAB_SENSE) != 0;
}

static void ui_task(void *arg)
{
    (void)arg;
    ui_ctx_t ui;
    ui_init(&ui);

    const TickType_t period = pdMS_TO_TICKS(5);
    TickType_t last_wake = xTaskGetTickCount();
    bool warned_tab = false;

    for (;;) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        bool tab_ok = tab_intact();

        if (!tab_ok && !warned_tab) {
            ESP_LOGW(TAG, "write-protect tab is snapped: recording disabled");
            warned_tab = true;
        }

        ui_event_t events[8];
        uint32_t n = ui_tick(&ui, now_ms, sample_buttons(), tab_ok, events,
                             (uint32_t)(sizeof(events) / sizeof(events[0])));
        for (uint32_t i = 0; i < n; i++) {
            ESP_LOGI(TAG, "%s -> %s (track %d)", ui_event_name(events[i]),
                     ui_state_name(ui.state), ui.track + 1);
        }
        vTaskDelayUntil(&last_wake, period);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "mixxtape firmware, M0 skeleton");
    ESP_LOGI(TAG, "tape: %d slots x %u MiB, SBC mono %u Hz bitpool %u",
             TAPE_SLOT_COUNT, (unsigned)(TAPE_SLOT_SIZE / (1024u * 1024u)),
             (unsigned)TAPE_SBC_SAMPLE_RATE, (unsigned)TAPE_SBC_BITPOOL);

    configure_inputs();
    xTaskCreate(ui_task, "ui", 4096, NULL, 5, NULL);
}
