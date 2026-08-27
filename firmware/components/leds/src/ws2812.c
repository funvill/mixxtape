/* WS2812 chain on RMT.
 *
 * NOT YET RUN ON HARDWARE.
 *
 * Timing is the whole driver. WS2812B wants, per bit:
 *
 *     0 bit:  high 0.4 us, low 0.85 us
 *     1 bit:  high 0.8 us, low 0.45 us
 *     reset:  low for >= 50 us
 *
 * At a 10 MHz RMT resolution one tick is 0.1 us, so those become 4/9 and
 * 8/4 ticks. The parts actually fitted are XINGLIGHT XL-2020RGBC-2812B,
 * which follow the same protocol; their datasheet tolerances are wider
 * than Worldsemi's, so these nominal figures suit both.
 *
 * Colour order is GRB, not RGB. Getting this wrong is the classic first
 * bring-up symptom: the reels light, but green and red are swapped.
 */

#include "ws2812.h"

#include <string.h>

#include "board.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "ws2812";

#define RMT_RESOLUTION_HZ 10000000  /* 0.1 us per tick */
#define RESET_US 60                 /* datasheet says >= 50 */

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_bytes_enc;
static rmt_encoder_handle_t s_copy_enc;
static rmt_symbol_word_t s_reset;
static uint8_t s_frame[LED_COUNT * 3];

int ws2812_init(void)
{
    if (s_chan) {
        return ESP_OK;
    }

    rmt_tx_channel_config_t tx = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = PIN_LED_DATA,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&tx, &s_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel: %s", esp_err_to_name(err));
        return err;
    }

    rmt_bytes_encoder_config_t bytes = {
        .bit0 = {.level0 = 1, .duration0 = 4,   /* 0.4 us high */
                 .level1 = 0, .duration1 = 9},  /* 0.85 us low */
        .bit1 = {.level0 = 1, .duration0 = 8,   /* 0.8 us high */
                 .level1 = 0, .duration1 = 4},  /* 0.45 us low */
        .flags.msb_first = 1,                   /* WS2812 is MSB first */
    };
    err = rmt_new_bytes_encoder(&bytes, &s_bytes_enc);
    if (err != ESP_OK) {
        return err;
    }

    rmt_copy_encoder_config_t copy = {0};
    err = rmt_new_copy_encoder(&copy, &s_copy_enc);
    if (err != ESP_OK) {
        return err;
    }

    /* The reset gap is a single long low symbol appended after the data. */
    s_reset.level0 = 0;
    s_reset.duration0 = RESET_US * RMT_RESOLUTION_HZ / 1000000 / 2;
    s_reset.level1 = 0;
    s_reset.duration1 = RESET_US * RMT_RESOLUTION_HZ / 1000000 / 2;

    err = rmt_enable(s_chan);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "RMT ready on GPIO%d for %d LEDs", PIN_LED_DATA, LED_COUNT);
    return ws2812_clear();
}

int ws2812_show(const rgb_t *pixels, uint32_t count)
{
    if (!s_chan || !pixels) {
        return ESP_ERR_INVALID_STATE;
    }
    if (count > LED_COUNT) {
        count = LED_COUNT;
    }

    for (uint32_t i = 0; i < count; i++) {
        /* GRB on the wire. */
        s_frame[i * 3 + 0] = pixels[i].g;
        s_frame[i * 3 + 1] = pixels[i].r;
        s_frame[i * 3 + 2] = pixels[i].b;
    }
    /* Any LEDs past `count` go dark rather than holding a stale colour. */
    if (count < LED_COUNT) {
        memset(s_frame + count * 3, 0, (LED_COUNT - count) * 3);
    }

    rmt_transmit_config_t cfg = {.loop_count = 0};
    esp_err_t err = rmt_transmit(s_chan, s_bytes_enc, s_frame,
                                 sizeof(s_frame), &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = rmt_transmit(s_chan, s_copy_enc, &s_reset, sizeof(s_reset), &cfg);
    if (err != ESP_OK) {
        return err;
    }
    /* Wait, so the caller cannot hand us a buffer that changes underneath
     * an in-flight transfer. At 29 LEDs a frame is about 900 us. */
    return rmt_tx_wait_all_done(s_chan, 100);
}

int ws2812_clear(void)
{
    static rgb_t dark[LED_COUNT];
    memset(dark, 0, sizeof(dark));
    return ws2812_show(dark, LED_COUNT);
}

void ws2812_deinit(void)
{
    if (s_chan) {
        ws2812_clear();
        rmt_disable(s_chan);
        rmt_del_encoder(s_bytes_enc);
        rmt_del_encoder(s_copy_enc);
        rmt_del_channel(s_chan);
        s_chan = NULL;
    }
}
