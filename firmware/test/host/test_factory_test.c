/* Factory harness tests, with mock probes standing in for the board. */

#include <stdio.h>
#include <string.h>

#include "factory_test.h"
#include "test_util.h"

/* --- mock probes ----------------------------------------------------- */

typedef struct {
    uint32_t calls;
    uint32_t clock;
} mock_ctx_t;

static mock_ctx_t g_ctx;
static uint32_t g_now;

static uint32_t fake_now(void)
{
    g_now += 7u; /* every call advances, so durations are non-zero */
    return g_now;
}

static int probe_pass(void *user, int32_t *measured, char *detail)
{
    mock_ctx_t *c = (mock_ctx_t *)user;
    if (c) {
        c->calls++;
    }
    *measured = 1234;
    snprintf(detail, FT_DETAIL_LEN, "ok");
    return 0;
}

static int probe_fail(void *user, int32_t *measured, char *detail)
{
    mock_ctx_t *c = (mock_ctx_t *)user;
    if (c) {
        c->calls++;
    }
    *measured = 0;
    snprintf(detail, FT_DETAIL_LEN, "no response");
    return -1;
}

static int probe_skip(void *user, int32_t *measured, char *detail)
{
    mock_ctx_t *c = (mock_ctx_t *)user;
    if (c) {
        c->calls++;
    }
    (void)measured;
    snprintf(detail, FT_DETAIL_LEN, "not fitted");
    return 1;
}

static int probe_hexid(void *user, int32_t *measured, char *detail)
{
    (void)user;
    (void)detail;
    *measured = 0xEF4018; /* W25Q128 JEDEC id */
    return 0;
}

static int probe_long_detail(void *user, int32_t *measured, char *detail)
{
    (void)user;
    *measured = 1;
    /* Deliberately overlong: a probe must not be able to scribble past the
     * detail buffer. */
    memset(detail, 'x', FT_DETAIL_LEN - 1u);
    detail[FT_DETAIL_LEN - 1u] = '\0';
    return 0;
}

/* --- tests ----------------------------------------------------------- */

static void test_init_guards(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {{"a", false, false, probe_pass}};

    CHECK_EQ(ft_init(0, steps, 1u, true), -1);
    CHECK_EQ(ft_init(&s, 0, 1u, true), -1);
    CHECK_EQ(ft_init(&s, steps, 0u, true), -1);
    CHECK_EQ(ft_init(&s, steps, FT_MAX_STEPS + 1u, true), -1);
    CHECK_EQ(ft_init(&s, steps, 1u, true), 0);

    /* A suite that has not run is not a pass. */
    CHECK(!ft_verdict(&s));
}

static void test_all_pass(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"flash", false, true, probe_hexid},
        {"mic", false, false, probe_pass},
        {"tab", false, false, probe_pass},
    };
    memset(&g_ctx, 0, sizeof(g_ctx));

    CHECK_EQ(ft_init(&s, steps, 3u, true), 0);
    CHECK(ft_run(&s, &g_ctx, fake_now));
    CHECK(ft_verdict(&s));
    CHECK_EQ(s.passed, 3u);
    CHECK_EQ(s.failed, 0u);
    CHECK_EQ(s.skipped, 0u);
    CHECK_EQ(g_ctx.calls, 2u); /* probe_hexid ignores the context */

    for (uint32_t i = 0; i < 3u; i++) {
        CHECK_EQ(s.results[i].result, FT_PASS);
        CHECK(s.results[i].duration_ms > 0u);
    }
}

static void test_failure_does_not_stop_the_run(void)
{
    /* The whole point: a board with three faults must report three faults
     * on one pass, not one fault per reflash. */
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"a", false, false, probe_fail},
        {"b", false, false, probe_fail},
        {"c", false, false, probe_pass},
        {"d", false, false, probe_fail},
    };
    memset(&g_ctx, 0, sizeof(g_ctx));

    CHECK_EQ(ft_init(&s, steps, 4u, true), 0);
    CHECK(!ft_run(&s, &g_ctx, fake_now));
    CHECK(!ft_verdict(&s));

    CHECK_EQ(g_ctx.calls, 4u); /* every probe was still called */
    CHECK_EQ(s.failed, 3u);
    CHECK_EQ(s.passed, 1u);
    CHECK_EQ(s.results[0].result, FT_FAIL);
    CHECK_EQ(s.results[2].result, FT_PASS);
    CHECK_EQ(s.results[3].result, FT_FAIL);
    CHECK_EQ(strcmp(s.results[0].detail, "no response"), 0);
}

static void test_skips_do_not_fail_the_board(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"present", false, false, probe_pass},
        {"dnp", false, false, probe_skip},
        {"missing", false, false, 0}, /* no probe wired up yet */
    };
    CHECK_EQ(ft_init(&s, steps, 3u, true), 0);
    CHECK(ft_run(&s, &g_ctx, 0));
    CHECK(ft_verdict(&s));
    CHECK_EQ(s.skipped, 2u);
    CHECK_EQ(s.results[1].result, FT_SKIP);
    CHECK_EQ(s.results[2].result, FT_SKIP);
    CHECK_EQ(strcmp(s.results[2].detail, "no probe"), 0);

    /* No clock supplied: durations report zero rather than garbage. */
    CHECK_EQ(s.results[0].duration_ms, 0u);
}

