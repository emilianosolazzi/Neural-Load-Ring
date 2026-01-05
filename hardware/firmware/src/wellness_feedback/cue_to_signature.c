/**
 * @file cue_to_signature.c
 * @brief Neural Load Ring - Cue to Signature Feel Mapping Implementation
 *
 * "Every touch should feel like the same friend reaching out."
 *
 * This bridge ensures all cue decisions express through the ring's
 * consistent personality, using the signature feel vocabulary.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "cue_to_signature.h"

/*******************************************************************************
 * MAPPING TABLE
 *
 * Translates cue types × priorities to signature patterns.
 *
 * Philosophy:
 *   - ALERT → Full Reset (the ring's most supportive intervention)
 *   - COMBINED → Heartbeat + Safety Embrace (grounding + comfort)
 *   - BREATHING → Breathing Guide (direct guidance)
 *   - VIBRATION → Grounding Pulse or Attention Tap (awareness)
 *   - THERMAL → Warm Exhale or Grounding Warmth (comfort)
 *   - CHECK_FIT → Presence Check (questioning, not demanding)
 ******************************************************************************/

static const cue_signature_mapping_t MAPPINGS[] = {
    /* Critical alert - most comprehensive response */
    { .cue_type = CUE_TYPE_COMBINED, .priority = CUE_PRIORITY_ALERT, .pattern = SIG_FULL_RESET },
    { .cue_type = CUE_TYPE_VIBRATION, .priority = CUE_PRIORITY_ALERT, .pattern = SIG_FULL_RESET },
    { .cue_type = CUE_TYPE_THERMAL, .priority = CUE_PRIORITY_ALERT, .pattern = SIG_SAFETY_EMBRACE },
    
    /* High priority combined - heartbeat for grounding */
    { .cue_type = CUE_TYPE_COMBINED, .priority = CUE_PRIORITY_HIGH, .pattern = SIG_HEARTBEAT },
    { .cue_type = CUE_TYPE_VIBRATION, .priority = CUE_PRIORITY_HIGH, .pattern = SIG_HEARTBEAT },
    { .cue_type = CUE_TYPE_THERMAL, .priority = CUE_PRIORITY_HIGH, .pattern = SIG_SAFETY_EMBRACE },
    
    /* Normal priority - situation-appropriate patterns */
    { .cue_type = CUE_TYPE_BREATHING, .priority = CUE_PRIORITY_NORMAL, .pattern = SIG_BREATHING_GUIDE },
    { .cue_type = CUE_TYPE_VIBRATION, .priority = CUE_PRIORITY_NORMAL, .pattern = SIG_ATTENTION_TAP },
    { .cue_type = CUE_TYPE_THERMAL, .priority = CUE_PRIORITY_NORMAL, .pattern = SIG_WARM_EXHALE },
    { .cue_type = CUE_TYPE_COMBINED, .priority = CUE_PRIORITY_NORMAL, .pattern = SIG_GENTLE_ALERT },
    
    /* Low priority - subtle touches */
    { .cue_type = CUE_TYPE_VIBRATION, .priority = CUE_PRIORITY_LOW, .pattern = SIG_GROUNDING_PULSE },
    { .cue_type = CUE_TYPE_THERMAL, .priority = CUE_PRIORITY_LOW, .pattern = SIG_GROUNDING_WARMTH },
    { .cue_type = CUE_TYPE_CHECK_FIT, .priority = CUE_PRIORITY_LOW, .pattern = SIG_PRESENCE_CHECK },
    
    /* Fallbacks */
    { .cue_type = CUE_TYPE_ALERT, .priority = CUE_PRIORITY_ALERT, .pattern = SIG_FULL_RESET },
    { .cue_type = CUE_TYPE_ALERT, .priority = CUE_PRIORITY_HIGH, .pattern = SIG_HEARTBEAT },
    { .cue_type = CUE_TYPE_ALERT, .priority = CUE_PRIORITY_NORMAL, .pattern = SIG_GENTLE_ALERT },
};

