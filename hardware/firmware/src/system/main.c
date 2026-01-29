/**
 * @file main.c
 * @brief Neural Load Ring Firmware - Main Application Entry
 *
 * Initializes all subsystems and runs the main processing loop:
 *   - BLE stack (SoftDevice S140)
 *   - PPG sensor + peak detection
 *   - Wellness feedback actuators
 *   - Battery/power management
 *
 * Hardware: nRF52833 @ 64MHz
 * 
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "system_init.h"
#include "../bluetooth/ble_stack.h"
#include "../sensors/ppg_driver.h"
#include "../sensors/temperature_sensor.h"
#include "../core/wellness_processor.h"
#include "../core/wellness_manager.h"
#include "../wellness_feedback/actuator_controller.h"
#include "../wellness_feedback/thermal_feature.h"
#include "../wellness_feedback/vibration_feature.h"

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * CONFIGURATION
 ******************************************************************************/

#define MAIN_LOOP_PERIOD_MS         10      /**< Main loop period (100 Hz) */
#define RR_SEND_INTERVAL_MS         250     /**< RR notification interval (4 Hz) */
#define COHERENCE_UPDATE_MS         15000   /**< Coherence update interval */
#define DEVICE_STATE_UPDATE_MS      5000    /**< Device state update interval */
#define RR_BUFFER_SIZE              16      /**< RR intervals to batch */

/*******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/

static struct {
    uint32_t last_rr_send_ms;
    uint32_t last_coherence_ms;
    uint32_t last_state_ms;
    uint16_t rr_buffer[RR_BUFFER_SIZE];
    uint8_t  rr_count;
    bool     streaming_enabled;
} m_app = {0};

/*******************************************************************************
 * BLE EVENT HANDLER
 ******************************************************************************/

static void on_ble_event(const nlr_ble_evt_t *p_evt)
{
    switch (p_evt->type) {
        case NLR_BLE_EVT_CONNECTED:
            /* Reset streaming state on new connection */
            m_app.rr_count = 0;
            m_app.streaming_enabled = false;
            break;
            
        case NLR_BLE_EVT_DISCONNECTED:
            /* Stop any active actuators on disconnect */
            actuator_stop_all();
            m_app.streaming_enabled = false;
            break;
            
        case NLR_BLE_EVT_ACTUATOR_CMD:
        {
            const nlr_actuator_cmd_t *cmd = &p_evt->data.actuator.cmd;
            
            /* Use unified actuator controller for BLE commands */
            /* The controller handles safety limits, patterns, and coordination */
            actuator_apply_ble(
                cmd->thermal_intensity,
                cmd->thermal_duration_s,
                cmd->vibration_pattern,
                cmd->vibration_intensity,
                0  /* now_ms - will be updated in tick */
            );
            break;
        }
        
        case NLR_BLE_EVT_CONFIG_CHANGED:
            /* Configuration updated via BLE - could adjust timers here */
            break;
            
        case NLR_BLE_EVT_NOTIFICATIONS_ENABLED:
            m_app.streaming_enabled = true;
            break;
            
        case NLR_BLE_EVT_NOTIFICATIONS_DISABLED:
            m_app.streaming_enabled = false;
            break;
            
        default:
            break;
    }
}

/*******************************************************************************
 * MAIN LOOP TASKS
 ******************************************************************************/

/**
 * Collect RR intervals from PPG processor and buffer for transmission
 */
static void task_collect_rr(void)
{
    float rr_ms;
    while (wellness_manager_pop_rr(&rr_ms)) {
        if (m_app.rr_count < RR_BUFFER_SIZE) {
            /* Store as u16 for BLE characteristic */
            m_app.rr_buffer[m_app.rr_count++] = (uint16_t)rr_ms;
        }
    }
}

/**
 * Send buffered RR intervals via BLE if interval elapsed
 */
static void task_send_rr(uint32_t now_ms)
{
    if (!m_app.streaming_enabled || m_app.rr_count == 0) {
        return;
    }
    
    if ((now_ms - m_app.last_rr_send_ms) >= RR_SEND_INTERVAL_MS) {
        m_app.last_rr_send_ms = now_ms;
        
        int err = nlr_ble_send_rr(m_app.rr_buffer, m_app.rr_count);
        if (err == 0 || err == -4) {
            /* Success or queue full - clear buffer either way */
            m_app.rr_count = 0;
        }
    }
}

