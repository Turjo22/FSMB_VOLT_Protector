#ifndef UART_H
#define UART_H

#include <avr/io.h>

/*
 *  Baud rate: 9600, U2X enabled 
 */
#define MYUBRR  103

void uart_init(void);
void uart_transmit_char(char data);
void uart_send_string(const char *str);
char uart_read_char(void);

#endif
