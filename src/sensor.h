/*
 * sensor.h - voltage & current measurement via ADC
 *
 * VOLTAGE (PA0 / ADC0): transformer -> bridge -> 1000uF -> 19k/10k divider
 *   Two-point calibration from MEASURED data (do not derive from parts):
 *     mains 160V -> Vadc 2.87V
 *     mains 260V -> Vadc 5.00V
 *   slope    = (5.00-2.87)/(260-160) = 0.0213 V/V
 *   Vadc     = 0.0213 * Vmains - 0.538
 *   Vmains   = (Vadc + 0.538) / 0.0213
 *   Per ADC count: Vmains = ADC * 0.2295 + 25.26
 *   NOTE: at 260V the divider output is exactly 5.00V = ADC full scale.
 *   Thresholds above ~260V still trip (clipped reading = max = "over"),
 *   but the displayed value will stick at 260V.
 *
 * CURRENT (PA1 / ADC1): ACS712-20A, 100mV/A, 2.5V offset, output is AC
 *   -> firmware peak detection over ~1 mains cycle, then /1.414 for RMS.
 */
#ifndef SENSOR_H
#define SENSOR_H

/* measured two-point calibration (edit if your readings differ) */
#define V_PER_COUNT      0.2295f   /* volts of mains per ADC count        */
#define V_OFFSET         25.26f    /* mains-volts offset (diode drops)    */

/* ACS712-20A */
#define I_OFFSET_CNT     512       /* 2.5V in ADC counts                  */
#define I_MV_PER_AMP     100.0     /* 100 mV/A                            */

void    sensor_init(void);
float   read_voltage(void);       /* mains voltage, V                    */
float   read_current(void);       /* load current, A rms                 */

#endif
