/**
 * @file thermal_feature.h
 * @brief Neural Load Ring Thermal Feedback Driver
 *
 * Controls the resistive heating element via N-channel MOSFET with PWM.
 * Provides warmth cues for relaxation/wellness feedback.
 *
 * Safety features:
 *   - Maximum duty cycle limit (prevents burns)
 *   - Soft-start ramp (gradual warmup)
 *   - Auto-shutoff timer
 *   - Skin temperature monitoring integration
 *   - Thermal runaway protection
 *
 * Hardware: N-ch MOSFET (Si2302) driving 10Ω heating element
 * PWM: 1kHz frequency, 0-80% max duty cycle
 * Power: ~330mW max at 80% duty (3.3V supply)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef THERMAL_FEATURE_H
#define THERMAL_FEATURE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * SAFETY LIMITS
 ******************************************************************************/

#define THERMAL_MAX_INTENSITY_PCT   80      /**< Maximum PWM duty (burn prevention) */
#define THERMAL_MAX_DURATION_S      60      /**< Maximum continuous run time */
#define THERMAL_MAX_SKIN_TEMP_C     42      /**< Shutdown if skin exceeds this */
#define THERMAL_COOLDOWN_S          30      /**< Minimum cooldown between sessions */
#define THERMAL_RAMP_TIME_MS        2000    /**< Soft-start ramp duration */

/*******************************************************************************
 * THERMAL PATTERNS
 ******************************************************************************/

/** Thermal pattern IDs (matches BLE protocol) */
typedef enum {
    THERMAL_PATTERN_OFF = 0,        /**< Off */
    THERMAL_PATTERN_CONSTANT = 1,   /**< Steady warmth */
    THERMAL_PATTERN_PULSE = 2,      /**< Slow pulse (breathing sync) */
    THERMAL_PATTERN_WAVE = 3,       /**< Gradual wave up/down */
    THERMAL_PATTERN_BURST = 4,      /**< Quick warmth burst then fade */
} thermal_pattern_t;

/** Thermal state for monitoring */
typedef enum {
    THERMAL_STATE_OFF,              /**< Heater disabled */
    THERMAL_STATE_RAMPING,          /**< Soft-start in progress */
    THERMAL_STATE_ACTIVE,           /**< Running at target */
    THERMAL_STATE_COOLDOWN,         /**< Mandatory cooldown period */
    THERMAL_STATE_FAULT,            /**< Safety shutdown */
} thermal_state_t;

/** Fault codes */
typedef enum {
    THERMAL_FAULT_NONE = 0,
    THERMAL_FAULT_OVER_TEMP,        /**< Skin temperature exceeded limit */
    THERMAL_FAULT_RUNAWAY,          /**< Temperature rising too fast */
    THERMAL_FAULT_SENSOR_FAIL,      /**< Temperature sensor not responding */
    THERMAL_FAULT_TIMEOUT,          /**< Max duration exceeded */
} thermal_fault_t;

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

/**
 * @brief Initialize thermal driver (PWM + GPIO)
 */
void thermal_feature_init(void);

/**
 * @brief Set thermal intensity with constant pattern
 * @param intensity_pct Target intensity 0-100% (clamped to MAX)
 */
void thermal_feature_set(uint8_t intensity_pct);

/**
 * @brief Set thermal intensity with duration limit
 * @param intensity_pct Target intensity 0-100%
 * @param duration_s Duration in seconds (0 = use default max)
 */
void thermal_feature_set_timed(uint8_t intensity_pct, uint8_t duration_s);

/**
 * @brief Play a thermal pattern
 * @param pattern Pattern ID
 * @param intensity_pct Base intensity 0-100%
 * @param duration_s Duration in seconds
 */
void thermal_feature_play(thermal_pattern_t pattern, uint8_t intensity_pct, uint8_t duration_s);

/**
 * @brief Stop thermal output immediately
 */
void thermal_feature_stop(void);

/**
 * @brief Process thermal state machine (call from main loop)
 * @param now_ms Current timestamp in milliseconds
 */
void thermal_feature_tick(uint32_t now_ms);

/**
 * @brief Update skin temperature reading (call periodically)
 * @param temp_c Current skin temperature in Celsius
 */
void thermal_feature_update_skin_temp(int8_t temp_c);

/**
 * @brief Get current thermal state
 * @return Current state
 */
thermal_state_t thermal_feature_get_state(void);

/**
 * @brief Get current fault code (if in FAULT state)
 * @return Fault code or THERMAL_FAULT_NONE
 */
thermal_fault_t thermal_feature_get_fault(void);

/**
 * @brief Get current PWM duty cycle
 * @return Duty cycle 0-100%
 */
uint8_t thermal_feature_get_duty(void);

/**
 * @brief Check if thermal is currently active
 * @return true if heater is on
 */
bool thermal_feature_is_active(void);

/**
 * @brief Clear fault and allow restart (after cooldown)
 */
void thermal_feature_clear_fault(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_FEATURE_H */