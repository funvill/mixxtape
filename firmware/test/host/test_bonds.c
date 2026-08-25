/* Bond table: auto-reconnect has to survive the cable being pulled. */

#include <stdio.h>
#include <string.h>

#include "bonds.h"
#include "mock_flash.h"
#include "test_util.h"

#define BASE TAPE_SPARE_BASE

static void mac(uint8_t out[6], uint8_t last)
{
    out[0] = 0xAA;
    out[1] = 0xBB;
    out[2] = 0xCC;
    out[3] = 0xDD;
    out[4] = 0xEE;
    out[5] = last;
}

static void test_format_and_mount(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b), 0u);
    CHECK(bonds_get(&b, 0) == 0);

    bonds_t b2;
    CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b2), 0u);
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    mock_flash_destroy(f);
}

static void test_mount_blank_region(void)
{
    /* A virgin chip has no bond log; mount must format rather than fail. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    CHECK_EQ(bonds_mount(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b), 0u);
    mock_flash_destroy(f);
}

static void test_add_and_survive_reboot(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t a[6], c[6];
    mac(a, 1);
    mac(c, 2);

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_add(&b, a, "Kitchen speaker"), BONDS_OK);
    CHECK_EQ(bonds_add(&b, c, "Earbuds"), BONDS_OK);
    CHECK_EQ(bonds_count(&b), 2u);

    /* Most recently paired comes first: that is what power-up tries. */
    CHECK_EQ(memcmp(bonds_get(&b, 0)->bda, c, 6), 0);
    CHECK_EQ(strcmp(bonds_get(&b, 0)->name, "Earbuds"), 0);
    CHECK_EQ(memcmp(bonds_get(&b, 1)->bda, a, 6), 0);

    bonds_t b2;
    CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b2), 2u);
    CHECK_EQ(memcmp(bonds_get(&b2, 0)->bda, c, 6), 0);
    CHECK_EQ(strcmp(bonds_get(&b2, 0)->name, "Earbuds"), 0);
    CHECK_EQ(strcmp(bonds_get(&b2, 1)->name, "Kitchen speaker"), 0);

    mock_flash_destroy(f);
}

static void test_touch_reorders(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t m[3][6];
    for (int i = 0; i < 3; i++) {
        mac(m[i], (uint8_t)(i + 1));
    }

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    for (int i = 0; i < 3; i++) {
        char name[8];
        snprintf(name, sizeof(name), "s%d", i);
        CHECK_EQ(bonds_add(&b, m[i], name), BONDS_OK);
    }
    CHECK_EQ(memcmp(bonds_get(&b, 0)->bda, m[2], 6), 0);

    /* Using the oldest one again promotes it to the front. */
    CHECK_EQ(bonds_touch(&b, m[0]), BONDS_OK);
    CHECK_EQ(memcmp(bonds_get(&b, 0)->bda, m[0], 6), 0);

    bonds_t b2;
    CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);
    CHECK_EQ(memcmp(bonds_get(&b2, 0)->bda, m[0], 6), 0);

    /* Touching something unknown is an error, not a silent insert. */
    uint8_t stranger[6];
    mac(stranger, 99);
    CHECK_EQ(bonds_touch(&b, stranger), BONDS_ERR_ARG);

    mock_flash_destroy(f);
}

static void test_readding_refreshes_rather_than_duplicates(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t a[6], c[6];
    mac(a, 1);
    mac(c, 2);

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_add(&b, a, "Speaker"), BONDS_OK);
    CHECK_EQ(bonds_add(&b, c, "Earbuds"), BONDS_OK);
    CHECK_EQ(bonds_add(&b, a, "Speaker renamed"), BONDS_OK);

    CHECK_EQ(bonds_count(&b), 2u); /* not 3 */
    CHECK_EQ(memcmp(bonds_get(&b, 0)->bda, a, 6), 0);
    CHECK_EQ(strcmp(bonds_get(&b, 0)->name, "Speaker renamed"), 0);

    mock_flash_destroy(f);
}

