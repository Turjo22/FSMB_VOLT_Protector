#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>

/* Set by ui.c while the on-device settings menu is open.
 * While active the PC may READ (GET) but may NOT change/save
 * thresholds - every such command is answered with "BUSY". */
extern volatile uint8_t g_menu_active;

void monitor_init(void);
void monitor_task(void);

/* ui.c hooks ------------------------------------------------------------ */
void monitor_menu_lock(uint8_t active);  /* call on menu enter(1)/exit(0)  */
void monitor_send_settings(void);        /* push OV/UV/OC to PC (local edit)*/

#endif