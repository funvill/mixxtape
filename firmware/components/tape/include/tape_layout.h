/* On-flash layout of the tape.  See docs/storage-budget.md for the
 * arithmetic behind every constant here.
 *
 * Layout (W25Q128JV, 16 MiB):
 *   0x000000   64 KiB   header  (sectors 0/1 = ping-pong record log)
 *   0x010000    5 MiB   slot 0
 *   0x510000    5 MiB   slot 1
 *   0xA10000    5 MiB   slot 2
 *   0xF10000  960 KiB   spare / reserved
 */
#ifndef TAPE_LAYOUT_H
#define TAPE_LAYOUT_H

#include <stdint.h>

#define TAPE_FLASH_SIZE     0x1000000u /* 16 MiB */
#define TAPE_PAGE_SIZE      256u
#define TAPE_SECTOR_SIZE    4096u
#define TAPE_BLOCK_SIZE     65536u

/* --- Header region ------------------------------------------------- */
#define TAPE_HDR_BASE       0x000000u
#define TAPE_HDR_REGION     TAPE_BLOCK_SIZE      /* 64 KiB reserved      */
#define TAPE_HDR_SECTORS    2u                   /* ping-pong log        */
#define TAPE_HDR_RECORD_SZ  64u                  /* 64 records / sector  */
#define TAPE_HDR_PER_SECTOR (TAPE_SECTOR_SIZE / TAPE_HDR_RECORD_SZ)

/* --- Audio slots ---------------------------------------------------- */
#define TAPE_SLOT_COUNT     3
#define TAPE_SLOT_SIZE      (5u * 1024u * 1024u) /* 5 MiB, 80 blocks     */
#define TAPE_SLOT_BASE      (TAPE_HDR_BASE + TAPE_HDR_REGION)
#define TAPE_SLOT_ADDR(n)   (TAPE_SLOT_BASE + (uint32_t)(n) * TAPE_SLOT_SIZE)

#define TAPE_SPARE_BASE     (TAPE_SLOT_BASE + TAPE_SLOT_COUNT * TAPE_SLOT_SIZE)

/* --- SBC stream configuration --------------------------------------
 * Must be a configuration real A2DP sinks will accept: we are the source
 * and propose it, but stored frames can never be re-encoded at playback.
 * Verify against real sinks in M6 before the 20-board run.
 */
#define TAPE_SBC_SAMPLE_RATE 44100u
#define TAPE_SBC_SUBBANDS    8u
#define TAPE_SBC_BLOCKS      16u
#define TAPE_SBC_CHANNELS    1u  /* mono */
#define TAPE_SBC_BITPOOL     26u /* 165 kbps; 4:13 max per slot */

#endif /* TAPE_LAYOUT_H */
