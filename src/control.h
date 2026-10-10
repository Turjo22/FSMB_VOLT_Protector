#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

typedef enum { FAULT_NONE = 0, FAULT_OV, FAULT_UV, FAULT_OC } fault_t;

/* live values, written by control_task, read by the UI */
extern float   g_live_v;        /* mains voltage, V                    */
extern float   g_live_a;        /* load current, A                     */
extern fault_t g_fault;         /* active fault, FAULT_NONE when OK    */
extern uint8_t g_retry_left;    /* seconds until reconnect attempt     */

/* user thresholds in tenths of a unit (2600 = 260.0 V, 150 = 15.0 A).
 * The UI edits these directly; control_task uses them live, so a
 * change takes effect immediately, even mid-settings.               */
extern uint16_t g_ov_x10;
extern uint16_t g_uv_x10;
extern uint16_t g_oc_x10;

void control_init(void);    /* load thresholds from EEPROM (or defaults) */
void control_save(void);    /* store thresholds to EEPROM                */
void control_reload(void);  /* re-read EEPROM (discards unsaved edits)   */
void control_clamp(void);   /* keep values inside sane ranges            */
void control_task(void);    /* non-blocking: measure + protect, ~200 ms  */

#endif
