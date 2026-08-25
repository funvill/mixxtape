#include "ui_state.h"

#include <string.h>

#define BTN_INDEX_REC   0
#define BTN_INDEX_PLAY  1
#define BTN_INDEX_TRACK 2
#define BTN_INDEX_MODE  3

typedef struct {
    ui_event_t *out;
    uint32_t max;
    uint32_t count;
} emitter_t;

static void emit(emitter_t *e, ui_event_t ev)
{
    if (e->count < e->max) {
        e->out[e->count++] = ev;
    }
}

void ui_init(ui_ctx_t *ui)
{
    if (ui) {
        memset(ui, 0, sizeof(*ui));
        ui->state = UI_STATE_IDLE;
        ui->track = 0;
    }
}

static bool held(const ui_ctx_t *ui, uint32_t bit)
{
    return (ui->stable_mask & bit) != 0u;
}

/* A transient mode (pairing/dub/dev-wifi) returns to idle on any press. */
static bool is_transient(ui_state_t s)
{
    return s == UI_STATE_PAIRING || s == UI_STATE_DUBBING ||
           s == UI_STATE_DEV_WIFI;
}

static void on_press(ui_ctx_t *ui, uint32_t bit, bool tab_ok, emitter_t *e)
{
    if (ui->state == UI_STATE_SLEEPING) {
        ui->state = UI_STATE_IDLE;
        emit(e, UI_EV_WAKE);
        ui->handled_mask |= bit; /* the waking press does nothing else */
        return;
    }

    /* Recording is exclusive: only releasing REC gets through. */
    if (ui->state == UI_STATE_RECORDING && bit != UI_BTN_REC) {
        ui->handled_mask |= bit;
        return;
    }

    if (is_transient(ui->state)) {
        ui->state = UI_STATE_IDLE;
    }

    switch (bit) {
    case UI_BTN_REC:
        if (!tab_ok) {
            emit(e, UI_EV_RECORD_REFUSED_TAB);
            ui->handled_mask |= bit;
            return;
        }
        if (ui->state == UI_STATE_PLAYING || ui->state == UI_STATE_PAUSED) {
            emit(e, UI_EV_PLAY_STOP);
        }
        ui->state = UI_STATE_RECORDING;
        emit(e, UI_EV_RECORD_START);
        ui->handled_mask |= bit; /* stop fires on release */
        break;

    case UI_BTN_TRACK:
        ui->track = (ui->track + 1) % TAPE_SLOT_COUNT;
        if (ui->state == UI_STATE_PLAYING || ui->state == UI_STATE_PAUSED) {
            emit(e, UI_EV_PLAY_STOP);
            ui->state = UI_STATE_IDLE;
        }
        emit(e, UI_EV_TRACK_CHANGED);
        ui->handled_mask |= bit;
        break;

    default:
        /* PLAY and MODE decide on release or at a hold threshold. */
        break;
    }
}

static void on_release(ui_ctx_t *ui, uint32_t bit, uint32_t now, emitter_t *e)
{
    uint32_t already = ui->handled_mask & bit;
    ui->handled_mask &= ~bit;

    if (bit == UI_BTN_REC) {
        if (ui->state == UI_STATE_RECORDING) {
            ui->state = UI_STATE_IDLE;
            emit(e, UI_EV_RECORD_STOP);
        }
        return;
    }

    if (already) {
        return; /* action already fired while held */
    }

    uint32_t idx = (bit == UI_BTN_PLAY) ? BTN_INDEX_PLAY : BTN_INDEX_MODE;
    uint32_t duration = now - ui->press_start[idx];

    if (bit == UI_BTN_PLAY) {
        switch (ui->state) {
        case UI_STATE_PLAYING:
            ui->state = UI_STATE_PAUSED;
            emit(e, UI_EV_PLAY_PAUSE);
            break;
        case UI_STATE_PAUSED:
            ui->state = UI_STATE_PLAYING;
            emit(e, UI_EV_PLAY_RESUME);
            break;
        default:
            ui->state = UI_STATE_PLAYING;
            emit(e, UI_EV_PLAY_START);
            break;
        }
    } else if (bit == UI_BTN_MODE) {
        if (duration >= UI_MODE_WIFI_MS) {
            ui->state = UI_STATE_DEV_WIFI;
            emit(e, UI_EV_ENTER_DEV_WIFI);
        } else if (duration >= UI_MODE_DUB_MS) {
            ui->state = UI_STATE_DUBBING;
            emit(e, UI_EV_ENTER_DUB);
        } else if (duration >= UI_MODE_PAIR_MS) {
            ui->state = UI_STATE_PAIRING;
            emit(e, UI_EV_ENTER_PAIRING);
        }
        /* Shorter than 2 s does nothing — deliberate, avoids accidents. */
    }
}

