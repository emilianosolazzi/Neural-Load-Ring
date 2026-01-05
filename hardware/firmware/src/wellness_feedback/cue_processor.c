/**
 * @file cue_processor.c
 * @brief Neural Load Ring - On-Device Cue Processor Implementation
 *
 * Autonomous cue generation when phone is disconnected. Uses simplified
 * algorithms suitable for embedded execution with limited RAM.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "cue_processor.h"
#include "actuator_controller.h"
#include "thermal_feature.h"
#include "vibration_feature.h"
#include <string.h>

/*******************************************************************************
 * INTENSITY PROFILES
 ******************************************************************************/

typedef struct {
    uint8_t thermal_base;
    uint8_t thermal_max;
    uint8_t vib_base;
    uint8_t vib_max;
    uint8_t duration_mult;  /* Multiplier / 10 (7=0.7x, 10=1.0x, 13=1.3x) */
} intensity_profile_t;

static const intensity_profile_t PROFILES[3] = {
    /* subtle */   { .thermal_base = 25, .thermal_max = 50, .vib_base = 15, .vib_max = 40, .duration_mult = 7 },
    /* normal */   { .thermal_base = 35, .thermal_max = 70, .vib_base = 30, .vib_max = 60, .duration_mult = 10 },
    /* assertive */{ .thermal_base = 45, .thermal_max = 85, .vib_base = 45, .vib_max = 80, .duration_mult = 13 },
};

/*******************************************************************************
 * MODULE STATE
 ******************************************************************************/

static struct {
    cue_preferences_t prefs;
    
    /* Timing state */
    uint32_t last_cue_ms;
    cue_type_t last_cue_type;
    uint8_t current_hour;
    
    /* Rate limiting */
    uint32_t hour_start_ms;
    uint8_t cues_this_hour;
    
    /* Confidence tracking */
    uint8_t consecutive_low_conf;
    
    /* History for trend detection */
    uint8_t coherence_history[CUE_HISTORY_SIZE];
    uint8_t history_idx;
    uint8_t history_count;
    
    /* Statistics */
    uint32_t total_generated;
    uint32_t total_suppressed;
    
    bool initialized;
} s_state;

/*******************************************************************************
 * INTERNAL HELPERS
 ******************************************************************************/

static uint8_t clamp_intensity(uint8_t value, uint8_t max)
{
    return (value > max) ? max : value;
}

static bool is_quiet_hours(void)
{
    uint8_t hour = s_state.current_hour;
    uint8_t start = s_state.prefs.quiet_start_hour;
    uint8_t end = s_state.prefs.quiet_end_hour;
    
    /* Handle overnight quiet hours (e.g., 22:00 - 07:00) */
    if (start > end) {
        return (hour >= start) || (hour < end);
    }
    /* Same-day quiet hours */
    return (hour >= start) && (hour < end);
}

static bool check_rate_limit(uint32_t now_ms)
{
    /* Reset hourly counter if needed */
    if ((now_ms - s_state.hour_start_ms) >= CUE_HOUR_MS) {
        s_state.cues_this_hour = 0;
        s_state.hour_start_ms = now_ms;
    }
    
    return s_state.cues_this_hour < CUE_MAX_PER_HOUR;
}

static bool can_trigger(cue_type_t type, uint32_t now_ms, uint32_t cooldown)
{
    /* Allow first cue immediately after reset */
    if (s_state.last_cue_ms == 0) {
        return true;
    }
    
    uint32_t elapsed = now_ms - s_state.last_cue_ms;
    
    /* Same type or combined requires full cooldown */
    if (s_state.last_cue_type == type || s_state.last_cue_type == CUE_TYPE_COMBINED) {
        return elapsed >= cooldown;
    }
    
    /* Different type has reduced cooldown */
    return elapsed >= (cooldown >> 1);
}

static void record_cue(cue_type_t type, uint32_t now_ms)
{
    s_state.last_cue_ms = now_ms;
    s_state.last_cue_type = type;
    s_state.cues_this_hour++;
    s_state.total_generated++;
}

static void suppress_cue(void)
{
    s_state.total_suppressed++;
}

static void update_history(uint8_t coherence)
{
    s_state.coherence_history[s_state.history_idx] = coherence;
    s_state.history_idx = (s_state.history_idx + 1) % CUE_HISTORY_SIZE;
    if (s_state.history_count < CUE_HISTORY_SIZE) {
        s_state.history_count++;
    }
}

