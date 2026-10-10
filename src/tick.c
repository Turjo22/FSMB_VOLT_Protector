#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "tick.h"
#include "button.h"

static volatile uint32_t g_ms;

ISR(TIMER0_COMP_vect)          /* 1 kHz */
{
    g_ms++;
    button_scan_1ms();         /* keypad-style key scan + debounce */
}

void tick_init(void)
{
    TCCR0  = (1<<WGM01) | (1<<CS01) | (1<<CS00);  /* CTC mode, clk/64  */
    OCR0   = 124;                                 /* 8MHz/64/125 = 1kHz */
    TIMSK |= (1<<OCIE0);
    sei();
}

uint32_t millis(void)
{
    uint32_t m;
    cli();
    m = g_ms;
    sei();
    return m;
}
