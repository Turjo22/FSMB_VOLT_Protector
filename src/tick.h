#ifndef TICK_H
#define TICK_H

#include <stdint.h>

/* 1 ms system tick on Timer0 (ATmega32, 8 MHz, prescaler 64, OCR0=124) */
void     tick_init(void);
uint32_t millis(void);

#endif
