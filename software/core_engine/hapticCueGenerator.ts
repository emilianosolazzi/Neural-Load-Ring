/**
 * @file hapticCueGenerator.ts
 * @brief Neural Load Ring - HRV-to-Haptic Cue Generator
 *
 * This is the "intelligence layer" that connects HRV analysis to physical feedback.
 * Converts wellness engine outputs into actuator commands based on:
 *
 *   • Micro-variability → Vibration (neural chaos → grounding pulse)
 *   • Coherence dips → Thermal (low coherence → warming comfort)
 *   • Stability → Breathing patterns (unstable → guided breathing)
 *   • Confidence gating → Suppress cues when data quality is poor
 *
 * Design Philosophy:
 *   - Subtle, not intrusive - the ring whispers, doesn't shout
 *   - Evidence-based thresholds from HRV literature
 *   - Respects user's autonomy (quiet hours, intensity caps)
 *   - Adaptive cooldown to prevent habituation
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * VIBRATION PATTERNS (mirrors bleService.ts)
 ******************************************************************************/

export enum VibrationPattern {
  OFF = 0,
  SINGLE = 1,
  DOUBLE = 2,
  TRIPLE = 3,
  HEARTBEAT = 4,
  BREATHING = 5,
  ALERT = 6,
}

/** Actuator command to ring */
export interface ActuatorCommand {
  thermalIntensity: number;      // 0-100
  thermalDurationS: number;      // 0 = off
  vibrationPattern: VibrationPattern;
  vibrationIntensity: number;    // 0-100
}

/*******************************************************************************
 * TYPES
 ******************************************************************************/

/** Processed result from wellness engine (subset needed for cue generation) */
export interface EngineMetrics {
  timestamp: number;
  microVariability: number;      // 0-1 scale, higher = more chaotic
  meanCoherence: number;         // 0-1 scale, higher = more coherent
  coherenceStability: number;    // 0-1 scale, higher = more stable
  confidence: number;            // 0-1 scale, measurement reliability
  stress_awarenessLevel: 'optimal' | 'low' | 'moderate' | 'high' | 'needs_attention';
  trend: 'improving' | 'stable' | 'deteriorating';
  artifactRate: number;          // 0-1, proportion of bad data
  respiratoryDetection?: {
    detected: boolean;
    frequencyHz: number;
    confidence: number;
  };
}

/** Cue output to send to ring */
export interface HapticCue {
  shouldTrigger: boolean;
  command: ActuatorCommand | null;
  reason: string;
  priority: 'low' | 'normal' | 'high' | 'alert';
  cooldownMs: number;            // Recommended wait before next cue
}

/** User preferences for cue behavior */
export interface CuePreferences {
  enabled: boolean;              // Master switch
  maxThermalIntensity: number;   // 0-100
  maxVibrationIntensity: number; // 0-100
  quietHoursStart: number;       // 0-23
  quietHoursEnd: number;         // 0-23
  sensitivityMode: 'subtle' | 'normal' | 'assertive';
  breathingGuidanceEnabled: boolean;
  thermalEnabled: boolean;
  vibrationEnabled: boolean;
}

/** Internal state for adaptive cue timing */
interface CueState {
  lastCueTimestamp: number;
  lastCueType: 'thermal' | 'vibration' | 'combined' | 'none';
  consecutiveLowConfidence: number;
  coherenceHistory: number[];    // Rolling window for trend detection
  microVarHistory: number[];
  cuesInLastHour: number;
  lastHourReset: number;
}

/*******************************************************************************
 * CONSTANTS - Evidence-Based Thresholds
 ******************************************************************************/

