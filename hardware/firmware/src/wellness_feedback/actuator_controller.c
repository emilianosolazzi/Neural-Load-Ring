/**
 * @file actuator_controller.c
 * @brief Neural Load Ring Actuator Scheduler Implementation
 *
 * Coordinates thermal and vibration feedback with safety limits.
 * Integrates with BLE commands and wellness engine prescriptions.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "actuator_controller.h"
#include "thermal_feature.h"
#include "vibration_feature.h"

/*******************************************************************************
 * CONFIGURATION
 ******************************************************************************/

#define MAX_INTENSITY           100U
#define MIN_INTENSITY           5U          /**< Avoid imperceptible outputs */
#define MAX_DURATION_MS         60000U      /**< 60s max for any single command */
#define COMBINED_VIB_CAP        60U         /**< Cap vibration when combined with thermal */

/*******************************************************************************
 * PRIVATE STATE
 ******************************************************************************/

static struct {
    actuator_cmd_t active;
    uint32_t active_end_ms;
    bool thermal_running;
    bool vibration_running;
    int8_t skin_temp_c;
} m_ctrl = {
    .active.type = ACTUATOR_NONE,
};

/*******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

/**
 * Apply outputs based on command
 */
static void apply_outputs(actuator_cmd_t *cmd)
{
    uint8_t duration_s = (uint8_t)(cmd->duration_ms / 1000);
    if (duration_s == 0 && cmd->duration_ms > 0) {
        duration_s = 1;  /* Minimum 1 second */
    }
    
    switch (cmd->type) {
        case ACTUATOR_THERMAL:
            /* Thermal only - use pattern if specified */
            if (cmd->thermal_pattern > 0) {
                thermal_feature_play(
                    (thermal_pattern_t)cmd->thermal_pattern,
                    cmd->intensity_pct,
                    duration_s
                );
            } else {
                thermal_feature_set_timed(cmd->intensity_pct, duration_s);
            }
            vibration_feature_stop();
            m_ctrl.thermal_running = true;
            m_ctrl.vibration_running = false;
            break;
            
        case ACTUATOR_VIBRATION:
            /* Vibration only - use pattern if specified */
            if (cmd->vibration_pattern > 0) {
                vibration_feature_play(
                    (vibration_pattern_t)cmd->vibration_pattern,
                    cmd->intensity_pct
                );
            } else {
                vibration_feature_on(cmd->intensity_pct);
            }
            thermal_feature_stop();
            m_ctrl.thermal_running = false;
            m_ctrl.vibration_running = true;
            break;
            
        case ACTUATOR_COMBINED:
        {
            /* Both - cap vibration to avoid overwhelming */
            uint8_t vib_intensity = cmd->intensity_pct;
            if (vib_intensity > COMBINED_VIB_CAP) {
                vib_intensity = COMBINED_VIB_CAP;
            }
            
            if (cmd->thermal_pattern > 0) {
                thermal_feature_play(
                    (thermal_pattern_t)cmd->thermal_pattern,
                    cmd->intensity_pct,
                    duration_s
                );
            } else {
                thermal_feature_set_timed(cmd->intensity_pct, duration_s);
            }
            
            if (cmd->vibration_pattern > 0) {
                vibration_feature_play(
                    (vibration_pattern_t)cmd->vibration_pattern,
                    vib_intensity
                );
            } else {
                vibration_feature_on(vib_intensity);
            }
            
            m_ctrl.thermal_running = true;
            m_ctrl.vibration_running = true;
            break;
        }
        
        default:
            thermal_feature_stop();
            vibration_feature_stop();
            m_ctrl.thermal_running = false;
            m_ctrl.vibration_running = false;
            break;
    }
}

/**
 * Stop all outputs
 */
