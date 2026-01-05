/**
 * @file actuator_controller.h
 * @brief Neural Load Ring Actuator Scheduler
 *
 * Coordinates thermal and vibration feedback based on wellness cues.
 * Provides unified interface for BLE commands and engine prescriptions.
 *
 * Features:
 *   - Priority-based command queuing
 *   - Safety limit enforcement
 *   - Coordinated thermal/vibration output
 *   - Pattern synchronization
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef ACTUATOR_CONTROLLER_H
#define ACTUATOR_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * TYPES
 ******************************************************************************/

/** Actuator output type */
typedef enum {
    ACTUATOR_NONE = 0,          /**< No output */
    ACTUATOR_THERMAL,           /**< Thermal only */
    ACTUATOR_VIBRATION,         /**< Vibration only */
    ACTUATOR_COMBINED,          /**< Both thermal and vibration */
} actuator_type_t;

/** Command priority levels */
typedef enum {
    ACTUATOR_PRIORITY_LOW = 0,  /**< Background cues */
    ACTUATOR_PRIORITY_NORMAL,   /**< Standard feedback */
    ACTUATOR_PRIORITY_HIGH,     /**< User-initiated */
    ACTUATOR_PRIORITY_ALERT,    /**< Safety/attention alerts */
} actuator_priority_t;

/** Actuator command structure */
typedef struct {
    actuator_type_t type;       /**< Output type */
    uint8_t intensity_pct;      /**< Intensity 0-100 */
    uint16_t duration_ms;       /**< Duration in milliseconds */
    uint32_t timestamp_ms;      /**< When scheduled */
    actuator_priority_t priority; /**< Command priority */
    uint8_t thermal_pattern;    /**< Thermal pattern ID (0=constant) */
    uint8_t vibration_pattern;  /**< Vibration pattern ID (0=constant) */
} actuator_cmd_t;

/** Actuator status for monitoring */
typedef struct {
    bool thermal_active;
    bool vibration_active;
    uint8_t thermal_duty;
    uint8_t vibration_duty;
    uint16_t remaining_ms;
    actuator_type_t current_type;
} actuator_status_t;

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

/**
 * @brief Initialize actuator controller and drivers
 */
void actuator_init(void);

/**
 * @brief Apply a new actuator command
 * 
 * Higher priority commands can override lower priority ones.
 * Returns success if command was accepted.
 *
 * @param cmd Command to apply
 * @param now_ms Current timestamp
 * @return 1 if accepted, 0 if rejected
 */
int actuator_apply(actuator_cmd_t cmd, uint32_t now_ms);

/**
 * @brief Apply command from BLE (simplified interface)
 * 
 * Matches nlr_actuator_cmd_t from BLE protocol.
 *
 * @param thermal_intensity Thermal intensity 0-100
 * @param thermal_duration_s Thermal duration in seconds
 * @param vibration_pattern Vibration pattern ID
 * @param vibration_intensity Vibration intensity 0-100
 * @param now_ms Current timestamp
 * @return 1 if accepted, 0 if rejected
 */
int actuator_apply_ble(
    uint8_t thermal_intensity,
    uint8_t thermal_duration_s,
    uint8_t vibration_pattern,
    uint8_t vibration_intensity,
    uint32_t now_ms
);

/**
 * @brief Process actuator state machines (call from main loop)
 * @param now_ms Current timestamp
 */
void actuator_tick(uint32_t now_ms);

/**
 * @brief Stop all actuators immediately
 */
void actuator_stop_all(void);

/**
 * @brief Get current actuator status
 * @param status Output status structure
 */
void actuator_get_status(actuator_status_t *status);

/**
 * @brief Check if any actuator is active
 * @return true if thermal or vibration is running
 */
bool actuator_is_active(void);

/**
 * @brief Update skin temperature for thermal safety
 * @param temp_c Current skin temperature
 */
void actuator_update_skin_temp(int8_t temp_c);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_CONTROLLER_H */