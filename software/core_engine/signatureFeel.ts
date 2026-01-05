/**
 * @file signatureFeel.ts
 * @brief Neural Load Ring - Signature Feel Vocabulary
 *
 * "A calm, steady presence that helps you return to yourself."
 *
 * This module defines the ring's haptic personality. Every interaction
 * should feel like the same thoughtful friend reaching out.
 *
 * DESIGN PHILOSOPHY:
 *   - The ring whispers, doesn't shout
 *   - Nothing that will make the user lose control of the hand
 *   - Thermal = comfort, safety, grounding
 *   - Vibration = attention, awareness, guidance
 *   - All transitions are organic, never jarring
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * SIGNATURE PATTERNS
 *
 * These are the ring's vocabulary. Each pattern has:
 *   - A name and emotional intention
 *   - A verbal cue (what the ring "says")
 *   - Recommended contexts for use
 ******************************************************************************/

export enum SignaturePattern {
  NONE = 0,

  // Vibration-primary patterns (attention, awareness)
  GROUNDING_PULSE = 1,    // "Come back to your body"
  ATTENTION_TAP = 2,      // "Notice this"
  PRESENCE_CHECK = 3,     // "I'm still here - are you?"
  HEARTBEAT = 4,          // "Let me steady you"
  BREATHING_GUIDE = 5,    // "Let's breathe together"

  // Thermal-primary patterns (comfort, safety)
  WARM_EXHALE = 6,        // "You're safe. Slow down."
  GROUNDING_WARMTH = 7,   // "I've got you"
  SAFETY_EMBRACE = 8,     // "Everything is okay"

  // Combined patterns (comprehensive support)
  GENTLE_ALERT = 9,       // "Pause. Something changed."
  FULL_RESET = 10,        // "Let's start fresh together"
}

/**
 * Pattern metadata for UI display and logging
 */
export const PATTERN_METADATA: Record<SignaturePattern, {
  name: string;
  verbal: string;
  emotion: string;
  modality: 'vibration' | 'thermal' | 'combined';
  duration: 'brief' | 'medium' | 'extended';
}> = {
  [SignaturePattern.NONE]: {
    name: 'None',
    verbal: '',
    emotion: 'neutral',
    modality: 'vibration',
    duration: 'brief',
  },
  [SignaturePattern.GROUNDING_PULSE]: {
    name: 'Grounding Pulse',
    verbal: 'Come back to your body',
    emotion: 'centering',
    modality: 'vibration',
    duration: 'brief',
  },
  [SignaturePattern.ATTENTION_TAP]: {
    name: 'Attention Tap',
    verbal: 'Notice this',
    emotion: 'alerting',
    modality: 'vibration',
    duration: 'brief',
  },
  [SignaturePattern.PRESENCE_CHECK]: {
    name: 'Presence Check',
    verbal: "I'm still here - are you?",
    emotion: 'questioning',
    modality: 'vibration',
    duration: 'brief',
  },
  [SignaturePattern.HEARTBEAT]: {
    name: 'Heartbeat',
    verbal: 'Let me steady you',
    emotion: 'grounding',
    modality: 'vibration',
    duration: 'medium',
  },
  [SignaturePattern.BREATHING_GUIDE]: {
    name: 'Breathing Guide',
    verbal: "Let's breathe together",
    emotion: 'guiding',
    modality: 'vibration',
    duration: 'extended',
  },
  [SignaturePattern.WARM_EXHALE]: {
    name: 'Warm Exhale',
    verbal: "You're safe. Slow down.",
    emotion: 'calming',
    modality: 'thermal',
    duration: 'extended',
  },
  [SignaturePattern.GROUNDING_WARMTH]: {
    name: 'Grounding Warmth',
    verbal: "I've got you",
    emotion: 'supportive',
    modality: 'thermal',
    duration: 'medium',
  },
  [SignaturePattern.SAFETY_EMBRACE]: {
    name: 'Safety Embrace',
    verbal: 'Everything is okay',
    emotion: 'comforting',
    modality: 'thermal',
    duration: 'extended',
  },
  [SignaturePattern.GENTLE_ALERT]: {
    name: 'Gentle Alert',
    verbal: 'Pause. Something changed.',
    emotion: 'attentive',
    modality: 'combined',
    duration: 'medium',
  },
  [SignaturePattern.FULL_RESET]: {
    name: 'Full Reset',
    verbal: "Let's start fresh together",
    emotion: 'comprehensive',
    modality: 'combined',
    duration: 'extended',
  },
};

