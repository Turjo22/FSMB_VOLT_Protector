#include "button.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* Keypad-style button scanning inside the 1 kHz TIMER0 ISR.
 * Modelled on the classic per-pin debounce-counter approach:
 * every tick the raw pins are sampled, and only when a pin stays
 * changed for DEBOUNCE_TICKS consecutive ticks is the debounced
 * state updated and an event queued. Works regardless of what the
 * main loop is doing - protection can never starve the keys. */

#define DEBOUNCE_TICKS  4      /* 4 ms stable before accepting a change   */
#define REPEAT_DELAY    600    /* ms a UP/DOWN must be held before repeat */
#define REPEAT_RATE     150    /* ms between auto-repeats                 */

/* Pin wiring (matches the Proteus schematic), active-low:
 *   PD1=SETTINGS  PD3=SELECT  PD5=OK  PD6=UP  PD7=DOWN                  */
#define BTN_PIN_MASK  ((1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6))

static volatile uint8_t press_ev;    /* bitmask of queued press events  */
static volatile uint8_t repeat_ev;   /* bitmask of queued repeat events */

void button_init(void)
{
    DDRD  &= (uint8_t)~BTN_PIN_MASK;      /* inputs            */
    PORTD |=           BTN_PIN_MASK;      /* internal pull-ups */
    press_ev  = 0;
    repeat_ev = 0;
}

/* Live pin state packed into a 5-bit mask (bit set = pressed).
 * BTN_MENU=bit0, BTN_UP=bit1, BTN_DOWN=bit2, BTN_SELECT=bit3, BTN_OK=bit4 */
static uint8_t read_pins(void)
{
    uint8_t r = 0;
    if (!(PIND & (1<<PD2))) r |= (1<<BTN_MENU);
    if (!(PIND & (1<<PD5))) r |= (1<<BTN_UP);
    if (!(PIND & (1<<PD6))) r |= (1<<BTN_DOWN);
    if (!(PIND & (1<<PD3))) r |= (1<<BTN_SELECT);
    if (!(PIND & (1<<PD4))) r |= (1<<BTN_OK);
    return r;
}

/* Called from the TIMER0 compare ISR every 1 ms. Keep it short. */
void button_scan_1ms(void)
{
    static uint8_t  state = 0;                 /* debounced state, 1=pressed */
    static uint8_t  cnt[BTN_COUNT];            /* per-button debounce count  */
    static uint16_t hold[BTN_COUNT];           /* ms UP/DOWN has been held   */
    uint8_t cur = read_pins();

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        uint8_t cb = (uint8_t)((cur >> i) & 1u);
        uint8_t sb = (uint8_t)((state >> i) & 1u);

        /* -- debounce: pin must stay changed for DEBOUNCE_TICKS ticks -- */
        if (cb != sb) {
            if (++cnt[i] >= DEBOUNCE_TICKS) {
                cnt[i] = 0;
                if (cb) {
                    state    |= (uint8_t)(1u << i);
                    press_ev |= (uint8_t)(1u << i);   /* queue press event */
                    hold[i]   = 0;
                } else {
                    state &= (uint8_t)~(1u << i);
                }
            }
        } else {
            cnt[i] = 0;
        }

        /* -- auto-repeat for UP / DOWN only -- */
        if (i == BTN_UP || i == BTN_DOWN) {
            if ((state & (1u<<i)) && (hold[i] < 65535)) hold[i]++;
            if ((state & (1u<<i)) && (hold[i] >= REPEAT_DELAY)) {
                if (((hold[i] - REPEAT_DELAY) % REPEAT_RATE) == 0)
                    repeat_ev |= (uint8_t)(1u << i);
            }
        }
    }
}

uint8_t button_pressed(uint8_t id)
{
    uint8_t m = (uint8_t)(1u << id);
    uint8_t r = 0;
    cli();
    if (press_ev & m) { press_ev = (uint8_t)(press_ev & (uint8_t)~m); r = 1; }
    sei();
    return r;
}

uint8_t button_repeat(uint8_t id)
{
    uint8_t m = (uint8_t)(1u << id);
    uint8_t r = 0;
    cli();
    if (repeat_ev & m) { repeat_ev = (uint8_t)(repeat_ev & (uint8_t)~m); r = 1; }
    sei();
    return r;
}

void button_flush(void)
{
    cli();
    press_ev  = 0;
    repeat_ev = 0;
    sei();
}
