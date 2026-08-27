/* A2DP source plumbing over Bluedroid.
 *
 * NOT YET RUN ON HARDWARE.
 *
 * The policy this file drives is deliberately elsewhere and already
 * tested: pairing.c decides which device to connect to, bonds.c remembers
 * them. What is left here is the part that can only be checked against
 * real earbuds.
 */

#include "bt_audio.h"

#include <string.h>

#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bt_audio";

#define MAX_SCAN_RESULTS 24
#define INQUIRY_SECONDS 8   /* x1.28 s units below */

static bt_audio_state_t s_state;
static bonds_t *s_bonds;
static bt_audio_pull_fn s_pull;
static void *s_pull_user;

static bt_device_t s_found[MAX_SCAN_RESULTS];
static uint32_t s_found_count;
static uint32_t s_last_scan_count;

static uint8_t s_peer[6];
static char s_peer_name[PAIRING_NAME_LEN];
static int8_t s_rssi_threshold = PAIRING_RSSI_DEFAULT;

static reconnect_t s_reconnect;
static bool s_reconnecting;
static uint32_t s_bond_try;

/* --- discovery --------------------------------------------------------- */

static void remember_result(esp_bt_gap_cb_param_t *param)
{
    bt_device_t dev;
    memset(&dev, 0, sizeof(dev));
    memcpy(dev.bda, param->disc_res.bda, 6);
    dev.rssi = -127;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            dev.cod = *(uint32_t *)p->val;
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            dev.rssi = *(int8_t *)p->val;
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME: {
            int n = p->len < (int)sizeof(dev.name) - 1
                        ? p->len : (int)sizeof(dev.name) - 1;
            memcpy(dev.name, p->val, n);
            dev.name[n] = '\0';
            break;
        }
        default:
            break;
        }
    }

    /* Inquiry reports the same device repeatedly, often with a better RSSI
     * as it settles. Keep the strongest reading rather than the first. */
    for (uint32_t i = 0; i < s_found_count; i++) {
        if (memcmp(s_found[i].bda, dev.bda, 6) == 0) {
            if (dev.rssi > s_found[i].rssi) {
                s_found[i].rssi = dev.rssi;
            }
            if (dev.cod) {
                s_found[i].cod = dev.cod;
            }
            if (dev.name[0]) {
                strncpy(s_found[i].name, dev.name, sizeof(dev.name) - 1);
            }
            return;
        }
    }
    if (s_found_count < MAX_SCAN_RESULTS) {
        s_found[s_found_count++] = dev;
    }
}

static void connect_to(const uint8_t bda[6], const char *name)
{
    memcpy(s_peer, bda, 6);
    strncpy(s_peer_name, name ? name : "", sizeof(s_peer_name) - 1);
    s_peer_name[sizeof(s_peer_name) - 1] = '\0';

    ESP_LOGI(TAG, "connecting to '%s' %02x:%02x:%02x:%02x:%02x:%02x",
             s_peer_name, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    s_state = BT_AUDIO_CONNECTING;
    esp_a2d_source_connect((uint8_t *)bda);
}

static void discovery_finished(void)
{
    uint32_t sinks = 0;
    for (uint32_t i = 0; i < s_found_count; i++) {
        if (pairing_is_audio_sink(s_found[i].cod)) {
            sinks++;
        }
    }
    s_last_scan_count = sinks;
    ESP_LOGI(TAG, "inquiry done: %u devices, %u audio sinks",
             (unsigned)s_found_count, (unsigned)sinks);

    int pick = pairing_select(s_found, s_found_count, s_rssi_threshold);
    if (pick < 0) {
        /* Better to find nothing than to pair with the wrong thing: the
         * user just moves the tape closer and holds MODE again. */
        ESP_LOGW(TAG, "no audio sink within %d dBm - hold it closer",
                 s_rssi_threshold);
        s_state = BT_AUDIO_IDLE;
        return;
    }
    ESP_LOGI(TAG, "picked '%s' at %d dBm", s_found[pick].name,
             s_found[pick].rssi);
    connect_to(s_found[pick].bda, s_found[pick].name);
}

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        remember_result(param);
        break;

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            discovery_finished();
        }
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "bonded with '%s'", param->auth_cmpl.device_name);
            if (s_bonds) {
                bonds_add(s_bonds, param->auth_cmpl.bda,
                          (const char *)param->auth_cmpl.device_name);
            }
        } else {
            ESP_LOGE(TAG, "pairing failed, status %d", param->auth_cmpl.stat);
        }
        break;

    default:
        break;
    }
}

/* --- A2DP -------------------------------------------------------------- */

static void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        switch (param->conn_stat.state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            ESP_LOGI(TAG, "connected");
            s_state = BT_AUDIO_CONNECTED;
            s_reconnecting = false;
            reconnect_succeeded(&s_reconnect);
            if (s_bonds) {
                bonds_touch(s_bonds, s_peer);
            }
            break;

        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            /* Expected on power-up: the sink is usually still talking to
             * its owner's phone. reconnect_* backs off and, after about
             * fifteen seconds, bt_audio_sink_busy() goes true so the LEDs
             * can say so honestly. */
            ESP_LOGW(TAG, "disconnected (reason %d)",
                     param->conn_stat.disc_rsn);
            s_state = BT_AUDIO_IDLE;
            break;

        default:
            break;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
            s_state = BT_AUDIO_STREAMING;
        } else if (s_state == BT_AUDIO_STREAMING) {
            s_state = BT_AUDIO_CONNECTED;
        }
        break;

    default:
        break;
    }
}

