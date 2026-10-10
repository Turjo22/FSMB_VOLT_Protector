#include "uart.h"

void uart_init(void)
{
    // Set baud rate
    UBRRH = (unsigned char)(MYUBRR >> 8);
    UBRRL = (unsigned char)MYUBRR;

    // Enable double speed mode
    UCSRA |= (1 << U2X);

    // Enable receiver, transmitter, and receive interrupt
    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);

    // Set frame format: 8 data bits, 1 stop bit (URSEL for ATmega32/16)
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void uart_transmit_char(char data)
{
    // Wait for empty transmit buffer
    while (!(UCSRA & (1 << UDRE)))
        ;
    // Put data into buffer, sends the data
    UDR = data;
}

void uart_send_string(const char *str)
{
    while (*str)
    {
        uart_transmit_char(*str++);
    }
    uart_transmit_char('\n'); // Append newline for protocol handling
}

char uart_read_char(void)
{
    return UDR;
}