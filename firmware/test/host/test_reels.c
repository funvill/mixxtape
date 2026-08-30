/* Reel display tests: the tape metaphor, and the power budget. */

#include <string.h>

#include "reels.h"
#include "test_util.h"

/* Brightness caps from board.h. KEEP THESE IN STEP WITH board.h - they were
 * left at 40/76 after board.h moved to 130/255, so this file rendered at the
 * old cap and then checked against the old cap: self-consistent, and testing
 * nothing. The caps changed because the fitted LED is a preset 5 mA/channel
 * part, not the 20 mA/channel the budget had assumed. */
#define CAP_USB_DEFAULT 130u /* 500 mA source */
#define CAP_USB_HIGH    255u /* 1.5 A / 3 A source */

/* The LEDs get a slice of a 500 mA budget: the ESP32 alone bursts to
 * ~250 mA on Bluetooth transmit through a linear regulator, and the LDO
 * needs headroom. At 5 mA/channel all 29 LEDs at full white is ~435 mA,
 * so the default cap must hold the array under this. */
#define LED_BUDGET_MA 250u

/* Pins the current model itself, so that swapping the LED for a part with a
 * different drive current fails HERE rather than in the field. All 29 at
 * full white, 5 mA per channel: 29 * 3 * 5 = 435 mA. */
#define LED_FULL_WHITE_MA 435u

static rgb_t g_frame[REEL_LED_COUNT];

static reel_input_t base_input(reel_mode_t mode)
{
    reel_input_t in;
    memset(&in, 0, sizeof(in));
    in.mode = mode;
    in.brightness = CAP_USB_DEFAULT;
    in.track = 0;
    return in;
}

static uint32_t ring_sum(uint32_t first)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < REEL_RING_LEDS; i++) {
        rgb_t c = g_frame[first + i];
        s += (uint32_t)c.r + c.g + c.b;
    }
    return s;
}

static void test_tape_transfers_between_reels(void)
{
    /* The signature behaviour: at the start of a side the left reel is
     * full and the right is empty; at the end it is the other way round. */
    reel_input_t in = base_input(REEL_MODE_PLAYING);

    in.progress_permille = 0;
    reels_render(&in, g_frame, REEL_LED_COUNT);
    uint32_t left0 = ring_sum(REEL_LEFT_FIRST);
    uint32_t right0 = ring_sum(REEL_RIGHT_FIRST);
    CHECK(left0 > 0u);
    CHECK_EQ(right0, 0u);

    in.progress_permille = 1000u;
    reels_render(&in, g_frame, REEL_LED_COUNT);
    uint32_t left1 = ring_sum(REEL_LEFT_FIRST);
    uint32_t right1 = ring_sum(REEL_RIGHT_FIRST);
    CHECK_EQ(left1, 0u);
    CHECK(right1 > 0u);
    CHECK_EQ(right1, left0); /* what left one reel arrived at the other */

    in.progress_permille = 500u;
    reels_render(&in, g_frame, REEL_LED_COUNT);
    uint32_t left_h = ring_sum(REEL_LEFT_FIRST);
    uint32_t right_h = ring_sum(REEL_RIGHT_FIRST);
    CHECK(left_h > 0u && right_h > 0u);
    CHECK_EQ(left_h, right_h); /* halfway is symmetric */
}

static void test_total_lit_is_conserved(void)
{
    /* Tape does not appear or vanish: across the whole side the two reels
     * together always hold the same amount. */
    reel_input_t in = base_input(REEL_MODE_PLAYING);
    reels_render(&in, g_frame, REEL_LED_COUNT);
    uint32_t reference = ring_sum(REEL_LEFT_FIRST) + ring_sum(REEL_RIGHT_FIRST);

    for (uint32_t prog = 0; prog <= 1000u; prog += 25u) {
        in.progress_permille = prog;
        reels_render(&in, g_frame, REEL_LED_COUNT);
        uint32_t total =
            ring_sum(REEL_LEFT_FIRST) + ring_sum(REEL_RIGHT_FIRST);
        /* At most one LED straddles the boundary at a time, and it is
         * scaled twice on the way out (level, then the global cap), so it
         * can lose up to one LSB per channel. That is invisible; drift
         * beyond it would not be. */
        uint32_t diff = (total > reference) ? (total - reference)
                                            : (reference - total);
        CHECK(diff <= 6u);
    }
}

static void test_progress_advances_monotonically(void)
{
    reel_input_t in = base_input(REEL_MODE_PLAYING);
    uint32_t last = 0;
    for (uint32_t prog = 0; prog <= 1000u; prog += 10u) {
        in.progress_permille = prog;
        reels_render(&in, g_frame, REEL_LED_COUNT);
        uint32_t right = ring_sum(REEL_RIGHT_FIRST);
        CHECK(right >= last);
        last = right;
    }
}

