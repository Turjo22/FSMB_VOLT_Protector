/* relay.h - mains contactor on PD0 via BC547 */
#ifndef RELAY_H
#define RELAY_H

#define RELAY_ON()   (PORTB |=  (1<<PB4))
#define RELAY_OFF()  (PORTB &= ~(1<<PB4))
void relay_init(void);

#endif
