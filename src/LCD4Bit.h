#ifndef LCD4BIT_H
#define LCD4BIT_H

/*
 *  LCD wiring (4-bit mode)
 *  Data  D4-D7  →  PC4-PC7 
 *  RS          →  PB0
 *  RW          →  PB1
 *  EN          →  PB2
 */

#include <avr/io.h>
#include <util/delay.h>

void lcd_data(unsigned char data);
void lcd_cmd(unsigned char cmd);
void lcd_string(const unsigned char *str, unsigned char length);
void lcd_initialise(void);
void lcd_goto(unsigned char row, unsigned char col);
void lcd_clear(void);

#endif