export const CUE_THRESHOLDS = {
  // Confidence gating (don't act on bad data)
  MIN_CONFIDENCE: 0.60,          // Below this, suppress all cues
  LOW_CONFIDENCE_STREAK: 3,      // After N low-conf readings, suggest rest

  // Micro-variability triggers (Pincus 1991, Goldberger 2002)
  MICROVAR: {
    HEALTHY: 0.02,               // No action needed
    ELEVATED: 0.05,              // Gentle vibration nudge
    HIGH: 0.08,                  // Stronger grounding pulse
    CRITICAL: 0.12,              // Alert pattern
  },

  // Coherence triggers (McCraty & Shaffer 2015)
  COHERENCE: {
    HIGH: 0.75,                  // Great state, no intervention
    MEDIUM: 0.50,                // Light thermal comfort
    LOW: 0.30,                   // More substantial warmth
    CRITICAL: 0.15,              // Combined intervention
  },

  // Stability triggers for breathing guidance
  STABILITY: {
    STABLE: 0.70,                // No guidance needed
    UNSTABLE: 0.40,              // Suggest breathing pattern
    CHAOTIC: 0.20,               // Stronger breathing cue
  },

  // Cooldown periods (prevent habituation)
  COOLDOWN_MS: {
    VIBRATION: 30_000,           // 30s between vibration cues
    THERMAL: 120_000,            // 2 min between thermal
    COMBINED: 180_000,           // 3 min between combined
    BREATHING: 300_000,          // 5 min between breathing cues
    AFTER_ALERT: 600_000,        // 10 min after alert
  },

  // Rate limiting
  MAX_CUES_PER_HOUR: 12,         // Don't overwhelm user

  // Trend detection window
  HISTORY_WINDOW: 10,            // Number of readings to track
} as const;

/*******************************************************************************
 * INTENSITY MAPPING
 ******************************************************************************/

const INTENSITY_PROFILES = {
  subtle: {
    thermalBase: 25,
    thermalMax: 50,
    vibrationBase: 15,
    vibrationMax: 40,
    durationScale: 0.7,
  },
  normal: {
    thermalBase: 35,
    thermalMax: 70,
    vibrationBase: 30,
    vibrationMax: 60,
    durationScale: 1.0,
  },
  assertive: {
    thermalBase: 45,
    thermalMax: 85,
    vibrationBase: 45,
    vibrationMax: 80,
    durationScale: 1.3,
  },
} as const;

/*******************************************************************************
 * CUE GENERATOR CLASS
 ******************************************************************************/

export class HapticCueGenerator {
  private state: CueState;
  private preferences: CuePreferences;

  constructor(preferences?: Partial<CuePreferences>) {
    this.preferences = {
      enabled: true,
      maxThermalIntensity: 80,
      maxVibrationIntensity: 70,
      quietHoursStart: 22,
      quietHoursEnd: 7,
      sensitivityMode: 'normal',
      breathingGuidanceEnabled: true,
      thermalEnabled: true,
      vibrationEnabled: true,
      ...preferences,
    };

    this.state = {
      lastCueTimestamp: 0,
      lastCueType: 'none',
      consecutiveLowConfidence: 0,
      coherenceHistory: [],
      microVarHistory: [],
      cuesInLastHour: 0,
      lastHourReset: Date.now(),
    };
  }

  /**
   * Main entry point: Generate haptic cue from engine metrics
   */
  generateCue(metrics: EngineMetrics): HapticCue {
    // Master switch
    if (!this.preferences.enabled) {
      return this.noCue('Cues disabled by user preference');
    }

    // Quiet hours check
    if (this.isQuietHours()) {
      return this.noCue('Quiet hours active');
    }

    // Rate limit check
    if (!this.checkRateLimit(metrics.timestamp)) {
      return this.noCue('Rate limit reached (max cues per hour)');
    }

    // Update history for trend detection
    this.updateHistory(metrics);

    // Confidence gating - the most critical filter
    if (metrics.confidence < CUE_THRESHOLDS.MIN_CONFIDENCE) {
      this.state.consecutiveLowConfidence++;
      
      // After streak of low confidence, suggest a "check fit" vibration
      if (this.state.consecutiveLowConfidence >= CUE_THRESHOLDS.LOW_CONFIDENCE_STREAK) {
        return this.createCheckFitCue(metrics.timestamp);
      }
      
      return this.noCue(`Low confidence (${(metrics.confidence * 100).toFixed(0)}%)`);
    }
    
    // Reset streak on good reading
    this.state.consecutiveLowConfidence = 0;

    // High artifact rate - data is noisy
    if (metrics.artifactRate > 0.25) {
      return this.noCue(`High artifact rate (${(metrics.artifactRate * 100).toFixed(0)}%)`);
    }

    // Decision cascade (priority order)
    
    // 1. ALERT: Critical states need immediate attention
    if (metrics.stress_awarenessLevel === 'needs_attention' || 
        metrics.microVariability > CUE_THRESHOLDS.MICROVAR.CRITICAL) {
      return this.createAlertCue(metrics);
    }

    // 2. COMBINED: Low coherence + high variability = comprehensive support
    if (metrics.meanCoherence < CUE_THRESHOLDS.COHERENCE.LOW && 
        metrics.microVariability > CUE_THRESHOLDS.MICROVAR.ELEVATED) {
      return this.createCombinedCue(metrics);
    }

    // 3. BREATHING: Unstable coherence + detected respiratory pattern
    if (metrics.coherenceStability < CUE_THRESHOLDS.STABILITY.UNSTABLE && 
        this.preferences.breathingGuidanceEnabled) {
      return this.createBreathingCue(metrics);
    }

    // 4. VIBRATION: Elevated micro-variability (neural noise → grounding)
    if (metrics.microVariability > CUE_THRESHOLDS.MICROVAR.ELEVATED) {
      return this.createVibrationCue(metrics);
    }

    // 5. THERMAL: Medium-low coherence (comfort warmth)
    if (metrics.meanCoherence < CUE_THRESHOLDS.COHERENCE.MEDIUM) {
      return this.createThermalCue(metrics);
    }

    // 6. TREND: Deteriorating trend even if current values are OK
    if (metrics.trend === 'deteriorating' && this.detectDeterioratingTrend()) {
      return this.createPreventiveCue(metrics);
    }

    // Optimal state - no intervention needed
    return this.noCue('Optimal autonomic state');
  }

