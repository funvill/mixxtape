#include "mock_flash.h"

#include <stdlib.h>
#include <string.h>

static mock_flash_t *ctx_of(const flash_hal_t *f)
{
    return (mock_flash_t *)f->ctx;
}

mock_flash_t *mock_flash_ctx(const flash_hal_t *f)
{
    return ctx_of(f);
}

/* Returns true if this op should tear. Consumes one op slot. */
static bool op_fails(mock_flash_t *m)
{
    if (!m->powered) {
        return true;
    }
    bool tear = (m->fail_at_op >= 0 && (int32_t)m->op_count == m->fail_at_op);
    m->op_count++;
    if (tear) {
        m->powered = false;
    }
    return tear;
}

static int mock_read(const flash_hal_t *f, uint32_t addr, void *dst,
                     uint32_t len)
{
    mock_flash_t *m = ctx_of(f);
    if (!m->powered) {
        return FLASH_ERR_POWER_LOSS;
    }
    if (addr + len > m->size) {
        return FLASH_ERR_ARG;
    }
    memcpy(dst, m->mem + addr, len);
    return FLASH_OK;
}

static int mock_program(const flash_hal_t *f, uint32_t addr, const void *src,
                        uint32_t len)
{
    mock_flash_t *m = ctx_of(f);
    const uint8_t *s = (const uint8_t *)src;

    if (addr + len > m->size) {
        return FLASH_ERR_ARG;
    }
    /* Must not straddle a page boundary. */
    if ((addr / f->page_size) != ((addr + len - 1u) / f->page_size)) {
        return FLASH_ERR_ARG;
    }
    for (uint32_t i = 0; i < len; i++) {
        if ((uint8_t)(~m->mem[addr + i] & s[i]) != 0u) {
            m->dirty_programs++; /* would need a 0 -> 1 transition */
            break;
        }
    }

    bool tear = op_fails(m);
    /* A torn program lands only part of the data — the classic NOR failure. */
    uint32_t applied = tear ? len / 2u : len;
    for (uint32_t i = 0; i < applied; i++) {
        m->mem[addr + i] &= s[i];
    }
    if (tear) {
        return FLASH_ERR_POWER_LOSS;
    }
    m->programs++;
    return FLASH_OK;
}

static int erase_span(const flash_hal_t *f, uint32_t addr, uint32_t unit)
{
    mock_flash_t *m = ctx_of(f);
    uint32_t base = (addr / unit) * unit;

    if (base + unit > m->size) {
        return FLASH_ERR_ARG;
    }
    bool tear = op_fails(m);
    /* A torn erase leaves part of the region erased and the rest stale. */
    uint32_t applied = tear ? unit / 2u : unit;
    memset(m->mem + base, 0xFF, applied);
    if (tear) {
        return FLASH_ERR_POWER_LOSS;
    }
    m->erases++;
    return FLASH_OK;
}

static int mock_erase_sector(const flash_hal_t *f, uint32_t addr)
{
    return erase_span(f, addr, f->sector_size);
}

static int mock_erase_block(const flash_hal_t *f, uint32_t addr)
{
    return erase_span(f, addr, f->block_size);
}

flash_hal_t *mock_flash_create(uint32_t size)
{
    flash_hal_t *f = (flash_hal_t *)calloc(1, sizeof(flash_hal_t));
    mock_flash_t *m = (mock_flash_t *)calloc(1, sizeof(mock_flash_t));
    if (!f || !m) {
        free(f);
        free(m);
        return NULL;
    }
    m->mem = (uint8_t *)malloc(size);
    if (!m->mem) {
        free(f);
        free(m);
        return NULL;
    }
    memset(m->mem, 0xFF, size);
    m->size = size;
    m->fail_at_op = -1;
    m->powered = true;

    f->size = size;
    f->page_size = 256u;
    f->sector_size = 4096u;
    f->block_size = 65536u;
    f->read = mock_read;
    f->program = mock_program;
    f->erase_sector = mock_erase_sector;
    f->erase_block = mock_erase_block;
    f->ctx = m;
    return f;
}

void mock_flash_destroy(flash_hal_t *f)
{
    if (!f) {
        return;
    }
    mock_flash_t *m = ctx_of(f);
    if (m) {
        free(m->mem);
        free(m);
    }
    free(f);
}

void mock_flash_fail_at(flash_hal_t *f, int32_t op_index)
{
    mock_flash_t *m = ctx_of(f);
    m->fail_at_op = op_index;
    m->op_count = 0;
}

void mock_flash_repower(flash_hal_t *f)
{
    mock_flash_t *m = ctx_of(f);
    m->powered = true;
    m->fail_at_op = -1;
    m->op_count = 0;
}
