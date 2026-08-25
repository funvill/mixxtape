/* Interaction-model tests: the whole button grammar from the brief,
 * exercised against a simulated clock. */

#include "test_util.h"
#include "ui_state.h"

#define MAX_EV 64

static ui_event_t g_evs[MAX_EV];
static uint32_t g_nev;
static uint32_t g_now;

static void begin(ui_ctx_t *ui)
{
    ui_init(ui);
    g_nev = 0;
    g_now = 1000u; /* start away from zero to catch bad time arithmetic */
}

/* Holds `mask` for `dur_ms`, ticking every 5 ms and collecting events. */
static void hold(ui_ctx_t *ui, uint32_t mask, bool tab_ok, uint32_t dur_ms)
{
    uint32_t end = g_now + dur_ms;
    while (g_now < end) {
        ui_event_t evs[8];
        uint32_t n = ui_tick(ui, g_now, mask, tab_ok, evs, 8u);
        for (uint32_t i = 0; i < n && g_nev < MAX_EV; i++) {
            g_evs[g_nev++] = evs[i];
        }
        g_now += 5u;
    }
}

static void tap(ui_ctx_t *ui, uint32_t mask, bool tab_ok, uint32_t dur_ms)
{
    hold(ui, mask, tab_ok, dur_ms);
    hold(ui, 0u, tab_ok, 100u); /* release, well past debounce */
}

static bool saw(ui_event_t ev)
{
    for (uint32_t i = 0; i < g_nev; i++) {
        if (g_evs[i] == ev) {
            return true;
        }
    }
    return false;
}

static void dump(void)
{
    printf("   events:");
    for (uint32_t i = 0; i < g_nev; i++) {
        printf(" %s", ui_event_name(g_evs[i]));
    }
    printf("\n");
}

/* --- tests ---------------------------------------------------------- */

static void test_record_hold(void)
{
    ui_ctx_t ui;
    begin(&ui);

    hold(&ui, UI_BTN_REC, true, 500u);
    CHECK_EQ(ui.state, UI_STATE_RECORDING);
    CHECK(saw(UI_EV_RECORD_START));
    CHECK(!saw(UI_EV_RECORD_STOP));

    hold(&ui, 0u, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_IDLE);
    CHECK(saw(UI_EV_RECORD_STOP));
    CHECK_EQ(g_nev, 2u);
}

static void test_record_refused_when_tab_snapped(void)
{
    ui_ctx_t ui;
    begin(&ui);

    tap(&ui, UI_BTN_REC, false, 800u);
    CHECK(saw(UI_EV_RECORD_REFUSED_TAB));
    CHECK(!saw(UI_EV_RECORD_START));
    CHECK_EQ(ui.state, UI_STATE_IDLE);

    /* And it stays refused however long you hold it. */
    begin(&ui);
    tap(&ui, UI_BTN_REC, false, 5000u);
    CHECK(!saw(UI_EV_RECORD_START));
}

static void test_play_pause_resume(void)
{
    ui_ctx_t ui;
    begin(&ui);

    tap(&ui, UI_BTN_PLAY, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_PLAYING);
    CHECK(saw(UI_EV_PLAY_START));

    g_nev = 0;
    tap(&ui, UI_BTN_PLAY, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_PAUSED);
    CHECK(saw(UI_EV_PLAY_PAUSE));

    g_nev = 0;
    tap(&ui, UI_BTN_PLAY, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_PLAYING);
    CHECK(saw(UI_EV_PLAY_RESUME));
}

static void test_play_long_press_sleeps_and_wakes(void)
{
    ui_ctx_t ui;
    begin(&ui);

    /* Sleep fires while the button is still down, not on release. */
    hold(&ui, UI_BTN_PLAY, true, 1200u);
    CHECK(saw(UI_EV_SLEEP));
    CHECK_EQ(ui.state, UI_STATE_SLEEPING);

    g_nev = 0;
    hold(&ui, 0u, true, 100u);
    CHECK_EQ(g_nev, 0u); /* the release must not also start playback */

    tap(&ui, UI_BTN_PLAY, true, 80u);
    CHECK(saw(UI_EV_WAKE));
    CHECK(!saw(UI_EV_PLAY_START)); /* the waking press does nothing else */
    CHECK_EQ(ui.state, UI_STATE_IDLE);
}

static void test_track_cycles(void)
{
    ui_ctx_t ui;
    begin(&ui);

    CHECK_EQ(ui.track, 0);
    tap(&ui, UI_BTN_TRACK, true, 80u);
    CHECK_EQ(ui.track, 1);
    tap(&ui, UI_BTN_TRACK, true, 80u);
    CHECK_EQ(ui.track, 2);
    tap(&ui, UI_BTN_TRACK, true, 80u);
    CHECK_EQ(ui.track, 0);
    CHECK_EQ(TAPE_SLOT_COUNT, 3);
}

