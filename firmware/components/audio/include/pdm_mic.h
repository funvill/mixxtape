/* PDM microphone capture (MSM261DHT006) via the ESP32's I2S peripheral.
 *
 * The mic is PDM, not I2S. The I2S peripheral receives PDM and decimates
 * to 16-bit PCM in hardware, so nothing downstream of this file knows or
 * cares — the limiter and the ADPCM encoder see ordinary samples.
 *
 * The one number that matters is the downsample ratio; see pdm_mic.c.
 *
 * NOT YET RUN ON HARDWARE — there are no boards.
 */
#ifndef PDM_MIC_H
#define PDM_MIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Starts the I2S channel in PDM RX mode at TAPE_SAMPLE_RATE, mono. */
int pdm_mic_init(void);

/* Begins/stops clocking the microphone. The mic draws 670 uA while
 * clocked and ~1 uA when the clock stops, so playback and idle leave it
 * off rather than running it for nothing. */
int pdm_mic_start(void);
int pdm_mic_stop(void);

/* Blocking read of up to `samples` mono 16-bit samples. Returns the number
 * actually read, or a negative error. */
int pdm_mic_read(int16_t *dst, size_t samples, uint32_t timeout_ms);

/* Discards whatever the mic produces for `ms`, so a take does not open
 * with the settling transient. See pdm_mic.c for why this is needed. */
int pdm_mic_discard(uint32_t ms);

/* RMS of a block, for the factory test's "is this microphone alive"
 * check and for a rough level reading during bring-up. */
uint32_t pdm_mic_rms(const int16_t *pcm, size_t samples);

void pdm_mic_deinit(void);

#endif /* PDM_MIC_H */
