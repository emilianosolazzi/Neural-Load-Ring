/**
 * @file thermal_feature.c
 * @brief Neural Load Ring Thermal Feedback Implementation
 *
 * PWM-controlled resistive heater with comprehensive safety features.
 * Uses nRF52833 PWM peripheral at 1kHz for smooth thermal control.
 *
 * Hardware Configuration:
 *   - Heater: 10Ω resistive element
 *   - Driver: Si2302 N-ch MOSFET (Vgs_th ~1.4V)
 *   - PWM Pin: P0.23
 *   - Max power: 3.3V² / 10Ω × 0.8 = 870mW (but battery limited to ~330mW)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "thermal_feature.h"
#include "../sensors/temperature_sensor.h"
#include <string.h>

/*******************************************************************************
 * CONFIGURATION
 ******************************************************************************/

#define PWM_FREQUENCY_HZ        1000    /**< PWM frequency */
#define PWM_TOP_VALUE           1000    /**< PWM counter top (1000 = 0.1% resolution) */
#define TEMP_CHECK_INTERVAL_MS  500     /**< Skin temp check interval */
#define RUNAWAY_RATE_C_PER_S    2       /**< Max safe temp rise rate */

/*******************************************************************************
 * PATTERN DEFINITIONS
 ******************************************************************************/

typedef struct {
    uint16_t duration_ms;
    uint8_t  intensity_pct;  /* Relative to base intensity */
} thermal_step_t;

/* Pulse pattern: slow breathing rhythm */
static const thermal_step_t PATTERN_PULSE[] = {
    {2000, 100}, {2000, 40}, {2000, 100}, {2000, 40}, {0, 0}
};

/* Wave pattern: gradual up/down */
static const thermal_step_t PATTERN_WAVE[] = {
    {1000, 20}, {1000, 40}, {1000, 60}, {1000, 80}, {1000, 100},
    {1000, 80}, {1000, 60}, {1000, 40}, {1000, 20}, {0, 0}
};

/* Burst pattern: quick warmth then fade */
static const thermal_step_t PATTERN_BURST[] = {
    {500, 100}, {500, 90}, {500, 70}, {1000, 50}, {1500, 30}, {1000, 10}, {0, 0}
};

static const thermal_step_t* PATTERNS[] = {
    NULL,           /* OFF */
    NULL,           /* CONSTANT (no pattern) */
    PATTERN_PULSE,
    PATTERN_WAVE,
    PATTERN_BURST,
};

#define NUM_PATTERNS (sizeof(PATTERNS) / sizeof(PATTERNS[0]))

/*******************************************************************************
 * PRIVATE STATE
 ******************************************************************************/

static struct {
    thermal_state_t state;
    thermal_fault_t fault;
    thermal_pattern_t pattern;
    
    /* Target and current values */
    uint8_t  target_intensity;      /* User-requested (0-100) */
    uint8_t  current_duty;          /* Actual PWM duty (0-100) */
    uint8_t  base_intensity;        /* For patterns */
    
    /* Timing */
    uint32_t start_ms;              /* Session start time */
    uint32_t end_ms;                /* Auto-shutoff time */
    uint32_t ramp_start_ms;         /* Soft-start begin */
    uint32_t last_temp_check_ms;    /* Last temperature sample */
    uint32_t cooldown_end_ms;       /* Cooldown period end */
    
    /* Pattern state */
    const thermal_step_t *pattern_steps;
    uint8_t  step_index;
    uint32_t step_start_ms;
    bool     pattern_looping;
    
    /* Safety monitoring */
    int8_t   skin_temp_c;
    int8_t   prev_temp_c;
    uint32_t prev_temp_ms;
    
} m_thermal = {
    .state = THERMAL_STATE_OFF,
    .fault = THERMAL_FAULT_NONE,
    .skin_temp_c = 25,
};

/*******************************************************************************
 * HARDWARE ABSTRACTION
 ******************************************************************************/

/**
 * Initialize PWM peripheral for thermal control
 */
