/* Reads the USB-C source's current advertisement from the CC pins.
 *
 * A USB-C source states how much current it can supply by pulling CC up
 * through a resistor; with our 5.1 k pull-downs that appears as a voltage:
 *
 *     ~0.41 V   default        (500 mA)
 *     ~0.92 V   1.5 A capable
 *     ~1.68 V   3.0 A capable
 *
 * Only one CC line is live — the other sits near ground, and which is
 * which depends on which way round the cable was plugged in — so both are
 * read and the higher wins.
 *
 * This is the only reason the board can run its LEDs brighter on a decent
 * charger without risking a brown-out on a laptop port. Extra resistors
 * cannot buy more current: the source advertises, the sink measures.
 *
 * NOT YET RUN ON HARDWARE — there are no boards.
 */
#ifndef CC_SENSE_H
#define CC_SENSE_H

#include <stdint.h>

typedef enum {
    USB_BUDGET_DEFAULT = 0,  /* 500 mA — assume this unless told otherwise */
    USB_BUDGET_1A5,
    USB_BUDGET_3A0,
} usb_budget_t;

int cc_sense_init(void);

/* Millivolts on the live CC line, averaged. */
int cc_sense_read_mv(void);

/* The source's advertised budget. Reads conservatively: anything
 * ambiguous is treated as the 500 mA default. */
usb_budget_t cc_sense_budget(void);

/* Global LED brightness cap for that budget, 0..255, matching the
 * constants in board.h. */
uint8_t cc_sense_led_brightness(usb_budget_t budget);

const char *cc_sense_budget_name(usb_budget_t budget);

void cc_sense_deinit(void);

#endif /* CC_SENSE_H */