static void test_evicts_least_recently_used_when_full(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t m[BONDS_MAX + 2u][6];
    for (uint32_t i = 0; i < BONDS_MAX + 2u; i++) {
        mac(m[i], (uint8_t)(i + 1u));
    }

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    for (uint32_t i = 0; i < BONDS_MAX; i++) {
        CHECK_EQ(bonds_add(&b, m[i], "x"), BONDS_OK);
    }
    CHECK_EQ(bonds_count(&b), BONDS_MAX);

    /* Keep the oldest one alive by using it, then overflow the table. */
    CHECK_EQ(bonds_touch(&b, m[0]), BONDS_OK);
    CHECK_EQ(bonds_add(&b, m[BONDS_MAX], "new"), BONDS_OK);

    CHECK_EQ(bonds_count(&b), BONDS_MAX);
    CHECK(bonds_find(&b, m[0]) >= 0);  /* recently used: kept */
    CHECK_EQ(bonds_find(&b, m[1]), -1); /* now the stalest: dropped */
    CHECK(bonds_find(&b, m[BONDS_MAX]) >= 0);

    mock_flash_destroy(f);
}

static void test_forget(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t a[6], c[6];
    mac(a, 1);
    mac(c, 2);

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_add(&b, a, "a"), BONDS_OK);
    CHECK_EQ(bonds_add(&b, c, "c"), BONDS_OK);

    CHECK_EQ(bonds_forget(&b, a), BONDS_OK);
    CHECK_EQ(bonds_count(&b), 1u);
    CHECK_EQ(bonds_find(&b, a), -1);
    CHECK_EQ(bonds_forget(&b, a), BONDS_ERR_ARG);

    CHECK_EQ(bonds_forget_all(&b), BONDS_OK);
    CHECK_EQ(bonds_count(&b), 0u);

    bonds_t b2;
    CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b2), 0u);

    mock_flash_destroy(f);
}

static void test_log_rollover(void)
{
    /* Every pairing and every reconnect writes a record; over a tape's life
     * that is far more than one sector holds. */
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t a[6];
    mac(a, 7);

    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_add(&b, a, "speaker"), BONDS_OK);

    uint32_t per_sector = TAPE_SECTOR_SIZE / BONDS_RECORD_SZ;
    for (uint32_t i = 0; i < per_sector * 3u; i++) {
        CHECK_EQ(bonds_touch(&b, a), BONDS_OK);
    }

    bonds_t b2;
    CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_count(&b2), 1u);
    CHECK_EQ(strcmp(bonds_get(&b2, 0)->name, "speaker"), 0);
    CHECK_EQ(b2.generation, b.generation);
    CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

    mock_flash_destroy(f);
}

static void test_power_yank_sweep(void)
{
    /* Pulling the cable while a bond is being written must never leave the
     * table unreadable. Losing the newest bond is acceptable — you re-pair;
     * losing all of them, or mounting garbage, is not. */
    uint8_t known[6], fresh[6];
    mac(known, 1);
    mac(fresh, 2);

    int total_ops = 0;
    {
        flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
        bonds_t b;
        bonds_format(&b, f, BASE);
        bonds_add(&b, known, "known");
        mock_flash_fail_at(f, -1);
        bonds_add(&b, fresh, "fresh");
        total_ops = (int)mock_flash_ctx(f)->op_count;
        mock_flash_destroy(f);
    }
    printf("   sweeping %d flash operations\n", total_ops);

    int before = g_failures;
    for (int op = 0; op < total_ops; op++) {
        flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
        bonds_t b;
        CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
        CHECK_EQ(bonds_add(&b, known, "known"), BONDS_OK);

        mock_flash_fail_at(f, op);
        bonds_add(&b, fresh, "fresh"); /* may die partway */

        mock_flash_repower(f);
        bonds_t b2;
        CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);

        /* The established bond must still be there and intact. */
        int idx = bonds_find(&b2, known);
        CHECK(idx >= 0);
        if (idx >= 0) {
            CHECK_EQ(strcmp(bonds_get(&b2, (uint32_t)idx)->name, "known"), 0);
        }
        /* The new one either landed completely or not at all. */
        int nidx = bonds_find(&b2, fresh);
        if (nidx >= 0) {
            CHECK_EQ(strcmp(bonds_get(&b2, (uint32_t)nidx)->name, "fresh"), 0);
        }
        CHECK(bonds_count(&b2) >= 1u && bonds_count(&b2) <= 2u);
        CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

        /* And the table is still usable afterwards. */
        uint8_t another[6];
        mac(another, 3);
        CHECK_EQ(bonds_add(&b2, another, "another"), BONDS_OK);

        mock_flash_destroy(f);
        if (g_failures != before) {
            printf("   (first failure while tearing operation %d)\n", op);
            break;
        }
    }
}

