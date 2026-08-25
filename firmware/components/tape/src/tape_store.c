#include "tape_store.h"

#include <string.h>

#include "sbc_frame.h"

#define HDR_MAGIC 0x3158544Du /* "MTX1" little-endian */

/* Header record field offsets (64 bytes, little-endian, CRC over 0..59). */
#define F_MAGIC      0
#define F_GEN        4
#define F_STATE      8  /* 3 bytes */
#define F_ERASED    11
#define F_LEN       12  /* 3 x u32 */
#define F_DUR       24  /* 3 x u32 */
#define F_BITPOOL   36
#define F_BLOCKS    37
#define F_SUBBANDS  38
#define F_CHANNELS  39
#define F_CRC       60

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

uint32_t tape_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

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

static uint32_t hdr_sector_addr(uint32_t sector)
{
    return TAPE_HDR_BASE + sector * TAPE_SECTOR_SIZE;
}

/* Programs an arbitrary span, splitting at page boundaries. */
static int program_span(const flash_hal_t *f, uint32_t addr, const uint8_t *src,
                        uint32_t len)
{
    while (len) {
        uint32_t page_end = (addr / f->page_size + 1u) * f->page_size;
        uint32_t chunk = page_end - addr;
        if (chunk > len) {
            chunk = len;
        }
        int rc = f->program(f, addr, src, chunk);
        if (rc != FLASH_OK) {
            return TAPE_ERR_IO;
        }
        addr += chunk;
        src += chunk;
        len -= chunk;
    }
    return TAPE_OK;
}

/* ------------------------------------------------------------------ */
/* header records                                                      */
/* ------------------------------------------------------------------ */

static void serialize(const tape_store_t *ts, uint32_t generation, uint8_t *rec)
{
    memset(rec, 0, TAPE_HDR_RECORD_SZ);
    put_u32(rec, F_MAGIC, HDR_MAGIC);
    put_u32(rec, F_GEN, generation);

    uint8_t erased_mask = 0;
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        rec[F_STATE + i] = (uint8_t)ts->slots[i].state;
        put_u32(rec, F_LEN + 4u * (uint32_t)i, ts->slots[i].length);
        put_u32(rec, F_DUR + 4u * (uint32_t)i, ts->slots[i].duration_ms);
        if (ts->slots[i].erased) {
            erased_mask |= (uint8_t)(1u << i);
        }
    }
    rec[F_ERASED] = erased_mask;
    rec[F_BITPOOL] = (uint8_t)TAPE_SBC_BITPOOL;
    rec[F_BLOCKS] = (uint8_t)TAPE_SBC_BLOCKS;
    rec[F_SUBBANDS] = (uint8_t)TAPE_SBC_SUBBANDS;
    rec[F_CHANNELS] = (uint8_t)TAPE_SBC_CHANNELS;

    put_u32(rec, F_CRC, tape_crc32(rec, F_CRC));
}

static bool deserialize(const uint8_t *rec, tape_store_t *ts, uint32_t *gen)
{
    if (get_u32(rec, F_MAGIC) != HDR_MAGIC) {
        return false;
    }
    if (get_u32(rec, F_CRC) != tape_crc32(rec, F_CRC)) {
        return false;
    }
    *gen = get_u32(rec, F_GEN);
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        uint8_t st = rec[F_STATE + i];
        if (st > TAPE_SLOT_RECORDING) {
            return false;
        }
        ts->slots[i].state = (tape_slot_state_t)st;
        ts->slots[i].length = get_u32(rec, F_LEN + 4u * (uint32_t)i);
        ts->slots[i].duration_ms = get_u32(rec, F_DUR + 4u * (uint32_t)i);
        ts->slots[i].erased = (rec[F_ERASED] & (1u << i)) != 0u;
    }
    return true;
}