/**
 * Compute and send coherence metrics periodically
 */
static void task_send_coherence(uint32_t now_ms)
{
    if (!m_app.streaming_enabled) {
        return;
    }
    
    if ((now_ms - m_app.last_coherence_ms) >= COHERENCE_UPDATE_MS) {
        m_app.last_coherence_ms = now_ms;
        
        const hr_metrics_t *p_metrics = wellness_manager_get_metrics();
        
        nlr_coherence_packet_t packet = {
            .stress_level = (uint8_t)(p_metrics->stress_score * 100.0f),
            .coherence_pct = (uint8_t)((1.0f - p_metrics->stress_score) * 100.0f),
            .confidence_pct = (p_metrics->valid_samples > 30) ? 90 : 50,
            .variability_level = (uint8_t)(p_metrics->rmssd > 100 ? 100 : p_metrics->rmssd),
            .mean_rr_ms = (uint16_t)p_metrics->mean_rr_ms,
            .rmssd_ms = (uint16_t)p_metrics->rmssd,
            .respiratory_rate_cpm = 0, /* TODO: Implement resp rate detection */
            .reserved = 0,
        };
        
        nlr_ble_send_coherence(&packet);
    }
}

/**
 * Update and broadcast device state periodically
 */
static void task_update_device_state(uint32_t now_ms)
{
    if ((now_ms - m_app.last_state_ms) >= DEVICE_STATE_UPDATE_MS) {
        m_app.last_state_ms = now_ms;
        
        /* Read skin temperature */
        int8_t skin_temp = temperature_read_skin();
        
        /* Feed skin temperature to actuator controller for thermal safety */
        actuator_update_skin_temp(skin_temp);
        
        /* Check for thermal faults */
        uint8_t error_flags = 0;
        if (thermal_feature_get_state() == THERMAL_STATE_FAULT) {
            error_flags |= 0x08;  /* Bit 3: thermal fault */
        }
        
        nlr_device_state_t state = {
            .battery_pct = 85,  /* TODO: Read from BQ25125 */
            .charging_state = 0,
            .connection_state = nlr_ble_is_connected() ? 2 : 1,
            .streaming_active = m_app.streaming_enabled ? 0x03 : 0x00,
            .skin_temp_c = skin_temp,
            .error_flags = error_flags,
            .uptime_min = (uint16_t)(now_ms / 60000),
        };
        
        nlr_ble_update_device_state(&state);
    }
}

/*******************************************************************************
 * MAIN
 ******************************************************************************/

int main(void)
{
    /* Initialize system clocks, GPIO, power management */
    system_init();
    
    /* Initialize BLE stack with event handler */
    int err = nlr_ble_init(on_ble_event);
    if (err != 0) {
        /* BLE init failed - enter error state */
        while (1) {
            /* TODO: Blink error LED */
        }
    }
    
    /* Initialize sensors */
    ppg_init();
    temperature_init();
    
    /* Initialize wellness core */
    wellness_manager_init();
    
    /* Initialize actuators */
    actuator_init();
    
    /* Start advertising */
    nlr_ble_advertising_start();
    
    /* Main processing loop */
    uint32_t now_ms = 0;
    
    while (1) {
        /* Process BLE events */
        nlr_ble_process();
        
        /* Run wellness analysis */
        wellness_manager_tick(now_ms);
        
        /* Run periodic tasks */
        task_collect_rr();
        task_send_rr(now_ms);
        task_send_coherence(now_ms);
        task_update_device_state(now_ms);
        
        /* Update actuator state machines */
        actuator_tick(now_ms);
        thermal_feature_tick(now_ms);
        vibration_feature_tick(now_ms);
        
        /* Wait for next tick (in real firmware: WFE or app_scheduler) */
        /* nrf_delay_ms(MAIN_LOOP_PERIOD_MS); */
        now_ms += MAIN_LOOP_PERIOD_MS;
    }
    
    return 0;
}