/* Limiter tests. The load-bearing one is simple: the output never exceeds
 * the ceiling, for any input, ever. Everything the record path does
 * downstream assumes that. */

#include <stdlib.h>
#include <string.h>

#include "limiter.h"
#include "test_util.h"

#define N 44100u /* one second */

static int16_t *g_in;
static int16_t *g_out;

static void alloc_buffers(void)
{
    if (!g_in) {
        g_in = (int16_t *)malloc(N * sizeof(int16_t));
        g_out = (int16_t *)malloc(N * sizeof(int16_t));
    }
    memset(g_in, 0, N * sizeof(int16_t));
    memset(g_out, 0, N * sizeof(int16_t));
}

static void fill_square(int16_t *b, uint32_t n, int32_t amp, uint32_t period)
{
    for (uint32_t i = 0; i < n; i++) {
        b[i] = (int16_t)(((i % period) < period / 2u) ? amp : -amp);
    }
}

static void fill_triangle(int16_t *b, uint32_t n, int32_t amp, uint32_t period)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t phase = i % period;
        int32_t half = (int32_t)(period / 2u);
        int32_t v = (phase < (uint32_t)half)
                        ? (int32_t)phase * amp / half
                        : (2 * half - (int32_t)phase) * amp / half;
        b[i] = (int16_t)(v - amp / 2);
    }
}

static int32_t peak_of(const int16_t *b, uint32_t n)
{
    int32_t p = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t a = (b[i] < 0) ? -b[i] : b[i];
        if (a > p) {
            p = a;
        }
    }
    return p;
}

/* --- the invariant ---------------------------------------------------- */

static void check_never_exceeds_ceiling(const char *what, const int16_t *in,
                                        uint32_t n)
{
    limiter_t l;
    limiter_init(&l);
    limiter_process(&l, in, g_out, n);

    int32_t peak = peak_of(g_out, n);
    if (peak > LIMITER_CEILING) {
        printf("   %s: peak %ld exceeds ceiling %d\n", what, (long)peak,
               LIMITER_CEILING);
    }
    CHECK(peak <= LIMITER_CEILING);
}

static void test_ceiling_holds_for_every_signal(void)
{
    alloc_buffers();

    fill_square(g_in, N, 32767, 100u);
    check_never_exceeds_ceiling("full-scale square", g_in, N);

    fill_square(g_in, N, 32767, 3u); /* near-Nyquist worst case */
    check_never_exceeds_ceiling("fast square", g_in, N);

    fill_triangle(g_in, N, 32767, 441u);
    check_never_exceeds_ceiling("full-scale triangle", g_in, N);

    /* Alternating extremes, including the asymmetric -32768. */
    for (uint32_t i = 0; i < N; i++) {
        g_in[i] = (i & 1u) ? 32767 : -32768;
    }
    check_never_exceeds_ceiling("rail-to-rail", g_in, N);

    /* Pseudo-random noise at full scale. */
    uint32_t x = 12345u;
    for (uint32_t i = 0; i < N; i++) {
        x = x * 1664525u + 1013904223u;
        g_in[i] = (int16_t)(x >> 16);
    }
    check_never_exceeds_ceiling("noise", g_in, N);

    /* A decaying signal: the case a simple decaying envelope detector gets
     * wrong, because the envelope falls faster than the delayed audio. */
    int32_t amp = 32767;
    for (uint32_t i = 0; i < N; i++) {
        if ((i % 64u) == 0u && amp > 100) {
            amp = (amp * 15) / 16;
        }
        g_in[i] = (int16_t)(((i % 8u) < 4u) ? amp : -amp);
    }
    check_never_exceeds_ceiling("decaying tone", g_in, N);
}

