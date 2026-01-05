/**
 * @file signature_feel.c
 * @brief Neural Load Ring - Signature Feel Implementation
 *
 * "A calm, steady presence that helps you return to yourself."
 *
 * This implements the ring's personality through organic easing curves,
 * carefully timed patterns, and a consistent emotional vocabulary.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "signature_feel.h"
#include "vibration_feature.h"
#include "thermal_feature.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*******************************************************************************
 * EASING FUNCTIONS
 *
 * These create organic, natural-feeling transitions.
 * Input t: 0.0 to 1.0 (progress through transition)
 * Output: 0.0 to 1.0 (eased value)
 ******************************************************************************/

float ease_calculate(ease_curve_t curve, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    
    switch (curve) {
        case EASE_LINEAR:
            return t;
            
        case EASE_IN_SINE:
            /* Gentle start, like waking up */
            return 1.0f - cosf((t * M_PI) / 2.0f);
            
        case EASE_OUT_SINE:
            /* Gentle end, like falling asleep */
            return sinf((t * M_PI) / 2.0f);
            
        case EASE_IN_OUT_SINE:
            /* Breathing rhythm - our signature curve */
            return -(cosf(M_PI * t) - 1.0f) / 2.0f;
            
        case EASE_OUT_QUAD:
            /* Natural deceleration */
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case EASE_IN_QUAD:
            /* Natural acceleration */
            return t * t;
            
        case EASE_BREATH:
            /* Asymmetric breathing: faster in (40%), slower out (60%) */
            if (t < 0.4f) {
                /* Inhale: ease-in-out over first 40% */
                float inhale_t = t / 0.4f;
                return (-(cosf(M_PI * inhale_t) - 1.0f) / 2.0f);
            } else {
                /* Exhale: ease-out over remaining 60% */
                float exhale_t = (t - 0.4f) / 0.6f;
                return 1.0f - (exhale_t * exhale_t);
            }
            
        default:
            return t;
    }
}

uint8_t ease_intensity(uint8_t from, uint8_t to, ease_curve_t curve, float t)
{
    float eased = ease_calculate(curve, t);
    float result = (float)from + ((float)to - (float)from) * eased;
    
    if (result < 0.0f) return 0;
    if (result > 100.0f) return 100;
    return (uint8_t)(result + 0.5f);  /* Round */
}

/*******************************************************************************
 * PATTERN DEFINITIONS
 *
 * Each pattern is a sequence of steps with:
 *   - duration_ms: How long this step lasts
 *   - target_intensity: Target at end of step (0-100)
 *   - ease: Easing curve for transition TO this step
 *   - is_vibration: true = vibration, false = thermal
 *
 * Terminated by {0, 0, 0, false}
 ******************************************************************************/

typedef struct {
    uint16_t duration_ms;
    uint8_t  target_intensity;
    ease_curve_t ease;
    bool is_vibration;
} sig_step_t;

/*******************************************************************************
 * THE GROUNDING PULSE
 * "Come back to your body"
 *
 * A single, soft pulse that feels like a gentle tap on the shoulder.
 * Ramps up slowly, holds briefly, fades naturally.
 ******************************************************************************/
