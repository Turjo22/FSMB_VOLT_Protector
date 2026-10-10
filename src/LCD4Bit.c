
#include "LCD4Bit.h"

/* D4-7 of lcd to PC4-7
RS, RW, E are PB0-2
*/ 
void lcd_data(unsigned char data)
{
	PORTC = data & 0xF0;
	PORTB |=(1<<PB0);
	PORTB &=(~(1<<PB1));
	PORTB |= (1<<PB2);
	_delay_ms(10);
	PORTB &=(~(1<<PB2));

	PORTC = (data<<4) & 0xF0;
	PORTB |=(1<<PB0);
	PORTB &=(~(1<<PB1));
	PORTB |= (1<<PB2);
	_delay_ms(10);
	PORTB &=(~(1<<PB2));
}

void lcd_cmd(unsigned char cmd)
{
	PORTC = cmd & 0xF0;
	PORTB &=(~(1<<PB0));
	PORTB &=(~(1<<PB1));
	PORTB |= (1<<PB2);
	_delay_ms(10);
	PORTB &=(~(1<<PB2));
	
	PORTC = (cmd<<4) & 0xF0;
	PORTB &=(~(1<<PB0));
	PORTB &=(~(1<<PB1));
	PORTB |= (1<<PB2);
	_delay_ms(10);
	PORTB &=(~(1<<PB2));
}

void lcd_string(const unsigned char *str, unsigned char length)
{
	char i;

	for (i = 0; i<length; i++)
	{
		lcd_data(str[i]);
	}
}

void lcd_initialise()
{
	lcd_cmd(0x02);
	lcd_cmd(0x28);
	lcd_cmd(0x06);
	lcd_cmd(0x0C);
	lcd_cmd(0x01);
	_delay_ms(2);
}

void lcd_goto(unsigned char row, unsigned char col)
{
    unsigned char addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_cmd(addr);
}
 
void lcd_clear(void)
{
    lcd_cmd(0x01);
    _delay_ms(2);
}