static void test_lookahead_catches_a_sudden_transient(void)
{
    /* The whole reason for the delay line: silence, then a full-scale
     * burst with no warning. Without look-ahead the first samples of the
     * burst leave at full scale before any gain reduction lands. */
    alloc_buffers();
    uint32_t onset = 10000u;
    for (uint32_t i = onset; i < N; i++) {
        g_in[i] = (int16_t)(((i % 100u) < 50u) ? 32767 : -32767);
    }

    limiter_t l;
    limiter_init(&l);
    limiter_process(&l, g_in, g_out, N);

    /* Nothing anywhere exceeds the ceiling — including the very first
     * cycle of the burst, which is where a naive limiter clips. */
    CHECK(peak_of(g_out, N) <= LIMITER_CEILING);

    uint32_t out_onset = onset + LIMITER_LOOKAHEAD;
    int32_t first_cycle = peak_of(g_out + out_onset, 200u);
    printf("   first burst cycle peaks at %ld (ceiling %d, input 32767)\n",
           (long)first_cycle, LIMITER_CEILING);
    CHECK(first_cycle <= LIMITER_CEILING);
    CHECK(first_cycle > LIMITER_CEILING / 2); /* attenuated, not squashed */
}

static void test_output_is_delayed_by_the_lookahead(void)
{
    /* An impulse should reappear exactly LIMITER_LOOKAHEAD samples later:
     * the record path has to account for this when it stops. */
    alloc_buffers();
    uint32_t at = 1000u;
    g_in[at] = 20000;

    limiter_t l;
    limiter_init(&l);
    limiter_process(&l, g_in, g_out, N);

    uint32_t found = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (g_out[i] != 0) {
            found = i;
            break;
        }
    }
    CHECK_EQ(found, at + LIMITER_LOOKAHEAD);

    /* And everything before it is exactly silent, not nearly silent. */
    for (uint32_t i = 0; i < at + LIMITER_LOOKAHEAD; i++) {
        CHECK_EQ(g_out[i], 0);
    }
}

static void test_quiet_material_is_lifted(void)
{
    /* A room recorded on a MEMS mic is quiet. If the limiter only ever
     * attenuated, ambient takes would be unlistenable. */
    alloc_buffers();
    fill_triangle(g_in, N, 2000, 441u);

    limiter_t l;
    limiter_init(&l);
    limiter_process(&l, g_in, g_out, N);

    /* Measure the settled second half, past the gain ramp. */
    int32_t in_peak = peak_of(g_in + N / 2u, N / 2u);
    int32_t out_peak = peak_of(g_out + N / 2u, N / 2u);
    printf("   quiet input peak %ld -> output peak %ld (gain %.2fx)\n",
           (long)in_peak, (long)out_peak, (double)out_peak / (double)in_peak);

    CHECK(out_peak > in_peak * 2);
    CHECK(out_peak <= LIMITER_CEILING);
    CHECK(limiter_gain_q16(&l) <= LIMITER_MAX_GAIN_Q16);
}

static void test_gain_is_bounded(void)
{
    /* Even given a long stretch of very quiet audio, gain must stop at the
     * configured maximum rather than chasing the ceiling forever. */
    alloc_buffers();
    fill_triangle(g_in, N, 300, 441u);

    limiter_t l;
    limiter_init(&l);
    for (int pass = 0; pass < 20; pass++) {
        limiter_process(&l, g_in, g_out, N);
        CHECK(limiter_gain_q16(&l) <= LIMITER_MAX_GAIN_Q16);
    }
    CHECK(peak_of(g_out, N) <= LIMITER_CEILING);
}

static void test_gate_does_not_amplify_silence(void)
{
    /* Near-silence must not be pumped up into audible hiss between words. */
    alloc_buffers();
    for (uint32_t i = 0; i < N; i++) {
        g_in[i] = (int16_t)((i % 7u) ? 0 : 20); /* well under the gate */
    }

    limiter_t l;
    limiter_init(&l);
    int32_t start_gain = limiter_gain_q16(&l);
    for (int pass = 0; pass < 10; pass++) {
        limiter_process(&l, g_in, g_out, N);
    }
    printf("   gain over near-silence: %ld -> %ld (Q16)\n", (long)start_gain,
           (long)limiter_gain_q16(&l));
    CHECK_EQ(limiter_gain_q16(&l), start_gain);
}

