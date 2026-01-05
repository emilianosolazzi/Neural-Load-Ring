/**
 * @file vibration_feature.h
 * @brief Neural Load Ring Vibration Feedback Driver
 *
 * Controls the LRA/ERM motor via DRV8837 H-bridge for haptic feedback.
 * Supports multiple patterns for different wellness cues.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef VIBRATION_FEATURE_H
#define VIBRATION_FEATURE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Vibration pattern IDs (matches BLE protocol) */
typedef enum {
    VIB_PATTERN_OFF = 0,        /**< Stop vibration */
    VIB_PATTERN_SINGLE = 1,     /**< Single short pulse (100ms) */
    VIB_PATTERN_DOUBLE = 2,     /**< Two pulses */
    VIB_PATTERN_TRIPLE = 3,     /**< Three pulses */
    VIB_PATTERN_HEARTBEAT = 4,  /**< Lub-dub cardiac rhythm */
    VIB_PATTERN_BREATHING = 5,  /**< Slow wave for breathing guidance */
    VIB_PATTERN_ALERT = 6,      /**< Rapid attention-getting */
} vibration_pattern_t;

/**
 * @brief Initialize vibration driver (PWM + GPIO)
 */
void vibration_feature_init(void);

/**
 * @brief Play a vibration pattern
 * @param pattern Pattern ID
 * @param intensity_pct Intensity 0-100%
 */
void vibration_feature_play(vibration_pattern_t pattern, uint8_t intensity_pct);

/**
 * @brief Stop all vibration immediately
 */
void vibration_feature_stop(void);

/**
 * @brief Turn vibration on at constant intensity (legacy)
 * @param intensity_pct Intensity 0-100%
 */
void vibration_feature_on(uint8_t intensity_pct);

/**
 * @brief Turn vibration off (legacy)
 */
void vibration_feature_off(void);

/**
 * @brief Process vibration patterns (call from main loop)
 * @param now_ms Current timestamp in milliseconds
 */
void vibration_feature_tick(uint32_t now_ms);

/**
 * @brief Check if vibration is currently active
 * @return true if motor is running
 */
bool vibration_feature_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* VIBRATION_FEATURE_H */