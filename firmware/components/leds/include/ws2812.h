/* WS2812 chain driver on the RMT peripheral.
 *
 * 29 LEDs on one data line, driven through the SN74AHCT1G125 that lifts
 * the ESP32's 3.3 V to the 5 V rail the LEDs run on.
 *
 * RMT rather than bit-banging: the protocol encodes bits as pulse widths
 * a few hundred nanoseconds apart, and any interrupt landing mid-frame on
 * a bit-banged line shows up as a wrong colour. RMT clocks the whole frame
 * out of hardware, so a busy Bluetooth stack cannot corrupt it.
 *
 * NOT YET RUN ON HARDWARE — there are no boards.
 */
#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include <stdint.h>

#include "reels.h"

int ws2812_init(void);

/* Sends one frame: `count` pixels, in chain order from D1. Blocks until
 * the RMT transfer completes. */
int ws2812_show(const rgb_t *pixels, uint32_t count);

/* Drives every LED dark. Worth calling before sleep: unlit WS2812s still
 * draw about a milliamp each, and 29 of them is not nothing on a device
 * with no battery to spare. */
int ws2812_clear(void);

void ws2812_deinit(void);

#endif /* WS2812_H */