static const sig_step_t PATTERN_GROUNDING_PULSE[] = {
    /* Gentle ramp up (like a breath in) */
    { .duration_ms = 300, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    /* Brief hold at peak */
    { .duration_ms = 150, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    /* Slow fade out (like a breath out) */
    { .duration_ms = 450, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { 0, 0, 0, false }  /* End marker */
};

/*******************************************************************************
 * THE ATTENTION TAP
 * "Notice this"
 *
 * Two gentle taps, like a friend touching your hand.
 * Not urgent - just drawing awareness.
 ******************************************************************************/
static const sig_step_t PATTERN_ATTENTION_TAP[] = {
    /* First tap - gentle approach */
    { .duration_ms = 200, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 200, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    /* Pause - let it breathe */
    { .duration_ms = 200, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    /* Second tap - confirming */
    { .duration_ms = 200, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 300, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE PRESENCE CHECK
 * "I'm still here - are you?"
 *
 * Three very soft taps when ring fit might be loose.
 * Questioning, not demanding.
 ******************************************************************************/
static const sig_step_t PATTERN_PRESENCE_CHECK[] = {
    /* Three gentle taps, very soft */
    { .duration_ms = 150, .target_intensity = 25, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 150, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 150, .target_intensity = 25, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 150, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 150, .target_intensity = 25, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 250, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE HEARTBEAT
 * "Let me steady you"
 *
 * Lub-dub rhythm at a calming 75 BPM.
 * Like feeling a calm heartbeat through held hands.
 ******************************************************************************/
static const sig_step_t PATTERN_HEARTBEAT[] = {
    /* First beat (lub) */
    { .duration_ms = 120, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = 15, .ease = EASE_OUT_QUAD, .is_vibration = true },
    /* Second beat (dub) - slightly softer */
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    /* Rest period (completes ~800ms cycle = 75 BPM) */
    { .duration_ms = 400, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    
    /* Repeat pattern two more times for grounding */
    { .duration_ms = 120, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = 15, .ease = EASE_OUT_QUAD, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { .duration_ms = 400, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    
    { .duration_ms = 120, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = 15, .ease = EASE_OUT_QUAD, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 200, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE BREATHING GUIDE
 * "Let's breathe together"
 *
 * 4 second inhale, 6 second exhale - proven calming ratio.
 * Continuous pattern that loops until stopped.
 ******************************************************************************/
static const sig_step_t PATTERN_BREATHING_GUIDE[] = {
    /* Inhale (4s) - gentle rise */
    { .duration_ms = 4000, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    /* Brief hold at top */
    { .duration_ms = 300, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    /* Exhale (6s) - slow release */
    { .duration_ms = 6000, .target_intensity = 8, .ease = EASE_OUT_QUAD, .is_vibration = true },
    /* Pause at bottom */
    { .duration_ms = 500, .target_intensity = 5, .ease = EASE_LINEAR, .is_vibration = true },
    { 0, 0, 0, false }  /* Loops back */
};

/*******************************************************************************
 * THE WARM EXHALE
 * "You're safe. Slow down."
 *
 * A slow wave of warmth, like a blanket being placed over you.
 * Thermal only - no vibration.
 ******************************************************************************/
static const sig_step_t PATTERN_WARM_EXHALE[] = {
    /* Gentle rise - like warmth approaching */
    { .duration_ms = 2000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_IN_SINE, .is_vibration = false },
    /* Hold - let it soak in */
    { .duration_ms = 3000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_LINEAR, .is_vibration = false },
    /* Slow fade - like warmth dissipating naturally */
    { .duration_ms = 4000, .target_intensity = 15, .ease = EASE_OUT_SINE, .is_vibration = false },
    /* Final release */
    { .duration_ms = 2000, .target_intensity = 0, .ease = EASE_OUT_QUAD, .is_vibration = false },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE GROUNDING WARMTH
 * "I've got you"
 *
 * Steady, gentle warmth for preventive care.
 * Reliable, consistent presence.
 ******************************************************************************/
static const sig_step_t PATTERN_GROUNDING_WARMTH[] = {
    /* Approach warmth gently */
    { .duration_ms = 1500, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = false },
    /* Maintain steady presence */
    { .duration_ms = 5000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_LINEAR, .is_vibration = false },
    /* Slow, natural fade */
    { .duration_ms = 3000, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = false },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE SAFETY EMBRACE
 * "Everything is okay"
 *
 * Deeper warmth with slow pulse for critical coherence.
 * Comforting, enveloping.
 ******************************************************************************/
static const sig_step_t PATTERN_SAFETY_EMBRACE[] = {
    /* Approach */
    { .duration_ms = 2000, .target_intensity = SIG_THERMAL_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = false },
    /* Wave 1 */
    { .duration_ms = 2500, .target_intensity = SIG_THERMAL_MEDIUM, .ease = EASE_LINEAR, .is_vibration = false },
    { .duration_ms = 1500, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = false },
    /* Wave 2 */
    { .duration_ms = 1500, .target_intensity = SIG_THERMAL_MEDIUM, .ease = EASE_IN_OUT_SINE, .is_vibration = false },
    { .duration_ms = 2000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = false },
    /* Slow release */
    { .duration_ms = 4000, .target_intensity = 0, .ease = EASE_OUT_QUAD, .is_vibration = false },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE GENTLE ALERT
 * "Pause. Something changed."
 *
 * Brief warm pulse + single vibration for significant changes.
 * Not alarming - just bringing attention.
 ******************************************************************************/
static const sig_step_t PATTERN_GENTLE_ALERT[] = {
    /* Start with subtle warmth */
    { .duration_ms = 500, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_IN_SINE, .is_vibration = false },
    /* Add gentle vibration tap */
    { .duration_ms = 250, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 350, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    /* Hold warmth briefly */
    { .duration_ms = 1000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_LINEAR, .is_vibration = false },
    /* Fade warmth */
    { .duration_ms = 1500, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = false },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * THE FULL RESET  
 * "Let's start fresh together"
 *
 * Comprehensive intervention: warmth + heartbeat, then breathing guide.
 * For critical stress - the ring's most supportive response.
 ******************************************************************************/
static const sig_step_t PATTERN_FULL_RESET[] = {
    /* Begin with grounding warmth */
    { .duration_ms = 1500, .target_intensity = SIG_THERMAL_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = false },
    /* First heartbeat with warmth */
    { .duration_ms = 120, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = 15, .ease = EASE_OUT_QUAD, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    /* Maintain warmth during rest */
    { .duration_ms = 500, .target_intensity = SIG_THERMAL_MEDIUM, .ease = EASE_LINEAR, .is_vibration = false },
    /* Second heartbeat */
    { .duration_ms = 120, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_MEDIUM, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = 15, .ease = EASE_OUT_QUAD, .is_vibration = true },
    { .duration_ms = 100, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_SINE, .is_vibration = true },
    { .duration_ms = 80, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_LINEAR, .is_vibration = true },
    { .duration_ms = 120, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = true },
    /* Begin warmth fade, transition to breathing */
    { .duration_ms = 2000, .target_intensity = SIG_THERMAL_GENTLE, .ease = EASE_OUT_SINE, .is_vibration = false },
    /* Continue as breathing guide (one cycle) */
    { .duration_ms = 4000, .target_intensity = SIG_VIB_GENTLE, .ease = EASE_IN_OUT_SINE, .is_vibration = true },
    { .duration_ms = 6000, .target_intensity = 8, .ease = EASE_OUT_QUAD, .is_vibration = true },
    /* Final fade */
    { .duration_ms = 2000, .target_intensity = 0, .ease = EASE_OUT_SINE, .is_vibration = false },
    { .duration_ms = 500, .target_intensity = 0, .ease = EASE_LINEAR, .is_vibration = true },
    { 0, 0, 0, false }
};

/*******************************************************************************
 * PATTERN TABLE
 ******************************************************************************/

static const sig_step_t* PATTERNS[] = {
    [SIG_PATTERN_NONE]      = NULL,
    [SIG_GROUNDING_PULSE]   = PATTERN_GROUNDING_PULSE,
    [SIG_ATTENTION_TAP]     = PATTERN_ATTENTION_TAP,
    [SIG_PRESENCE_CHECK]    = PATTERN_PRESENCE_CHECK,
    [SIG_HEARTBEAT]         = PATTERN_HEARTBEAT,
    [SIG_BREATHING_GUIDE]   = PATTERN_BREATHING_GUIDE,
    [SIG_WARM_EXHALE]       = PATTERN_WARM_EXHALE,
    [SIG_GROUNDING_WARMTH]  = PATTERN_GROUNDING_WARMTH,
    [SIG_SAFETY_EMBRACE]    = PATTERN_SAFETY_EMBRACE,
    [SIG_GENTLE_ALERT]      = PATTERN_GENTLE_ALERT,
    [SIG_FULL_RESET]        = PATTERN_FULL_RESET,
};

/* Which patterns loop continuously */
static const bool PATTERN_LOOPS[] = {
    [SIG_PATTERN_NONE]      = false,
    [SIG_GROUNDING_PULSE]   = false,
    [SIG_ATTENTION_TAP]     = false,
    [SIG_PRESENCE_CHECK]    = false,
    [SIG_HEARTBEAT]         = false,
    [SIG_BREATHING_GUIDE]   = true,   /* Loops until stopped */
    [SIG_WARM_EXHALE]       = false,
    [SIG_GROUNDING_WARMTH]  = false,
    [SIG_SAFETY_EMBRACE]    = false,
    [SIG_GENTLE_ALERT]      = false,
    [SIG_FULL_RESET]        = false,
};

/*******************************************************************************
 * PLAYER STATE
 ******************************************************************************/

static struct {
    signature_pattern_t current_pattern;
    const sig_step_t   *steps;
    uint16_t           step_index;
    uint8_t            intensity_scale;  /* 0-100 user scaling */
    
    /* Current step timing */
    uint32_t           step_start_ms;
    uint8_t            step_from_intensity;
    
    /* Output tracking */
    uint8_t            vib_output;
    uint8_t            thermal_output;
    
    /* Fade-out state */
    bool               fading_out;
    uint32_t           fade_start_ms;
    uint8_t            fade_from_vib;
    uint8_t            fade_from_thermal;
    
    bool               initialized;
} s_sig = {0};

/*******************************************************************************
 * HARDWARE INTERFACE (wraps lower-level drivers)
 ******************************************************************************/

static void set_vibration(uint8_t intensity)
{
    if (intensity != s_sig.vib_output) {
        s_sig.vib_output = intensity;
        if (intensity == 0) {
            vibration_feature_off();
        } else {
            vibration_feature_on(intensity);
        }
    }
}

static void set_thermal(uint8_t intensity)
{
    if (intensity != s_sig.thermal_output) {
        s_sig.thermal_output = intensity;
        if (intensity == 0) {
            thermal_feature_stop();
        } else {
            thermal_feature_set_timed(intensity, 60);  /* 60s max safety limit */
        }
    }
}

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

void signature_init(void)
{
    memset(&s_sig, 0, sizeof(s_sig));
    s_sig.current_pattern = SIG_PATTERN_NONE;
    s_sig.initialized = true;
}

void signature_play(signature_pattern_t pattern, uint8_t intensity_scale)
{
    if (!s_sig.initialized) signature_init();
    
    if (pattern >= SIG_PATTERN_COUNT || pattern == SIG_PATTERN_NONE) {
        signature_stop();
        return;
    }
    
    if (PATTERNS[pattern] == NULL) {
        return;
    }
    
    /* Stop any current pattern */
    set_vibration(0);
    set_thermal(0);
    
    /* Start new pattern */
    s_sig.current_pattern = pattern;
    s_sig.steps = PATTERNS[pattern];
    s_sig.step_index = 0;
    s_sig.intensity_scale = (intensity_scale > 100) ? 100 : intensity_scale;
    s_sig.step_start_ms = 0;  /* Will be set on first tick */
    s_sig.step_from_intensity = 0;
    s_sig.fading_out = false;
}

void signature_stop(void)
{
    if (s_sig.current_pattern == SIG_PATTERN_NONE) return;
    
    /* Graceful fade-out */
    s_sig.fading_out = true;
    s_sig.fade_start_ms = 0;  /* Set on next tick */
    s_sig.fade_from_vib = s_sig.vib_output;
    s_sig.fade_from_thermal = s_sig.thermal_output;
}

void signature_stop_immediate(void)
{
    set_vibration(0);
    set_thermal(0);
    s_sig.current_pattern = SIG_PATTERN_NONE;
    s_sig.steps = NULL;
    s_sig.fading_out = false;
}

void signature_tick(uint32_t now_ms)
{
    if (!s_sig.initialized) return;
    
    /* Handle fade-out */
    if (s_sig.fading_out) {
        if (s_sig.fade_start_ms == 0) {
            s_sig.fade_start_ms = now_ms;
        }
        
        uint32_t elapsed = now_ms - s_sig.fade_start_ms;
        float t = (float)elapsed / (float)SIG_RAMP_DOWN_MS;
        
        if (t >= 1.0f) {
            /* Fade complete */
            signature_stop_immediate();
            return;
        }
        
        /* Fade both outputs using ease-out-sine */
        set_vibration(ease_intensity(s_sig.fade_from_vib, 0, EASE_OUT_SINE, t));
        set_thermal(ease_intensity(s_sig.fade_from_thermal, 0, EASE_OUT_SINE, t));
        return;
    }
    
    /* No pattern playing */
    if (s_sig.current_pattern == SIG_PATTERN_NONE || s_sig.steps == NULL) {
        return;
    }
    
    /* Initialize step timing on first tick */
    if (s_sig.step_start_ms == 0) {
        s_sig.step_start_ms = now_ms;
        s_sig.step_from_intensity = 0;
    }
    
    const sig_step_t *step = &s_sig.steps[s_sig.step_index];
    
    /* Calculate progress through current step */
    uint32_t elapsed = now_ms - s_sig.step_start_ms;
    float t = (step->duration_ms > 0) ? 
              (float)elapsed / (float)step->duration_ms : 1.0f;
    
    if (t > 1.0f) t = 1.0f;
    
    /* Calculate eased intensity */
    uint8_t target = (step->target_intensity * s_sig.intensity_scale) / 100;
    target = step->is_vibration ? 
             signature_safe_vibration(target) : 
             signature_safe_thermal(target);
    
    uint8_t current = ease_intensity(
        s_sig.step_from_intensity, 
        target, 
        step->ease, 
        t
    );
    
    /* Apply to appropriate output */
    if (step->is_vibration) {
        set_vibration(current);
    } else {
        set_thermal(current);
    }
    
    /* Check if step complete */
    if (elapsed >= step->duration_ms) {
        s_sig.step_index++;
        s_sig.step_start_ms = now_ms;
        s_sig.step_from_intensity = target;  /* Smooth transition */
        
        const sig_step_t *next = &s_sig.steps[s_sig.step_index];
        
        /* Check for pattern end */
        if (next->duration_ms == 0) {
            if (PATTERN_LOOPS[s_sig.current_pattern]) {
                /* Loop back to start */
                s_sig.step_index = 0;
            } else {
                /* Pattern complete - graceful end */
                signature_stop();
            }
        }
    }
}

bool signature_is_playing(void)
{
    return s_sig.current_pattern != SIG_PATTERN_NONE && !s_sig.fading_out;
}

signature_pattern_t signature_current_pattern(void)
{
    return s_sig.current_pattern;
}

uint8_t signature_safe_vibration(uint8_t requested)
{
    /* Never exceed comfortable maximum */
    if (requested > SIG_VIB_MAX_INTENSITY) {
        return SIG_VIB_MAX_INTENSITY;
    }
    return requested;
}

uint8_t signature_safe_thermal(uint8_t requested)
{
    /* Never exceed comfortable maximum */
    if (requested > SIG_THERMAL_MAX) {
        return SIG_THERMAL_MAX;
    }
    return requested;
}