  /**
   * Update preferences (from settings screen)
   */
  updatePreferences(updates: Partial<CuePreferences>): void {
    this.preferences = { ...this.preferences, ...updates };
  }

  /**
   * Get current preferences (for settings display)
   */
  getPreferences(): CuePreferences {
    return { ...this.preferences };
  }

  /**
   * Reset state (e.g., after user explicitly calms down)
   */
  reset(): void {
    this.state = {
      lastCueTimestamp: 0,
      lastCueType: 'none',
      consecutiveLowConfidence: 0,
      coherenceHistory: [],
      microVarHistory: [],
      cuesInLastHour: 0,
      lastHourReset: Date.now(),
    };
  }

  /*******************************************************************************
   * CUE CREATION METHODS
   ******************************************************************************/

  private createAlertCue(metrics: EngineMetrics): HapticCue {
    if (!this.canTriggerCue('combined', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.COMBINED)) {
      return this.noCue('Alert cooldown active');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];
    
    const command: ActuatorCommand = {
      thermalIntensity: this.clampIntensity(
        profile.thermalMax, 
        this.preferences.maxThermalIntensity
      ),
      thermalDurationS: Math.round(20 * profile.durationScale),
      vibrationPattern: VibrationPattern.ALERT,
      vibrationIntensity: this.clampIntensity(
        profile.vibrationMax,
        this.preferences.maxVibrationIntensity
      ),
    };

    this.recordCue('combined', metrics.timestamp);
    
    return {
      shouldTrigger: true,
      command,
      reason: 'Critical stress state detected - needs attention',
      priority: 'alert',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.AFTER_ALERT,
    };
  }

  private createCombinedCue(metrics: EngineMetrics): HapticCue {
    if (!this.canTriggerCue('combined', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.COMBINED)) {
      return this.noCue('Combined cue cooldown active');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];
    
    // Scale intensity by severity
    const coherenceSeverity = 1 - (metrics.meanCoherence / CUE_THRESHOLDS.COHERENCE.MEDIUM);
    const microVarSeverity = (metrics.microVariability - CUE_THRESHOLDS.MICROVAR.ELEVATED) / 
                             (CUE_THRESHOLDS.MICROVAR.CRITICAL - CUE_THRESHOLDS.MICROVAR.ELEVATED);
    
    const severity = Math.min(1, (coherenceSeverity + microVarSeverity) / 2);

    const command: ActuatorCommand = {
      thermalIntensity: this.clampIntensity(
        profile.thermalBase + (profile.thermalMax - profile.thermalBase) * severity,
        this.preferences.maxThermalIntensity
      ),
      thermalDurationS: Math.round(15 * profile.durationScale),
      vibrationPattern: VibrationPattern.HEARTBEAT, // Calming rhythm
      vibrationIntensity: this.clampIntensity(
        profile.vibrationBase + (profile.vibrationMax - profile.vibrationBase) * severity,
        this.preferences.maxVibrationIntensity
      ),
    };

    this.recordCue('combined', metrics.timestamp);

    return {
      shouldTrigger: true,
      command,
      reason: `Low coherence (${(metrics.meanCoherence * 100).toFixed(0)}%) with elevated variability`,
      priority: 'high',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.COMBINED,
    };
  }

