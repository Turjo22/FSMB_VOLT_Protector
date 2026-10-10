#define F_CPU 8000000UL
#include "ui.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>
#include "LCD4Bit.h"
#include "control.h"
#include "button.h"
#include "tick.h"
#include "monitor.h"          /* menu lock + PC sync hooks */

/* 16x2 UI layer, fully non-blocking.
 *
 * RUN  mode:  line0 live V/I, line1 status (fault / retry / normal)
 * MENU mode:  line0 '>PARAM', line1 live values or fault banner
 * EDIT mode:  line0 'OV: 260V ^', line1 live values or fault banner
 *
 * control_task() keeps running underneath in every mode, so the relay
 * keeps protecting the load while the user is changing thresholds,
 * and every change takes effect immediately (before it is even saved).
 *
 * PC link: while MENU/EDIT is active the device owns the settings -
 * the monitor answers every PC write attempt with "BUSY" and the GUI
 * shows a warning. Every local edit is pushed to the PC
 * (monitor_send_settings) so both sides stay in sync.
 */
#define LCD_PRINT(s)  lcd_string((const unsigned char*)(s), strlen(s))

typedef enum { MODE_RUN, MODE_MENU, MODE_EDIT } ui_mode_t;

static const char *param_name[]  = { "OVER-VOLTAGE", "UNDER-VOLTAGE", "OVER-CURRENT", "SAVE & EXIT" };
static const char *param_short[] = { "OV", "UV", "OC" };
#define PARAM_COUNT 4

static ui_mode_t mode = MODE_RUN;
static uint8_t   sel  = 0;                 /* parameter index in menu   */
static uint32_t  last_render = 0;
static uint32_t  saved_flash = 0;          /* 'SAVED' confirmation timer */
static uint8_t   force = 1;
static fault_t   last_fault = (fault_t)0xFF;
static uint8_t   last_retry = 0xFF;

/* ---- helpers ------------------------------------------------------- */

static uint16_t *param_ptr(uint8_t i)
{
    if (i == 0) return &g_ov_x10;
    if (i == 1) return &g_uv_x10;
    return &g_oc_x10;
}

static uint16_t param_step(uint8_t i)
{
    return (i == 2) ? 10u : 50u;          /* OC: 1 A/step, V: 5 V/step */
}

static void lcd_row(uint8_t r, const char *s)
{
    char line[17];
    uint8_t n = strlen(s);
    if (n > 16) n = 16;
    memcpy(line, s, n);
    while (n < 16) line[n++] = ' ';
    line[16] = 0;
    lcd_goto(r, 0);
    LCD_PRINT(line);
}

static void fmt_v(uint16_t x10, char *buf)   /* 2600 -> "260"  */
{
    itoa(x10 / 10, buf, 10);
}

static void fmt_i(uint16_t x10, char *buf)   /* 150 -> "15.0"  */
{
    itoa(x10 / 10, buf, 10);
    uint8_t l = strlen(buf);
    buf[l]     = '.';
    buf[l + 1] = (char)('0' + (x10 % 10));
    buf[l + 2] = 0;
}

static const char *fault_msg(fault_t f)
{
    switch (f) {
        case FAULT_OV:  return "  OVER-VOLTAGE! ";
        case FAULT_UV:  return "  UNDER-VOLTAGE ";
        case FAULT_OC:  return "  OVER-CURRENT! ";
        default:        return "    NORMAL :)   ";
    }
}

/* bottom line: fault > retry-countdown > live values.
 * Shown in every mode so protection status is always visible. */
static void show_live_row1(void)
{
    char line[17], b[8];

    if (g_fault != FAULT_NONE) {
        lcd_row(1, fault_msg(g_fault));
        return;
    }
    if (g_retry_left) {
        strcpy(line, "RETRY IN ");
        itoa(g_retry_left, b, 10);
        strcat(line, b);
        strcat(line, "s");
        lcd_row(1, line);
        return;
    }
    strcpy(line, "V:");
    dtostrf(g_live_v, 5, 1, b); strcat(line, b);
    strcat(line, " I:");
    dtostrf(g_live_a, 5, 2, b); strcat(line, b);
    lcd_row(1, line);
}