/*******************************************************************************
 * TIMING PHILOSOPHY
 *
 * All transitions follow these principles:
 *   - Ramp up slowly (like waking)
 *   - Hold briefly at peak
 *   - Fade naturally (like falling asleep)
 *   - Never cut off abruptly
 ******************************************************************************/

export const TIMING = {
  /** Time to reach peak intensity (ms) */
  RAMP_UP_MS: 400,
  
  /** Time to fade to zero (ms) */
  RAMP_DOWN_MS: 600,
  
  /** Hold time at peak (ms) */
  HOLD_MS: 150,
  
  /** Gap between repeated elements (ms) */
  INTER_ELEMENT_GAP_MS: 200,
  
  /** Breathing guide: inhale duration (ms) */
  BREATH_INHALE_MS: 4000,
  
  /** Breathing guide: exhale duration (ms) - longer for calming */
  BREATH_EXHALE_MS: 6000,
  
  /** Heartbeat cycle (ms) - targets 75 BPM */
  HEARTBEAT_CYCLE_MS: 800,
} as const;

/*******************************************************************************
 * INTENSITY LIMITS
 *
 * Safety and comfort boundaries. The ring should never be jarring.
 ******************************************************************************/

export const INTENSITY = {
  /** Maximum vibration intensity (0-100) - ensures comfort */
  VIB_MAX: 65,
  
  /** Gentle vibration for subtle cues */
  VIB_GENTLE: 35,
  
  /** Medium vibration for attention */
  VIB_MEDIUM: 50,
  
  /** Maximum thermal intensity (0-100) - safety limit */
  THERMAL_MAX: 70,
  
  /** Gentle warmth for comfort */
  THERMAL_GENTLE: 45,
  
  /** Medium warmth for grounding */
  THERMAL_MEDIUM: 60,
  
  /** Minimum perceptible intensity */
  MIN_PERCEPTIBLE: 15,
} as const;

/*******************************************************************************
 * EASING CURVES
 *
 * Organic transitions - never linear, never jarring
 ******************************************************************************/

export type EaseCurve = 
  | 'linear'
  | 'easeInSine'
  | 'easeOutSine'
  | 'easeInOutSine'
  | 'easeOutQuad'
  | 'easeInQuad'
  | 'breath';

/**
 * Calculate eased value
 * @param curve - Easing curve type
 * @param t - Progress (0 to 1)
 * @returns Eased value (0 to 1)
 */
export function ease(curve: EaseCurve, t: number): number {
  if (t <= 0) return 0;
  if (t >= 1) return 1;

  switch (curve) {
    case 'linear':
      return t;

    case 'easeInSine':
      // Gentle start, like waking up
      return 1 - Math.cos((t * Math.PI) / 2);

    case 'easeOutSine':
      // Gentle end, like falling asleep
      return Math.sin((t * Math.PI) / 2);

    case 'easeInOutSine':
      // Breathing rhythm - our signature curve
      return -(Math.cos(Math.PI * t) - 1) / 2;

    case 'easeOutQuad':
      // Natural deceleration
      return 1 - (1 - t) * (1 - t);

    case 'easeInQuad':
      // Natural acceleration
      return t * t;

    case 'breath':
      // Asymmetric breathing: faster in (40%), slower out (60%)
      if (t < 0.4) {
        const inhaleT = t / 0.4;
        return -(Math.cos(Math.PI * inhaleT) - 1) / 2;
      } else {
        const exhaleT = (t - 0.4) / 0.6;
        return 1 - exhaleT * exhaleT;
      }

    default:
      return t;
  }
}

/**
 * Calculate intensity transition
 */
export function easeIntensity(
  from: number,
  to: number,
  curve: EaseCurve,
  t: number
): number {
  const eased = ease(curve, t);
  const result = from + (to - from) * eased;
  return Math.max(0, Math.min(100, Math.round(result)));
}

