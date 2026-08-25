/* IMA/DVI ADPCM, 4 bits per sample, mono.
 *
 * Not used by the default one-pass record path (docs/storage-budget.md
 * Option D), but kept implemented and tested: it is what the two-pass
 * pipeline needs if Steven reinstates it, and it is the obvious codec for
 * any future scratch/preview use.
 */
#ifndef ADPCM_H
#define ADPCM_H

#include <stdint.h>

typedef struct {
    int32_t predictor; /* current predicted sample  */
    int16_t index;     /* index into the step table */
} adpcm_state_t;

void adpcm_state_init(adpcm_state_t *st);

/* Encodes `count` 16-bit samples into `count/2` bytes (two nibbles/byte,
 * low nibble first). `count` must be even. Returns bytes written. */
uint32_t adpcm_encode(adpcm_state_t *st, const int16_t *pcm, uint32_t count,
                      uint8_t *out);

/* Decodes `bytes` ADPCM bytes into `bytes*2` samples. Returns samples written. */
uint32_t adpcm_decode(adpcm_state_t *st, const uint8_t *in, uint32_t bytes,
                      int16_t *pcm);

#endif /* ADPCM_H */