static bool detect_deteriorating_trend(void)
{
    if (s_state.history_count < 6) {
        return false;
    }
    
    /* Simple slope: compare first half to second half */
    uint16_t first_sum = 0;
    uint16_t second_sum = 0;
    uint8_t half = s_state.history_count >> 1;
    
    uint8_t read_idx = (s_state.history_idx + CUE_HISTORY_SIZE - s_state.history_count) % CUE_HISTORY_SIZE;
    
    for (uint8_t i = 0; i < half; i++) {
        first_sum += s_state.coherence_history[(read_idx + i) % CUE_HISTORY_SIZE];
    }
    for (uint8_t i = half; i < s_state.history_count; i++) {
        second_sum += s_state.coherence_history[(read_idx + i) % CUE_HISTORY_SIZE];
    }
    
    uint8_t first_avg = first_sum / half;
    uint8_t second_avg = second_sum / (s_state.history_count - half);
    
    /* Deteriorating if dropped by >10% */
    return (first_avg > second_avg) && ((first_avg - second_avg) > (first_avg / 10));
}

static void build_no_cue(cue_output_t *output)
{
    output->type = CUE_TYPE_NONE;
    output->priority = CUE_PRIORITY_LOW;
    output->thermal_intensity = 0;
    output->thermal_duration_s = 0;
    output->vib_pattern = 0;
    output->vib_intensity = 0;
    output->cooldown_ms = 0;
}

/*******************************************************************************
 * CUE GENERATION FUNCTIONS
 ******************************************************************************/

static void build_alert_cue(cue_output_t *output, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    output->type = CUE_TYPE_ALERT;
    output->priority = CUE_PRIORITY_ALERT;
    output->thermal_intensity = clamp_intensity(p->thermal_max, s_state.prefs.max_thermal_pct);
    output->thermal_duration_s = (20 * p->duration_mult) / 10;
    output->vib_pattern = VIB_PATTERN_ALERT;
    output->vib_intensity = clamp_intensity(p->vib_max, s_state.prefs.max_vib_pct);
    output->cooldown_ms = CUE_COOLDOWN_ALERT_MS;
    
    record_cue(CUE_TYPE_COMBINED, now_ms);
}

static void build_combined_cue(cue_output_t *output, const cue_input_t *input, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    /* Scale by severity */
    uint8_t coherence_deficit = CUE_COHERENCE_MEDIUM - input->coherence_pct;
    uint8_t max_deficit = CUE_COHERENCE_MEDIUM - CUE_COHERENCE_CRITICAL;
    uint8_t severity = (coherence_deficit * 100) / max_deficit;
    if (severity > 100) severity = 100;
    
    uint8_t thermal_range = p->thermal_max - p->thermal_base;
    uint8_t vib_range = p->vib_max - p->vib_base;
    
    output->type = CUE_TYPE_COMBINED;
    output->priority = CUE_PRIORITY_HIGH;
    output->thermal_intensity = clamp_intensity(
        p->thermal_base + (thermal_range * severity) / 100,
        s_state.prefs.max_thermal_pct
    );
    output->thermal_duration_s = (15 * p->duration_mult) / 10;
    output->vib_pattern = VIB_PATTERN_HEARTBEAT;
    output->vib_intensity = clamp_intensity(
        p->vib_base + (vib_range * severity) / 100,
        s_state.prefs.max_vib_pct
    );
    output->cooldown_ms = CUE_COOLDOWN_COMBINED_MS;
    
    record_cue(CUE_TYPE_COMBINED, now_ms);
}

static void build_breathing_cue(cue_output_t *output, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    output->type = CUE_TYPE_BREATHING;
    output->priority = CUE_PRIORITY_NORMAL;
    output->thermal_intensity = 0;
    output->thermal_duration_s = 0;
    output->vib_pattern = VIB_PATTERN_BREATHING;
    output->vib_intensity = clamp_intensity(
        (p->vib_base * 80) / 100,  /* Gentle for breathing */
        s_state.prefs.max_vib_pct
    );
    output->cooldown_ms = CUE_COOLDOWN_COMBINED_MS;  /* Long cooldown */
    
    record_cue(CUE_TYPE_VIBRATION, now_ms);
}

static void build_vibration_cue(cue_output_t *output, const cue_input_t *input, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    uint8_t pattern;
    uint8_t intensity;
    
    if (input->micro_var_pct100 > CUE_MICROVAR_HIGH) {
        pattern = VIB_PATTERN_DOUBLE;
        intensity = p->vib_max;
    } else {
        pattern = VIB_PATTERN_SINGLE;
        /* Scale between base and max */
        uint16_t range = input->micro_var_pct100 - CUE_MICROVAR_ELEVATED;
        uint16_t max_range = CUE_MICROVAR_HIGH - CUE_MICROVAR_ELEVATED;
        intensity = p->vib_base + ((p->vib_max - p->vib_base) * range) / max_range;
    }
    
    output->type = CUE_TYPE_VIBRATION;
    output->priority = CUE_PRIORITY_NORMAL;
    output->thermal_intensity = 0;
    output->thermal_duration_s = 0;
    output->vib_pattern = pattern;
    output->vib_intensity = clamp_intensity(intensity, s_state.prefs.max_vib_pct);
    output->cooldown_ms = CUE_COOLDOWN_VIBRATION_MS;
    
    record_cue(CUE_TYPE_VIBRATION, now_ms);
}

