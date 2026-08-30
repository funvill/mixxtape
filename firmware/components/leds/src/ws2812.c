/* Addressable LED chain on RMT.
 *
 * NOT YET RUN ON HARDWARE.
 *
 * Timing is the whole driver, and it is NOT the same for every part that
 * claims the WS2812 protocol. The fitted part is XINGLIGHT
 * XL-1615RGBC-2812B (LCSC C5349954), whose datasheet demands MORE than
 * Worldsemi's original:
 *
 *                     Worldsemi WS2812B     XL-1615RGBC-2812B
 *     0 code high     0.4 us                >= 0.3 us
 *     1 code high     0.8 us                >= 0.9 us      <-- more
 *     0 code low      0.85 us               >= 0.9 us      <-- more
 *     1 code low      0.45 us               >= 0.3 us
 *     code cycle      1.25 us               1.2 us
 *     reset low       >= 50 us              >= 200 us      <-- 4x more
 *
 * This driver previously emitted Worldsemi's 0.8 us for a 1 bit and a
 * 60 us reset, which are BOTH below the fitted part's minimums. Every 1
 * bit in every frame was under-spec, and back-to-back frames would not
 * latch - the second frame's bits append to the first and the whole chain
 * shifts. Clones are often tolerant, so this might have half-worked, and
 * "half-works, degrades down the chain" is the worst way for it to fail
 * on a run of 20 with no prototype.
 *
 * The values below clear BOTH parts' minimums with margin, so a board
 * built with either is correct:
 *
 *     0 bit:  high 0.4 us, low 1.0 us
 *     1 bit:  high 1.0 us, low 0.4 us
 *     reset:  low for 280 us
 *
 * At a 10 MHz RMT resolution one tick is 0.1 us. A 1.4 us cycle over
 * 29 LEDs x 24 bits is 975 us per frame, so the refresh ceiling is still
 * about 1 kHz - far more than the UI needs.
 *
 * Colour order is GRB, not RGB - confirmed against the XL-1615 datasheet,
 * which specifies "send data in the order of GRB", high order first.
 * Getting this wrong is the classic first bring-up symptom: the reels
 * light, but green and red are swapped.
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
#define RESET_US 280                /* XL-1615 needs >= 200; was 60 */

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
        .bit0 = {.level0 = 1, .duration0 = 4,    /* 0.4 us high, >= 0.3 */
                 .level1 = 0, .duration1 = 10},  /* 1.0 us low,  >= 0.9 */
        .bit1 = {.level0 = 1, .duration0 = 10,   /* 1.0 us high, >= 0.9 */
                 .level1 = 0, .duration1 = 4},   /* 0.4 us low,  >= 0.3 */
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
