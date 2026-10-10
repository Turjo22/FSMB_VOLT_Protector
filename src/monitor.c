#include "monitor.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uart.h"
#include "tick.h"
#include "control.h"

#define RX_BUF_SIZE  32
#define TX_PERIOD_MS 500

/* --- receive ring line-buffer, filled by the RX interrupt ---------- */
static volatile char     rx_buf[RX_BUF_SIZE];
static volatile uint8_t  rx_len      = 0;
static volatile uint8_t  line_ready  = 0;

/* 1 while the on-device settings menu is open (ui.c owns the settings) */
volatile uint8_t g_menu_active = 0;

ISR(USART_RXC_vect)          /* ATmega32 USART receive-complete */
{
    char c = (char)UDR;                       /* reading UDR clears the flag */
    if (c == '\n' || c == '\r') {
        line_ready = 1;                       /* main loop will consume it   */
    } else if (rx_len < RX_BUF_SIZE - 1) {
        rx_buf[rx_len++] = c;
        rx_buf[rx_len] = 0;
    }
    /* buffer full without newline -> drop and resync */
    else rx_len = 0;
}

void monitor_init(void)
{
    rx_len = 0;
    line_ready = 0;
}

/* --- ui.c hooks ----------------------------------------------------- */

/* Called by ui.c when the settings menu is entered/exited. */
void monitor_menu_lock(uint8_t active)
{
    g_menu_active = active;
    uart_send_string(active ? "M=1" : "M=0");
}

/* Push the current thresholds to the PC. ui.c calls this after every
 * local edit (and on menu exit) so the GUI always shows live values. */
void monitor_send_settings(void)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "OV=%u,UV=%u,OC=%u",
             g_ov_x10, g_uv_x10, g_oc_x10);
    uart_send_string(buf);
}

/* --- command handling ------------------------------------------------ */

/* Refuse configuration while the device's own menu is open. */
static uint8_t write_locked(void)
{
    if (g_menu_active) {
        uart_send_string("BUSY");
        return 1;
    }
    return 0;
}

static void handle_line(const char *cmd)
{
    uint16_t v;

    if (!strncmp(cmd, "OV=", 3)) {
        if (write_locked()) return;
        v = (uint16_t)atoi(cmd + 3);
        if (v) { g_ov_x10 = v; control_clamp(); uart_send_string("OK"); }
        else uart_send_string("ERR");
    }
    else if (!strncmp(cmd, "UV=", 3)) {
        if (write_locked()) return;
        v = (uint16_t)atoi(cmd + 3);
        if (v) { g_uv_x10 = v; control_clamp(); uart_send_string("OK"); }
        else uart_send_string("ERR");
    }
    else if (!strncmp(cmd, "OC=", 3)) {
        if (write_locked()) return;
        v = (uint16_t)atoi(cmd + 3);
        if (v) { g_oc_x10 = v; control_clamp(); uart_send_string("OK"); }
        else uart_send_string("ERR");
    }
    else if (!strcmp(cmd, "GET")) {
        /* read-only: always allowed, even while the menu is open */
        monitor_send_settings();
    }
    else if (!strcmp(cmd, "SAVE")) {
        if (write_locked()) return;
        control_save();
        uart_send_string("OK");
    }
    else {
        uart_send_string("ERR");
    }
}

void monitor_task(void)
{
    static uint32_t last_tx = 0;
    uint32_t now = millis();

    /* ---- consume a finished command line (copy it out atomically) --- */
    if (line_ready) {
        char cmd[RX_BUF_SIZE];
        cli();
        strncpy(cmd, (const char *)rx_buf, RX_BUF_SIZE);
        cmd[RX_BUF_SIZE - 1] = 0;
        rx_len = 0;
        rx_buf[0] = 0;
        line_ready = 0;
        sei();
        handle_line(cmd);
    }

    /* ---- telemetry every TX_PERIOD_MS ------------------------------- */
    if ((uint32_t)(now - last_tx) < TX_PERIOD_MS) return;
    last_tx = now;

    {
        char buf[56];
        char vbuf[10], abuf[10];
        dtostrf(g_live_v, 6, 1, vbuf);
        dtostrf(g_live_a, 5, 2, abuf);
        snprintf(buf, sizeof(buf), "V=%s,A=%s,F=%u,R=%u",
                 vbuf, abuf, (unsigned)g_fault, (unsigned)g_retry_left);
        uart_send_string(buf);
    }
}