/* PLAY's long press fires while still held so sleep feels immediate. */
static void check_holds(ui_ctx_t *ui, uint32_t now, emitter_t *e)
{
    if (held(ui, UI_BTN_PLAY) && !(ui->handled_mask & UI_BTN_PLAY)) {
        if (now - ui->press_start[BTN_INDEX_PLAY] >= UI_LONG_PRESS_MS) {
            if (ui->state == UI_STATE_PLAYING || ui->state == UI_STATE_PAUSED) {
                emit(e, UI_EV_PLAY_STOP);
            }
            ui->state = UI_STATE_SLEEPING;
            emit(e, UI_EV_SLEEP);
            ui->handled_mask |= UI_BTN_PLAY;
        }
    }
}

uint32_t ui_tick(ui_ctx_t *ui, uint32_t now_ms, uint32_t raw_mask, bool tab_ok,
                 ui_event_t *events, uint32_t max_events)
{
    emitter_t e = {events, max_events, 0};
    if (!ui || !events) {
        return 0;
    }

    /* Debounce: a new raw value must persist before it becomes stable. */
    if (raw_mask != ui->pending_mask) {
        ui->pending_mask = raw_mask;
        ui->pending_since = now_ms;
    }
    ui->raw_mask = raw_mask;

    if (ui->pending_mask != ui->stable_mask &&
        now_ms - ui->pending_since >= UI_DEBOUNCE_MS) {
        uint32_t changed = ui->pending_mask ^ ui->stable_mask;
        uint32_t previous = ui->stable_mask;
        ui->stable_mask = ui->pending_mask;

        static const uint32_t bits[4] = {UI_BTN_REC, UI_BTN_PLAY, UI_BTN_TRACK,
                                         UI_BTN_MODE};
        for (int i = 0; i < 4; i++) {
            uint32_t bit = bits[i];
            if (!(changed & bit)) {
                continue;
            }
            if (ui->stable_mask & bit) {
                /* Timestamp the press at the debounce start, not now. */
                ui->press_start[i] = ui->pending_since;
                on_press(ui, bit, tab_ok, &e);
            } else if (previous & bit) {
                on_release(ui, bit, now_ms, &e);
            }
        }
    }

    check_holds(ui, now_ms, &e);
    return e.count;
}

ui_mode_stage_t ui_mode_stage(const ui_ctx_t *ui, uint32_t now_ms)
{
    if (!ui || !held(ui, UI_BTN_MODE)) {
        return UI_MODE_STAGE_NONE;
    }
    uint32_t d = now_ms - ui->press_start[BTN_INDEX_MODE];
    if (d >= UI_MODE_WIFI_MS) return UI_MODE_STAGE_WIFI;
    if (d >= UI_MODE_DUB_MS)  return UI_MODE_STAGE_DUB;
    if (d >= UI_MODE_PAIR_MS) return UI_MODE_STAGE_PAIR;
    return UI_MODE_STAGE_NONE;
}

const char *ui_state_name(ui_state_t s)
{
    switch (s) {
    case UI_STATE_IDLE:      return "IDLE";
    case UI_STATE_RECORDING: return "RECORDING";
    case UI_STATE_PLAYING:   return "PLAYING";
    case UI_STATE_PAUSED:    return "PAUSED";
    case UI_STATE_PAIRING:   return "PAIRING";
    case UI_STATE_DUBBING:   return "DUBBING";
    case UI_STATE_DEV_WIFI:  return "DEV_WIFI";
    case UI_STATE_SLEEPING:  return "SLEEPING";
    default:                 return "?";
    }
}

const char *ui_event_name(ui_event_t ev)
{
    switch (ev) {
    case UI_EV_NONE:               return "NONE";
    case UI_EV_RECORD_START:       return "RECORD_START";
    case UI_EV_RECORD_STOP:        return "RECORD_STOP";
    case UI_EV_RECORD_REFUSED_TAB: return "RECORD_REFUSED_TAB";
    case UI_EV_PLAY_START:         return "PLAY_START";
    case UI_EV_PLAY_PAUSE:         return "PLAY_PAUSE";
    case UI_EV_PLAY_RESUME:        return "PLAY_RESUME";
    case UI_EV_PLAY_STOP:          return "PLAY_STOP";
    case UI_EV_TRACK_CHANGED:      return "TRACK_CHANGED";
    case UI_EV_ENTER_PAIRING:      return "ENTER_PAIRING";
    case UI_EV_ENTER_DUB:          return "ENTER_DUB";
    case UI_EV_ENTER_DEV_WIFI:     return "ENTER_DEV_WIFI";
    case UI_EV_SLEEP:              return "SLEEP";
    case UI_EV_WAKE:               return "WAKE";
    default:                       return "?";
    }
}
