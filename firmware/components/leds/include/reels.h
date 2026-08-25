/* The reel display.
 *
 * Twelve RGB LEDs ring each routed-out reel window. As a track plays the
 * left ring empties and the right ring fills, so tape position is legible
 * without anyone having to learn what the lights mean — that is the whole
 * point of the object.
 *
 * Rendering is a pure function of (mode, progress, level, clock). No
 * hardware, no timers: the RMT peripheral just gets handed the frame this
 * produces, and the whole visual language is host-testable.
 *
 * Power is a first-class constraint here, not an afterthought. All 29 LEDs
 * at full white is roughly 1.7 A and the board has a 500 mA USB budget
 * until CC sensing says otherwise, so every frame is scaled by a global
 * brightness cap and `reels_estimated_ma()` exists so tests can prove no
 * mode ever blows the budget.
 */
#ifndef REELS_H
#define REELS_H

#include <stdbool.h>
#include <stdint.h>

/* Chain geometry — must match board.h. D1-D12 left, D13-D24 right,
 * D25-D27 track, D28 REC, D29 BT. */
#define REEL_RING_LEDS   12u
#define REEL_LEFT_FIRST  0u
#define REEL_RIGHT_FIRST 12u
#define REEL_TRACK_FIRST 24u
#define REEL_TRACK_COUNT 3u
#define REEL_LED_REC     27u
#define REEL_LED_BT      28u
#define REEL_LED_COUNT   29u

typedef struct {
    uint8_t r, g, b;
} rgb_t;

typedef enum {
    REEL_MODE_IDLE = 0,
    REEL_MODE_SEARCHING,  /* amber sweep: looking for a sink            */
    REEL_MODE_CONNECTED,  /* green lock                                 */
    REEL_MODE_PLAYING,    /* white: left empties, right fills           */
    REEL_MODE_RECORDING,  /* red pulse, brightness tracks level         */
    REEL_MODE_ERASING,    /* slow reverse spin, dim — background erase  */
    REEL_MODE_DUBBING,    /* violet                                     */
    REEL_MODE_SINK_BUSY,  /* slow red pulse: the speaker is busy        */
} reel_mode_t;

typedef struct {
    reel_mode_t mode;
    uint32_t now_ms;
    uint32_t progress_permille; /* 0..1000, playback position           */
    uint8_t  level;             /* 0..255, record VU                    */
    int      track;             /* 0..2, which track indicator is lit   */
    bool     bt_connected;
    uint8_t  brightness;        /* global cap, 0..255                   */
} reel_input_t;

/* Renders one frame into `out` (at least REEL_LED_COUNT entries). */
void reels_render(const reel_input_t *in, rgb_t *out, uint32_t count);

/* Rough current draw of a rendered frame, in milliamps, at ~20 mA per
 * fully-lit channel — the figure the BOM's 1.7 A all-white number implies. */
uint32_t reels_estimated_ma(const rgb_t *px, uint32_t count);

#endif /* REELS_H */
