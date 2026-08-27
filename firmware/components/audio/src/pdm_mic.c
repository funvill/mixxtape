/* PDM capture through the ESP32's I2S peripheral.
 *
 * NOT YET RUN ON HARDWARE.
 *
 * Two things here are easy to get wrong and expensive to debug on a board
 * you cannot probe, so they are spelled out rather than left to a constant:
 *
 * 1. The downsample ratio must be 8, not 16. The PDM clock the peripheral
 *    generates is sample_rate x 64 at DSR_8S and x 128 at DSR_16S. At
 *    44.1 kHz that is 2.8224 MHz or 5.6448 MHz, and the MSM261DHT006's
 *    standard-performance window is 1.1-4.8 MHz. DSR_16S runs the part
 *    out of spec: it may work on the bench and fail warm.
 *
 * 2. The microphone needs time after its clock starts. The datasheet gives
 *    6 ms typical and 20 ms maximum power-up, and the sigma-delta
 *    modulator settles after that. Recording straight away captures a DC
 *    thump, which is exactly the sort of thing that gets blamed on
 *    handling noise later.
 */

#include "pdm_mic.h"

#include <string.h>

#include "board.h"
#include "driver/i2s_pdm.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tape_layout.h"

static const char *TAG = "pdm_mic";

/* 6 ms typical / 20 ms max power-up, plus modulator settling. */
#define MIC_SETTLE_MS 40

#define DMA_FRAME_SAMPLES 512
#define DMA_FRAME_COUNT   4

static i2s_chan_handle_t s_rx;
static bool s_running;

int pdm_mic_init(void)
{
    if (s_rx) {
        return ESP_OK;
    }

    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                        I2S_ROLE_MASTER);
    chan.dma_desc_num = DMA_FRAME_COUNT;
    chan.dma_frame_num = DMA_FRAME_SAMPLES;
    /* Do not auto-start on enable: the settle delay below has to happen
     * between the clock starting and the first sample being kept. */
    chan.auto_clear = true;

    esp_err_t err = i2s_new_channel(&chan, NULL, &s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err));
        return err;
    }

    i2s_pdm_rx_config_t cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(TAPE_SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = PIN_PDM_CLK,
            .din = PIN_PDM_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    /* See the header comment: 8, never 16. */
    cfg.clk_cfg.dn_sample_mode = I2S_PDM_DSR_8S;

    /* L/R is strapped low on the board, so the mic drives the left slot. */
    cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;

    err = i2s_channel_init_pdm_rx_mode(s_rx, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_pdm_rx_mode: %s",
                 esp_err_to_name(err));
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return err;
    }

    ESP_LOGI(TAG, "PDM RX ready: %u Hz mono, clock %u Hz",
             (unsigned)TAPE_SAMPLE_RATE, (unsigned)PDM_CLOCK_HZ);
    return ESP_OK;
}

int pdm_mic_start(void)
{
    if (!s_rx) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_OK;
    }
    esp_err_t err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        return err;
    }
    s_running = true;

    vTaskDelay(pdMS_TO_TICKS(MIC_SETTLE_MS));
    pdm_mic_discard(10);
    return ESP_OK;
}

int pdm_mic_stop(void)
{
    if (!s_rx || !s_running) {
        return ESP_OK;
    }
    s_running = false;
    return i2s_channel_disable(s_rx);
}

int pdm_mic_read(int16_t *dst, size_t samples, uint32_t timeout_ms)
{
    if (!s_rx || !s_running || !dst) {
        return -1;
    }
    size_t got = 0;
    esp_err_t err = i2s_channel_read(s_rx, dst, samples * sizeof(int16_t),
                                     &got, pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return -1;
    }
    return (int)(got / sizeof(int16_t));
}

int pdm_mic_discard(uint32_t ms)
{
    if (!s_rx || !s_running) {
        return -1;
    }
    static int16_t scratch[256];
    uint32_t want = (TAPE_SAMPLE_RATE * ms) / 1000u;
    uint32_t done = 0;
    while (done < want) {
        uint32_t chunk = want - done;
        if (chunk > 256u) {
            chunk = 256u;
        }
        int n = pdm_mic_read(scratch, chunk, 100);
        if (n <= 0) {
            break;
        }
        done += (uint32_t)n;
    }
    return (int)done;
}

uint32_t pdm_mic_rms(const int16_t *pcm, size_t samples)
{
    if (!pcm || samples == 0) {
        return 0;
    }
    uint64_t acc = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t v = pcm[i];
        acc += (uint64_t)(v * v);
    }
    uint64_t mean = acc / samples;

    /* Integer square root: no libm, and deterministic. */
    uint32_t root = 0;
    uint32_t bit = 1u << 30;
    while (bit > mean) {
        bit >>= 2;
    }
    uint64_t rem = mean;
    while (bit) {
        if (rem >= (uint64_t)root + bit) {
            rem -= (uint64_t)root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

void pdm_mic_deinit(void)
{
    if (s_rx) {
        pdm_mic_stop();
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
}