static void build_thermal_cue(cue_output_t *output, const cue_input_t *input, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    /* Scale by coherence deficit */
    uint8_t deficit = CUE_COHERENCE_MEDIUM - input->coherence_pct;
    uint8_t max_deficit = CUE_COHERENCE_MEDIUM - CUE_COHERENCE_CRITICAL;
    uint8_t severity = (deficit * 100) / max_deficit;
    if (severity > 100) severity = 100;
    
    uint8_t thermal_range = p->thermal_max - p->thermal_base;
    uint8_t intensity = p->thermal_base + (thermal_range * severity) / 100;
    uint8_t duration = 10 + (10 * severity) / 100;
    duration = (duration * p->duration_mult) / 10;
    
    output->type = CUE_TYPE_THERMAL;
    output->priority = CUE_PRIORITY_LOW;
    output->thermal_intensity = clamp_intensity(intensity, s_state.prefs.max_thermal_pct);
    output->thermal_duration_s = duration;
    output->vib_pattern = VIB_PATTERN_OFF;
    output->vib_intensity = 0;
    output->cooldown_ms = CUE_COOLDOWN_THERMAL_MS;
    
    record_cue(CUE_TYPE_THERMAL, now_ms);
}

static void build_preventive_cue(cue_output_t *output, uint32_t now_ms)
{
    const intensity_profile_t *p = &PROFILES[s_state.prefs.sensitivity];
    
    output->type = CUE_TYPE_THERMAL;
    output->priority = CUE_PRIORITY_LOW;
    output->thermal_intensity = clamp_intensity(p->thermal_base, s_state.prefs.max_thermal_pct);
    output->thermal_duration_s = (8 * p->duration_mult) / 10;
    output->vib_pattern = VIB_PATTERN_OFF;
    output->vib_intensity = 0;
    output->cooldown_ms = (CUE_COOLDOWN_THERMAL_MS * 3) / 2;
    
    record_cue(CUE_TYPE_THERMAL, now_ms);
}

static void build_check_fit_cue(cue_output_t *output, uint32_t now_ms)
{
    output->type = CUE_TYPE_CHECK_FIT;
    output->priority = CUE_PRIORITY_LOW;
    output->thermal_intensity = 0;
    output->thermal_duration_s = 0;
    output->vib_pattern = VIB_PATTERN_TRIPLE;
    output->vib_intensity = 20;  /* Very gentle */
    output->cooldown_ms = CUE_COOLDOWN_VIBRATION_MS * 3;
    
    s_state.consecutive_low_conf = 0;  /* Reset streak */
    record_cue(CUE_TYPE_VIBRATION, now_ms);
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void cue_processor_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    
    /* Default preferences */
    s_state.prefs.enabled = true;
    s_state.prefs.max_thermal_pct = 80;
    s_state.prefs.max_vib_pct = 70;
    s_state.prefs.quiet_start_hour = 22;
    s_state.prefs.quiet_end_hour = 7;
    s_state.prefs.sensitivity = 1;  /* normal */
    s_state.prefs.breathing_enabled = true;
    s_state.prefs.thermal_enabled = true;
    s_state.prefs.vibration_enabled = true;
    
    s_state.last_cue_type = CUE_TYPE_NONE;
    s_state.current_hour = 12;  /* Default to noon */
    
    s_state.initialized = true;
}

void cue_processor_set_preferences(const cue_preferences_t *prefs)
{
    if (prefs) {
        memcpy(&s_state.prefs, prefs, sizeof(cue_preferences_t));
    }
}

void cue_processor_get_preferences(cue_preferences_t *prefs)
{
    if (prefs) {
        memcpy(prefs, &s_state.prefs, sizeof(cue_preferences_t));
    }
}

