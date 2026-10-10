#include "control.h"
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include "tick.h"
#include "sensor.h"
#include "relay.h"

#define UV_IGNORE_V   15.0f     /* below this = "no supply", not a fault */
#define RETRY_S       5

/* defaults (used on first power-up) */
#define DEF_OV_X10    2600u     /* 260.0 V */
#define DEF_UV_X10    1600u     /* 160.0 V */
#define DEF_OC_X10    150u      /*  15.0 A */

/* allowed ranges */
#define OV_MIN 1800u
#define OV_MAX 3000u
#define UV_MIN 1000u
#define UV_MAX 2400u
#define OC_MIN 10u
#define OC_MAX 200u

uint16_t g_ov_x10 = DEF_OV_X10;
uint16_t g_uv_x10 = DEF_UV_X10;
uint16_t g_oc_x10 = DEF_OC_X10;

float   g_live_v = 0.0f;
float   g_live_a = 0.0f;
fault_t g_fault  = FAULT_NONE;
uint8_t g_retry_left = 0;

static uint16_t EEMEM ee_cfg[4];            /* ov, uv, oc, magic */
static const uint16_t CFG_MAGIC = 0xA55Au;

void control_init(void)
{
    if (eeprom_read_word(&ee_cfg[3]) == CFG_MAGIC) {
        g_ov_x10 = eeprom_read_word(&ee_cfg[0]);
        g_uv_x10 = eeprom_read_word(&ee_cfg[1]);
        g_oc_x10 = eeprom_read_word(&ee_cfg[2]);
        control_clamp();
    } else {
        control_save();                     /* first run: store defaults */
    }
}

void control_save(void)
{
    eeprom_write_word(&ee_cfg[0], g_ov_x10);
    eeprom_write_word(&ee_cfg[1], g_uv_x10);
    eeprom_write_word(&ee_cfg[2], g_oc_x10);
    eeprom_write_word(&ee_cfg[3], CFG_MAGIC);
}

void control_reload(void)
{
    control_init();
}

void control_clamp(void)
{
    if (g_ov_x10 > OV_MAX) g_ov_x10 = OV_MAX;
    if (g_ov_x10 < OV_MIN) g_ov_x10 = OV_MIN;
    if (g_uv_x10 > UV_MAX) g_uv_x10 = UV_MAX;
    if (g_uv_x10 < UV_MIN) g_uv_x10 = UV_MIN;
    if (g_uv_x10 > g_ov_x10 - 100u) g_uv_x10 = (uint16_t)(g_ov_x10 - 100u);
    if (g_oc_x10 > OC_MAX) g_oc_x10 = OC_MAX;
    if (g_oc_x10 < OC_MIN) g_oc_x10 = OC_MIN;
}

/* Non-blocking protection task. Called every main-loop pass; samples
 * every 200 ms. Runs in ALL UI modes, so settings never pause the
 * protection of the load. */
void control_task(void)
{
    static uint32_t last_sample = 0;
    static uint32_t retry_mark  = 0;
    uint32_t now = millis();

    if ((uint32_t)(now - last_sample) < 200) return;
    last_sample = now;

    g_live_v = read_voltage();
    g_live_a = read_current();

    float ov = g_ov_x10 / 10.0f;
    float uv = g_uv_x10 / 10.0f;
    float oc = g_oc_x10 / 10.0f;

    if (g_live_v > ov || g_live_v >= 900.0f) {          /* 900+ = clipped/out-of-range */
        g_fault = FAULT_OV;
        g_retry_left = RETRY_S;
        retry_mark = now;
        RELAY_OFF();
    }
    else if (g_live_v < uv && g_live_v > UV_IGNORE_V) {
        g_fault = FAULT_UV;
        g_retry_left = RETRY_S;
        retry_mark = now;
        RELAY_OFF();
    }
    else if (g_live_a > oc) {
        g_fault = FAULT_OC;
        g_retry_left = RETRY_S;
        retry_mark = now;
        RELAY_OFF();
    }
    else if (g_retry_left) {              /* cooling down, relay stays off */
        g_fault = FAULT_NONE;
        RELAY_OFF();
        if ((uint32_t)(now - retry_mark) >= 1000) {
            retry_mark = now;
            g_retry_left--;
        }
    }
    else {                                /* all good */
        g_fault = FAULT_NONE;
        RELAY_ON();
    }
}
