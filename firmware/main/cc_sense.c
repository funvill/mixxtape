/* USB-C CC sensing on ADC1.
 *
 * NOT YET RUN ON HARDWARE.
 *
 * The 10 k resistors between CC and the ADC pins are isolation, not a
 * divider: the ADC input is high impedance, so what is measured is the CC
 * voltage itself. That is why the thresholds below are the raw USB-C
 * figures rather than something scaled.
 *
 * The ESP32's ADC is noisy and not especially linear, which is fine here
 * because the three levels being told apart are hundreds of millivolts
 * apart. Averaging is still worth it: a single reading can land 50 mV off.
 */

#include "cc_sense.h"

#include "board.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "cc";

/* GPIO36 = ADC1_CH0 (SENSOR_VP), GPIO39 = ADC1_CH3 (SENSOR_VN). */
#define CC1_ADC_CHANNEL ADC_CHANNEL_0
#define CC2_ADC_CHANNEL ADC_CHANNEL_3

#define SAMPLES 16

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_ready;
static bool s_calibrated;

int cc_sense_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit: %s", esp_err_to_name(err));
        return err;
    }

    /* 12 dB attenuation gives roughly 0-3.1 V, which covers the 1.68 V a
     * 3 A source advertises with room to spare. */
    adc_oneshot_chan_cfg_t chan = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc, CC1_ADC_CHANNEL, &chan);
    if (err == ESP_OK) {
        err = adc_oneshot_config_channel(s_adc, CC2_ADC_CHANNEL, &chan);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel: %s", esp_err_to_name(err));
        return err;
    }

    /* Line fitting is the only scheme the original ESP32 supports. If the
     * chip has no factory calibration burned in this fails, and the raw
     * conversion below is used instead — good enough to tell 0.4 V from
     * 1.7 V, which is all this needs to do. */
    adc_cali_line_fitting_config_t cali = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_calibrated = (adc_cali_create_scheme_line_fitting(&cali, &s_cali)
                    == ESP_OK);
    if (!s_calibrated) {
        ESP_LOGW(TAG, "no ADC calibration; falling back to raw scaling");
    }

    s_ready = true;
    return ESP_OK;
}

static int read_channel_mv(adc_channel_t ch)
{
    int acc = 0, taken = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, ch, &raw) != ESP_OK) {
            continue;
        }
        int mv = 0;
        if (s_calibrated) {
            if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) {
                continue;
            }
        } else {
            /* 12-bit over ~3.1 V full scale. */
            mv = (raw * 3100) / 4095;
        }
        acc += mv;
        taken++;
    }
    return taken ? (acc / taken) : -1;
}

int cc_sense_read_mv(void)
{
    if (!s_ready) {
        return -1;
    }
    int a = read_channel_mv(CC1_ADC_CHANNEL);
    int b = read_channel_mv(CC2_ADC_CHANNEL);
    /* Whichever line the cable made live. */
    return a > b ? a : b;
}

usb_budget_t cc_sense_budget(void)
{
    int mv = cc_sense_read_mv();
    if (mv < 0) {
        return USB_BUDGET_DEFAULT;
    }
    if (mv >= CC_MV_THRESHOLD_3A0) {
        return USB_BUDGET_3A0;
    }
    if (mv >= CC_MV_THRESHOLD_1A5) {
        return USB_BUDGET_1A5;
    }
    /* Below the 1.5 A band, including "no source detected at all": assume
     * the least the port could be offering. Guessing high here browns out
     * the board mid-recording, which costs a take. */
    return USB_BUDGET_DEFAULT;
}

uint8_t cc_sense_led_brightness(usb_budget_t budget)
{
    switch (budget) {
    case USB_BUDGET_3A0:
    case USB_BUDGET_1A5:
        return LED_BRIGHTNESS_USB_HIGH;
    case USB_BUDGET_DEFAULT:
    default:
        return LED_BRIGHTNESS_USB_DEFAULT;
    }
}

const char *cc_sense_budget_name(usb_budget_t budget)
{
    switch (budget) {
    case USB_BUDGET_3A0: return "3.0 A";
    case USB_BUDGET_1A5: return "1.5 A";
    default:             return "500 mA (default)";
    }
}

void cc_sense_deinit(void)
{
    if (s_ready) {
        if (s_calibrated) {
            adc_cali_delete_scheme_line_fitting(s_cali);
            s_calibrated = false;
        }
        adc_oneshot_del_unit(s_adc);
        s_ready = false;
    }
}
