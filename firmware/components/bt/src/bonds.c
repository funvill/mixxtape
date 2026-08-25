#include "bonds.h"

#include <string.h>

#include "tape_store.h" /* tape_crc32 */

#define BONDS_MAGIC 0x324E4F42u /* "BON2" */

/* Record layout (256 B, little-endian, CRC over 0..251):
 *   0    magic          u32
 *   4    generation     u32
 *   8    count          u8
 *   9    reserved       3 B
 *   12   use_counter    u32
 *   16   entries        8 x 30 B
 *   252  crc            u32
 */
#define F_MAGIC   0u
#define F_GEN     4u
#define F_COUNT   8u
#define F_USECTR  12u
#define F_ENTRIES 16u
#define F_CRC     252u
#define ENTRY_SZ  30u

static void put_u32(uint8_t *buf, uint32_t off, uint32_t v)
{
    buf[off + 0] = (uint8_t)(v & 0xFFu);
    buf[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
    buf[off + 2] = (uint8_t)((v >> 16) & 0xFFu);
    buf[off + 3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get_u32(const uint8_t *buf, uint32_t off)
{
    return (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
           ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
}

static uint32_t sector_addr(const bonds_t *b, uint32_t sector)
{
    return b->base + sector * TAPE_SECTOR_SIZE;
}

static int program_span(const flash_hal_t *f, uint32_t addr,
                        const uint8_t *src, uint32_t len)
{
    while (len) {
        uint32_t page_end = (addr / f->page_size + 1u) * f->page_size;
        uint32_t chunk = page_end - addr;
        if (chunk > len) {
            chunk = len;
        }
        if (f->program(f, addr, src, chunk) != FLASH_OK) {
            return BONDS_ERR_IO;
        }
        addr += chunk;
        src += chunk;
        len -= chunk;
    }
    return BONDS_OK;
}

static void serialize(const bonds_t *b, uint32_t generation, uint8_t *rec)
{
    memset(rec, 0, BONDS_RECORD_SZ);
    put_u32(rec, F_MAGIC, BONDS_MAGIC);
    put_u32(rec, F_GEN, generation);
    rec[F_COUNT] = (uint8_t)b->count;
    put_u32(rec, F_USECTR, b->use_counter);

    for (uint32_t i = 0; i < b->count && i < BONDS_MAX; i++) {
        uint8_t *e = rec + F_ENTRIES + i * ENTRY_SZ;
        memcpy(e, b->list[i].bda, 6);
        put_u32(e, 6u, b->list[i].last_used);
        memcpy(e + 10u, b->list[i].name, BONDS_NAME_LEN);
    }
    put_u32(rec, F_CRC, tape_crc32(rec, F_CRC));
}

static bool deserialize(const uint8_t *rec, bonds_t *out, uint32_t *gen)
{
    if (get_u32(rec, F_MAGIC) != BONDS_MAGIC) {
        return false;
    }
    if (get_u32(rec, F_CRC) != tape_crc32(rec, F_CRC)) {
        return false;
    }
    uint32_t count = rec[F_COUNT];
    if (count > BONDS_MAX) {
        return false;
    }
    *gen = get_u32(rec, F_GEN);
    out->count = count;
    out->use_counter = get_u32(rec, F_USECTR);

    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = rec + F_ENTRIES + i * ENTRY_SZ;
        memcpy(out->list[i].bda, e, 6);
        out->list[i].last_used = get_u32(e, 6u);
        memcpy(out->list[i].name, e + 10u, BONDS_NAME_LEN);
        out->list[i].name[BONDS_NAME_LEN - 1u] = '\0';
    }
    return true;
}

static int commit(bonds_t *b)
{
    const flash_hal_t *f = b->flash;

    if (b->offset + BONDS_RECORD_SZ > TAPE_SECTOR_SIZE) {
        uint32_t next = b->sector ^ 1u;
        if (f->erase_sector(f, sector_addr(b, next)) != FLASH_OK) {
            return BONDS_ERR_IO;
        }
        b->sector = next;
        b->offset = 0;
    }
    uint8_t rec[BONDS_RECORD_SZ];
    serialize(b, b->generation + 1u, rec);

    int rc = program_span(f, sector_addr(b, b->sector) + b->offset, rec,
                          BONDS_RECORD_SZ);
    if (rc != BONDS_OK) {
        return rc;
    }
    b->generation++;
    b->offset += BONDS_RECORD_SZ;
    return BONDS_OK;
}

/* Keeps the table sorted most-recently-used first. */
static void sort_by_recency(bonds_t *b)
{
    for (uint32_t i = 1; i < b->count; i++) {
        bond_t key = b->list[i];
        uint32_t j = i;
        while (j > 0 && b->list[j - 1u].last_used < key.last_used) {
            b->list[j] = b->list[j - 1u];
            j--;
        }
        b->list[j] = key;
    }
}

int bonds_format(bonds_t *b, const flash_hal_t *flash, uint32_t base)
{
    if (!b || !flash) {
        return BONDS_ERR_ARG;
    }
    memset(b, 0, sizeof(*b));
    b->flash = flash;
    b->base = base;

    for (uint32_t s = 0; s < BONDS_SECTORS; s++) {
        if (flash->erase_sector(flash, base + s * TAPE_SECTOR_SIZE) !=
            FLASH_OK) {
            return BONDS_ERR_IO;
        }
    }
    b->sector = 0;
    b->offset = 0;
    b->generation = 0;
    return commit(b);
}

int bonds_mount(bonds_t *b, const flash_hal_t *flash, uint32_t base)
{
    if (!b || !flash) {
        return BONDS_ERR_ARG;
    }
    memset(b, 0, sizeof(*b));
    b->flash = flash;
    b->base = base;

    bonds_t best;
    uint32_t best_gen = 0, best_sector = 0, best_offset = 0;
    bool found = false;

    for (uint32_t s = 0; s < BONDS_SECTORS; s++) {
        for (uint32_t off = 0; off + BONDS_RECORD_SZ <= TAPE_SECTOR_SIZE;
             off += BONDS_RECORD_SZ) {
            uint8_t rec[BONDS_RECORD_SZ];
            if (flash->read(flash, base + s * TAPE_SECTOR_SIZE + off, rec,
                            BONDS_RECORD_SZ) != FLASH_OK) {
                return BONDS_ERR_IO;
            }
            bonds_t candidate;
            memset(&candidate, 0, sizeof(candidate));
            uint32_t gen = 0;
            if (!deserialize(rec, &candidate, &gen)) {
                continue;
            }
            if (!found || gen > best_gen) {
                best = candidate;
                best_gen = gen;
                best_sector = s;
                best_offset = off;
                found = true;
            }
        }
    }

    if (!found) {
        return bonds_format(b, flash, base);
    }
    memcpy(b->list, best.list, sizeof(b->list));
    b->count = best.count;
    b->use_counter = best.use_counter;
    b->generation = best_gen;
    b->sector = best_sector;
    b->offset = best_offset + BONDS_RECORD_SZ;
    sort_by_recency(b);
    return BONDS_OK;
}

int bonds_find(const bonds_t *b, const uint8_t bda[6])
{
    if (!b || !bda) {
        return -1;
    }
    for (uint32_t i = 0; i < b->count; i++) {
        if (memcmp(b->list[i].bda, bda, 6) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int bonds_add(bonds_t *b, const uint8_t bda[6], const char *name)
{
    if (!b || !b->flash || !bda) {
        return BONDS_ERR_ARG;
    }
    int existing = bonds_find(b, bda);

    if (existing >= 0) {
        b->list[existing].last_used = ++b->use_counter;
        if (name) {
            strncpy(b->list[existing].name, name, BONDS_NAME_LEN - 1u);
            b->list[existing].name[BONDS_NAME_LEN - 1u] = '\0';
        }
        sort_by_recency(b);
        return commit(b);
    }

    if (b->count >= BONDS_MAX) {
        /* Full: the table is sorted, so the last entry is the one that has
         * gone longest without being used. Dropping it is what lets a
         * well-used tape keep working forever without any UI. */
        b->count = BONDS_MAX - 1u;
    }
    bond_t *slot = &b->list[b->count];
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->bda, bda, 6);
    slot->last_used = ++b->use_counter;
    if (name) {
        strncpy(slot->name, name, BONDS_NAME_LEN - 1u);
        slot->name[BONDS_NAME_LEN - 1u] = '\0';
    }
    b->count++;
    sort_by_recency(b);
    return commit(b);
}

int bonds_touch(bonds_t *b, const uint8_t bda[6])
{
    if (!b || !b->flash || !bda) {
        return BONDS_ERR_ARG;
    }
    int i = bonds_find(b, bda);
    if (i < 0) {
        return BONDS_ERR_ARG;
    }
    b->list[i].last_used = ++b->use_counter;
    sort_by_recency(b);
    return commit(b);
}

int bonds_forget(bonds_t *b, const uint8_t bda[6])
{
    if (!b || !b->flash || !bda) {
        return BONDS_ERR_ARG;
    }
    int i = bonds_find(b, bda);
    if (i < 0) {
        return BONDS_ERR_ARG;
    }
    for (uint32_t j = (uint32_t)i; j + 1u < b->count; j++) {
        b->list[j] = b->list[j + 1u];
    }
    b->count--;
    memset(&b->list[b->count], 0, sizeof(b->list[0]));
    return commit(b);
}

int bonds_forget_all(bonds_t *b)
{
    if (!b || !b->flash) {
        return BONDS_ERR_ARG;
    }
    memset(b->list, 0, sizeof(b->list));
    b->count = 0;
    return commit(b);
}

const bond_t *bonds_get(const bonds_t *b, uint32_t index)
{
    if (!b || index >= b->count) {
        return 0;
    }
    return &b->list[index];
}

uint32_t bonds_count(const bonds_t *b)
{
    return b ? b->count : 0u;
}