  private createBreathingCue(metrics: EngineMetrics): HapticCue {
    if (!this.canTriggerCue('vibration', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.BREATHING)) {
      return this.noCue('Breathing cue cooldown active');
    }

    // Only if vibration is enabled
    if (!this.preferences.vibrationEnabled) {
      return this.noCue('Vibration disabled in preferences');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];
    
    // Breathing pattern vibration to guide user
    const command: ActuatorCommand = {
      thermalIntensity: 0,
      thermalDurationS: 0,
      vibrationPattern: VibrationPattern.BREATHING,
      vibrationIntensity: this.clampIntensity(
        profile.vibrationBase * 0.8, // Gentle for breathing
        this.preferences.maxVibrationIntensity
      ),
    };

    this.recordCue('vibration', metrics.timestamp);

    return {
      shouldTrigger: true,
      command,
      reason: `Unstable coherence (stability: ${(metrics.coherenceStability * 100).toFixed(0)}%) - breathing guidance`,
      priority: 'normal',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.BREATHING,
    };
  }

  private createVibrationCue(metrics: EngineMetrics): HapticCue {
    if (!this.canTriggerCue('vibration', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.VIBRATION)) {
      return this.noCue('Vibration cooldown active');
    }

    if (!this.preferences.vibrationEnabled) {
      return this.noCue('Vibration disabled in preferences');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];
    
    // Map micro-variability to pattern
    let pattern: VibrationPattern;
    let intensity: number;

    if (metrics.microVariability > CUE_THRESHOLDS.MICROVAR.HIGH) {
      pattern = VibrationPattern.DOUBLE;
      intensity = profile.vibrationMax;
    } else {
      pattern = VibrationPattern.SINGLE;
      intensity = profile.vibrationBase + 
        (profile.vibrationMax - profile.vibrationBase) * 
        ((metrics.microVariability - CUE_THRESHOLDS.MICROVAR.ELEVATED) / 
         (CUE_THRESHOLDS.MICROVAR.HIGH - CUE_THRESHOLDS.MICROVAR.ELEVATED));
    }

    const command: ActuatorCommand = {
      thermalIntensity: 0,
      thermalDurationS: 0,
      vibrationPattern: pattern,
      vibrationIntensity: this.clampIntensity(intensity, this.preferences.maxVibrationIntensity),
    };

    this.recordCue('vibration', metrics.timestamp);

    return {
      shouldTrigger: true,
      command,
      reason: `Elevated micro-variability (${(metrics.microVariability * 100).toFixed(1)}%) - grounding pulse`,
      priority: 'normal',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.VIBRATION,
    };
  }

  private createThermalCue(metrics: EngineMetrics): HapticCue {
    if (!this.canTriggerCue('thermal', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.THERMAL)) {
      return this.noCue('Thermal cooldown active');
    }

    if (!this.preferences.thermalEnabled) {
      return this.noCue('Thermal disabled in preferences');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];
    
    // Scale intensity by how far below threshold
    const deficit = CUE_THRESHOLDS.COHERENCE.MEDIUM - metrics.meanCoherence;
    const maxDeficit = CUE_THRESHOLDS.COHERENCE.MEDIUM - CUE_THRESHOLDS.COHERENCE.CRITICAL;
    const severity = Math.min(1, deficit / maxDeficit);

    const intensity = profile.thermalBase + 
      (profile.thermalMax - profile.thermalBase) * severity;
    
    const duration = Math.round((10 + 10 * severity) * profile.durationScale);

    const command: ActuatorCommand = {
      thermalIntensity: this.clampIntensity(intensity, this.preferences.maxThermalIntensity),
      thermalDurationS: duration,
      vibrationPattern: VibrationPattern.OFF,
      vibrationIntensity: 0,
    };

    this.recordCue('thermal', metrics.timestamp);

    return {
      shouldTrigger: true,
      command,
      reason: `Low coherence (${(metrics.meanCoherence * 100).toFixed(0)}%) - comfort warmth`,
      priority: 'low',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.THERMAL,
    };
  }

