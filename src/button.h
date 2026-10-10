#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

/* Five active-low buttons (keypad style, one side to pin, other to GND):
 *
 *   PD2 = SETTINGS (menu)      PD5 = UP
 *   PD3 = SELECT               PD6 = DOWN
 *   PD4 = OK
 *
 * Internal pull-ups enabled in button_init().
 *
 * THE TIMER0 ISR LIVES IN THIS FILE: it increments the 1 ms system
 * tick and scans the buttons every tick (per-pin debounce counters
 * + auto-repeat). The main loop drains events with
 * button_pressed()/button_repeat().
 */
enum { BTN_MENU = 0, BTN_UP, BTN_DOWN, BTN_SELECT, BTN_OK, BTN_COUNT };

void    button_init(void);          /* pins + pull-ups (call once)          */
uint8_t button_pressed(uint8_t id); /* one-shot press event, returns 1 once */
uint8_t button_repeat(uint8_t id);  /* auto-repeat event while UP/DOWN held */
void    button_flush(void);         /* discard all pending events           */

#endif