static void show_run(void)
{
    char line[17], b[8];

    if ((uint32_t)(millis() - saved_flash) < 1200) {   /* save confirmation */
        lcd_row(0, "  SETTINGS SAVED ");
        show_live_row1();
        return;
    }

    strcpy(line, "V:");
    dtostrf(g_live_v, 5, 1, b); strcat(line, b);
    strcat(line, "V I:");
    dtostrf(g_live_a, 5, 2, b); strcat(line, b);
    strcat(line, "A");
    lcd_row(0, line);

    if (g_fault != FAULT_NONE)      lcd_row(1, fault_msg(g_fault));
    else if (g_retry_left)          show_live_row1();
    else                            lcd_row(1, fault_msg(FAULT_NONE));
}

static void show_menu(void)
{
    char line[17];
    strcpy(line, ">");
    strcat(line, param_name[sel]);
    lcd_row(0, line);
    show_live_row1();
}

static void show_edit(uint8_t blink_on)
{
    char line[17], b[8];
    uint16_t *p = param_ptr(sel);

    strcpy(line, param_short[sel]);
    strcat(line, ":");
    if (sel == 2) fmt_i(*p, b); else fmt_v(*p, b);
    strcat(line, b);
    strcat(line, (sel == 2) ? "A" : "V");
    strcat(line, blink_on ? " ^" : "  ");
    lcd_row(0, line);
    show_live_row1();
}

/* ---- public -------------------------------------------------------- */

void ui_init(void)
{
    lcd_row(0, " V-PROTECTOR ON ");
    lcd_row(1, "  5-BTN SETTING ");
    _delay_ms(1500);
    force = 1;
}

void ui_task(void)
{
    /* fault/retry transitions must show up at once */
    if (g_fault != last_fault || g_retry_left != last_retry) {
        last_fault = g_fault;
        last_retry = g_retry_left;
        force = 1;
    }

    /* ---- button handling by mode ---- */
    if (button_pressed(BTN_MENU)) {
        button_flush();
        if (mode == MODE_RUN) {
            mode = MODE_MENU; sel = 0;
            monitor_menu_lock(1);              /* PC writes now rejected */
        }
        else if (mode == MODE_MENU) {
            mode = MODE_RUN;
            monitor_menu_lock(0);              /* hand control back to PC */
            monitor_send_settings();           /* final values -> GUI     */
        }
        else {                                 /* cancel edit, stay in menu */
            control_reload();
            mode = MODE_MENU;
        }
        force = 1;
    }

    if (mode == MODE_MENU) {
        if (button_pressed(BTN_SELECT)) {
            sel = (uint8_t)((sel + 1) % PARAM_COUNT);
            force = 1;
        }
        if (button_pressed(BTN_OK)) {
            button_flush();
            if (sel == 3) {                       /* SAVE & EXIT */
                control_clamp();
                control_save();
                saved_flash = millis();           /* show confirmation */
                mode = MODE_RUN;
                monitor_menu_lock(0);             /* PC may write again  */
                monitor_send_settings();          /* saved values -> GUI */
            } else {
                mode = MODE_EDIT;                 /* start editing */
            }
            force = 1;
        }
        /* consume repeats so none are queued for EDIT mode */
        button_repeat(BTN_UP);
        button_repeat(BTN_DOWN);
    }
    else if (mode == MODE_EDIT) {
        uint16_t *p = param_ptr(sel);
        int8_t d = 0;

        if (button_pressed(BTN_UP)   || button_repeat(BTN_UP))   d =  1;
        if (button_pressed(BTN_DOWN) || button_repeat(BTN_DOWN)) d = -1;
        if (d) {
            if (d > 0) *p = (uint16_t)(*p + param_step(sel));
            else       *p = (uint16_t)(*p - param_step(sel));
            control_clamp();                      /* live while editing */
            monitor_send_settings();              /* push edit to GUI   */
            force = 1;
        }
        if (button_pressed(BTN_OK)) {             /* confirm parameter */
            button_flush();
            control_clamp();
            control_save();
            saved_flash = millis();               /* show confirmation */
            monitor_send_settings();              /* push edit to GUI  */
            mode = MODE_MENU;
            force = 1;
        }
    }

    /* ---- non-blocking rendering ---- */
    {
        uint32_t now   = millis();
        uint8_t  blink = (uint8_t)((now / 500) & 1);
        uint32_t period = (mode == MODE_RUN) ? 300 :
                          (mode == MODE_MENU) ? 500 : 250;

        if (force || (uint32_t)(now - last_render) >= period) {
            if (mode == MODE_RUN)       show_run();
            else if (mode == MODE_MENU) show_menu();
            else                        show_edit(blink);
            last_render = now;
            force = 0;
        }
    }
}