static void test_gain_recovers_smoothly_after_a_transient(void)
{
    /* A door slam should duck the recording and then let it back up
     * gradually. A jump back to full gain is the pumping artefact that
     * makes cheap recorders sound cheap. */
    alloc_buffers();
    fill_triangle(g_in, N, 6000, 441u);
    for (uint32_t i = 20000u; i < 20500u; i++) {
        g_in[i] = (int16_t)((i & 1u) ? 32767 : -32767);
    }

    limiter_t l;
    limiter_init(&l);

    int32_t prev = limiter_gain_q16(&l);
    int32_t worst_rise = 0;
    for (uint32_t i = 0; i < N; i++) {
        limiter_process(&l, g_in + i, g_out + i, 1u);
        int32_t g = limiter_gain_q16(&l);
        int32_t rise = g - prev;
        if (rise > worst_rise) {
            worst_rise = rise;
        }
        prev = g;
    }
    /* Upward movement is limited by the release shift; anything bigger
     * would be a step, not a recovery. */
    printf("   largest single-sample gain rise: %ld Q16\n", (long)worst_rise);
    CHECK(worst_rise <= LIMITER_MAX_GAIN_Q16 >> LIMITER_RELEASE_SHIFT);
    CHECK(peak_of(g_out, N) <= LIMITER_CEILING);
}

static void test_in_place_matches_out_of_place(void)
{
    /* The record path wants to limit a DMA buffer without copying it. */
    alloc_buffers();
    fill_triangle(g_in, N, 25000, 331u);

    int16_t *copy = (int16_t *)malloc(N * sizeof(int16_t));
    memcpy(copy, g_in, N * sizeof(int16_t));

    limiter_t a, b;
    limiter_init(&a);
    limiter_init(&b);
    limiter_process(&a, g_in, g_out, N);
    limiter_process(&b, copy, copy, N); /* aliased */

    CHECK_MEM(copy, g_out, N * sizeof(int16_t));
    free(copy);
}

static void test_chunking_does_not_change_the_result(void)
{
    /* The I2S driver hands over whatever size buffer it feels like; the
     * result must not depend on that. */
    alloc_buffers();
    fill_triangle(g_in, N, 30000, 257u);

    limiter_t whole;
    limiter_init(&whole);
    limiter_process(&whole, g_in, g_out, N);

    int16_t *chunked = (int16_t *)malloc(N * sizeof(int16_t));
    limiter_t part;
    limiter_init(&part);
    uint32_t pos = 0;
    uint32_t sizes[5] = {1u, 63u, 441u, 1024u, 7u};
    int k = 0;
    while (pos < N) {
        uint32_t n = sizes[k++ % 5];
        if (pos + n > N) {
            n = N - pos;
        }
        limiter_process(&part, g_in + pos, chunked + pos, n);
        pos += n;
    }
    CHECK_MEM(chunked, g_out, N * sizeof(int16_t));
    free(chunked);
}

static void test_level_meter(void)
{
    alloc_buffers();

    limiter_t l;
    limiter_init(&l);
    CHECK_EQ(limiter_level(&l), 0);

    fill_triangle(g_in, N, 30000, 441u);
    limiter_process(&l, g_in, g_out, N);
    uint8_t loud = limiter_level(&l);
    printf("   level on a loud take: %u/255\n", loud);
    CHECK(loud > 128u);

    /* It falls back once the room goes quiet, so the REC lamp reads as a
     * meter rather than a latch. */
    memset(g_in, 0, N * sizeof(int16_t));
    limiter_process(&l, g_in, g_out, N);
    CHECK_EQ(limiter_level(&l), 0);
}

static void test_guards(void)
{
    limiter_t l;
    limiter_init(&l);
    limiter_init(0);
    limiter_process(0, g_in, g_out, 8u);
    limiter_process(&l, 0, g_out, 8u);
    limiter_process(&l, g_in, 0, 8u);
    limiter_process(&l, g_in, g_out, 0u);
    CHECK_EQ(limiter_gain_q16(0), LIMITER_UNITY_Q16);
    CHECK_EQ(limiter_level(0), 0);
}

TEST_MAIN_BEGIN()

RUN(test_ceiling_holds_for_every_signal);
RUN(test_lookahead_catches_a_sudden_transient);
RUN(test_output_is_delayed_by_the_lookahead);
RUN(test_quiet_material_is_lifted);
RUN(test_gain_is_bounded);
RUN(test_gate_does_not_amplify_silence);
RUN(test_gain_recovers_smoothly_after_a_transient);
RUN(test_in_place_matches_out_of_place);
RUN(test_chunking_does_not_change_the_result);
RUN(test_level_meter);
RUN(test_guards);

TEST_MAIN_END()
