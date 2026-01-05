/**
 * @file vibration_feature.c
 * @brief Neural Load Ring Vibration Feedback Implementation
 *
 * Pattern-based haptic feedback using DRV8837 H-bridge driver.
 * PWM frequency: 200Hz (optimal for LRA)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "vibration_feature.h"

/*******************************************************************************
 * PATTERN DEFINITIONS
 * Each pattern is a sequence of {duration_ms, intensity_pct} pairs.
 * Terminated by {0, 0}.
 ******************************************************************************/

typedef struct {
    uint16_t duration_ms;
    uint8_t  intensity_pct;
} pattern_step_t;

/* Single pulse: 100ms on */
static const pattern_step_t PATTERN_SINGLE[] = {
    {100, 100}, {0, 0}
};

/* Double pulse: 100ms on, 100ms off, 100ms on */
static const pattern_step_t PATTERN_DOUBLE[] = {
    {100, 100}, {100, 0}, {100, 100}, {0, 0}
};

/* Triple pulse */
static const pattern_step_t PATTERN_TRIPLE[] = {
    {80, 100}, {80, 0}, {80, 100}, {80, 0}, {80, 100}, {0, 0}
};

/* Heartbeat: lub-dub pattern */
static const pattern_step_t PATTERN_HEARTBEAT[] = {
    {80, 100}, {60, 0}, {100, 80}, {760, 0},  /* ~75 BPM rhythm */
    {80, 100}, {60, 0}, {100, 80}, {760, 0},
    {80, 100}, {60, 0}, {100, 80}, {0, 0}
};

/* Breathing guide: slow sine-ish wave (4s inhale, 6s exhale) */
static const pattern_step_t PATTERN_BREATHING[] = {
    /* Inhale ramp up (4s) */
    {500, 20}, {500, 35}, {500, 50}, {500, 65}, {500, 80}, {500, 90}, {500, 95}, {500, 100},
    /* Exhale ramp down (6s) */
    {600, 90}, {600, 75}, {600, 60}, {600, 45}, {600, 30}, {600, 20}, {600, 10}, {600, 5},
    {400, 0},  /* Brief pause */
    {0, 0}
};

/* Alert: rapid attention-getting */
static const pattern_step_t PATTERN_ALERT[] = {
    {50, 100}, {50, 0}, {50, 100}, {50, 0}, {50, 100}, {50, 0},
    {150, 0},
    {50, 100}, {50, 0}, {50, 100}, {50, 0}, {50, 100}, {50, 0},
    {0, 0}
};

static const pattern_step_t* PATTERNS[] = {
    NULL,               /* OFF */
    PATTERN_SINGLE,
    PATTERN_DOUBLE,
    PATTERN_TRIPLE,
    PATTERN_HEARTBEAT,
    PATTERN_BREATHING,
    PATTERN_ALERT,
};

#define NUM_PATTERNS (sizeof(PATTERNS) / sizeof(PATTERNS[0]))

/*******************************************************************************
 * PRIVATE STATE
 ******************************************************************************/

static struct {
    const pattern_step_t *pattern;
    uint8_t  step_index;
    uint8_t  base_intensity;    /* User-requested intensity (scaled) */
    uint8_t  current_intensity; /* Actual PWM output */
    uint32_t step_start_ms;
    bool     active;
    bool     looping;           /* For continuous patterns like breathing */
} m_vib = {0};

/*******************************************************************************
 * HARDWARE ABSTRACTION
 ******************************************************************************/

/**
 * Set motor PWM duty cycle (0-100%)
 * In real firmware: Configure nRF52 PWM peripheral
 */
static void hw_set_pwm(uint8_t duty_pct)
{
    /* 
     * nRF52833 PWM setup:
     *   - PWM0 instance
     *   - 200Hz frequency (5ms period) - optimal for LRA
     *   - Pin: P0.xx (motor driver IN1 or IN2)
     *
     * Example (using nRF SDK):
     *   nrf_pwm_values_individual_t pwm_values = {
     *       .channel_0 = (duty_pct * PWM_MAX) / 100
     *   };
     *   nrf_drv_pwm_simple_playback(&m_pwm, &pwm_seq, 1, 0);
     */
    (void)duty_pct;
}