static void stop_outputs(void)
{
    thermal_feature_stop();
    vibration_feature_stop();
    m_ctrl.thermal_running = false;
    m_ctrl.vibration_running = false;
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void actuator_init(void)
{
    /* Initialize drivers */
    thermal_feature_init();
    vibration_feature_init();
    
    /* Clear state */
    m_ctrl.active.type = ACTUATOR_NONE;
    m_ctrl.active.intensity_pct = 0;
    m_ctrl.active.duration_ms = 0;
    m_ctrl.active.timestamp_ms = 0;
    m_ctrl.active.priority = ACTUATOR_PRIORITY_LOW;
    m_ctrl.active_end_ms = 0;
    m_ctrl.thermal_running = false;
    m_ctrl.vibration_running = false;
    m_ctrl.skin_temp_c = 25;
    
    stop_outputs();
}

int actuator_apply(actuator_cmd_t cmd, uint32_t now_ms)
{
    /* Validate parameters */
    if (cmd.intensity_pct > MAX_INTENSITY) {
        return 0;
    }
    
    if (cmd.duration_ms > MAX_DURATION_MS) {
        return 0;
    }
    
    if (cmd.intensity_pct < MIN_INTENSITY && cmd.intensity_pct != 0) {
        return 0;  /* Too weak to perceive */
    }
    
    /* Check priority - can override lower priority commands */
    if (m_ctrl.active.type != ACTUATOR_NONE) {
        if (cmd.priority < m_ctrl.active.priority) {
            return 0;  /* Lower priority, reject */
        }
    }
    
    /* Accept command */
    m_ctrl.active = cmd;
    m_ctrl.active.timestamp_ms = now_ms;
    m_ctrl.active_end_ms = now_ms + cmd.duration_ms;
    
    /* Update skin temperature for thermal safety */
    thermal_feature_update_skin_temp(m_ctrl.skin_temp_c);
    
    /* Apply outputs */
    apply_outputs(&m_ctrl.active);
    
    return 1;
}

int actuator_apply_ble(
    uint8_t thermal_intensity,
    uint8_t thermal_duration_s,
    uint8_t vibration_pattern,
    uint8_t vibration_intensity,
    uint32_t now_ms)
{
    actuator_cmd_t cmd = {
        .type = ACTUATOR_NONE,
        .intensity_pct = 0,
        .duration_ms = 0,
        .timestamp_ms = now_ms,
        .priority = ACTUATOR_PRIORITY_HIGH,  /* BLE commands are user-initiated */
        .thermal_pattern = 0,
        .vibration_pattern = vibration_pattern,
    };
    
    /* Determine command type based on what's requested */
    bool want_thermal = (thermal_intensity > 0 && thermal_duration_s > 0);
    bool want_vibration = (vibration_pattern > 0 && vibration_intensity > 0);
    
    if (want_thermal && want_vibration) {
        cmd.type = ACTUATOR_COMBINED;
        cmd.intensity_pct = thermal_intensity;  /* Use thermal for base */
        cmd.duration_ms = thermal_duration_s * 1000U;
    } else if (want_thermal) {
        cmd.type = ACTUATOR_THERMAL;
        cmd.intensity_pct = thermal_intensity;
        cmd.duration_ms = thermal_duration_s * 1000U;
    } else if (want_vibration) {
        cmd.type = ACTUATOR_VIBRATION;
        cmd.intensity_pct = vibration_intensity;
        cmd.duration_ms = 5000;  /* Default 5s for vibration-only */
    } else {
        /* Both off - stop everything */
        actuator_stop_all();
        return 1;
    }
    
    return actuator_apply(cmd, now_ms);
}

void actuator_tick(uint32_t now_ms)
{
    /* Update skin temperature for safety monitoring */
    thermal_feature_update_skin_temp(m_ctrl.skin_temp_c);
    
    /* Process driver state machines */
    thermal_feature_tick(now_ms);
    vibration_feature_tick(now_ms);
    
    /* Check for command timeout */
    if (m_ctrl.active.type != ACTUATOR_NONE) {
        if (now_ms >= m_ctrl.active_end_ms) {
            /* Command duration expired */
            stop_outputs();
            m_ctrl.active.type = ACTUATOR_NONE;
        }
    }
    
    /* Update running state based on driver status */
    m_ctrl.thermal_running = thermal_feature_is_active();
    m_ctrl.vibration_running = vibration_feature_is_active();
    
    /* If both stopped but command still active, clear it */
    if (!m_ctrl.thermal_running && !m_ctrl.vibration_running) {
        if (m_ctrl.active.type != ACTUATOR_NONE) {
            m_ctrl.active.type = ACTUATOR_NONE;
        }
    }
}

void actuator_stop_all(void)
{
    stop_outputs();
    m_ctrl.active.type = ACTUATOR_NONE;
    m_ctrl.active_end_ms = 0;
}

void actuator_get_status(actuator_status_t *status)
{
    if (status == NULL) return;
    
    status->thermal_active = m_ctrl.thermal_running;
    status->vibration_active = m_ctrl.vibration_running;
    status->thermal_duty = thermal_feature_get_duty();
    status->vibration_duty = 0;  /* TODO: add to vibration API if needed */
    status->current_type = m_ctrl.active.type;
    
    if (m_ctrl.active.type != ACTUATOR_NONE && m_ctrl.active_end_ms > 0) {
        /* This is approximate - would need actual now_ms for accuracy */
        status->remaining_ms = 0;
    } else {
        status->remaining_ms = 0;
    }
}

bool actuator_is_active(void)
{
    return m_ctrl.thermal_running || m_ctrl.vibration_running;
}

void actuator_update_skin_temp(int8_t temp_c)
{
    m_ctrl.skin_temp_c = temp_c;
    thermal_feature_update_skin_temp(temp_c);
}