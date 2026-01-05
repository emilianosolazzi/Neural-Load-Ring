/**
 * @file temperature_sensor.h
 * @brief Neural Load Ring NTC Temperature Sensor Driver
 *
 * Reads skin temperature via NTC thermistor connected to ADC.
 * Provides calibrated readings in Celsius.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize temperature sensor (ADC)
 */
void temperature_init(void);

/**
 * @brief Read skin temperature
 * @return Temperature in degrees Celsius (signed)
 */
int8_t temperature_read_skin(void);

/**
 * @brief Read raw ADC value (for calibration)
 * @return 12-bit ADC value (0-4095)
 */
uint16_t temperature_read_raw(void);

/**
 * @brief Read temperature as float (legacy)
 * @return Temperature in degrees Celsius
 */
float read_temperature(void);

#ifdef __cplusplus
}
#endif

#endif /* TEMPERATURE_SENSOR_H */