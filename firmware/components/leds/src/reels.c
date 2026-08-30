#include "reels.h"

#include <string.h>

/* The fitted XL-1615RGBC-2812B is a PRESET 5 mA/CHANNEL CONSTANT-CURRENT
 * part - LCSC's own listing says "Optimized preset 5mA/channel
 * constant-current mode", and every optical figure in its datasheet is
 * tested at IF = 5 mA. This was 20u, carried over from the 1.7 A figure in
 * the brief, which was itself derived from the 5050-size WS2812B. All 29
 * at full white is therefore about 435 mA, not 1.7 A.
 *
 * Getting this wrong is safe in the current direction but wrong in the
 * useful one: the brightness caps derived from it ran the reels at roughly
 * a quarter of what the power budget actually allows, on a product whose
 * whole premise is a legible reel display. */
#define MA_PER_CHANNEL 5u

static uint8_t scale(uint32_t value, uint32_t k)
{
    uint32_t v = (value * k) / 255u;
    return (uint8_t)(v > 255u ? 255u : v);
}

static rgb_t dim(rgb_t c, uint32_t k)
{
    rgb_t o;
    o.r = scale(c.r, k);
    o.g = scale(c.g, k);
    o.b = scale(c.b, k);
    return o;
}

/* Triangle wave 0..255, no libm and no phase drift. */
static uint32_t triangle(uint32_t now_ms, uint32_t period_ms)
{
    if (!period_ms) {
        return 0;
    }
    uint32_t phase = now_ms % period_ms;
    uint32_t half = period_ms / 2u;
    if (!half) {
        return 0;
    }
    return (phase < half) ? (phase * 255u) / half
                          : ((period_ms - phase) * 255u) / half;
}

/* How lit reel LED `i` is, given how far the tape has advanced. Returns
 * 0..255 for the *filling* (right) ring; the emptying ring is the
 * complement, which makes both an arc rather than a crossfade. */
static uint32_t fill_level(uint32_t i, uint32_t progress_permille)
{
    uint32_t filled = progress_permille * REEL_RING_LEDS; /* 0..12000 */
    uint32_t lo = i * 1000u;
    uint32_t hi = lo + 1000u;
    if (filled >= hi) {
        return 255u;
    }
    if (filled <= lo) {
        return 0u;
    }
    return ((filled - lo) * 255u) / 1000u;
}

static void render_rings(const reel_input_t *in, rgb_t *out)
{
    const rgb_t white = {255u, 255u, 255u};
    const rgb_t amber = {255u, 140u, 0u};
    const rgb_t green = {0u, 255u, 60u};
    const rgb_t red = {255u, 0u, 0u};
    const rgb_t violet = {160u, 0u, 255u};
    const rgb_t off = {0u, 0u, 0u};

    for (uint32_t i = 0; i < REEL_RING_LEDS; i++) {
        rgb_t left = off, right = off;

        switch (in->mode) {
        case REEL_MODE_PLAYING: {
            /* The tape metaphor: what leaves the left reel arrives at the
             * right one. Deliberately not a progress bar. */
            uint32_t v = fill_level(i, in->progress_permille);
            right = dim(white, v);
            left = dim(white, 255u - v);
            break;
        }
        case REEL_MODE_RECORDING: {
            /* Red, riding the input level, with a floor so silence still
             * reads as "armed and running". */
            uint32_t lvl = 70u + ((uint32_t)in->level * 185u) / 255u;
            uint32_t pulse = triangle(in->now_ms, 1400u) / 4u;
            uint32_t v = lvl > pulse ? lvl : pulse;
            left = right = dim(red, v);
            break;
        }
        case REEL_MODE_SEARCHING: {
            /* A dot with a short tail, one revolution per second. */
            uint32_t pos = (in->now_ms / 83u) % REEL_RING_LEDS;
            uint32_t back1 = (pos + REEL_RING_LEDS - 1u) % REEL_RING_LEDS;
            uint32_t back2 = (pos + REEL_RING_LEDS - 2u) % REEL_RING_LEDS;
            uint32_t v = (i == pos) ? 255u
                                    : (i == back1) ? 90u
                                                   : (i == back2) ? 30u : 0u;
            left = right = dim(amber, v);
            break;
        }
        case REEL_MODE_CONNECTED:
            left = right = dim(green, 110u);
            break;
        case REEL_MODE_ERASING: {
            /* Slow reverse spin, dim — what a deck does before it records
             * over something. */
            uint32_t pos = REEL_RING_LEDS - 1u -
                           ((in->now_ms / 200u) % REEL_RING_LEDS);
            left = right = dim(white, (i == pos) ? 70u : 12u);
            break;
        }
        case REEL_MODE_DUBBING: {
            uint32_t v = 60u + triangle(in->now_ms, 900u) / 3u;
            left = right = dim(violet, v);
            break;
        }
        case REEL_MODE_SINK_BUSY: {
            /* Honest feedback: the speaker is busy talking to a phone. */
            uint32_t v = triangle(in->now_ms, 2000u) / 2u;
            left = right = dim(red, v);
            break;
        }
        case REEL_MODE_IDLE:
        default:
            break;
        }

        out[REEL_LEFT_FIRST + i] = left;
        out[REEL_RIGHT_FIRST + i] = right;
    }
}

void reels_render(const reel_input_t *in, rgb_t *out, uint32_t count)
{
    if (!in || !out || count == 0) {
        return;
    }
    rgb_t frame[REEL_LED_COUNT];
    memset(frame, 0, sizeof(frame));

    render_rings(in, frame);

    /* Track indicators: the selected one only. */
    for (uint32_t i = 0; i < REEL_TRACK_COUNT; i++) {
        bool on = ((int)i == in->track);
        rgb_t c = {0u, 0u, 0u};
        if (on) {
            c.r = c.g = c.b = 140u;
        }
        frame[REEL_TRACK_FIRST + i] = c;
    }

    /* REC lamp follows the level while recording. */
    if (in->mode == REEL_MODE_RECORDING) {
        uint32_t v = 70u + ((uint32_t)in->level * 185u) / 255u;
        rgb_t red = {255u, 0u, 0u};
        frame[REEL_LED_REC] = dim(red, v);
    }

    /* BT lamp: steady when bonded, blinking while hunting. */
    if (in->mode == REEL_MODE_SEARCHING) {
        rgb_t blue = {0u, 80u, 255u};
        frame[REEL_LED_BT] = dim(blue, triangle(in->now_ms, 600u));
    } else if (in->bt_connected) {
        rgb_t blue = {0u, 80u, 255u};
        frame[REEL_LED_BT] = dim(blue, 120u);
    }

    /* Global cap last, so no mode can ever exceed the power budget by
     * being clever about its own brightness. */
    uint32_t cap = in->brightness;
    uint32_t n = (count < REEL_LED_COUNT) ? count : REEL_LED_COUNT;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = dim(frame[i], cap);
    }
}

uint32_t reels_estimated_ma(const rgb_t *px, uint32_t count)
{
    if (!px) {
        return 0;
    }
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += (uint32_t)px[i].r + px[i].g + px[i].b;
    }
    /* Each channel draws MA_PER_CHANNEL at 255. */
    return (sum * MA_PER_CHANNEL) / 255u;
}