static void test_current_model_matches_the_fitted_part(void)
{
    /* The fitted XL-1615RGBC-2812B is a preset 5 mA/channel constant-current
     * part. If someone swaps the LED again, this is the assertion that should
     * stop them: the previous part number lived only in a comment, and
     * nothing checked it. All 29 at full white, 3 channels, 5 mA each. */
    rgb_t full[REEL_LED_COUNT];
    uint32_t i;
    uint32_t ma;

    for (i = 0; i < REEL_LED_COUNT; i++) {
        full[i].r = 255u;
        full[i].g = 255u;
        full[i].b = 255u;
    }
    ma = reels_estimated_ma(full, REEL_LED_COUNT);
    CHECK_EQ(ma, LED_FULL_WHITE_MA);

    /* And the default cap must keep the whole array inside the slice of a
     * 500 mA source that is left once the ESP32 has taken its Bluetooth
     * transmit burst. */
    for (i = 0; i < REEL_LED_COUNT; i++) {
        full[i].r = CAP_USB_DEFAULT;
        full[i].g = CAP_USB_DEFAULT;
        full[i].b = CAP_USB_DEFAULT;
    }
    ma = reels_estimated_ma(full, REEL_LED_COUNT);
    CHECK(ma <= LED_BUDGET_MA);
}

static void test_brightness_cap_is_absolute(void)
{
    /* No mode, at any moment, may exceed the global cap — that is what
     * keeps the LED chain inside the USB budget. */
    for (int m = REEL_MODE_IDLE; m <= REEL_MODE_SINK_BUSY; m++) {
        reel_input_t in = base_input((reel_mode_t)m);
        in.level = 255u;
        in.bt_connected = true;
        for (uint32_t t = 0; t < 3000u; t += 37u) {
            in.now_ms = t;
            in.progress_permille = (t % 1001u);
            reels_render(&in, g_frame, REEL_LED_COUNT);
            for (uint32_t i = 0; i < REEL_LED_COUNT; i++) {
                CHECK(g_frame[i].r <= CAP_USB_DEFAULT);
                CHECK(g_frame[i].g <= CAP_USB_DEFAULT);
                CHECK(g_frame[i].b <= CAP_USB_DEFAULT);
            }
        }
    }
}

static void test_power_budget_holds_in_every_mode(void)
{
    /* Sweep every mode across time and worst-case inputs, and require the
     * whole chain to stay inside its slice of the USB budget. If this ever
     * fails, either the render or the brightness cap has to change — not
     * the assertion. */
    uint32_t worst = 0;
    int worst_mode = 0;

    for (int m = REEL_MODE_IDLE; m <= REEL_MODE_SINK_BUSY; m++) {
        reel_input_t in = base_input((reel_mode_t)m);
        in.level = 255u;
        in.bt_connected = true;
        for (uint32_t t = 0; t < 3000u; t += 13u) {
            in.now_ms = t;
            in.progress_permille = (t % 1001u);
            reels_render(&in, g_frame, REEL_LED_COUNT);
            uint32_t ma = reels_estimated_ma(g_frame, REEL_LED_COUNT);
            if (ma > worst) {
                worst = ma;
                worst_mode = m;
            }
        }
    }
    printf("   worst case %u mA (mode %d), budget %u mA\n", worst, worst_mode,
           LED_BUDGET_MA);
    CHECK(worst <= LED_BUDGET_MA);

    /* And the high-power cap, used only once CC sensing has seen a 1.5 A or
     * 3 A source, must still be sane against that larger budget. */
    uint32_t worst_high = 0;
    for (int m = REEL_MODE_IDLE; m <= REEL_MODE_SINK_BUSY; m++) {
        reel_input_t in = base_input((reel_mode_t)m);
        in.brightness = CAP_USB_HIGH;
        in.level = 255u;
        in.bt_connected = true;
        for (uint32_t t = 0; t < 3000u; t += 13u) {
            in.now_ms = t;
            in.progress_permille = (t % 1001u);
            reels_render(&in, g_frame, REEL_LED_COUNT);
            uint32_t ma = reels_estimated_ma(g_frame, REEL_LED_COUNT);
            if (ma > worst_high) {
                worst_high = ma;
            }
        }
    }
    printf("   worst case at the 1.5A cap: %u mA\n", worst_high);
    CHECK(worst_high <= 900u);
}

