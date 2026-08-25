/* On-flash layout of the tape. See docs/storage-budget.md for the
 * arithmetic behind every constant here.
 *
 * Stored format is block-framed IMA ADPCM, NOT SBC. ESP-IDF's A2DP source
 * accepts PCM only ("the input should be PCM data stream") and encodes SBC
 * inside Bluedroid; there is no API to hand it pre-encoded frames. Storing
 * SBC would therefore mean decoding SBC to PCM at playback purely so the
 * stack could re-encode it. ADPCM decode is a handful of table lookups per
 * sample, so playback stays as cheap as the locked "no DSP at playback"
 * decision intends.
 *
 * Layout (W25Q128JV, 16 MiB):
 *   0x000000     64 KiB   header  (sectors 0/1 = ping-pong record log)
 *   0x010000   5.125 MiB  slot 0
 *   0x530000   5.125 MiB  slot 1
 *   0xA50000   5.125 MiB  slot 2
 *   0xF70000    576 KiB   spare / reserved (bond table, wear relief)
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

/* --- Audio slots ----------------------------------------------------
 * 82 erase blocks each: the smallest whole-block size that holds a full
 * 4-minute take in ADPCM (4:01.8 actual). Three of them plus the header
 * leave 576 KiB spare.
 */
#define TAPE_SLOT_COUNT     3
#define TAPE_SLOT_BLOCKS    82u
#define TAPE_SLOT_SIZE      (TAPE_SLOT_BLOCKS * TAPE_BLOCK_SIZE) /* 5.125 MiB */
#define TAPE_SLOT_BASE      (TAPE_HDR_BASE + TAPE_HDR_REGION)
#define TAPE_SLOT_ADDR(n)   (TAPE_SLOT_BASE + (uint32_t)(n) * TAPE_SLOT_SIZE)

#define TAPE_SPARE_BASE     (TAPE_SLOT_BASE + TAPE_SLOT_COUNT * TAPE_SLOT_SIZE)

/* --- Audio format ---------------------------------------------------
 * 44.1 kHz matches what A2DP negotiates, so nothing ever resamples.
 */
#define TAPE_SAMPLE_RATE    44100u
#define TAPE_CHANNELS       1u /* mono: one microphone */

/* --- Air format (Bluedroid encodes this for us) ---------------------
 * Kept here because pairing must negotiate a configuration the sink
 * accepts; verify against real sinks in M6.
 */
#define TAPE_SBC_SUBBANDS   8u
#define TAPE_SBC_BLOCKS     16u
#define TAPE_SBC_BITPOOL    26u

#endif /* TAPE_LAYOUT_H */