  private createPreventiveCue(metrics: EngineMetrics): HapticCue {
    // Light touch when trend is bad but current values aren't critical
    if (!this.canTriggerCue('thermal', metrics.timestamp, CUE_THRESHOLDS.COOLDOWN_MS.THERMAL * 2)) {
      return this.noCue('Preventive cooldown active');
    }

    if (!this.preferences.thermalEnabled) {
      return this.noCue('Thermal disabled in preferences');
    }

    const profile = INTENSITY_PROFILES[this.preferences.sensitivityMode];

    const command: ActuatorCommand = {
      thermalIntensity: this.clampIntensity(profile.thermalBase, this.preferences.maxThermalIntensity),
      thermalDurationS: Math.round(8 * profile.durationScale),
      vibrationPattern: VibrationPattern.OFF,
      vibrationIntensity: 0,
    };

    this.recordCue('thermal', metrics.timestamp);

    return {
      shouldTrigger: true,
      command,
      reason: 'Deteriorating trend detected - gentle preventive warmth',
      priority: 'low',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.THERMAL * 1.5,
    };
  }

  private createCheckFitCue(timestamp: number): HapticCue {
    // Gentle prompt to check ring fit after multiple low-confidence readings
    if (!this.canTriggerCue('vibration', timestamp, CUE_THRESHOLDS.COOLDOWN_MS.VIBRATION * 2)) {
      return this.noCue('Check-fit cooldown active');
    }

    const command: ActuatorCommand = {
      thermalIntensity: 0,
      thermalDurationS: 0,
      vibrationPattern: VibrationPattern.TRIPLE,
      vibrationIntensity: 20, // Very gentle
    };

    this.recordCue('vibration', timestamp);
    this.state.consecutiveLowConfidence = 0; // Reset streak

    return {
      shouldTrigger: true,
      command,
      reason: 'Multiple low-confidence readings - check ring fit',
      priority: 'low',
      cooldownMs: CUE_THRESHOLDS.COOLDOWN_MS.VIBRATION * 3,
    };
  }

  private noCue(reason: string): HapticCue {
    return {
      shouldTrigger: false,
      command: null,
      reason,
      priority: 'low',
      cooldownMs: 0,
    };
  }

  /*******************************************************************************
   * HELPER METHODS
   ******************************************************************************/

  private isQuietHours(): boolean {
    const now = new Date();
    const hour = now.getHours();
    const { quietHoursStart, quietHoursEnd } = this.preferences;

    // Handle overnight quiet hours (e.g., 22:00 - 07:00)
    if (quietHoursStart > quietHoursEnd) {
      return hour >= quietHoursStart || hour < quietHoursEnd;
    }
    
    // Same-day quiet hours (e.g., 14:00 - 16:00)
    return hour >= quietHoursStart && hour < quietHoursEnd;
  }

  private checkRateLimit(timestamp: number): boolean {
    // Reset hourly counter
    const hourAgo = timestamp - 3_600_000;
    if (this.state.lastHourReset < hourAgo) {
      this.state.cuesInLastHour = 0;
      this.state.lastHourReset = timestamp;
    }

    return this.state.cuesInLastHour < CUE_THRESHOLDS.MAX_CUES_PER_HOUR;
  }

  private canTriggerCue(type: 'thermal' | 'vibration' | 'combined', timestamp: number, cooldown: number): boolean {
    const timeSinceLast = timestamp - this.state.lastCueTimestamp;
    
    // Respect cooldown for same type or combined
    if (this.state.lastCueType === type || this.state.lastCueType === 'combined') {
      return timeSinceLast >= cooldown;
    }
    
    // Different type has shorter cooldown
    return timeSinceLast >= cooldown * 0.5;
  }

  private recordCue(type: 'thermal' | 'vibration' | 'combined', timestamp: number): void {
    this.state.lastCueTimestamp = timestamp;
    this.state.lastCueType = type;
    this.state.cuesInLastHour++;
  }

  private clampIntensity(value: number, max: number): number {
    return Math.round(Math.min(max, Math.max(0, value)));
  }

  private updateHistory(metrics: EngineMetrics): void {
    // Rolling window for trend detection
    this.state.coherenceHistory.push(metrics.meanCoherence);
    this.state.microVarHistory.push(metrics.microVariability);

    if (this.state.coherenceHistory.length > CUE_THRESHOLDS.HISTORY_WINDOW) {
      this.state.coherenceHistory.shift();
    }
    if (this.state.microVarHistory.length > CUE_THRESHOLDS.HISTORY_WINDOW) {
      this.state.microVarHistory.shift();
    }
  }

