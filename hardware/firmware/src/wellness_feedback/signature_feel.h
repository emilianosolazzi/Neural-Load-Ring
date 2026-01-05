/**
 * @file signature_feel.h
 * @brief Neural Load Ring - Signature Feel Definition
 *
 * "A calm, steady presence that helps you return to yourself."
 *
 * This file defines the ring's personality through:
 *   - Consistent easing curves (how cues ramp up and down)
 *   - Emotional vocabulary for each modality
 *   - Signature patterns that form the ring's "voice"
 *   - Timing philosophy that feels intentional, not mechanical
 *
 * Design Philosophy:
 *   - The ring whispers, never shouts
 *   - Every cue feels like a gentle companion
 *   - Warmth = comfort, grounding, safety ("a hand on your shoulder")
 *   - Vibration = attention, awareness, reset ("a gentle tap")
 *   - Nothing should startle or overwhelm
 *   - All cues share the same "breath" - organic, not robotic
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef SIGNATURE_FEEL_H
#define SIGNATURE_FEEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * THE RING'S PERSONALITY
 *
 * North Star: "A calm, steady presence that helps you return to yourself."
 *
 * Thermal voice: Comfort, grounding, safety
 *   - Feels like: a hand on your shoulder, a warm blanket, a slow exhale
 *   - Character: Gentle, nurturing, patient
 *
 * Vibration voice: Attention, awareness, presence
 *   - Feels like: a gentle tap, a heartbeat, a quiet "I'm here"
 *   - Character: Soft, grounding, non-intrusive
 *
 ******************************************************************************/

/*******************************************************************************
 * EASING CURVES
 *
 * All cues use organic easing - never linear, never jarring.
 * Inspired by natural movements: breathing, heartbeats, ocean waves.
 ******************************************************************************/

/** Easing curve types for organic feel */
typedef enum {
    EASE_LINEAR = 0,        /**< Linear (use sparingly) */
    EASE_IN_SINE,           /**< Gentle start, like waking up */
    EASE_OUT_SINE,          /**< Gentle end, like falling asleep */
    EASE_IN_OUT_SINE,       /**< Breathing rhythm - the signature curve */
    EASE_OUT_QUAD,          /**< Natural deceleration */
    EASE_IN_QUAD,           /**< Natural acceleration */
    EASE_BREATH,            /**< Special: 4s in, 6s out asymmetric */
} ease_curve_t;

/** Calculate eased value (0.0-1.0 input, 0.0-1.0 output) */
float ease_calculate(ease_curve_t curve, float t);

/** Get intensity at time t using easing (for smooth transitions) */
uint8_t ease_intensity(uint8_t from, uint8_t to, ease_curve_t curve, float t);

/*******************************************************************************
 * TIMING PHILOSOPHY
 *
 * All durations designed around human perception and comfort.
 * Key principle: slow enough to feel intentional, fast enough to notice.
 ******************************************************************************/

/** Standard timing constants (milliseconds) */
#define SIG_RAMP_UP_MS          400     /**< How long to fade in (gentle) */
#define SIG_RAMP_DOWN_MS        600     /**< How long to fade out (slower, natural) */
#define SIG_PULSE_HOLD_MS       200     /**< How long to hold a pulse peak */
#define SIG_INTER_PULSE_MS      150     /**< Gap between pulse beats */
#define SIG_BREATH_INHALE_MS    4000    /**< Breathing: inhale duration */
#define SIG_BREATH_EXHALE_MS    6000    /**< Breathing: exhale duration */
#define SIG_BREATH_PAUSE_MS     500     /**< Breathing: pause between cycles */
#define SIG_HEARTBEAT_PERIOD_MS 800     /**< ~75 BPM, calming heart rate */

/** Intensity limits for safety and subtlety */
#define SIG_VIB_MAX_INTENSITY   65      /**< Never exceed (protects hand) */
#define SIG_VIB_GENTLE          35      /**< Default "whisper" level */
#define SIG_VIB_MEDIUM          50      /**< Noticeable but comfortable */
#define SIG_THERMAL_MAX         70      /**< Warm, not hot */
#define SIG_THERMAL_GENTLE      40      /**< Subtle warmth */
#define SIG_THERMAL_MEDIUM      55      /**< Comforting warmth */

