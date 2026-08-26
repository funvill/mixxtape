/* Pin map for the rev 0.1 board.
 *
 * Authoritative source: docs/firmware-plan.md sec.2, which supersedes the
 * GPIO table in docs/cassette-recorder-agent-brief.md sec.5 (USB-C is now
 * power-only, programming is via the J5 pogo pads, and SENSOR_VP/VN sense
 * the USB-C CC lines).
 *
 * ESP32 constraints honoured here:
 *   GPIO 6-11  are the module's own flash — never used
 *   GPIO 12    is the MTDI strap and must be low at boot — left unused
 *   GPIO 34/35/36/39 are input-only
 */
#ifndef BOARD_H
#define BOARD_H

#include "tape_layout.h" /* TAPE_SAMPLE_RATE */

/* --- PDM microphone (MSM261DHT006, mono, L/R tied low) --------------
 * PDM rather than I2S: two wires instead of three, so the old bit-clock
 * pin (GPIO26) is now free. The ESP32's I2S peripheral does PDM receive
 * in hardware, decimating to PCM on the way in, so nothing downstream
 * changes.
 *
 * Clock rate matters here. PDM clock = sample rate x downsample ratio:
 *   44100 x 64  = 2.8224 MHz  -> inside the mic's 1.1-4.8 MHz window
 *   44100 x 128 = 5.6448 MHz  -> ABOVE its 4.8 MHz maximum
 * so the downsample ratio must be 64. Picking 128 would run the part out
 * of spec and is the obvious mistake to make here.
 *
 * The part is TOP-ported: sound enters from the component side, and there
 * is deliberately no hole through the board beneath it. Keep its top face
 * clear of case ribs, labels and fingers.
 */
#define PIN_PDM_CLK  25 /* ESP32 -> mic, 2.8224 MHz */
#define PIN_PDM_DATA 33 /* mic -> ESP32            */

#define PDM_DOWNSAMPLE_RATIO 64
#define PDM_CLOCK_HZ         (TAPE_SAMPLE_RATE * PDM_DOWNSAMPLE_RATIO)

/* --- VSPI: audio flash (W25Q128) + optional microSD (DNP) ----------- */
#define PIN_SPI_SCK   18
#define PIN_SPI_MISO  19
#define PIN_SPI_MOSI  23
#define PIN_FLASH_CS   5
#define PIN_SD_CS     22 /* microSD is DNP on the default build         */
#define PIN_SD_DETECT 32 /* switch to GND; needs an internal pull-up    */

/* --- WS2812 chain (via SN74AHCT1G125 level shifter) ----------------- */
#define PIN_LED_DATA 27
#define LED_COUNT    29
#define LED_REEL_LEFT_FIRST   0  /* D1-D12  */
#define LED_REEL_LEFT_COUNT   12
#define LED_REEL_RIGHT_FIRST  12 /* D13-D24 */
#define LED_REEL_RIGHT_COUNT  12
#define LED_TRACK_FIRST       24 /* D25-D27 */
#define LED_TRACK_COUNT       3
#define LED_REC               27 /* D28 */
#define LED_BT                28 /* D29 */

/* Global brightness ceiling. All 29 LEDs at full white is ~1.7 A; the
 * default USB-C budget is 500 mA until CC sensing says otherwise. */
#define LED_BRIGHTNESS_USB_DEFAULT 40 /* /255, ~15%  */
#define LED_BRIGHTNESS_USB_HIGH    76 /* /255, ~30%  */

/* --- Buttons: active low, internal pull-ups -------------------------- */
#define PIN_BTN_REC   0  /* also the BOOT strap — safe as active-low     */
#define PIN_BTN_PLAY  4
#define PIN_BTN_TRACK 16
#define PIN_BTN_MODE  17

/* --- Write-protect tab: external 10k pull-down, intact = high -------- */
#define PIN_TAB_SENSE 21

/* --- USB-C CC sense (ADC1) ------------------------------------------ */
#define PIN_CC1_SENSE 36 /* SENSOR_VP */
#define PIN_CC2_SENSE 39 /* SENSOR_VN */

/* CC voltage thresholds in millivolts, at the divider we fit (Rd = 5.1k,
 * sensed through 10k). Source advertises: ~0.41 V default (500 mA),
 * ~0.92 V (1.5 A), ~1.68 V (3 A). Thresholds sit mid-band. */
#define CC_MV_THRESHOLD_1A5 660
#define CC_MV_THRESHOLD_3A0 1230

/* --- UART to the programming jig (J5 pogo pads) ---------------------- */
#define PIN_UART_TX 1
#define PIN_UART_RX 3

/* --- Unpopulated (DNP) ---------------------------------------------- */
#define PIN_BATT_SENSE 34 /* divider not fitted  */
#define PIN_LINE_IN    35 /* jack not fitted     */

#endif /* BOARD_H */