static void hw_pwm_init(void)
{
    /*
     * nRF52833 PWM configuration:
     *   - PWM1 instance (PWM0 used by vibration)
     *   - 1kHz frequency
     *   - Pin: P0.23
     *   - Polarity: Active high (MOSFET gate)
     *
     * Example (nRF SDK):
     *   nrf_drv_pwm_config_t config = {
     *       .output_pins = { PIN_THERMAL_PWM, NRF_DRV_PWM_PIN_NOT_USED, ... },
     *       .base_clock = NRF_PWM_CLK_1MHz,
     *       .count_mode = NRF_PWM_MODE_UP,
     *       .top_value = PWM_TOP_VALUE,
     *       .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
     *   };
     *   nrf_drv_pwm_init(&m_pwm1, &config, NULL);
     */
}

/**
 * Set PWM duty cycle (0-100%)
 */
static void hw_set_pwm(uint8_t duty_pct)
{
    if (duty_pct > THERMAL_MAX_INTENSITY_PCT) {
        duty_pct = THERMAL_MAX_INTENSITY_PCT;
    }
    
    /*
     * Convert percentage to PWM compare value
     * duty_value = (duty_pct * PWM_TOP_VALUE) / 100
     *
     * Example:
     *   nrf_pwm_values_individual_t values = {
     *       .channel_0 = (duty_pct * PWM_TOP_VALUE) / 100
     *   };
     *   nrf_drv_pwm_simple_playback(&m_pwm1, &seq, 1, 0);
     */
    (void)duty_pct;
}

/**
 * Enable/disable heater MOSFET gate driver
 */
static void hw_enable(bool enable)
{
    if (!enable) {
        hw_set_pwm(0);
    }
    /*
     * Optional: separate enable pin for MOSFET driver
     * nrf_gpio_pin_write(PIN_THERMAL_EN, enable ? 1 : 0);
     */
    (void)enable;
}

/*******************************************************************************
 * SAFETY CHECKS
 ******************************************************************************/

/**
 * Check if skin temperature is within safe limits
 */
static bool check_temperature_safe(void)
{
    if (m_thermal.skin_temp_c >= THERMAL_MAX_SKIN_TEMP_C) {
        m_thermal.fault = THERMAL_FAULT_OVER_TEMP;
        return false;
    }
    return true;
}

/**
 * Check for thermal runaway (temperature rising too fast)
 */
static bool check_runaway(uint32_t now_ms)
{
    if (m_thermal.prev_temp_ms == 0) {
        m_thermal.prev_temp_c = m_thermal.skin_temp_c;
        m_thermal.prev_temp_ms = now_ms;
        return true;
    }
    
    uint32_t dt_ms = now_ms - m_thermal.prev_temp_ms;
    if (dt_ms >= 1000) {  /* Check every second */
        int8_t delta_t = m_thermal.skin_temp_c - m_thermal.prev_temp_c;
        
        if (delta_t > RUNAWAY_RATE_C_PER_S) {
            m_thermal.fault = THERMAL_FAULT_RUNAWAY;
            return false;
        }
        
        m_thermal.prev_temp_c = m_thermal.skin_temp_c;
        m_thermal.prev_temp_ms = now_ms;
    }
    
    return true;
}

/**
 * Enter fault state and shut down
 */
static void enter_fault(thermal_fault_t fault)
{
    m_thermal.fault = fault;
    m_thermal.state = THERMAL_STATE_FAULT;
    m_thermal.current_duty = 0;
    hw_enable(false);
}

/*******************************************************************************
 * SOFT-START RAMP
 ******************************************************************************/

/**
 * Calculate ramped duty cycle during soft-start
 */
static uint8_t calculate_ramp_duty(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - m_thermal.ramp_start_ms;
    
    if (elapsed >= THERMAL_RAMP_TIME_MS) {
        return m_thermal.target_intensity;
    }
    
    /* Linear ramp from 0 to target */
    uint32_t ramped = (m_thermal.target_intensity * elapsed) / THERMAL_RAMP_TIME_MS;
    return (uint8_t)ramped;
}

/*******************************************************************************
 * PATTERN PROCESSING
 ******************************************************************************/

/**
 * Process pattern step transitions
 */