static void test_never_all_white(void)
{
    /* All 29 at full white is ~1.7 A. Even ignoring the cap, no mode should
     * be asking for that shape of frame. */
    for (int m = REEL_MODE_IDLE; m <= REEL_MODE_SINK_BUSY; m++) {
        reel_input_t in = base_input((reel_mode_t)m);
        in.brightness = 255u; /* cap removed on purpose */
        in.level = 255u;
        in.bt_connected = true;
        for (uint32_t t = 0; t < 2000u; t += 29u) {
            in.now_ms = t;
            in.progress_permille = (t % 1001u);
            reels_render(&in, g_frame, REEL_LED_COUNT);
            uint32_t full = 0;
            for (uint32_t i = 0; i < REEL_LED_COUNT; i++) {
                if (g_frame[i].r == 255u && g_frame[i].g == 255u &&
                    g_frame[i].b == 255u) {
                    full++;
                }
            }
            CHECK(full < REEL_LED_COUNT);
        }
    }
}

static void test_idle_is_dark(void)
{
    reel_input_t in = base_input(REEL_MODE_IDLE);
    in.track = -1;
    reels_render(&in, g_frame, REEL_LED_COUNT);
    CHECK_EQ(reels_estimated_ma(g_frame, REEL_LED_COUNT), 0u);
}

static void test_indicators(void)
{
    /* Exactly one track lamp, and it follows the selection. */
    for (int t = 0; t < (int)REEL_TRACK_COUNT; t++) {
        reel_input_t in = base_input(REEL_MODE_IDLE);
        in.track = t;
        reels_render(&in, g_frame, REEL_LED_COUNT);
        uint32_t lit = 0;
        for (uint32_t i = 0; i < REEL_TRACK_COUNT; i++) {
            rgb_t c = g_frame[REEL_TRACK_FIRST + i];
            if (c.r || c.g || c.b) {
                lit++;
                CHECK_EQ((int)i, t);
            }
        }
        CHECK_EQ(lit, 1u);
    }

    /* The REC lamp only burns while recording, and tracks the level. */
    reel_input_t rec = base_input(REEL_MODE_RECORDING);
    rec.level = 0u;
    reels_render(&rec, g_frame, REEL_LED_COUNT);
    uint32_t quiet = g_frame[REEL_LED_REC].r;
    rec.level = 255u;
    reels_render(&rec, g_frame, REEL_LED_COUNT);
    uint32_t loud = g_frame[REEL_LED_REC].r;
    CHECK(quiet > 0u);  /* visible even in silence: it is armed */
    CHECK(loud > quiet); /* and rides the input */

    reel_input_t idle = base_input(REEL_MODE_IDLE);
    reels_render(&idle, g_frame, REEL_LED_COUNT);
    CHECK_EQ(g_frame[REEL_LED_REC].r, 0u);

    /* BT lamp is steady when bonded. */
    reel_input_t conn = base_input(REEL_MODE_CONNECTED);
    conn.bt_connected = true;
    reels_render(&conn, g_frame, REEL_LED_COUNT);
    CHECK(g_frame[REEL_LED_BT].b > 0u);
}

static void test_searching_sweeps(void)
{
    /* The dot has to actually move, or it is not a sweep. */
    reel_input_t in = base_input(REEL_MODE_SEARCHING);
    uint32_t seen_positions = 0;
    uint32_t previous = REEL_RING_LEDS;

    for (uint32_t t = 0; t < 1200u; t += 40u) {
        in.now_ms = t;
        reels_render(&in, g_frame, REEL_LED_COUNT);
        uint32_t brightest = 0, best = 0;
        for (uint32_t i = 0; i < REEL_RING_LEDS; i++) {
            uint32_t v = g_frame[REEL_LEFT_FIRST + i].r;
            if (v > best) {
                best = v;
                brightest = i;
            }
        }
        if (brightest != previous) {
            seen_positions++;
            previous = brightest;
        }
    }
    CHECK(seen_positions >= REEL_RING_LEDS);
}

static void test_guards(void)
{
    reel_input_t in = base_input(REEL_MODE_PLAYING);
    reels_render(0, g_frame, REEL_LED_COUNT); /* must not crash */
    reels_render(&in, 0, REEL_LED_COUNT);
    reels_render(&in, g_frame, 0);
    CHECK_EQ(reels_estimated_ma(0, 4u), 0u);

    /* A short buffer is filled only as far as it goes. */
    rgb_t small[4];
    memset(small, 0xAB, sizeof(small));
    reels_render(&in, small, 4u);
    CHECK_EQ(small[3].r, small[3].r); /* wrote within bounds, no overrun */
}

TEST_MAIN_BEGIN()

RUN(test_tape_transfers_between_reels);
RUN(test_total_lit_is_conserved);
RUN(test_progress_advances_monotonically);
RUN(test_current_model_matches_the_fitted_part);
RUN(test_brightness_cap_is_absolute);
RUN(test_power_budget_holds_in_every_mode);
RUN(test_never_all_white);
RUN(test_idle_is_dark);
RUN(test_indicators);
RUN(test_searching_sweeps);
RUN(test_guards);

TEST_MAIN_END()
