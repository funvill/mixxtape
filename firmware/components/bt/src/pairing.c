#include "pairing.h"

bool pairing_is_audio_sink(uint32_t cod)
{
    uint32_t major = (cod & COD_MAJOR_MASK) >> COD_MAJOR_SHIFT;
    uint32_t minor = (cod & COD_MINOR_MASK) >> COD_MINOR_SHIFT;

    /* Some speakers report an odd major class but still set the Audio
     * service bit; some phones set the Audio bit too, which is why the
     * major class is checked first and phones are rejected outright. */
    if (major == COD_MAJOR_PHONE || major == COD_MAJOR_COMPUTER ||
        major == COD_MAJOR_PERIPH) {
        return false;
    }
    if (major != COD_MAJOR_AUDIO) {
        /* Wearables occasionally carry audio (some watches), but they are
         * not what anyone is holding this against. */
        return false;
    }

    switch (minor) {
    case COD_AV_HEADSET:
    case COD_AV_HANDSFREE:
    case COD_AV_LOUDSPEAKER:
    case COD_AV_HEADPHONES:
    case COD_AV_PORTABLE:
    case COD_AV_HIFI:
        return true;
    default:
        /* Microphones, set-top boxes, VCRs, cameras, monitors and video
         * conferencing kit all live in this major class and are not
         * things to stream a mixtape into. */
        return false;
    }
}

bool pairing_in_range(int8_t rssi, int8_t threshold)
{
    return rssi >= threshold;
}

int pairing_select(const bt_device_t *devices, uint32_t count,
                   int8_t rssi_threshold)
{
    if (!devices) {
        return -1;
    }
    int best = -1;
    int8_t best_rssi = 0;

    for (uint32_t i = 0; i < count; i++) {
        const bt_device_t *d = &devices[i];
        if (!pairing_is_audio_sink(d->cod)) {
            continue;
        }
        if (!pairing_in_range(d->rssi, rssi_threshold)) {
            continue;
        }
        /* Strictly greater, so an equal signal keeps the earlier entry and
         * repeated inquiries do not flip between two devices. */
        if (best < 0 || d->rssi > best_rssi) {
            best = (int)i;
            best_rssi = d->rssi;
        }
    }
    return best;
}

/* ------------------------------------------------------------------ */

void reconnect_init(reconnect_t *r, uint32_t now_ms)
{
    if (!r) {
        return;
    }
    r->attempts = 0;
    r->started_ms = now_ms;
    r->next_try_ms = now_ms;
    r->connected = false;
}

uint32_t reconnect_delay_ms(const reconnect_t *r)
{
    if (!r || r->attempts == 0) {
        return RECONNECT_FIRST_DELAY_MS;
    }
    uint32_t delay = RECONNECT_FIRST_DELAY_MS;
    for (uint32_t i = 1; i < r->attempts && delay < RECONNECT_MAX_DELAY_MS;
         i++) {
        delay *= 2u;
    }
    return (delay > RECONNECT_MAX_DELAY_MS) ? RECONNECT_MAX_DELAY_MS : delay;
}

bool reconnect_should_try(const reconnect_t *r, uint32_t now_ms)
{
    if (!r || r->connected) {
        return false;
    }
    /* Unsigned subtraction, so this stays correct across the millisecond
     * counter wrapping. */
    return (int32_t)(now_ms - r->next_try_ms) >= 0;
}

void reconnect_attempted(reconnect_t *r, uint32_t now_ms)
{
    if (!r) {
        return;
    }
    r->attempts++;
    r->next_try_ms = now_ms + reconnect_delay_ms(r);
}

void reconnect_failed(reconnect_t *r, uint32_t now_ms)
{
    reconnect_attempted(r, now_ms);
}

void reconnect_succeeded(reconnect_t *r)
{
    if (r) {
        r->connected = true;
        r->attempts = 0;
    }
}

bool reconnect_sink_busy(const reconnect_t *r, uint32_t now_ms)
{
    if (!r || r->connected) {
        return false;
    }
    return (now_ms - r->started_ms) >= RECONNECT_BUSY_AFTER_MS;
}
