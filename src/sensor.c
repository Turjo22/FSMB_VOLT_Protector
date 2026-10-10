#include "sensor.h"
#include <avr/io.h>

void sensor_init(void)
{
    ADMUX  =  (1<<REFS0);                     /* reference = AVcc (5V)   */
    ADCSRA =  (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);  /* ADC on, /64 prescaler */
}

static uint16_t adc_read(uint8_t ch)
{
    ADMUX = (ADMUX & 0xF8) | (ch & 0x07);     /* select channel          */
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC));
    return ADC;
}

float read_voltage(void)
{
    uint32_t s = 0;
    for (uint8_t i = 0; i < 16; i++) s += adc_read(0);
    /* Vmains = Vadc * V_PER_COUNT... using measured calibration:
       Vmains = avg_counts * V_PER_COUNT + V_OFFSET                  */
    return (s / 16.0f) * V_PER_COUNT + V_OFFSET;
}

float read_current(void)
{
    /* ACS712 VOUT is a sine around 2.5V. Sample ~1 mains cycle
     * (200 samples x ~104us = ~21ms) and take the peak deviation.   */
    uint16_t mx = 0, mn = 1023;
    for (uint16_t i = 0; i < 200; i++) {
        uint16_t r = adc_read(1);
        if (r > mx) mx = r;
        if (r < mn) mn = r;
    }
    int32_t up   = (int32_t)mx - I_OFFSET_CNT;
    int32_t down = I_OFFSET_CNT - (int32_t)mn;
    int32_t dev  = (up > down) ? up : down;    /* peak deviation, counts */

    float i_peak = dev * (5.0/1023.0) * 1000.0 / I_MV_PER_AMP;
    return i_peak / 1.414f;                    /* peak -> rms            */
}
