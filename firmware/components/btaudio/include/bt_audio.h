/* A2DP source: discovery, connection, and streaming audio to a sink.
 *
 * The tape is the *source* — it behaves like a phone, pushing audio to
 * earbuds or a speaker. Selection policy lives in pairing.h and bond
 * storage in bonds.h; both are plain logic and host-tested. This file is
 * the Bluedroid plumbing that feeds them.
 *
 * TWO THINGS THAT BITE, both handled in bt_audio.c:
 *
 *  - The data callback is handed a buffer to fill with **PCM**, not SBC.
 *    Bluedroid does the SBC encoding. This is why the tape stores ADPCM
 *    rather than SBC (docs/storage-budget.md).
 *
 *  - That PCM is **interleaved stereo**, even for a mono recording. Every
 *    mono sample has to be written twice. Miss this and playback comes out
 *    an octave low and half-speed, which reads like a sample-rate bug and
 *    sends you looking in the wrong place.
 *
 * NOT YET RUN ON HARDWARE — there are no boards.
 */
#ifndef BT_AUDIO_H
#define BT_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "bonds.h"
#include "pairing.h"

typedef enum {
    BT_AUDIO_IDLE = 0,
    BT_AUDIO_DISCOVERING,
    BT_AUDIO_CONNECTING,
    BT_AUDIO_CONNECTED,
    BT_AUDIO_STREAMING,
} bt_audio_state_t;

/* Fills `dst` with up to `samples` of MONO 16-bit PCM and returns how many
 * it produced. Called from the Bluetooth task, so it must not block, take
 * a mutex held by a slow task, or touch flash directly — read from a ring
 * that another task keeps topped up. Returning fewer samples than asked
 * is fine; the rest is padded with silence. */
typedef uint32_t (*bt_audio_pull_fn)(int16_t *dst, uint32_t samples,
                                     void *user);

int bt_audio_init(bonds_t *bonds);

/* Registers the source of audio. Safe to call before or after init. */
void bt_audio_set_source(bt_audio_pull_fn fn, void *user);

/* Inquiry, then connect to the nearest audio sink above the RSSI
 * threshold. This is the MODE-held-2-seconds gesture. */
int bt_audio_start_pairing(int8_t rssi_threshold);

/* Try known bonds, most recently used first. This is what runs on
 * power-up. */
int bt_audio_reconnect_last(void);

int bt_audio_disconnect(void);

/* Begins/stops pushing audio to a connected sink. */
int bt_audio_start_stream(void);
int bt_audio_stop_stream(void);

bt_audio_state_t bt_audio_state(void);
bool bt_audio_is_connected(void);

/* True once we have been failing to reconnect long enough that the user
 * deserves the "the speaker is busy" pulse rather than a hopeful blink. */
bool bt_audio_sink_busy(uint32_t now_ms);

/* Name of the connected sink, or "" — for logs and the factory test. */
const char *bt_audio_peer_name(void);

/* How many audio-class devices the last inquiry turned up. The factory
 * test uses this as its proof that the radio transmits and receives. */
uint32_t bt_audio_last_scan_count(void);

void bt_audio_deinit(void);

#endif /* BT_AUDIO_H */
