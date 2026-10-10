#include "relay.h"
#include <avr/io.h>

void relay_init(void)
{
    DDRB  |= (1<<PB0) | (1<<PB1) | (1<<PB2);  /* LCD control lines      */
    DDRC  |= 0xF0;                            /* LCD data lines PC4-PC7 */
    DDRB  |= (1<<PB4);                        /* relay pin              */
    RELAY_OFF();                              /* safe state at power-up */
}