/* Appends the current state as a new header record. */
static int commit(tape_store_t *ts)
{
    const flash_hal_t *f = ts->flash;

    if (ts->hdr_offset + TAPE_HDR_RECORD_SZ > TAPE_SECTOR_SIZE) {
        /* Current log sector is full: switch to the other one. Erasing it
         * is safe — the newest record still lives in the sector we are
         * leaving until the new record below lands. */
        uint32_t next = ts->hdr_sector ^ 1u;
        if (f->erase_sector(f, hdr_sector_addr(next)) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
        ts->hdr_sector = next;
        ts->hdr_offset = 0;
    }

    uint8_t rec[TAPE_HDR_RECORD_SZ];
    serialize(ts, ts->generation + 1u, rec);

    int rc = program_span(f, hdr_sector_addr(ts->hdr_sector) + ts->hdr_offset,
                          rec, TAPE_HDR_RECORD_SZ);
    if (rc != TAPE_OK) {
        return rc;
    }
    ts->generation++;
    ts->hdr_offset += TAPE_HDR_RECORD_SZ;
    return TAPE_OK;
}

/* ------------------------------------------------------------------ */
/* slot data scanning                                                  */
/* ------------------------------------------------------------------ */

/* Finds the end of sequentially-written data in a slot: the offset of the
 * first fully-erased page.
 *
 * Deliberately a linear forward scan, not a binary search. A power loss
 * during the erase-ahead leaves a half-erased block whose tail still holds
 * the previous recording, so the slot is NOT monotonic (written prefix,
 * erased gap, stale tail) and a binary search could land in the stale tail
 * and resurrect old audio. Scanning forward stops at the erased gap, which
 * is exactly where the new recording ended.
 *
 * A page of real SBC cannot be all-0xFF (every frame starts with 0x9C), so
 * a fully-erased page is an unambiguous terminator. Costs ~5k reads on a
 * 5 MiB slot, but only ever runs on crash recovery.
 */
static int scan_data_end(tape_store_t *ts, int slot, uint32_t *out_end)
{
    const flash_hal_t *f = ts->flash;
    uint32_t base = TAPE_SLOT_ADDR(slot);
    uint8_t buf[1024];
    uint32_t chunk = (uint32_t)sizeof(buf);

    if (f->page_size > chunk) {
        return TAPE_ERR_ARG;
    }

    uint32_t first_erased = TAPE_SLOT_SIZE;
    for (uint32_t off = 0; off < TAPE_SLOT_SIZE; off += chunk) {
        uint32_t n = chunk;
        if (off + n > TAPE_SLOT_SIZE) {
            n = TAPE_SLOT_SIZE - off;
        }
        if (f->read(f, base + off, buf, n) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
        for (uint32_t p = 0; p + f->page_size <= n; p += f->page_size) {
            bool erased = true;
            for (uint32_t i = 0; i < f->page_size; i++) {
                if (buf[p + i] != 0xFFu) {
                    erased = false;
                    break;
                }
            }
            if (erased) {
                first_erased = off + p;
                break;
            }
        }
        if (first_erased != TAPE_SLOT_SIZE) {
            break;
        }
    }

    if (first_erased == 0) {
        *out_end = 0;
        return TAPE_OK;
    }
    if (first_erased == TAPE_SLOT_SIZE) {
        *out_end = TAPE_SLOT_SIZE; /* slot completely full */
        return TAPE_OK;
    }

    /* Walk back through the last written page to the final non-0xFF byte. */
    uint32_t last_page = first_erased - f->page_size;
    if (f->read(f, base + last_page, buf, f->page_size) != FLASH_OK) {
        return TAPE_ERR_IO;
    }
    uint32_t i = f->page_size;
    while (i > 0 && buf[i - 1u] == 0xFFu) {
        i--;
    }
    *out_end = last_page + i;
    return TAPE_OK;
}

/* Repairs a slot left in RECORDING state by a power loss: keeps every
 * complete SBC frame that made it to flash and discards the partial tail. */
static int repair_slot(tape_store_t *ts, int slot)
{
    uint32_t end = 0;
    int rc = scan_data_end(ts, slot, &end);
    if (rc != TAPE_OK) {
        return rc;
    }

    uint32_t frames = ts->frame_bytes ? end / ts->frame_bytes : 0;
    uint32_t length = frames * ts->frame_bytes;

    if (length == 0) {
        ts->slots[slot].state = TAPE_SLOT_EMPTY;
        ts->slots[slot].length = 0;
        ts->slots[slot].duration_ms = 0;
        ts->slots[slot].erased = false;
    } else {
        ts->slots[slot].state = TAPE_SLOT_VALID;
        ts->slots[slot].length = length;
        ts->slots[slot].duration_ms =
            sbc_bytes_to_ms(length, TAPE_SBC_SAMPLE_RATE, TAPE_SBC_SUBBANDS,
                            TAPE_SBC_BLOCKS, TAPE_SBC_CHANNELS,
                            TAPE_SBC_BITPOOL);
        ts->slots[slot].erased = false;
    }
    return commit(ts);
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

static void init_common(tape_store_t *ts, const flash_hal_t *flash)
{
    memset(ts, 0, sizeof(*ts));
    ts->flash = flash;
    ts->rec_slot = -1;
    ts->frame_bytes = sbc_frame_bytes(TAPE_SBC_SUBBANDS, TAPE_SBC_BLOCKS,
                                      TAPE_SBC_CHANNELS, TAPE_SBC_BITPOOL);
}

int tape_store_format(tape_store_t *ts, const flash_hal_t *flash)
{
    if (!ts || !flash) {
        return TAPE_ERR_ARG;
    }
    init_common(ts, flash);

    for (uint32_t s = 0; s < TAPE_HDR_SECTORS; s++) {
        if (flash->erase_sector(flash, hdr_sector_addr(s)) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
    }
    ts->hdr_sector = 0;
    ts->hdr_offset = 0;
    ts->generation = 0;
    return commit(ts);
}

int tape_store_mount(tape_store_t *ts, const flash_hal_t *flash)
{
    if (!ts || !flash) {
        return TAPE_ERR_ARG;
    }
    init_common(ts, flash);

    tape_store_t best;
    uint32_t best_gen = 0;
    bool found = false;
    uint32_t best_sector = 0, best_offset = 0;

    for (uint32_t s = 0; s < TAPE_HDR_SECTORS; s++) {
        for (uint32_t off = 0; off + TAPE_HDR_RECORD_SZ <= TAPE_SECTOR_SIZE;
             off += TAPE_HDR_RECORD_SZ) {
            uint8_t rec[TAPE_HDR_RECORD_SZ];
            if (flash->read(flash, hdr_sector_addr(s) + off, rec,
                            TAPE_HDR_RECORD_SZ) != FLASH_OK) {
                return TAPE_ERR_IO;
            }
            tape_store_t candidate;
            uint32_t gen = 0;
            if (!deserialize(rec, &candidate, &gen)) {
                /* Erased or torn record: the log for this sector ends here,
                 * but keep scanning in case a torn record sits mid-sector. */
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
        return tape_store_format(ts, flash);
    }

    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        ts->slots[i] = best.slots[i];
    }
    ts->generation = best_gen;
    ts->hdr_sector = best_sector;
    ts->hdr_offset = best_offset + TAPE_HDR_RECORD_SZ;

    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        if (ts->slots[i].state == TAPE_SLOT_RECORDING) {
            int rc = repair_slot(ts, i);
            if (rc != TAPE_OK) {
                return rc;
            }
        }
    }
    return TAPE_OK;
}

int tape_store_begin_record(tape_store_t *ts, int slot)
{
    if (!ts || slot < 0 || slot >= TAPE_SLOT_COUNT) {
        return TAPE_ERR_ARG;
    }
    if (ts->rec_slot >= 0) {
        return TAPE_ERR_STATE;
    }
    const flash_hal_t *f = ts->flash;

    bool was_erased = ts->slots[slot].erased;

    /* Mark the slot as in-flight *before* touching its data, so a yank at
     * any point from here on is recoverable by repair_slot(). */
    ts->slots[slot].state = TAPE_SLOT_RECORDING;
    ts->slots[slot].length = 0;
    ts->slots[slot].duration_ms = 0;
    ts->slots[slot].erased = false;
    int rc = commit(ts);
    if (rc != TAPE_OK) {
        return rc;
    }

    ts->rec_slot = slot;
    ts->rec_written = 0;

    if (was_erased) {
        ts->rec_erased_to = TAPE_SLOT_SIZE;
    } else {
        /* Erase only the first block; the rest happens ahead of the write
         * pointer so REC responds immediately. */
        if (f->erase_block(f, TAPE_SLOT_ADDR(slot)) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
        ts->rec_erased_to = TAPE_BLOCK_SIZE;
    }
    return TAPE_OK;
}

int tape_store_write(tape_store_t *ts, const void *data, uint32_t len)
{
    if (!ts || (!data && len)) {
        return TAPE_ERR_ARG;
    }
    if (ts->rec_slot < 0) {
        return TAPE_ERR_STATE;
    }
    if (ts->rec_written + len > TAPE_SLOT_SIZE) {
        return TAPE_ERR_FULL;
    }
    const flash_hal_t *f = ts->flash;

    /* Keep the erase frontier ahead of the data we are about to write. */
    while (ts->rec_written + len > ts->rec_erased_to) {
        if (ts->rec_erased_to >= TAPE_SLOT_SIZE) {
            return TAPE_ERR_FULL;
        }
        if (f->erase_block(f, TAPE_SLOT_ADDR(ts->rec_slot) +
                                  ts->rec_erased_to) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
        ts->rec_erased_to += TAPE_BLOCK_SIZE;
    }

    int rc = program_span(f, TAPE_SLOT_ADDR(ts->rec_slot) + ts->rec_written,
                          (const uint8_t *)data, len);
    if (rc != TAPE_OK) {
        return rc;
    }
    ts->rec_written += len;
    return TAPE_OK;
}

int tape_store_end_record(tape_store_t *ts)
{
    if (!ts) {
        return TAPE_ERR_ARG;
    }
    if (ts->rec_slot < 0) {
        return TAPE_ERR_STATE;
    }
    int slot = ts->rec_slot;

    /* Only whole frames are playable. */
    uint32_t frames = ts->frame_bytes ? ts->rec_written / ts->frame_bytes : 0;
    uint32_t length = frames * ts->frame_bytes;

    ts->slots[slot].length = length;
    ts->slots[slot].duration_ms =
        sbc_bytes_to_ms(length, TAPE_SBC_SAMPLE_RATE, TAPE_SBC_SUBBANDS,
                        TAPE_SBC_BLOCKS, TAPE_SBC_CHANNELS, TAPE_SBC_BITPOOL);
    ts->slots[slot].state = length ? TAPE_SLOT_VALID : TAPE_SLOT_EMPTY;
    ts->slots[slot].erased = false;

    ts->rec_slot = -1;
    ts->rec_written = 0;
    ts->rec_erased_to = 0;

    return commit(ts);
}

int tape_store_read(tape_store_t *ts, int slot, uint32_t offset, void *dst,
                    uint32_t len)
{
    if (!ts || slot < 0 || slot >= TAPE_SLOT_COUNT || !dst) {
        return TAPE_ERR_ARG;
    }
    if (ts->slots[slot].state != TAPE_SLOT_VALID) {
        return TAPE_ERR_STATE;
    }
    if (offset + len > ts->slots[slot].length) {
        return TAPE_ERR_ARG;
    }
    const flash_hal_t *f = ts->flash;
    if (f->read(f, TAPE_SLOT_ADDR(slot) + offset, dst, len) != FLASH_OK) {
        return TAPE_ERR_IO;
    }
    return TAPE_OK;
}

int tape_store_erase_slot(tape_store_t *ts, int slot)
{
    if (!ts || slot < 0 || slot >= TAPE_SLOT_COUNT) {
        return TAPE_ERR_ARG;
    }
    if (ts->rec_slot == slot) {
        return TAPE_ERR_STATE;
    }
    const flash_hal_t *f = ts->flash;
    uint32_t base = TAPE_SLOT_ADDR(slot);

    /* Disown the audio *before* erasing it. A yank partway through the erase
     * must not leave the header claiming a playable slot whose data is now
     * half 0xFF. */
    ts->slots[slot].state = TAPE_SLOT_EMPTY;
    ts->slots[slot].length = 0;
    ts->slots[slot].duration_ms = 0;
    ts->slots[slot].erased = false;
    int rc = commit(ts);
    if (rc != TAPE_OK) {
        return rc;
    }

    for (uint32_t off = 0; off < TAPE_SLOT_SIZE; off += TAPE_BLOCK_SIZE) {
        if (f->erase_block(f, base + off) != FLASH_OK) {
            return TAPE_ERR_IO;
        }
    }
    ts->slots[slot].erased = true;
    return commit(ts);
}

int tape_store_next_preerase(const tape_store_t *ts)
{
    if (!ts) {
        return -1;
    }
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        if (ts->slots[i].state == TAPE_SLOT_EMPTY && !ts->slots[i].erased) {
            return i;
        }
    }
    return -1;
}

bool tape_store_is_blank(const tape_store_t *ts)
{
    if (!ts) {
        return true;
    }
    for (int i = 0; i < TAPE_SLOT_COUNT; i++) {
        if (ts->slots[i].state == TAPE_SLOT_VALID) {
            return false;
        }
    }
    return true;
}