static void test_power_yank_during_rollover(void)
{
    /* A bond commit is a single page program, so the plain sweep above has
     * only one tear point. The dangerous moment is the sector rollover:
     * the log erases the *other* sector before writing into it, and a yank
     * there could plausibly take the whole table with it. */
    uint8_t known[6], fresh[6];
    mac(known, 1);
    mac(fresh, 2);
    uint32_t per_sector = TAPE_SECTOR_SIZE / BONDS_RECORD_SZ;

    int before = g_failures;
    for (int op = 0; op < 4; op++) {
        flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
        bonds_t b;
        CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
        CHECK_EQ(bonds_add(&b, known, "known"), BONDS_OK);

        /* Fill the current sector right up to its last record. */
        while (b.offset + BONDS_RECORD_SZ <= TAPE_SECTOR_SIZE) {
            CHECK_EQ(bonds_touch(&b, known), BONDS_OK);
        }
        CHECK(b.generation >= per_sector - 1u);

        /* The next commit must roll over: erase, then program. */
        mock_flash_fail_at(f, op);
        bonds_add(&b, fresh, "fresh");

        mock_flash_repower(f);
        bonds_t b2;
        CHECK_EQ(bonds_mount(&b2, f, BASE), BONDS_OK);

        /* Whatever happened, the established bond is still there. */
        int idx = bonds_find(&b2, known);
        CHECK(idx >= 0);
        if (idx >= 0) {
            CHECK_EQ(strcmp(bonds_get(&b2, (uint32_t)idx)->name, "known"), 0);
        }
        int nidx = bonds_find(&b2, fresh);
        if (nidx >= 0) {
            CHECK_EQ(strcmp(bonds_get(&b2, (uint32_t)nidx)->name, "fresh"), 0);
        }
        CHECK_EQ(mock_flash_ctx(f)->dirty_programs, 0);

        /* And the log keeps working across further rollovers. */
        for (uint32_t i = 0; i < per_sector + 2u; i++) {
            CHECK_EQ(bonds_touch(&b2, known), BONDS_OK);
        }
        bonds_t b3;
        CHECK_EQ(bonds_mount(&b3, f, BASE), BONDS_OK);
        CHECK(bonds_find(&b3, known) >= 0);

        mock_flash_destroy(f);
        if (g_failures != before) {
            printf("   (first failure while tearing rollover op %d)\n", op);
            break;
        }
    }
}

static void test_guards(void)
{
    flash_hal_t *f = mock_flash_create(TAPE_FLASH_SIZE);
    bonds_t b;
    uint8_t a[6];
    mac(a, 1);

    CHECK_EQ(bonds_format(0, f, BASE), BONDS_ERR_ARG);
    CHECK_EQ(bonds_format(&b, 0, BASE), BONDS_ERR_ARG);
    CHECK_EQ(bonds_mount(0, f, BASE), BONDS_ERR_ARG);
    CHECK_EQ(bonds_format(&b, f, BASE), BONDS_OK);
    CHECK_EQ(bonds_add(&b, 0, "x"), BONDS_ERR_ARG);
    CHECK_EQ(bonds_find(0, a), -1);
    CHECK_EQ(bonds_count(0), 0u);
    CHECK(bonds_get(0, 0) == 0);
    CHECK(bonds_get(&b, 99u) == 0);

    /* An overlong name is truncated, not written past its field. */
    CHECK_EQ(bonds_add(&b, a, "a name far longer than the field allows"),
             BONDS_OK);
    CHECK(strlen(bonds_get(&b, 0)->name) < BONDS_NAME_LEN);

    mock_flash_destroy(f);
}

TEST_MAIN_BEGIN()

RUN(test_format_and_mount);
RUN(test_mount_blank_region);
RUN(test_add_and_survive_reboot);
RUN(test_touch_reorders);
RUN(test_readding_refreshes_rather_than_duplicates);
RUN(test_evicts_least_recently_used_when_full);
RUN(test_forget);
RUN(test_log_rollover);
RUN(test_guards);
RUN(test_power_yank_sweep);
RUN(test_power_yank_during_rollover);

TEST_MAIN_END()