static void test_unattended_run_skips_interactive_steps(void)
{
    /* On the line, nobody should be waiting for someone to press buttons
     * unless they asked for that. */
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"flash", false, false, probe_pass},
        {"buttons", true, false, probe_fail}, /* would fail if it ran */
        {"leds", true, false, probe_fail},
    };
    memset(&g_ctx, 0, sizeof(g_ctx));

    CHECK_EQ(ft_init(&s, steps, 3u, false), 0);
    CHECK(ft_run(&s, &g_ctx, 0));
    CHECK(ft_verdict(&s)); /* skipped, not failed */
    CHECK_EQ(g_ctx.calls, 1u);
    CHECK_EQ(s.skipped, 2u);
    CHECK_EQ(strcmp(s.results[1].detail, "unattended run"), 0);

    /* With an operator present the same steps do run. */
    memset(&g_ctx, 0, sizeof(g_ctx));
    CHECK_EQ(ft_init(&s, steps, 3u, true), 0);
    CHECK(!ft_run(&s, &g_ctx, 0));
    CHECK_EQ(g_ctx.calls, 3u);
    CHECK_EQ(s.failed, 2u);
}

static void test_machine_report(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"flash_id", false, true, probe_hexid},
        {"mic", false, false, probe_pass},
        {"dnp", false, false, probe_skip},
        {"tab", false, false, probe_fail},
    };
    char out[FT_REPORT_LEN];

    CHECK_EQ(ft_init(&s, steps, 4u, true), 0);
    ft_run(&s, &g_ctx, 0);

    uint32_t n = ft_format_machine(&s, out, sizeof(out));
    printf("   %s\n", out);
    CHECK(n > 0u);
    CHECK_EQ(strlen(out), n);
    CHECK_EQ(strcmp(out,
                    "FT|1|flash_id=PASS:0xEF4018|mic=PASS:1234|dnp=SKIP"
                    "|tab=FAIL:0|VERDICT=FAIL"),
             0);
}

static void test_human_report(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"flash_id", false, true, probe_hexid},
        {"mic", false, false, probe_pass},
    };
    char out[FT_REPORT_LEN];

    CHECK_EQ(ft_init(&s, steps, 2u, true), 0);
    ft_run(&s, &g_ctx, fake_now);

    uint32_t n = ft_format_human(&s, out, sizeof(out));
    printf("%s", out);
    CHECK(n > 0u);
    CHECK(strstr(out, "flash_id") != 0);
    CHECK(strstr(out, "0xEF4018") != 0);
    CHECK(strstr(out, "VERDICT: PASS") != 0);
    CHECK(strstr(out, "2 passed, 0 failed, 0 skipped") != 0);
}

static void test_reports_never_overflow(void)
{
    /* The jig's buffer is fixed and the report is built from operator- and
     * probe-supplied strings, so truncation must be safe at every size. */
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"aaaaaaaaaaaaaaa", false, false, probe_long_detail},
        {"bbbbbbbbbbbbbbb", false, false, probe_long_detail},
        {"ccccccccccccccc", false, false, probe_long_detail},
        {"ddddddddddddddd", false, false, probe_long_detail},
    };
    CHECK_EQ(ft_init(&s, steps, 4u, true), 0);
    ft_run(&s, &g_ctx, 0);

    for (uint32_t cap = 1u; cap <= 200u; cap++) {
        char buf[256];
        memset(buf, 0x7E, sizeof(buf));

        uint32_t n = ft_format_machine(&s, buf, cap);
        CHECK(n < cap);
        CHECK_EQ(strlen(buf), n);
        CHECK_EQ((unsigned char)buf[cap], 0x7Eu); /* nothing written past cap */

        memset(buf, 0x7E, sizeof(buf));
        n = ft_format_human(&s, buf, cap);
        CHECK(n < cap);
        CHECK_EQ(strlen(buf), n);
        CHECK_EQ((unsigned char)buf[cap], 0x7Eu);
    }
}

static void test_report_guards(void)
{
    ft_suite_t s;
    const ft_step_def_t steps[] = {{"a", false, false, probe_pass}};
    char out[64];

    CHECK_EQ(ft_init(&s, steps, 1u, true), 0);
    ft_run(&s, &g_ctx, 0);

    CHECK_EQ(ft_format_machine(0, out, sizeof(out)), 0u);
    CHECK_EQ(ft_format_machine(&s, 0, sizeof(out)), 0u);
    CHECK_EQ(ft_format_machine(&s, out, 0u), 0u);
    CHECK_EQ(ft_format_human(0, out, sizeof(out)), 0u);
    CHECK(!ft_run(0, 0, 0));
    CHECK(!ft_verdict(0));
    CHECK_EQ(strcmp(ft_result_name(FT_PENDING), "PEND"), 0);
}

static void test_rerun_resets_counters(void)
{
    /* The jig runs board after board; a stale count would mislabel one. */
    ft_suite_t s;
    const ft_step_def_t steps[] = {
        {"a", false, false, probe_fail},
        {"b", false, false, probe_pass},
    };
    CHECK_EQ(ft_init(&s, steps, 2u, true), 0);
    ft_run(&s, &g_ctx, 0);
    CHECK_EQ(s.failed, 1u);
    CHECK_EQ(s.passed, 1u);

    ft_run(&s, &g_ctx, 0);
    CHECK_EQ(s.failed, 1u); /* not 2 */
    CHECK_EQ(s.passed, 1u);
}

TEST_MAIN_BEGIN()

RUN(test_init_guards);
RUN(test_all_pass);
RUN(test_failure_does_not_stop_the_run);
RUN(test_skips_do_not_fail_the_board);
RUN(test_unattended_run_skips_interactive_steps);
RUN(test_machine_report);
RUN(test_human_report);
RUN(test_reports_never_overflow);
RUN(test_report_guards);
RUN(test_rerun_resets_counters);

TEST_MAIN_END()
