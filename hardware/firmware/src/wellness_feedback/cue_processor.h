/**
 * @file cue_processor.h
 * @brief Neural Load Ring - On-Device Cue Processor (Autonomous Mode)
 *
 * When the phone is disconnected, the ring can still provide intelligent
 * feedback using a simplified on-device algorithm. This processes the
 * coherence data computed locally and triggers actuators.
 *
 * Key differences from phone-side processing:
 *   - Uses fixed thresholds (no ML)
 *   - Limited history tracking (RAM constrained)
 *   - More conservative triggering (battery life)
 *   - Simpler pattern selection
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef CUE_PROCESSOR_H
#define CUE_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * CONSTANTS
 ******************************************************************************/

/** Minimum confidence to trigger any cue (0-100) */
#define CUE_MIN_CONFIDENCE          60

/** Micro-variability thresholds (percentage * 100) */
#define CUE_MICROVAR_ELEVATED       500     /**< 5.0% */
#define CUE_MICROVAR_HIGH           800     /**< 8.0% */
#define CUE_MICROVAR_CRITICAL      1200     /**< 12.0% */

/** Coherence thresholds (percentage) */
#define CUE_COHERENCE_HIGH           75
#define CUE_COHERENCE_MEDIUM         50
#define CUE_COHERENCE_LOW            30
#define CUE_COHERENCE_CRITICAL       15

/** Stability threshold (percentage) */
#define CUE_STABILITY_UNSTABLE       40

/** Cooldown periods (milliseconds) */
#define CUE_COOLDOWN_VIBRATION_MS   30000   /**< 30 seconds */
#define CUE_COOLDOWN_THERMAL_MS    120000   /**< 2 minutes */
#define CUE_COOLDOWN_COMBINED_MS   180000   /**< 3 minutes */
#define CUE_COOLDOWN_ALERT_MS      600000   /**< 10 minutes after alert */

/** Rate limiting */
#define CUE_MAX_PER_HOUR             12
#define CUE_HOUR_MS             3600000

/** History tracking */
#define CUE_HISTORY_SIZE              8

/*******************************************************************************
 * TYPES
 ******************************************************************************/

/** Cue type output */
typedef enum {
    CUE_TYPE_NONE = 0,          /**< No cue generated */
    CUE_TYPE_THERMAL,           /**< Thermal comfort only */
    CUE_TYPE_VIBRATION,         /**< Vibration nudge only */
    CUE_TYPE_BREATHING,         /**< Breathing guidance pattern */
    CUE_TYPE_COMBINED,          /**< Both thermal and vibration */
    CUE_TYPE_ALERT,             /**< Alert pattern */
    CUE_TYPE_CHECK_FIT,         /**< Check ring fit nudge */
} cue_type_t;

/** Priority levels */
typedef enum {
    CUE_PRIORITY_LOW = 0,
    CUE_PRIORITY_NORMAL,
    CUE_PRIORITY_HIGH,
    CUE_PRIORITY_ALERT,
} cue_priority_t;

/** Input metrics (from local coherence calculation) */
typedef struct {
    uint32_t timestamp_ms;      /**< Current time */
    uint16_t micro_var_pct100;  /**< Micro-variability * 100 (0-1200+) */
    uint8_t  coherence_pct;     /**< Phase coherence 0-100 */
    uint8_t  stability_pct;     /**< Coherence stability 0-100 */
    uint8_t  confidence_pct;    /**< Measurement confidence 0-100 */
    uint8_t  stress_level;      /**< Stress 0-100 (0=optimal, 100=critical) */
    uint8_t  artifact_rate_pct; /**< Artifact percentage 0-100 */
} cue_input_t;

/** Output cue command */
typedef struct {
    cue_type_t type;            /**< Type of cue */
    cue_priority_t priority;    /**< Priority level */
    uint8_t thermal_intensity;  /**< 0-100 */
    uint8_t thermal_duration_s; /**< Duration in seconds */
    uint8_t vib_pattern;        /**< Vibration pattern ID */
    uint8_t vib_intensity;      /**< 0-100 */
    uint32_t cooldown_ms;       /**< Suggested wait before next cue */
} cue_output_t;

/** User preferences (stored in flash) */
typedef struct {
    bool     enabled;           /**< Master switch */
    uint8_t  max_thermal_pct;   /**< Maximum thermal intensity */
    uint8_t  max_vib_pct;       /**< Maximum vibration intensity */
    uint8_t  quiet_start_hour;  /**< Quiet hours start (0-23) */
    uint8_t  quiet_end_hour;    /**< Quiet hours end (0-23) */
    uint8_t  sensitivity;       /**< 0=subtle, 1=normal, 2=assertive */
    bool     breathing_enabled; /**< Enable breathing guidance */
    bool     thermal_enabled;   /**< Enable thermal cues */
    bool     vibration_enabled; /**< Enable vibration cues */
} cue_preferences_t;

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

/**
 * @brief Initialize cue processor with default preferences
 */
void cue_processor_init(void);

/**
 * @brief Update user preferences
 * @param prefs New preferences
 */
void cue_processor_set_preferences(const cue_preferences_t *prefs);

/**
 * @brief Get current preferences
 * @param prefs Output buffer
 */
void cue_processor_get_preferences(cue_preferences_t *prefs);

/**
 * @brief Process metrics and generate cue
 *
 * This is the main entry point. Call this with coherence data
 * and it returns the appropriate actuator command (or none).
 *
 * @param input  Coherence metrics from local calculation
 * @param output Generated cue command (if any)
 * @return true if a cue was generated, false otherwise
 */
bool cue_processor_generate(const cue_input_t *input, cue_output_t *output);

/**
 * @brief Reset processor state
 *
 * Call after user explicitly calms down or changes settings.
 */
void cue_processor_reset(void);

/**
 * @brief Set current hour (for quiet hours checking without RTC)
 * @param hour Current hour 0-23
 */
void cue_processor_set_hour(uint8_t hour);

/**
 * @brief Check if cue processor is enabled and ready
 * @return true if ready to generate cues
 */
bool cue_processor_is_ready(void);

/*******************************************************************************
 * STATISTICS (for debugging/app display)
 ******************************************************************************/

/**
 * @brief Get statistics since last reset
 * @param cues_generated Total cues generated
 * @param cues_suppressed Total cues suppressed (cooldown/confidence)
 * @param last_cue_type Type of last cue
 * @param last_cue_ms Timestamp of last cue
 */
void cue_processor_get_stats(
    uint32_t *cues_generated,
    uint32_t *cues_suppressed,
    cue_type_t *last_cue_type,
    uint32_t *last_cue_ms
);

#ifdef __cplusplus
}
#endif

#endif /* CUE_PROCESSOR_H */
