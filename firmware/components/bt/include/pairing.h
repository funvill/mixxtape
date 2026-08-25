/* Pairing policy: which device to connect to, and when to try again.
 *
 * The mixxtape is the A2DP *source*, so it chooses. Bluetooth sinks have no
 * source-selection UI and do not need one — but that means the choosing has
 * to be good enough that no phone, list or app is involved.
 *
 * Two filters do the work:
 *
 *  - Class of Device drops everything that is not an audio sink. Phones,
 *    laptops, keyboards and watches never appear as candidates, so the
 *    user is not picking from a list of their own hardware.
 *  - RSSI makes *proximity* the selection mechanism. You hold the board
 *    against the speaker; the nearest thing wins. That is the whole
 *    interaction.
 *
 * The threshold is a guess until it is measured on real boards (M6): radio
 * performance depends on the antenna keepout and the case, and -50 dBm at
 * roughly 30 cm is a starting point, not a fact.
 *
 * Pure logic, so the policy is host-tested even though the Bluetooth
 * plumbing underneath it cannot be.
 */
#ifndef PAIRING_H
#define PAIRING_H

#include <stdbool.h>
#include <stdint.h>

#define PAIRING_NAME_LEN 32u

/* Class of Device, 24 bits: minor class in 2..7, major in 8..12, service
 * class bits in 13..23. */
#define COD_MAJOR_MASK      0x1F00u
#define COD_MAJOR_SHIFT     8u
#define COD_MINOR_MASK      0x00FCu
#define COD_MINOR_SHIFT     2u
#define COD_SERVICE_AUDIO   0x200000u /* bit 21 */

#define COD_MAJOR_COMPUTER  0x01u
#define COD_MAJOR_PHONE     0x02u
#define COD_MAJOR_AUDIO     0x04u
#define COD_MAJOR_PERIPH    0x05u
#define COD_MAJOR_WEARABLE  0x07u

/* Audio/Video minor classes we are willing to stream to. */
#define COD_AV_HEADSET      0x01u
#define COD_AV_HANDSFREE    0x02u
#define COD_AV_LOUDSPEAKER  0x05u
#define COD_AV_HEADPHONES   0x06u
#define COD_AV_PORTABLE     0x07u
#define COD_AV_HIFI         0x0Au

/* Starting point; tune against real sinks in M6. */
#define PAIRING_RSSI_DEFAULT (-50)

typedef struct {
    uint8_t bda[6];
    uint32_t cod;
    int8_t   rssi;
    char     name[PAIRING_NAME_LEN];
} bt_device_t;

/* True if the Class of Device says this is something we can play into —
 * a headset, headphones, a speaker or a hi-fi. Microphones, set-top
 * boxes, camcorders, phones and computers are all rejected. */
bool pairing_is_audio_sink(uint32_t cod);

/* True if the device is close enough that the user clearly meant it. */
bool pairing_in_range(int8_t rssi, int8_t threshold);

/* Picks the strongest in-range audio sink from a discovery result, or -1
 * if nothing qualifies. Ties go to the earlier entry so the choice is
 * stable across repeated inquiries. */
int pairing_select(const bt_device_t *devices, uint32_t count,
                   int8_t rssi_threshold);

/* ---------------------------------------------------------------------
 * Reconnect backoff.
 *
 * On power-up the sink is usually busy reconnecting to its own last
 * source — normally the owner's phone — so the first few attempts are
 * expected to fail. Retry with backoff, and after a while say so honestly
 * rather than blinking hopefully forever.
 */

#define RECONNECT_FIRST_DELAY_MS   500u
#define RECONNECT_MAX_DELAY_MS     8000u
#define RECONNECT_BUSY_AFTER_MS    15000u

typedef struct {
    uint32_t attempts;
    uint32_t started_ms;
    uint32_t next_try_ms;
    bool     connected;
} reconnect_t;

void reconnect_init(reconnect_t *r, uint32_t now_ms);

/* True when it is time for another attempt. */
bool reconnect_should_try(const reconnect_t *r, uint32_t now_ms);

void reconnect_attempted(reconnect_t *r, uint32_t now_ms);
void reconnect_failed(reconnect_t *r, uint32_t now_ms);
void reconnect_succeeded(reconnect_t *r);

/* How long the next wait will be — doubles per failure, capped. */
uint32_t reconnect_delay_ms(const reconnect_t *r);

/* True once we have been failing long enough that the user deserves to be
 * told the speaker is busy (the slow red pulse in the LED language). */
bool reconnect_sink_busy(const reconnect_t *r, uint32_t now_ms);

#endif /* PAIRING_H */