/**
 * Enable/disable motor driver (DRV8837 nSLEEP pin)
 */
static void hw_enable_driver(bool enable)
{
    /*
     * DRV8837 control:
     *   - nSLEEP pin: HIGH = active, LOW = sleep mode
     *   - IN1/IN2: PWM drive signals
     *
     * Example:
     *   nrf_gpio_pin_write(PIN_DRV_NSLEEP, enable ? 1 : 0);
     */
    (void)enable;
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void vibration_feature_init(void)
{
    m_vib.active = false;
    m_vib.current_intensity = 0;
    hw_enable_driver(false);
    hw_set_pwm(0);
}

void vibration_feature_play(vibration_pattern_t pattern, uint8_t intensity_pct)
{
    if (pattern >= NUM_PATTERNS || PATTERNS[pattern] == NULL) {
        vibration_feature_stop();
        return;
    }
    
    if (intensity_pct > 100) intensity_pct = 100;
    
    m_vib.pattern = PATTERNS[pattern];
    m_vib.step_index = 0;
    m_vib.base_intensity = intensity_pct;
    m_vib.step_start_ms = 0;  /* Will be set on first tick */
    m_vib.active = true;
    m_vib.looping = (pattern == VIB_PATTERN_BREATHING); /* Breathing loops */
    
    hw_enable_driver(true);
}

void vibration_feature_stop(void)
{
    m_vib.active = false;
    m_vib.pattern = NULL;
    m_vib.current_intensity = 0;
    hw_set_pwm(0);
    hw_enable_driver(false);
}

void vibration_feature_on(uint8_t intensity_pct)
{
    if (intensity_pct > 100) intensity_pct = 100;
    if (intensity_pct == 0) {
        vibration_feature_stop();
        return;
    }
    
    m_vib.pattern = NULL;  /* No pattern, constant output */
    m_vib.active = true;
    m_vib.base_intensity = intensity_pct;
    m_vib.current_intensity = intensity_pct;
    
    hw_enable_driver(true);
    hw_set_pwm(intensity_pct);
}

void vibration_feature_off(void)
{
    vibration_feature_stop();
}

void vibration_feature_tick(uint32_t now_ms)
{
    if (!m_vib.active) return;
    
    /* Constant intensity mode (no pattern) */
    if (m_vib.pattern == NULL) {
        return;
    }
    
    /* Initialize step start time on first tick */
    if (m_vib.step_start_ms == 0) {
        m_vib.step_start_ms = now_ms;
        
        /* Apply first step immediately */
        const pattern_step_t *step = &m_vib.pattern[m_vib.step_index];
        uint8_t scaled = (uint8_t)((step->intensity_pct * m_vib.base_intensity) / 100);
        m_vib.current_intensity = scaled;
        hw_set_pwm(scaled);
    }
    
    /* Check if current step duration elapsed */
    const pattern_step_t *step = &m_vib.pattern[m_vib.step_index];
    uint32_t elapsed = now_ms - m_vib.step_start_ms;
    
    if (elapsed >= step->duration_ms) {
        /* Move to next step */
        m_vib.step_index++;
        m_vib.step_start_ms = now_ms;
        
        const pattern_step_t *next = &m_vib.pattern[m_vib.step_index];
        
        /* Check for pattern end */
        if (next->duration_ms == 0 && next->intensity_pct == 0) {
            if (m_vib.looping) {
                /* Restart pattern */
                m_vib.step_index = 0;
                next = &m_vib.pattern[0];
            } else {
                /* Pattern complete */
                vibration_feature_stop();
                return;
            }
        }
        
        /* Apply new step intensity (scaled by user intensity) */
        uint8_t scaled = (uint8_t)((next->intensity_pct * m_vib.base_intensity) / 100);
        m_vib.current_intensity = scaled;
        hw_set_pwm(scaled);
    }
}

bool vibration_feature_is_active(void)
{
    return m_vib.active;
}