#define NUM_MAPPINGS (sizeof(MAPPINGS) / sizeof(MAPPINGS[0]))

/*******************************************************************************
 * INTENSITY SCALING
 *
 * The cue processor calculates intensity 0-100. We scale this to the
 * signature feel system's intensity range while respecting safety limits.
 ******************************************************************************/

static uint8_t calculate_intensity_scale(const cue_output_t *cue)
{
    /* Use the higher of thermal or vibration intensity */
    uint8_t base = cue->vib_intensity;
    if (cue->thermal_intensity > base) {
        base = cue->thermal_intensity;
    }
    
    /* Map cue intensity (0-100) to signature scale (0-100) */
    /* Apply a gentle curve: sqrt for perceptual linearity */
    /* This makes subtle cues more noticeable and prevents harsh peaks */
    
    if (base == 0) return 0;
    
    /* Perceptual scaling: intensity = sqrt(requested) * sqrt(100) */
    /* For 25 → ~50, for 50 → ~71, for 75 → ~87, for 100 → 100 */
    uint16_t scaled = base;
    
    /* Simple approximation of sqrt curve using linear segments */
    if (base < 25) {
        scaled = (base * 2);  /* 0-25 → 0-50 */
    } else if (base < 50) {
        scaled = 50 + ((base - 25) * 21 / 25);  /* 25-50 → 50-71 */
    } else if (base < 75) {
        scaled = 71 + ((base - 50) * 16 / 25);  /* 50-75 → 71-87 */
    } else {
        scaled = 87 + ((base - 75) * 13 / 25);  /* 75-100 → 87-100 */
    }
    
    if (scaled > 100) scaled = 100;
    
    return (uint8_t)scaled;
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

signature_pattern_t cue_to_signature_pattern(const cue_output_t *cue)
{
    if (!cue || cue->type == CUE_TYPE_NONE) {
        return SIG_PATTERN_NONE;
    }
    
    /* Search for best match: type + priority */
    for (uint8_t i = 0; i < NUM_MAPPINGS; i++) {
        if (MAPPINGS[i].cue_type == cue->type && 
            MAPPINGS[i].priority == cue->priority) {
            return MAPPINGS[i].pattern;
        }
    }
    
    /* Fallback: search by type only */
    for (uint8_t i = 0; i < NUM_MAPPINGS; i++) {
        if (MAPPINGS[i].cue_type == cue->type) {
            return MAPPINGS[i].pattern;
        }
    }
    
    /* Final fallback based on cue type category */
    switch (cue->type) {
        case CUE_TYPE_ALERT:
        case CUE_TYPE_COMBINED:
            return SIG_GENTLE_ALERT;
            
        case CUE_TYPE_BREATHING:
            return SIG_BREATHING_GUIDE;
            
        case CUE_TYPE_VIBRATION:
            return SIG_GROUNDING_PULSE;
            
        case CUE_TYPE_THERMAL:
            return SIG_WARM_EXHALE;
            
        case CUE_TYPE_CHECK_FIT:
            return SIG_PRESENCE_CHECK;
            
        default:
            return SIG_PATTERN_NONE;
    }
}

void cue_execute_as_signature(const cue_output_t *cue)
{
    if (!cue || cue->type == CUE_TYPE_NONE) {
        return;
    }
    
    signature_pattern_t pattern = cue_to_signature_pattern(cue);
    
    if (pattern == SIG_PATTERN_NONE) {
        return;
    }
    
    uint8_t intensity = calculate_intensity_scale(cue);
    
    /* Ensure minimum perceptible intensity */
    if (intensity > 0 && intensity < 15) {
        intensity = 15;
    }
    
    signature_play(pattern, intensity);
}

const cue_signature_mapping_t* cue_get_signature_mappings(uint8_t *count)
{
    if (count) {
        *count = NUM_MAPPINGS;
    }
    return MAPPINGS;
}