static void test_mode_hold_thresholds(void)
{
    ui_ctx_t ui;

    begin(&ui);
    tap(&ui, UI_BTN_MODE, true, 800u);
    CHECK_EQ(g_nev, 0u); /* too short to do anything */
    CHECK_EQ(ui.state, UI_STATE_IDLE);

    begin(&ui);
    tap(&ui, UI_BTN_MODE, true, 3000u);
    CHECK(saw(UI_EV_ENTER_PAIRING));
    CHECK_EQ(ui.state, UI_STATE_PAIRING);

    begin(&ui);
    tap(&ui, UI_BTN_MODE, true, 6000u);
    CHECK(saw(UI_EV_ENTER_DUB));
    CHECK(!saw(UI_EV_ENTER_PAIRING)); /* passing 2 s must not also pair */
    CHECK_EQ(ui.state, UI_STATE_DUBBING);

    begin(&ui);
    tap(&ui, UI_BTN_MODE, true, 11000u);
    CHECK(saw(UI_EV_ENTER_DEV_WIFI));
    CHECK(!saw(UI_EV_ENTER_DUB));
    CHECK_EQ(ui.state, UI_STATE_DEV_WIFI);
}

static void test_mode_stage_feedback(void)
{
    ui_ctx_t ui;
    begin(&ui);

    hold(&ui, UI_BTN_MODE, true, 500u);
    CHECK_EQ(ui_mode_stage(&ui, g_now), UI_MODE_STAGE_NONE);
    hold(&ui, UI_BTN_MODE, true, 2000u);
    CHECK_EQ(ui_mode_stage(&ui, g_now), UI_MODE_STAGE_PAIR);
    hold(&ui, UI_BTN_MODE, true, 3000u);
    CHECK_EQ(ui_mode_stage(&ui, g_now), UI_MODE_STAGE_DUB);
    hold(&ui, UI_BTN_MODE, true, 5000u);
    CHECK_EQ(ui_mode_stage(&ui, g_now), UI_MODE_STAGE_WIFI);

    hold(&ui, 0u, true, 100u);
    CHECK_EQ(ui_mode_stage(&ui, g_now), UI_MODE_STAGE_NONE);
}

static void test_debounce_rejects_glitch(void)
{
    ui_ctx_t ui;
    begin(&ui);

    /* A contact bounce shorter than the debounce window must be invisible. */
    hold(&ui, UI_BTN_PLAY, true, 10u);
    hold(&ui, 0u, true, 200u);
    CHECK_EQ(g_nev, 0u);
    CHECK_EQ(ui.state, UI_STATE_IDLE);
}

static void test_recording_is_exclusive(void)
{
    ui_ctx_t ui;
    begin(&ui);

    hold(&ui, UI_BTN_REC, true, 200u);
    CHECK_EQ(ui.state, UI_STATE_RECORDING);
    g_nev = 0;

    /* Mashing other buttons mid-take changes nothing. */
    hold(&ui, UI_BTN_REC | UI_BTN_TRACK, true, 200u);
    hold(&ui, UI_BTN_REC, true, 100u);
    hold(&ui, UI_BTN_REC | UI_BTN_PLAY, true, 200u);
    hold(&ui, UI_BTN_REC, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_RECORDING);
    CHECK_EQ(ui.track, 0);
    CHECK_EQ(g_nev, 0u);

    hold(&ui, 0u, true, 100u);
    CHECK(saw(UI_EV_RECORD_STOP));
    CHECK_EQ(ui.state, UI_STATE_IDLE);
}

static void test_record_interrupts_playback(void)
{
    ui_ctx_t ui;
    begin(&ui);

    tap(&ui, UI_BTN_PLAY, true, 100u);
    CHECK_EQ(ui.state, UI_STATE_PLAYING);
    g_nev = 0;

    hold(&ui, UI_BTN_REC, true, 200u);
    CHECK(saw(UI_EV_PLAY_STOP));
    CHECK(saw(UI_EV_RECORD_START));
    CHECK_EQ(ui.state, UI_STATE_RECORDING);
    if (g_nev < 2u) {
        dump();
    }
}

static void test_track_change_stops_playback(void)
{
    ui_ctx_t ui;
    begin(&ui);

    tap(&ui, UI_BTN_PLAY, true, 100u);
    g_nev = 0;
    tap(&ui, UI_BTN_TRACK, true, 80u);
    CHECK(saw(UI_EV_PLAY_STOP));
    CHECK(saw(UI_EV_TRACK_CHANGED));
    CHECK_EQ(ui.state, UI_STATE_IDLE);
    CHECK_EQ(ui.track, 1);
}

TEST_MAIN_BEGIN()

RUN(test_record_hold);
RUN(test_record_refused_when_tab_snapped);
RUN(test_play_pause_resume);
RUN(test_play_long_press_sleeps_and_wakes);
RUN(test_track_cycles);
RUN(test_mode_hold_thresholds);
RUN(test_mode_stage_feedback);
RUN(test_debounce_rejects_glitch);
RUN(test_recording_is_exclusive);
RUN(test_record_interrupts_playback);
RUN(test_track_change_stops_playback);

TEST_MAIN_END()