/* Bluedroid asks for PCM here, and it wants interleaved STEREO even
 * though the tape is mono. Each mono sample is written to both channels.
 * Getting this wrong plays everything at half speed an octave down. */
static int32_t data_cb(uint8_t *buf, int32_t len)
{
    if (!buf || len <= 0) {
        return 0;
    }
    int16_t *out = (int16_t *)buf;
    uint32_t frames = (uint32_t)len / 4u;   /* 2 channels x 16 bits */
    if (frames == 0) {
        return 0;
    }

    static int16_t mono[256];
    uint32_t written = 0;
    while (written < frames) {
        uint32_t want = frames - written;
        if (want > (uint32_t)(sizeof(mono) / sizeof(mono[0]))) {
            want = sizeof(mono) / sizeof(mono[0]);
        }
        uint32_t got = s_pull ? s_pull(mono, want, s_pull_user) : 0;
        if (got == 0) {
            break;
        }
        for (uint32_t i = 0; i < got; i++) {
            out[(written + i) * 2 + 0] = mono[i];
            out[(written + i) * 2 + 1] = mono[i];
        }
        written += got;
        if (got < want) {
            break;
        }
    }

    /* Pad the rest with silence rather than returning short: a starved
     * A2DP stream stutters audibly, and the sink may drop the link. */
    if (written < frames) {
        memset(out + written * 2, 0, (frames - written) * 4u);
    }
    return (int32_t)(frames * 4u);
}

/* --- public ------------------------------------------------------------ */

int bt_audio_init(bonds_t *bonds)
{
    s_bonds = bonds;

    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "BLE mem release: %s", esp_err_to_name(err));
    }

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "controller init: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "controller enable: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = esp_bluedroid_init()) != ESP_OK) {
        return err;
    }
    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        return err;
    }

    esp_bt_gap_register_callback(gap_cb);
    esp_a2d_register_callback(a2d_cb);
    esp_a2d_source_register_data_callback(data_cb);
    esp_a2d_source_init();

    /* The tape connects outwards and never accepts connections, so it
     * stays invisible: nobody should be able to pair *with* a mixtape. */
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,
                             ESP_BT_NON_DISCOVERABLE);

    /* Just Works: no keyboard, no display, nothing to confirm on. */
    esp_bt_sp_param_t iocap = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t cap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(iocap, &cap, sizeof(cap));

    reconnect_init(&s_reconnect, 0);
    s_state = BT_AUDIO_IDLE;
    ESP_LOGI(TAG, "A2DP source ready");
    return ESP_OK;
}

void bt_audio_set_source(bt_audio_pull_fn fn, void *user)
{
    s_pull = fn;
    s_pull_user = user;
}

int bt_audio_start_pairing(int8_t rssi_threshold)
{
    if (s_state == BT_AUDIO_DISCOVERING) {
        return ESP_OK;
    }
    s_rssi_threshold = rssi_threshold;
    s_found_count = 0;
    s_state = BT_AUDIO_DISCOVERING;
    ESP_LOGI(TAG, "scanning for a sink within %d dBm", rssi_threshold);
    return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                      INQUIRY_SECONDS, 0);
}

int bt_audio_reconnect_last(void)
{
    if (!s_bonds || bonds_count(s_bonds) == 0) {
        ESP_LOGI(TAG, "no bonds stored; nothing to reconnect to");
        return ESP_ERR_NOT_FOUND;
    }
    /* Most recently used first, then the rest — bonds keeps that order. */
    const bond_t *b = bonds_get(s_bonds, s_bond_try % bonds_count(s_bonds));
    s_bond_try++;
    if (!b) {
        return ESP_ERR_NOT_FOUND;
    }
    s_reconnecting = true;
    connect_to(b->bda, b->name);
    return ESP_OK;
}

int bt_audio_disconnect(void)
{
    if (s_state == BT_AUDIO_IDLE) {
        return ESP_OK;
    }
    return esp_a2d_source_disconnect(s_peer);
}

int bt_audio_start_stream(void)
{
    if (s_state != BT_AUDIO_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
}

int bt_audio_stop_stream(void)
{
    if (s_state != BT_AUDIO_STREAMING) {
        return ESP_OK;
    }
    return esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
}

bt_audio_state_t bt_audio_state(void)
{
    return s_state;
}

bool bt_audio_is_connected(void)
{
    return s_state == BT_AUDIO_CONNECTED || s_state == BT_AUDIO_STREAMING;
}

bool bt_audio_sink_busy(uint32_t now_ms)
{
    return s_reconnecting && reconnect_sink_busy(&s_reconnect, now_ms);
}

const char *bt_audio_peer_name(void)
{
    return s_peer_name;
}

uint32_t bt_audio_last_scan_count(void)
{
    return s_last_scan_count;
}

void bt_audio_deinit(void)
{
    esp_a2d_source_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    s_state = BT_AUDIO_IDLE;
}
