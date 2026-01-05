/**
 * @file temperature_sensor.c
 * @brief Neural Load Ring NTC Temperature Sensor Implementation
 *
 * Uses nRF52833 SAADC (Successive Approximation ADC) to read an NTC
 * thermistor in a voltage divider configuration.
 *
 * Circuit: VDD --- [10kΩ] --- ADC_IN --- [NTC 10kΩ@25°C] --- GND
 * NTC: Murata NCP18XH103F03RB (10kΩ, B=3380K)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "temperature_sensor.h"
#include <math.h>

/*******************************************************************************
 * NTC THERMISTOR PARAMETERS (Murata NCP18XH103F03RB)
 ******************************************************************************/

#define NTC_R25             10000.0f    /**< Resistance at 25°C (ohms) */
#define NTC_BETA            3380.0f     /**< Beta coefficient (K) */
#define NTC_T25_KELVIN      298.15f     /**< 25°C in Kelvin */
#define SERIES_RESISTOR     10000.0f    /**< Series resistor (ohms) */
#define ADC_RESOLUTION      4096        /**< 12-bit ADC */
#define VDD_MV              3300        /**< Supply voltage (mV) */

/*******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/

static struct {
    int8_t  last_temp_c;
    uint16_t last_raw;
    uint8_t  sample_count;
    uint32_t sample_sum;
} m_temp = {
    .last_temp_c = 25,  /* Default room temp */
};

/*******************************************************************************
 * ADC HARDWARE ABSTRACTION
 ******************************************************************************/

/**
 * Read ADC value from NTC channel (AIN0 = P0.02)
 * Returns 12-bit value (0-4095)
 */
static uint16_t hw_adc_read(void)
{
    /*
     * nRF52833 SAADC configuration:
     *   - Resolution: 12-bit
     *   - Reference: Internal 0.6V with 1/6 gain (3.6V full scale)
     *   - Acquisition time: 10µs
     *   - Input: AIN0 (P0.02)
     *
     * Example (nRF SDK):
     *   nrf_saadc_value_t value;
     *   nrf_drv_saadc_sample_convert(0, &value);
     *   return (uint16_t)value;
     */
    
    /* Placeholder: return mid-scale (~1.65V = ~25°C with our divider) */
    return 2048;
}

/*******************************************************************************
 * TEMPERATURE CALCULATION
 ******************************************************************************/

/**
 * Convert ADC reading to temperature using Steinhart-Hart simplified equation
 * (Beta parameter method)
 *
 * R_ntc = R_series * (ADC_MAX / ADC_VALUE - 1)   [voltage divider]
 * 1/T = 1/T0 + (1/B) * ln(R_ntc / R0)            [Beta equation]
 */
static float adc_to_celsius(uint16_t adc_value)
{
    if (adc_value == 0 || adc_value >= ADC_RESOLUTION) {
        return 25.0f;  /* Invalid reading, return default */
    }
    
    /* Calculate NTC resistance from voltage divider */
    float ratio = (float)ADC_RESOLUTION / (float)adc_value - 1.0f;
    float r_ntc = SERIES_RESISTOR * ratio;
    
    /* Guard against invalid resistance */
    if (r_ntc <= 0.0f) {
        return 25.0f;
    }
    
    /* Steinhart-Hart Beta method */
    float ln_r = logf(r_ntc / NTC_R25);
    float inv_t = (1.0f / NTC_T25_KELVIN) + (ln_r / NTC_BETA);
    
    /* Convert Kelvin to Celsius */
    float temp_c = (1.0f / inv_t) - 273.15f;
    
    /* Clamp to physiological range */
    if (temp_c < -10.0f) temp_c = -10.0f;
    if (temp_c > 50.0f) temp_c = 50.0f;
    
    return temp_c;
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void temperature_init(void)
{
    /*
     * Initialize SAADC for single-channel NTC reading
     *
     * Example (nRF SDK):
     *   nrf_drv_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
     *   nrf_drv_saadc_init(&saadc_config, saadc_event_handler);
     *
     *   nrf_saadc_channel_config_t channel_config = 
     *       NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
     *   nrf_drv_saadc_channel_init(0, &channel_config);
     */
    
    m_temp.sample_count = 0;
    m_temp.sample_sum = 0;
}

int8_t temperature_read_skin(void)
{
    /* Read and average multiple samples for noise reduction */
    uint16_t raw = hw_adc_read();
    
    /* Simple moving average (4 samples) */
    m_temp.sample_sum += raw;
    m_temp.sample_count++;
    
    if (m_temp.sample_count >= 4) {
        uint16_t avg_raw = (uint16_t)(m_temp.sample_sum / 4);
        m_temp.last_raw = avg_raw;
        
        float temp_f = adc_to_celsius(avg_raw);
        m_temp.last_temp_c = (int8_t)roundf(temp_f);
        
        /* Reset averaging */
        m_temp.sample_count = 0;
        m_temp.sample_sum = 0;
    }
    
    return m_temp.last_temp_c;
}

uint16_t temperature_read_raw(void)
{
    return m_temp.last_raw;
}

float read_temperature(void)
{
    return (float)temperature_read_skin();
}