static void process_pattern(uint32_t now_ms)
{
    if (m_thermal.pattern_steps == NULL) {
        return;  /* Constant mode, no pattern */
    }
    
    /* Initialize step timing on first call */
    if (m_thermal.step_start_ms == 0) {
        m_thermal.step_start_ms = now_ms;
    }
    
    const thermal_step_t *step = &m_thermal.pattern_steps[m_thermal.step_index];
    uint32_t elapsed = now_ms - m_thermal.step_start_ms;
    
    if (elapsed >= step->duration_ms) {
        /* Move to next step */
        m_thermal.step_index++;
        m_thermal.step_start_ms = now_ms;
        
        const thermal_step_t *next = &m_thermal.pattern_steps[m_thermal.step_index];
        
        /* Check for pattern end */
        if (next->duration_ms == 0 && next->intensity_pct == 0) {
            if (m_thermal.pattern_looping) {
                m_thermal.step_index = 0;
                next = &m_thermal.pattern_steps[0];
            } else {
                /* Pattern complete */
                thermal_feature_stop();
                return;
            }
        }
        
        /* Calculate new target from pattern step */
        m_thermal.target_intensity = 
            (uint8_t)((next->intensity_pct * m_thermal.base_intensity) / 100);
    }
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void thermal_feature_init(void)
{
    memset(&m_thermal, 0, sizeof(m_thermal));
    m_thermal.state = THERMAL_STATE_OFF;
    m_thermal.skin_temp_c = 25;
    
    hw_pwm_init();
    hw_enable(false);
}

void thermal_feature_set(uint8_t intensity_pct)
{
    thermal_feature_set_timed(intensity_pct, 0);
}

void thermal_feature_set_timed(uint8_t intensity_pct, uint8_t duration_s)
{
    if (intensity_pct == 0) {
        thermal_feature_stop();
        return;
    }
    
    /* Check cooldown period */
    if (m_thermal.state == THERMAL_STATE_COOLDOWN) {
        return;  /* Still cooling down, ignore request */
    }
    
    /* Check for fault state */
    if (m_thermal.state == THERMAL_STATE_FAULT) {
        return;  /* Must clear fault first */
    }
    
    /* Clamp to safety limits */
    if (intensity_pct > THERMAL_MAX_INTENSITY_PCT) {
        intensity_pct = THERMAL_MAX_INTENSITY_PCT;
    }
    
    if (duration_s == 0 || duration_s > THERMAL_MAX_DURATION_S) {
        duration_s = THERMAL_MAX_DURATION_S;
    }
    
    /* Check temperature before starting */
    if (!check_temperature_safe()) {
        enter_fault(THERMAL_FAULT_OVER_TEMP);
        return;
    }
    
    /* Configure session */
    m_thermal.target_intensity = intensity_pct;
    m_thermal.base_intensity = intensity_pct;
    m_thermal.pattern = THERMAL_PATTERN_CONSTANT;
    m_thermal.pattern_steps = NULL;
    
    /* Get current time (will be updated in tick) */
    m_thermal.start_ms = 0;  /* Set in first tick */
    m_thermal.end_ms = 0;
    m_thermal.ramp_start_ms = 0;
    
    /* Store duration for later calculation */
    m_thermal.end_ms = duration_s * 1000;  /* Temporary: actual calc in tick */
    
    /* Start ramping */
    m_thermal.state = THERMAL_STATE_RAMPING;
    m_thermal.current_duty = 0;
    
    hw_enable(true);
}

void thermal_feature_play(thermal_pattern_t pattern, uint8_t intensity_pct, uint8_t duration_s)
{
    if (pattern >= NUM_PATTERNS || pattern == THERMAL_PATTERN_OFF) {
        thermal_feature_stop();
        return;
    }
    
    if (pattern == THERMAL_PATTERN_CONSTANT) {
        thermal_feature_set_timed(intensity_pct, duration_s);
        return;
    }
    
    /* Set up pattern playback */
    thermal_feature_set_timed(intensity_pct, duration_s);
    
    m_thermal.pattern = pattern;
    m_thermal.pattern_steps = PATTERNS[pattern];
    m_thermal.step_index = 0;
    m_thermal.step_start_ms = 0;
    m_thermal.pattern_looping = true;  /* Patterns loop until duration expires */
}

void thermal_feature_stop(void)
{
    uint32_t now_ms = m_thermal.start_ms;  /* Approximate */
    
    m_thermal.target_intensity = 0;
    m_thermal.current_duty = 0;
    m_thermal.pattern_steps = NULL;
    
    hw_set_pwm(0);
    hw_enable(false);
    
    /* Enter cooldown if we were active */
    if (m_thermal.state == THERMAL_STATE_ACTIVE || 
        m_thermal.state == THERMAL_STATE_RAMPING) {
        m_thermal.state = THERMAL_STATE_COOLDOWN;
        m_thermal.cooldown_end_ms = now_ms + (THERMAL_COOLDOWN_S * 1000);
    } else {
        m_thermal.state = THERMAL_STATE_OFF;
    }
}

void thermal_feature_tick(uint32_t now_ms)
{
    /* Initialize timing on first tick after start */
    if (m_thermal.state == THERMAL_STATE_RAMPING && m_thermal.start_ms == 0) {
        m_thermal.start_ms = now_ms;
        m_thermal.ramp_start_ms = now_ms;
        /* Convert stored duration to actual end time */
        uint32_t duration_ms = m_thermal.end_ms;  /* Was temporarily stored */
        m_thermal.end_ms = now_ms + duration_ms;
    }
    
    switch (m_thermal.state) {
        case THERMAL_STATE_OFF:
            /* Nothing to do */
            break;
            
        case THERMAL_STATE_RAMPING:
        {
            /* Soft-start ramp */
            m_thermal.current_duty = calculate_ramp_duty(now_ms);
            hw_set_pwm(m_thermal.current_duty);
            
            /* Check if ramp complete */
            if (now_ms >= m_thermal.ramp_start_ms + THERMAL_RAMP_TIME_MS) {
                m_thermal.state = THERMAL_STATE_ACTIVE;
            }
            
            /* Safety checks during ramp */
            if (!check_temperature_safe() || !check_runaway(now_ms)) {
                enter_fault(m_thermal.fault);
            }
            break;
        }
        
        case THERMAL_STATE_ACTIVE:
        {
            /* Process pattern (updates target_intensity) */
            process_pattern(now_ms);
            
            /* Apply current intensity */
            m_thermal.current_duty = m_thermal.target_intensity;
            hw_set_pwm(m_thermal.current_duty);
            
            /* Check auto-shutoff */
            if (now_ms >= m_thermal.end_ms) {
                m_thermal.fault = THERMAL_FAULT_TIMEOUT;
                thermal_feature_stop();
                break;
            }
            
            /* Periodic temperature check */
            if (now_ms - m_thermal.last_temp_check_ms >= TEMP_CHECK_INTERVAL_MS) {
                m_thermal.last_temp_check_ms = now_ms;
                
                if (!check_temperature_safe() || !check_runaway(now_ms)) {
                    enter_fault(m_thermal.fault);
                }
            }
            break;
        }
        
        case THERMAL_STATE_COOLDOWN:
            /* Wait for cooldown period */
            if (now_ms >= m_thermal.cooldown_end_ms) {
                m_thermal.state = THERMAL_STATE_OFF;
            }
            break;
            
        case THERMAL_STATE_FAULT:
            /* Stay in fault until cleared */
            hw_enable(false);
            break;
    }
}

void thermal_feature_update_skin_temp(int8_t temp_c)
{
    m_thermal.skin_temp_c = temp_c;
}

thermal_state_t thermal_feature_get_state(void)
{
    return m_thermal.state;
}

thermal_fault_t thermal_feature_get_fault(void)
{
    return m_thermal.fault;
}

uint8_t thermal_feature_get_duty(void)
{
    return m_thermal.current_duty;
}

bool thermal_feature_is_active(void)
{
    return (m_thermal.state == THERMAL_STATE_RAMPING || 
            m_thermal.state == THERMAL_STATE_ACTIVE);
}

void thermal_feature_clear_fault(void)
{
    if (m_thermal.state == THERMAL_STATE_FAULT) {
        /* Check if temperature is now safe */
        if (m_thermal.skin_temp_c < THERMAL_MAX_SKIN_TEMP_C - 5) {
            m_thermal.fault = THERMAL_FAULT_NONE;
            m_thermal.state = THERMAL_STATE_OFF;
            m_thermal.prev_temp_ms = 0;  /* Reset runaway detection */
        }
    }
}