#include "limiter.h"

#include <string.h>

/* --- sliding maximum -------------------------------------------------
 * Values are kept in decreasing order, so the front is always the maximum
 * of the window. Each sample is pushed and popped at most once.
 */

static void dq_push(limiter_t *l, int32_t v, uint32_t idx)
{
    while (l->dq_head != l->dq_tail) {
        uint32_t back = (l->dq_tail + LIMITER_WINDOW - 1u) % LIMITER_WINDOW;
        if (l->dq_val[back] <= v) {
            l->dq_tail = back; /* it can never be the max again */
        } else {
            break;
        }
    }
    l->dq_val[l->dq_tail] = v;
    l->dq_idx[l->dq_tail] = idx;
    l->dq_tail = (l->dq_tail + 1u) % LIMITER_WINDOW;
}

static void dq_evict_before(limiter_t *l, uint32_t min_idx)
{
    while (l->dq_head != l->dq_tail && l->dq_idx[l->dq_head] < min_idx) {
        l->dq_head = (l->dq_head + 1u) % LIMITER_WINDOW;
    }
}

static int32_t dq_max(const limiter_t *l)
{
    if (l->dq_head == l->dq_tail) {
        return 0;
    }
    return l->dq_val[l->dq_head];
}

/* --------------------------------------------------------------------- */

void limiter_init(limiter_t *l)
{
    if (!l) {
        return;
    }
    memset(l, 0, sizeof(*l));
    /* Start at unity rather than fully open: the first moments of a take
     * should not be a surge while the gain finds its level. */
    l->gain_q16 = LIMITER_UNITY_Q16;
}

void limiter_process(limiter_t *l, const int16_t *in, int16_t *out,
                     uint32_t count)
{
    if (!l || !in || !out) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        int32_t x = in[i];
        uint32_t idx = l->n;

        int32_t mag = (x < 0) ? -x : x; /* -32768 needs the wider type */
        dq_push(l, mag, idx);

        /* The window spans [idx - LOOKAHEAD, idx], so it always contains
         * the sample leaving the delay line this iteration. That is what
         * makes the ceiling a guarantee. */
        uint32_t min_idx = (idx >= LIMITER_LOOKAHEAD)
                               ? (idx - LIMITER_LOOKAHEAD)
                               : 0u;
        dq_evict_before(l, min_idx);
        int32_t env = dq_max(l);

        int32_t target;
        if (env <= 0) {
            target = l->gain_q16; /* digital silence: hold */
        } else {
            int64_t t = ((int64_t)LIMITER_CEILING << 16) / env;
            if (t > LIMITER_MAX_GAIN_Q16) {
                t = LIMITER_MAX_GAIN_Q16;
            }
            target = (int32_t)t;
            if (env < LIMITER_GATE && target > l->gain_q16) {
                target = l->gain_q16; /* do not amplify a quiet room's hiss */
            }
        }

        if (target > l->gain_q16) {
            /* Rising: ease up, and never overshoot the target. */
            l->gain_q16 += (target - l->gain_q16) >> LIMITER_RELEASE_SHIFT;
            if (l->gain_q16 > target) {
                l->gain_q16 = target;
            }
        } else {
            /* Falling: immediately. The audio this affects is still inside
             * the delay line and quiet, so the step is not audible — and it
             * keeps gain <= ceiling/env, which is the whole guarantee. */
            l->gain_q16 = target;
        }

        int32_t delayed = l->delay[l->dpos];
        l->delay[l->dpos] = (int16_t)x;
        l->dpos = (l->dpos + 1u) % LIMITER_LOOKAHEAD;

        int32_t y = (int32_t)(((int64_t)delayed * l->gain_q16) >> 16);
        /* Saturation is a backstop only: if the invariant above holds this
         * never fires, and a test asserts that it does not. */
        if (y > 32767) {
            y = 32767;
        } else if (y < -32768) {
            y = -32768;
        }
        out[i] = (int16_t)y;

        int32_t ay = (y < 0) ? -y : y;
        if (ay > l->level_peak) {
            l->level_peak = ay;
        } else {
            /* Subtract at least one: a pure shift stalls once the value
             * drops below 2^SHIFT, which would leave the REC lamp glowing
             * faintly forever after a loud take. */
            int32_t decay = l->level_peak >> LIMITER_LEVEL_SHIFT;
            l->level_peak -= (decay > 0) ? decay : 1;
            if (l->level_peak < 0) {
                l->level_peak = 0;
            }
        }

        l->n++;
    }
}

int32_t limiter_gain_q16(const limiter_t *l)
{
    return l ? l->gain_q16 : LIMITER_UNITY_Q16;
}

uint8_t limiter_level(const limiter_t *l)
{
    if (!l) {
        return 0;
    }
    int32_t v = l->level_peak >> 7; /* 0..32767 -> 0..255 */
    if (v > 255) {
        v = 255;
    }
    return (uint8_t)v;
}
