/* Record-path level control: a look-ahead peak limiter with bounded
 * makeup gain.
 *
 * One-pass recording cannot know a take's peak in advance, so the brief's
 * "analyze peak, normalize" is not available (docs/storage-budget.md §4).
 * This replaces it, and is the better tool anyway: peak normalisation is
 * fooled by a single door slam, and a MEMS mic in a room needs quiet
 * material lifted as much as it needs loud material caught.
 *
 * Two jobs, one gain:
 *
 *   target = min(max_gain, ceiling / envelope)
 *
 *   - When the room is loud, ceiling/envelope pulls the gain down and the
 *     signal cannot clip.
 *   - When the room is quiet, max_gain lifts it, so an ambient recording
 *     is actually audible.
 *
 * The look-ahead is what makes it transparent. The envelope is a true
 * sliding maximum over a window that *includes the sample about to leave
 * the delay line*, so gain is already correct when a transient emerges —
 * a hard guarantee rather than a fast attack that mostly keeps up.
 * That gives the invariant the tests lean on:
 *
 *     |output| <= LIMITER_CEILING, always, for any input
 *
 * Gain falls immediately and rises slowly. The immediate fall happens
 * during the quiet audio still in the delay line, so it is inaudible; the
 * slow rise is what stops the noise floor pumping between words.
 *
 * Integer throughout: deterministic, cheap, and byte-identical on the host
 * and on the ESP32, so these tests mean something.
 */
#ifndef LIMITER_H
#define LIMITER_H

#include <stdbool.h>
#include <stdint.h>

/* 10 ms at 44.1 kHz — enough for the gain to settle before a transient
 * arrives, short enough that the delay costs nothing anyone notices. */
#define LIMITER_LOOKAHEAD 441u
#define LIMITER_WINDOW    (LIMITER_LOOKAHEAD + 2u)

/* About -0.5 dBFS. Leaves headroom for the ADPCM quantiser rather than
 * parking the signal on the rail. */
#define LIMITER_CEILING 31000

/* Gain is Q16: 65536 == unity. */
#define LIMITER_UNITY_Q16    65536
#define LIMITER_MAX_GAIN_Q16 (8 * LIMITER_UNITY_Q16) /* +18 dB */

/* Below this envelope the gain is frozen rather than pushed higher, so a
 * silent room does not get amplified into a hiss. */
#define LIMITER_GATE 150

/* Gain rise: 2^14 samples ~ 371 ms. Slow enough not to pump. */
#define LIMITER_RELEASE_SHIFT 14u
/* Level meter decay: 2^9 samples ~ 12 ms, which reads as a VU needle. */
#define LIMITER_LEVEL_SHIFT 9u

typedef struct {
    int16_t  delay[LIMITER_LOOKAHEAD];
    uint32_t dpos;

    /* Monotonic deque giving the sliding maximum in O(1) per sample. A
     * decaying envelope would be cheaper but would let a decaying signal
     * overshoot the ceiling, which is the one thing this must not do. */
    int32_t  dq_val[LIMITER_WINDOW];
    uint32_t dq_idx[LIMITER_WINDOW];
    uint32_t dq_head, dq_tail;

    uint32_t n;          /* samples seen                        */
    int32_t  gain_q16;
    int32_t  level_peak; /* decaying peak of the output         */
} limiter_t;

void limiter_init(limiter_t *l);

/* Processes `count` mono samples. `out` may alias `in`. Output lags input
 * by LIMITER_LOOKAHEAD samples; the first call therefore emits that many
 * samples of the (silent) initial delay line. */
void limiter_process(limiter_t *l, const int16_t *in, int16_t *out,
                     uint32_t count);

/* Current gain, Q16. */
int32_t limiter_gain_q16(const limiter_t *l);

/* Output level 0..255, for the REC lamp and the reel brightness. */
uint8_t limiter_level(const limiter_t *l);

#endif /* LIMITER_H */
