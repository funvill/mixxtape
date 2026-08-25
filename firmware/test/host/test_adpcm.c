#include <stdlib.h>

#include "adpcm.h"
#include "test_util.h"

#define N 4096

/* Triangle wave — avoids depending on libm while still exercising the
 * step-size adaptation in both directions. */
static void make_triangle(int16_t *pcm, uint32_t n, int32_t amplitude,
                          uint32_t period)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t phase = i % period;
        int32_t half = (int32_t)(period / 2u);
        int32_t v = (phase < (uint32_t)half)
                        ? (int32_t)phase * amplitude / half
                        : (2 * half - (int32_t)phase) * amplitude / half;
        pcm[i] = (int16_t)(v - amplitude / 2);
    }
}

static void test_size_and_guards(void)
{
    adpcm_state_t st;
    int16_t pcm[16] = {0};
    uint8_t enc[16] = {0};

    adpcm_state_init(&st);
    CHECK_EQ(adpcm_encode(&st, pcm, 16u, enc), 8u); /* 4 bits per sample */

    adpcm_state_init(&st);
    CHECK_EQ(adpcm_encode(&st, pcm, 15u, enc), 0u); /* odd count refused */
    CHECK_EQ(adpcm_encode(&st, 0, 16u, enc), 0u);
    CHECK_EQ(adpcm_decode(&st, enc, 8u, 0), 0u);
}

static void test_round_trip_tracks_signal(void)
{
    int16_t *pcm = (int16_t *)malloc(N * sizeof(int16_t));
    int16_t *out = (int16_t *)malloc(N * sizeof(int16_t));
    uint8_t *enc = (uint8_t *)malloc(N / 2);
    adpcm_state_t st;

    make_triangle(pcm, N, 20000, 256u);

    adpcm_state_init(&st);
    CHECK_EQ(adpcm_encode(&st, pcm, N, enc), N / 2u);

    adpcm_state_init(&st);
    CHECK_EQ(adpcm_decode(&st, enc, N / 2u, out), (uint32_t)N);

    /* Skip the attack while the step size adapts, then require the decoded
     * signal to track the input closely. */
    long long total = 0;
    int32_t worst = 0;
    for (uint32_t i = 64; i < N; i++) {
        int32_t err = (int32_t)out[i] - (int32_t)pcm[i];
        if (err < 0) {
            err = -err;
        }
        total += err;
        if (err > worst) {
            worst = err;
        }
    }
    int32_t mean = (int32_t)(total / (N - 64));
    printf("   mean abs error %d, worst %d (full scale 32768)\n", mean, worst);
    CHECK(mean < 400);
    CHECK(worst < 4000);

    free(pcm);
    free(out);
    free(enc);
}

static void test_silence_stays_silent(void)
{
    int16_t pcm[512] = {0};
    int16_t out[512];
    uint8_t enc[256];
    adpcm_state_t st;

    adpcm_state_init(&st);
    adpcm_encode(&st, pcm, 512u, enc);
    adpcm_state_init(&st);
    adpcm_decode(&st, enc, 256u, out);

    for (uint32_t i = 0; i < 512u; i++) {
        CHECK(out[i] < 16 && out[i] > -16);
    }
}

static void test_no_overflow_at_full_scale(void)
{
    /* Alternating extremes: the predictor must clamp, not wrap. */
    int16_t pcm[1024];
    int16_t out[1024];
    uint8_t enc[512];
    adpcm_state_t st;

    for (uint32_t i = 0; i < 1024u; i++) {
        pcm[i] = (i & 1u) ? 32767 : -32768;
    }
    adpcm_state_init(&st);
    adpcm_encode(&st, pcm, 1024u, enc);
    adpcm_state_init(&st);
    adpcm_decode(&st, enc, 512u, out);

    for (uint32_t i = 0; i < 1024u; i++) {
        CHECK(out[i] >= -32768 && out[i] <= 32767);
    }
}

static void test_encoder_is_deterministic(void)
{
    int16_t pcm[1024];
    uint8_t a[512], b[512];
    adpcm_state_t st;

    make_triangle(pcm, 1024u, 12000, 97u);

    adpcm_state_init(&st);
    adpcm_encode(&st, pcm, 1024u, a);
    adpcm_state_init(&st);
    adpcm_encode(&st, pcm, 1024u, b);
    CHECK_MEM(a, b, 512u);
}

TEST_MAIN_BEGIN()

RUN(test_size_and_guards);
RUN(test_round_trip_tracks_signal);
RUN(test_silence_stays_silent);
RUN(test_no_overflow_at_full_scale);
RUN(test_encoder_is_deterministic);

TEST_MAIN_END()