/*******************************************************************************
 * CUE MAPPING
 *
 * Maps HRV states to appropriate signature patterns
 ******************************************************************************/

export type HRVCueType = 
  | 'alert'
  | 'combined'
  | 'breathing'
  | 'vibration'
  | 'thermal'
  | 'checkFit';

export type CuePriority = 'low' | 'normal' | 'high' | 'alert';

/**
 * Map HRV cue type and priority to signature pattern
 */
export function mapCueToPattern(
  cueType: HRVCueType,
  priority: CuePriority
): SignaturePattern {
  // Alert priority - most comprehensive response
  if (priority === 'alert') {
    switch (cueType) {
      case 'alert':
      case 'combined':
      case 'vibration':
        return SignaturePattern.FULL_RESET;
      case 'thermal':
        return SignaturePattern.SAFETY_EMBRACE;
      default:
        return SignaturePattern.FULL_RESET;
    }
  }

  // High priority - grounding response
  if (priority === 'high') {
    switch (cueType) {
      case 'combined':
      case 'vibration':
        return SignaturePattern.HEARTBEAT;
      case 'thermal':
        return SignaturePattern.SAFETY_EMBRACE;
      default:
        return SignaturePattern.GENTLE_ALERT;
    }
  }

  // Low priority - subtle patterns
  if (priority === 'low') {
    switch (cueType) {
      case 'vibration':
        return SignaturePattern.GROUNDING_PULSE;
      case 'thermal':
        return SignaturePattern.GROUNDING_WARMTH;
      case 'checkFit':
        return SignaturePattern.PRESENCE_CHECK;
      default:
        return SignaturePattern.GROUNDING_PULSE;
    }
  }

  // Normal priority - situation-appropriate
  switch (cueType) {
    case 'breathing':
      return SignaturePattern.BREATHING_GUIDE;
    case 'vibration':
      return SignaturePattern.ATTENTION_TAP;
    case 'thermal':
      return SignaturePattern.WARM_EXHALE;
    case 'combined':
      return SignaturePattern.GENTLE_ALERT;
    case 'checkFit':
      return SignaturePattern.PRESENCE_CHECK;
    default:
      return SignaturePattern.GROUNDING_PULSE;
  }
}

/**
 * Get recommended intensity scale for a pattern
 * @param pattern - Signature pattern
 * @param sensitivityMode - User's sensitivity preference
 * @returns Intensity scale factor (0-100)
 */
export function getPatternIntensityScale(
  pattern: SignaturePattern,
  sensitivityMode: 'subtle' | 'normal' | 'assertive'
): number {
  const metadata = PATTERN_METADATA[pattern];
  
  const baseScales: Record<typeof sensitivityMode, number> = {
    subtle: 60,
    normal: 80,
    assertive: 100,
  };

  let scale = baseScales[sensitivityMode];

  // Adjust for pattern type
  if (metadata.modality === 'thermal') {
    // Thermal is less intrusive, can be slightly higher
    scale = Math.min(100, scale + 10);
  }

  if (metadata.duration === 'extended') {
    // Longer patterns should be gentler
    scale = Math.max(50, scale - 10);
  }

  return scale;
}

/*******************************************************************************
 * BLE COMMAND GENERATION
 *
 * Generate commands for the ring's signature feel system
 ******************************************************************************/

/**
 * BLE command to play a signature pattern
 */
export interface SignatureCommand {
  pattern: SignaturePattern;
  intensityScale: number;  // 0-100
}

/**
 * Create BLE command bytes for signature pattern
 * Command format: [0x05, pattern_id, intensity_scale]
 */
export function createSignatureCommand(
  pattern: SignaturePattern,
  intensityScale: number
): Uint8Array {
  const SIGNATURE_COMMAND_ID = 0x05;
  const clampedIntensity = Math.max(0, Math.min(100, Math.round(intensityScale)));
  
  return new Uint8Array([
    SIGNATURE_COMMAND_ID,
    pattern,
    clampedIntensity,
  ]);
}

/**
 * Create BLE command to stop any playing pattern
 * Command format: [0x05, 0x00, 0x00] (pattern NONE)
 */
export function createStopCommand(): Uint8Array {
  return createSignatureCommand(SignaturePattern.NONE, 0);
}
