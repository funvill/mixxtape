/* Button handling and top-level device state machine.
 *
 * Pure logic: fed a debounced-in-time button mask and a millisecond clock,
 * it emits events. No hardware, no RTOS — so the whole interaction model is
 * host-testable (see test/host/test_ui_state.c).
 *
 * Behaviour per docs/cassette-recorder-agent-brief.md sec.6:
 *   REC    hold to record; release stops. Refused if the tab is snapped.
 *   PLAY   short = play/pause, long = sleep
 *   TRACK  short = cycle 1 -> 2 -> 3
 *   MODE   2 s = pair, 5 s = dub, 10 s = dev WiFi (decided on release)
 */
#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "tape_layout.h"

#define UI_BTN_REC   (1u << 0)
#define UI_BTN_PLAY  (1u << 1)
#define UI_BTN_TRACK (1u << 2)
#define UI_BTN_MODE  (1u << 3)

#define UI_DEBOUNCE_MS   20u
#define UI_LONG_PRESS_MS 1000u
#define UI_MODE_PAIR_MS  2000u
#define UI_MODE_DUB_MS   5000u
#define UI_MODE_WIFI_MS  10000u

typedef enum {
    UI_STATE_IDLE = 0,
    UI_STATE_RECORDING,
    UI_STATE_PLAYING,
    UI_STATE_PAUSED,
    UI_STATE_PAIRING,
    UI_STATE_DUBBING,
    UI_STATE_DEV_WIFI,
    UI_STATE_SLEEPING,
} ui_state_t;

typedef enum {
    UI_EV_NONE = 0,
    UI_EV_RECORD_START,
    UI_EV_RECORD_STOP,
    UI_EV_RECORD_REFUSED_TAB,
    UI_EV_PLAY_START,
    UI_EV_PLAY_PAUSE,
    UI_EV_PLAY_RESUME,
    UI_EV_PLAY_STOP,
    UI_EV_TRACK_CHANGED,
    UI_EV_ENTER_PAIRING,
    UI_EV_ENTER_DUB,
    UI_EV_ENTER_DEV_WIFI,
    UI_EV_SLEEP,
    UI_EV_WAKE,
} ui_event_t;

/* MODE hold feedback for the LEDs while the button is still down. */
typedef enum {
    UI_MODE_STAGE_NONE = 0,
    UI_MODE_STAGE_PAIR,
    UI_MODE_STAGE_DUB,
    UI_MODE_STAGE_WIFI,
} ui_mode_stage_t;

typedef struct {
    ui_state_t state;
    int track; /* 0..TAPE_SLOT_COUNT-1 */

    uint32_t raw_mask;      /* last raw sample                       */
    uint32_t stable_mask;   /* debounced                             */
    uint32_t pending_mask;  /* raw sample awaiting debounce          */
    uint32_t pending_since; /* when pending_mask was first seen      */

    uint32_t press_start[4]; /* per-button press timestamp           */
    uint32_t handled_mask;   /* buttons whose action already fired   */
} ui_ctx_t;

void ui_init(ui_ctx_t *ui);

/* Feed a raw button mask (1 = pressed) and the write-protect tab state
 * (true = intact = recording allowed). Emits up to `max_events` events and
 * returns how many were written. Call at any rate finer than the debounce. */
uint32_t ui_tick(ui_ctx_t *ui, uint32_t now_ms, uint32_t raw_mask, bool tab_ok,
                 ui_event_t *events, uint32_t max_events);

/* Which MODE action would fire if the button were released right now. */
ui_mode_stage_t ui_mode_stage(const ui_ctx_t *ui, uint32_t now_ms);

const char *ui_state_name(ui_state_t s);
const char *ui_event_name(ui_event_t e);

#endif /* UI_STATE_H */