  private detectDeterioratingTrend(): boolean {
    const history = this.state.coherenceHistory;
    if (history.length < 5) return false;

    // Simple slope detection: compare first half average to second half
    const mid = Math.floor(history.length / 2);
    const firstHalf = history.slice(0, mid);
    const secondHalf = history.slice(mid);

    const firstAvg = firstHalf.reduce((a, b) => a + b, 0) / firstHalf.length;
    const secondAvg = secondHalf.reduce((a, b) => a + b, 0) / secondHalf.length;

    // Deteriorating if coherence dropped by >10%
    return (firstAvg - secondAvg) / firstAvg > 0.10;
  }
}

/*******************************************************************************
 * CONVENIENCE FUNCTIONS
 ******************************************************************************/

/** Singleton instance for typical use */
let defaultGenerator: HapticCueGenerator | null = null;

export function getDefaultCueGenerator(): HapticCueGenerator {
  if (!defaultGenerator) {
    defaultGenerator = new HapticCueGenerator();
  }
  return defaultGenerator;
}

export function resetDefaultCueGenerator(): void {
  defaultGenerator?.reset();
}

/**
 * Quick function to generate a cue from engine output
 */
export function generateHapticCue(metrics: EngineMetrics, preferences?: Partial<CuePreferences>): HapticCue {
  const generator = getDefaultCueGenerator();
  if (preferences) {
    generator.updatePreferences(preferences);
  }
  return generator.generateCue(metrics);
}

/*******************************************************************************
 * SIGNATURE FEEL INTEGRATION
 *
 * Maps raw cues to the ring's signature haptic vocabulary for a consistent
 * emotional experience across all interactions.
 ******************************************************************************/

import {
  SignaturePattern,
  mapCueToPattern,
  createSignatureCommand,
  getPatternIntensityScale,
  PATTERN_METADATA,
  type HRVCueType,
  type CuePriority,
} from './signatureFeel';

/** Extended cue with signature pattern information */
export interface SignatureCue extends HapticCue {
  signaturePattern: SignaturePattern;
  signatureCommand: Uint8Array | null;
  emotionalIntent: string;
}

/**
 * Map a HapticCue to its signature feel equivalent
 */
export function cueToSignature(
  cue: HapticCue, 
  sensitivityMode: 'subtle' | 'normal' | 'assertive' = 'normal'
): SignatureCue {
  if (!cue.shouldTrigger || !cue.command) {
    return {
      ...cue,
      signaturePattern: SignaturePattern.NONE,
      signatureCommand: null,
      emotionalIntent: '',
    };
  }

  // Determine cue type from the command
  let cueType: HRVCueType;
  if (cue.command.vibrationPattern !== VibrationPattern.OFF && cue.command.thermalIntensity > 0) {
    cueType = 'combined';
  } else if (cue.command.vibrationPattern === VibrationPattern.BREATHING) {
    cueType = 'breathing';
  } else if (cue.command.vibrationPattern !== VibrationPattern.OFF) {
    cueType = 'vibration';
  } else {
    cueType = 'thermal';
  }

  // Map priority
  const cuePriority: CuePriority = cue.priority;

  // Get signature pattern
  const pattern = mapCueToPattern(cueType, cuePriority);
  
  // Calculate intensity scale
  const intensityScale = getPatternIntensityScale(pattern, sensitivityMode);
  
  // Generate BLE command
  const signatureCommand = createSignatureCommand(pattern, intensityScale);
  
  // Get emotional intent
  const metadata = PATTERN_METADATA[pattern];

  return {
    ...cue,
    signaturePattern: pattern,
    signatureCommand,
    emotionalIntent: metadata.verbal,
  };
}

/**
 * Generate a signature-feel cue directly from engine metrics
 */
export function generateSignatureCue(
  metrics: EngineMetrics, 
  preferences?: Partial<CuePreferences>
): SignatureCue {
  const rawCue = generateHapticCue(metrics, preferences);
  const sensitivityMode = preferences?.sensitivityMode ?? 'normal';
  return cueToSignature(rawCue, sensitivityMode);
}

// Re-export signature types for convenience
export {
  SignaturePattern,
  PATTERN_METADATA,
  TIMING,
  INTENSITY,
  ease,
  easeIntensity,
  mapCueToPattern,
  getPatternIntensityScale,
  createSignatureCommand,
  createStopCommand,
  type EaseCurve,
  type HRVCueType,
  type CuePriority,
} from './signatureFeel';