bool cue_processor_generate(const cue_input_t *input, cue_output_t *output)
{
    if (!s_state.initialized || !input || !output) {
        build_no_cue(output);
        return false;
    }
    
    uint32_t now_ms = input->timestamp_ms;
    
    /* Master switch */
    if (!s_state.prefs.enabled) {
        build_no_cue(output);
        suppress_cue();
        return false;
    }
    
    /* Quiet hours */
    if (is_quiet_hours()) {
        build_no_cue(output);
        suppress_cue();
        return false;
    }
    
    /* Rate limit */
    if (!check_rate_limit(now_ms)) {
        build_no_cue(output);
        suppress_cue();
        return false;
    }
    
    /* Update history */
    update_history(input->coherence_pct);
    
    /* Confidence gating - most critical filter */
    if (input->confidence_pct < CUE_MIN_CONFIDENCE) {
        s_state.consecutive_low_conf++;
        
        /* After streak of low confidence, suggest check fit */
        if (s_state.consecutive_low_conf >= 3) {
            if (s_state.prefs.vibration_enabled &&
                can_trigger(CUE_TYPE_VIBRATION, now_ms, CUE_COOLDOWN_VIBRATION_MS * 2)) {
                build_check_fit_cue(output, now_ms);
                return true;
            }
        }
        
        build_no_cue(output);
        suppress_cue();
        return false;
    }
    
    /* Reset low confidence streak */
    s_state.consecutive_low_conf = 0;
    
    /* High artifact rate */
    if (input->artifact_rate_pct > 25) {
        build_no_cue(output);
        suppress_cue();
        return false;
    }
    
    /* =========== DECISION CASCADE =========== */
    
    /* 1. ALERT: Critical states */
    if (input->stress_level > 90 || input->micro_var_pct100 > CUE_MICROVAR_CRITICAL) {
        if (can_trigger(CUE_TYPE_COMBINED, now_ms, CUE_COOLDOWN_COMBINED_MS)) {
            build_alert_cue(output, now_ms);
            return true;
        }
    }
    
    /* 2. COMBINED: Low coherence + high variability */
    if (input->coherence_pct < CUE_COHERENCE_LOW && 
        input->micro_var_pct100 > CUE_MICROVAR_ELEVATED) {
        if (can_trigger(CUE_TYPE_COMBINED, now_ms, CUE_COOLDOWN_COMBINED_MS)) {
            build_combined_cue(output, input, now_ms);
            return true;
        }
    }
    
    /* 3. BREATHING: Unstable coherence */
    if (input->stability_pct < CUE_STABILITY_UNSTABLE && 
        s_state.prefs.breathing_enabled && 
        s_state.prefs.vibration_enabled) {
        if (can_trigger(CUE_TYPE_VIBRATION, now_ms, CUE_COOLDOWN_COMBINED_MS)) {
            build_breathing_cue(output, now_ms);
            return true;
        }
    }
    
    /* 4. VIBRATION: Elevated micro-variability */
    if (input->micro_var_pct100 > CUE_MICROVAR_ELEVATED && s_state.prefs.vibration_enabled) {
        if (can_trigger(CUE_TYPE_VIBRATION, now_ms, CUE_COOLDOWN_VIBRATION_MS)) {
            build_vibration_cue(output, input, now_ms);
            return true;
        }
    }
    
    /* 5. THERMAL: Medium-low coherence */
    if (input->coherence_pct < CUE_COHERENCE_MEDIUM && s_state.prefs.thermal_enabled) {
        if (can_trigger(CUE_TYPE_THERMAL, now_ms, CUE_COOLDOWN_THERMAL_MS)) {
            build_thermal_cue(output, input, now_ms);
            return true;
        }
    }
    
    /* 6. PREVENTIVE: Deteriorating trend */
    if (detect_deteriorating_trend() && s_state.prefs.thermal_enabled) {
        if (can_trigger(CUE_TYPE_THERMAL, now_ms, CUE_COOLDOWN_THERMAL_MS * 2)) {
            build_preventive_cue(output, now_ms);
            return true;
        }
    }
    
    /* No intervention needed */
    build_no_cue(output);
    return false;
}

void cue_processor_reset(void)
{
    s_state.last_cue_ms = 0;
    s_state.last_cue_type = CUE_TYPE_NONE;
    s_state.consecutive_low_conf = 0;
    s_state.history_idx = 0;
    s_state.history_count = 0;
    s_state.cues_this_hour = 0;
    s_state.hour_start_ms = 0;
    memset(s_state.coherence_history, 0, sizeof(s_state.coherence_history));
}

void cue_processor_set_hour(uint8_t hour)
{
    if (hour < 24) {
        s_state.current_hour = hour;
    }
}

bool cue_processor_is_ready(void)
{
    return s_state.initialized && s_state.prefs.enabled;
}

void cue_processor_get_stats(
    uint32_t *cues_generated,
    uint32_t *cues_suppressed,
    cue_type_t *last_cue_type,
    uint32_t *last_cue_ms)
{
    if (cues_generated) *cues_generated = s_state.total_generated;
    if (cues_suppressed) *cues_suppressed = s_state.total_suppressed;
    if (last_cue_type) *last_cue_type = s_state.last_cue_type;
    if (last_cue_ms) *last_cue_ms = s_state.last_cue_ms;
}
