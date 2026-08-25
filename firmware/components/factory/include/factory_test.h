/* Factory test harness.
 *
 * Twenty hand-assembled boards need to be checked quickly and identically,
 * by someone who is not the person who designed them. So:
 *
 *  - Every step runs, even after one fails. A board with a dead mic and a
 *    dead flash should say so in one pass, not one fault per reflash.
 *  - Each result carries a measured number, not just a verdict. "mic PASS"
 *    tells you nothing when board 14 comes back with intermittent audio;
 *    "mic PASS:1832" lets you spot the one board reading 40.
 *  - The report has a one-line machine form. Pipe the jig's serial output
 *    to a file and that file *is* the build record for the run.
 *
 * The sequencing and reporting here are portable and host-tested with mock
 * probes; the probes themselves are the only part that needs a board.
 */
#ifndef FACTORY_TEST_H
#define FACTORY_TEST_H

#include <stdbool.h>
#include <stdint.h>

#define FT_MAX_STEPS   16u
#define FT_DETAIL_LEN  48u
#define FT_REPORT_LEN  512u

typedef enum {
    FT_PENDING = 0,
    FT_PASS,
    FT_FAIL,
    FT_SKIP,
} ft_result_t;

/* A probe returns 0 to pass, a negative value to fail, or a positive value
 * to skip (a step that cannot apply — e.g. an interactive check on an
 * unattended run). It may write a measured value and a short detail
 * string; `detail` is always at least FT_DETAIL_LEN bytes. */
typedef int (*ft_probe_fn)(void *user, int32_t *measured, char *detail);

typedef struct {
    const char *name;   /* short, stable id — it ends up in the log      */
    bool interactive;   /* needs a human at the jig                      */
    bool hex;           /* report `measured` in hex (part IDs and such)  */
    ft_probe_fn probe;
} ft_step_def_t;

typedef struct {
    ft_result_t result;
    int32_t     measured;
    bool        has_measured;
    char        detail[FT_DETAIL_LEN];
    uint32_t    duration_ms;
} ft_step_result_t;

typedef struct {
    const ft_step_def_t *steps;
    uint32_t count;

    ft_step_result_t results[FT_MAX_STEPS];
    uint32_t passed, failed, skipped;
    bool     ran;

    /* Set false for an unattended run: interactive steps are skipped
     * rather than blocking the line on someone pressing buttons. */
    bool interactive_enabled;
} ft_suite_t;

/* Prepares a suite. `steps` must outlive it. */
int ft_init(ft_suite_t *s, const ft_step_def_t *steps, uint32_t count,
            bool interactive_enabled);

/* Runs every step in order, never stopping early. `now_ms` may be NULL, in
 * which case durations are reported as zero. Returns true if the board
 * passed. */
bool ft_run(ft_suite_t *s, void *user, uint32_t (*now_ms)(void));

/* True when no step failed. A suite that has not run is not a pass. */
bool ft_verdict(const ft_suite_t *s);

/* One greppable line, e.g.
 *   FT|1|flash_id=PASS:0xEF4018|mic=PASS:1832|tab=FAIL:0|VERDICT=FAIL
 * Returns the length written (excluding the terminator). */
uint32_t ft_format_machine(const ft_suite_t *s, char *out, uint32_t len);

/* Multi-line operator-facing table. Returns length written. */
uint32_t ft_format_human(const ft_suite_t *s, char *out, uint32_t len);

const char *ft_result_name(ft_result_t r);

#endif /* FACTORY_TEST_H */
