/*
 * main.c - Voltage & Over-Current Protector with 5-button settings
 * ATmega32 @ 8 MHz (set the internal RC fuse to 8 MHz or use 8 MHz xtal)
 *
 * Hardware (matches the Proteus schematic):
 *   LCD (4-bit):  D4-D7 -> PC4-PC7, RS->PB0, RW->PB1, EN->PB2
 *   Buzzer:       PC0 (BZ)
 *   Relay (BC547 via R3): PC1
 *   Buttons (active-low, internal pull-up, common rail to GND):
 *     SETTINGS->PD1, SELECT->PD3, OK->PD5, UP->PD6, DOWN->PD7
 *   Sensing:  ADC0/PA0 = mains voltage (divider), ADC1/PA1 = ACS712
 *   UART: PD1(TX) -> COMPIM TXD pin, PD0(RX) -> COMPIM RXD pin
 *
 * NEW: uart.c + monitor.c add a 9600 baud link to the PC GUI.
 *   The GUI shows live V/A/status and can edit OV/UV/OC thresholds.
 *
 * Architecture: non-blocking cooperative loop.
 *   control_task() ALWAYS runs (measure + relay), even while the user
 *   is inside the settings menu - protection is never paused, and any
 *   threshold change takes effect immediately.
 */
#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include "LCD4Bit.h"
#include "tick.h"
#include "button.h"
#include "sensor.h"
#include "relay.h"
#include "control.h"
#include "ui.h"
#include "uart.h"
#include "monitor.h"

int main(void)
{
    relay_init();      /* LCD pins + relay pin, relay starts OFF */
    button_init();     /* PD1,PD3,PD5,PD6,PD7 inputs w/ pull-ups */
    tick_init();       /* 1 ms tick + ISR button scan            */
    sensor_init();     /* ADC                                    */
    lcd_initialise();
    control_init();    /* thresholds from EEPROM (or defaults)   */
    ui_init();         /* splash screen                          */
    uart_init();       /* 9600 8N1, double speed                 */
    monitor_init();    /* PC link command handler                */

    sei();
    while (1) {
        control_task();  /* measurement + relay: NEVER blocked  */
        ui_task();       /* consumes key events, updates LCD    */
        monitor_task();  /* UART: commands + telemetry to PC    */
    }
}