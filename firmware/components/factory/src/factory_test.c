#include "factory_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *ft_result_name(ft_result_t r)
{
    switch (r) {
    case FT_PASS:    return "PASS";
    case FT_FAIL:    return "FAIL";
    case FT_SKIP:    return "SKIP";
    case FT_PENDING: return "PEND";
    default:         return "?";
    }
}

int ft_init(ft_suite_t *s, const ft_step_def_t *steps, uint32_t count,
            bool interactive_enabled)
{
    if (!s || !steps || count == 0 || count > FT_MAX_STEPS) {
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->steps = steps;
    s->count = count;
    s->interactive_enabled = interactive_enabled;
    return 0;
}

bool ft_run(ft_suite_t *s, void *user, uint32_t (*now_ms)(void))
{
    if (!s || !s->steps) {
        return false;
    }
    s->passed = s->failed = s->skipped = 0;

    for (uint32_t i = 0; i < s->count; i++) {
        const ft_step_def_t *def = &s->steps[i];
        ft_step_result_t *r = &s->results[i];

        memset(r, 0, sizeof(*r));

        if (def->interactive && !s->interactive_enabled) {
            r->result = FT_SKIP;
            snprintf(r->detail, FT_DETAIL_LEN, "unattended run");
            s->skipped++;
            continue;
        }
        if (!def->probe) {
            r->result = FT_SKIP;
            snprintf(r->detail, FT_DETAIL_LEN, "no probe");
            s->skipped++;
            continue;
        }

        uint32_t t0 = now_ms ? now_ms() : 0u;
        int32_t measured = 0;
        char detail[FT_DETAIL_LEN];
        detail[0] = '\0';

        int rc = def->probe(user, &measured, detail);

        r->duration_ms = now_ms ? (now_ms() - t0) : 0u;
        r->measured = measured;
        r->has_measured = true;
        memcpy(r->detail, detail, FT_DETAIL_LEN - 1u);
        r->detail[FT_DETAIL_LEN - 1u] = '\0';

        if (rc == 0) {
            r->result = FT_PASS;
            s->passed++;
        } else if (rc > 0) {
            r->result = FT_SKIP;
            s->skipped++;
        } else {
            /* Deliberately no early exit: one pass should surface every
             * fault on the board, not just the first. */
            r->result = FT_FAIL;
            s->failed++;
        }
    }

    s->ran = true;
    return ft_verdict(s);
}

bool ft_verdict(const ft_suite_t *s)
{
    return s && s->ran && s->failed == 0u;
}

/* Appends to a bounded buffer, tracking the cursor. Returns false once the
 * buffer is full so callers stop trying. */
static bool append(char *out, uint32_t len, uint32_t *pos, const char *fmt, ...)
{
    if (*pos >= len) {
        return false;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + *pos, len - *pos, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return false;
    }
    if ((uint32_t)n >= len - *pos) {
        *pos = len - 1u; /* truncated */
        return false;
    }
    *pos += (uint32_t)n;
    return true;
}

uint32_t ft_format_machine(const ft_suite_t *s, char *out, uint32_t len)
{
    if (!s || !out || len == 0) {
        return 0;
    }
    out[0] = '\0';
    uint32_t pos = 0;

    append(out, len, &pos, "FT|1");

    for (uint32_t i = 0; i < s->count; i++) {
        const ft_step_def_t *def = &s->steps[i];
        const ft_step_result_t *r = &s->results[i];

        append(out, len, &pos, "|%s=%s", def->name, ft_result_name(r->result));

        if (r->result != FT_SKIP && r->has_measured) {
            if (def->hex) {
                append(out, len, &pos, ":0x%X", (unsigned)r->measured);
            } else {
                append(out, len, &pos, ":%ld", (long)r->measured);
            }
        }
    }
    append(out, len, &pos, "|VERDICT=%s", ft_verdict(s) ? "PASS" : "FAIL");
    return pos;
}

uint32_t ft_format_human(const ft_suite_t *s, char *out, uint32_t len)
{
    if (!s || !out || len == 0) {
        return 0;
    }
    out[0] = '\0';
    uint32_t pos = 0;

    for (uint32_t i = 0; i < s->count; i++) {
        const ft_step_def_t *def = &s->steps[i];
        const ft_step_result_t *r = &s->results[i];

        append(out, len, &pos, "  %-10s %-4s", def->name,
               ft_result_name(r->result));

        if (r->result != FT_SKIP && r->has_measured) {
            if (def->hex) {
                append(out, len, &pos, " 0x%-10X", (unsigned)r->measured);
            } else {
                append(out, len, &pos, " %-12ld", (long)r->measured);
            }
        } else {
            append(out, len, &pos, " %-13s", "");
        }
        if (r->detail[0]) {
            append(out, len, &pos, " %s", r->detail);
        }
        append(out, len, &pos, "\n");
    }
    append(out, len, &pos, "  VERDICT: %s  (%u passed, %u failed, %u skipped)\n",
           ft_verdict(s) ? "PASS" : "FAIL", (unsigned)s->passed,
           (unsigned)s->failed, (unsigned)s->skipped);
    return pos;
}