/*******************************************************************************
 * SIGNATURE PATTERNS
 *
 * These are the ring's "vocabulary" - recognizable, consistent cues.
 ******************************************************************************/

typedef enum {
    SIG_PATTERN_NONE = 0,
    
    /* === VIBRATION PATTERNS === */
    
    /** The Grounding Pulse
     *  When: Micro-variability rises (neural chaos)
     *  Feel: "Come back to your body"
     *  Character: Single soft pulse with organic ramp */
    SIG_GROUNDING_PULSE,
    
    /** The Attention Tap
     *  When: Important state change detected
     *  Feel: "Notice this"
     *  Character: Two gentle taps, like a friend's touch */
    SIG_ATTENTION_TAP,
    
    /** The Presence Check
     *  When: Ring fit may be loose (low confidence streak)
     *  Feel: "I'm still here - are you?"
     *  Character: Three very soft taps */
    SIG_PRESENCE_CHECK,
    
    /** The Heartbeat
     *  When: Combined intervention needed
     *  Feel: "Let me steady you"
     *  Character: Lub-dub rhythm at calming 75 BPM */
    SIG_HEARTBEAT,
    
    /** The Breathing Guide
     *  When: Coherence unstable, needs rhythm
     *  Feel: "Let's breathe together"
     *  Character: 4s inhale, 6s exhale, continuous */
    SIG_BREATHING_GUIDE,
    
    /* === THERMAL PATTERNS === */
    
    /** The Warm Exhale
     *  When: Coherence dips
     *  Feel: "You're safe. Slow down."
     *  Character: Slow wave of warmth, like a blanket */
    SIG_WARM_EXHALE,
    
    /** The Grounding Warmth
     *  When: Preventive care during deteriorating trend
     *  Feel: "I've got you"
     *  Character: Steady, gentle warmth */
    SIG_GROUNDING_WARMTH,
    
    /** The Safety Embrace
     *  When: Critical coherence, needs comfort
     *  Feel: "Everything is okay"
     *  Character: Deeper warmth with slow pulse */
    SIG_SAFETY_EMBRACE,
    
    /* === COMBINED PATTERNS === */
    
    /** The Gentle Alert
     *  When: Significant state change
     *  Feel: "Pause. Something changed."
     *  Character: Brief warm pulse + single vibration */
    SIG_GENTLE_ALERT,
    
    /** The Full Reset
     *  When: Critical stress state
     *  Feel: "Let's start fresh together"
     *  Character: Warmth + heartbeat, then breathing guide */
    SIG_FULL_RESET,
    
    SIG_PATTERN_COUNT
} signature_pattern_t;

/*******************************************************************************
 * PATTERN PLAYER API
 ******************************************************************************/

/**
 * @brief Initialize signature feel system
 */
void signature_init(void);

/**
 * @brief Play a signature pattern
 * 
 * @param pattern The pattern to play
 * @param intensity_scale Scale factor 0-100 (applies to pattern's base intensity)
 */
void signature_play(signature_pattern_t pattern, uint8_t intensity_scale);

/**
 * @brief Stop any playing pattern gracefully (with fade-out)
 */
void signature_stop(void);

/**
 * @brief Stop immediately (for emergencies only)
 */
void signature_stop_immediate(void);

/**
 * @brief Process signature patterns (call from main loop)
 * @param now_ms Current timestamp
 */
void signature_tick(uint32_t now_ms);

/**
 * @brief Check if a pattern is currently playing
 * @return true if pattern active
 */
bool signature_is_playing(void);

/**
 * @brief Get currently playing pattern
 * @return Current pattern or SIG_PATTERN_NONE
 */
signature_pattern_t signature_current_pattern(void);

/*******************************************************************************
 * COMFORT UTILITIES
 ******************************************************************************/

/**
 * @brief Ensure intensity is within safe, comfortable range for vibration
 * @param requested Requested intensity 0-100
 * @return Clamped intensity that won't overwhelm
 */
uint8_t signature_safe_vibration(uint8_t requested);

/**
 * @brief Ensure intensity is within safe, comfortable range for thermal
 * @param requested Requested intensity 0-100
 * @return Clamped intensity that won't burn
 */
uint8_t signature_safe_thermal(uint8_t requested);

#ifdef __cplusplus
}
#endif

#endif /* SIGNATURE_FEEL_H */
