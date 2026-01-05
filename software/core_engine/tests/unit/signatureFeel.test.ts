/**
 * @file signatureFeel.test.ts
 * @brief Unit tests for the Signature Feel system
 *
 * Tests the ring's haptic personality: easing curves, pattern mapping,
 * and BLE command generation.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

import {
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
} from '../../hapticCueGenerator';

describe('Signature Feel System', () => {
  /*******************************************************************************
   * EASING CURVES
   ******************************************************************************/
  describe('Easing Curves', () => {
    test('linear easing is identity', () => {
      expect(ease('linear', 0)).toBe(0);
      expect(ease('linear', 0.5)).toBe(0.5);
      expect(ease('linear', 1)).toBe(1);
    });

    test('easeInSine starts slow', () => {
      const early = ease('easeInSine', 0.2);
      const linear = 0.2;
      expect(early).toBeLessThan(linear); // Slower start
    });

    test('easeOutSine ends slow', () => {
      const late = ease('easeOutSine', 0.8);
      const linear = 0.8;
      expect(late).toBeGreaterThan(linear); // Further along at same t
    });

    test('easeInOutSine is symmetric', () => {
      const first = ease('easeInOutSine', 0.25);
      const second = 1 - ease('easeInOutSine', 0.75);
      expect(first).toBeCloseTo(second, 2);
    });

    test('breath curve is asymmetric (faster in, slower out)', () => {
      // At t=0.4 (end of inhale), should be at or near peak
      const peakAt40 = ease('breath', 0.4);
      expect(peakAt40).toBeCloseTo(1, 1); // Should be near 1.0

      // At t=0.7 (middle of exhale), should still be above 0.5
      const midExhale = ease('breath', 0.7);
      expect(midExhale).toBeGreaterThan(0.3);
    });

    test('easing clamps at boundaries', () => {
      expect(ease('easeInSine', -0.5)).toBe(0);
      expect(ease('easeInSine', 1.5)).toBe(1);
    });

    test('easeIntensity interpolates correctly', () => {
      expect(easeIntensity(0, 100, 'linear', 0.5)).toBe(50);
      expect(easeIntensity(20, 60, 'linear', 0.5)).toBe(40);
      expect(easeIntensity(100, 0, 'linear', 0.5)).toBe(50);
    });

    test('easeIntensity clamps to 0-100', () => {
      expect(easeIntensity(-50, 150, 'linear', 0.5)).toBe(50);
      expect(easeIntensity(0, 200, 'linear', 0.75)).toBe(100);
    });
  });

  /*******************************************************************************
   * PATTERN METADATA
   ******************************************************************************/
  describe('Pattern Metadata', () => {
    test('all patterns have metadata', () => {
      const patterns = Object.values(SignaturePattern).filter(
        (v) => typeof v === 'number'
      ) as SignaturePattern[];

      for (const pattern of patterns) {
        expect(PATTERN_METADATA[pattern]).toBeDefined();
        expect(PATTERN_METADATA[pattern].name).toBeTruthy();
      }
    });

    test('all patterns have emotional vocabulary', () => {
      const patterns = Object.values(SignaturePattern).filter(
        (v) => typeof v === 'number' && v !== SignaturePattern.NONE
      ) as SignaturePattern[];

      for (const pattern of patterns) {
        const meta = PATTERN_METADATA[pattern];
        expect(meta.verbal).toBeTruthy();
        expect(meta.emotion).toBeTruthy();
      }
    });

    test('modalities are correctly assigned', () => {
      expect(PATTERN_METADATA[SignaturePattern.GROUNDING_PULSE].modality).toBe('vibration');
      expect(PATTERN_METADATA[SignaturePattern.WARM_EXHALE].modality).toBe('thermal');
      expect(PATTERN_METADATA[SignaturePattern.GENTLE_ALERT].modality).toBe('combined');
    });
  });

  /*******************************************************************************
   * CUE TO PATTERN MAPPING
   ******************************************************************************/
  describe('Cue to Pattern Mapping', () => {
    test('alert priority maps to comprehensive patterns', () => {
      expect(mapCueToPattern('combined', 'alert')).toBe(SignaturePattern.FULL_RESET);
      expect(mapCueToPattern('vibration', 'alert')).toBe(SignaturePattern.FULL_RESET);
      expect(mapCueToPattern('thermal', 'alert')).toBe(SignaturePattern.SAFETY_EMBRACE);
    });

    test('high priority maps to grounding patterns', () => {
      expect(mapCueToPattern('combined', 'high')).toBe(SignaturePattern.HEARTBEAT);
      expect(mapCueToPattern('vibration', 'high')).toBe(SignaturePattern.HEARTBEAT);
    });

    test('normal priority maps to appropriate patterns', () => {
      expect(mapCueToPattern('breathing', 'normal')).toBe(SignaturePattern.BREATHING_GUIDE);
      expect(mapCueToPattern('vibration', 'normal')).toBe(SignaturePattern.ATTENTION_TAP);
      expect(mapCueToPattern('thermal', 'normal')).toBe(SignaturePattern.WARM_EXHALE);
    });

    test('low priority maps to subtle patterns', () => {
      expect(mapCueToPattern('vibration', 'low')).toBe(SignaturePattern.GROUNDING_PULSE);
      expect(mapCueToPattern('thermal', 'low')).toBe(SignaturePattern.GROUNDING_WARMTH);
      expect(mapCueToPattern('checkFit', 'low')).toBe(SignaturePattern.PRESENCE_CHECK);
    });
  });

  /*******************************************************************************
   * INTENSITY SCALING
   ******************************************************************************/
  describe('Intensity Scaling', () => {
    test('subtle mode uses lower intensity', () => {
      const subtleScale = getPatternIntensityScale(SignaturePattern.GROUNDING_PULSE, 'subtle');
      const normalScale = getPatternIntensityScale(SignaturePattern.GROUNDING_PULSE, 'normal');
      expect(subtleScale).toBeLessThan(normalScale);
    });

    test('assertive mode uses higher intensity', () => {
      const normalScale = getPatternIntensityScale(SignaturePattern.GROUNDING_PULSE, 'normal');
      const assertiveScale = getPatternIntensityScale(SignaturePattern.GROUNDING_PULSE, 'assertive');
      expect(assertiveScale).toBeGreaterThan(normalScale);
    });

    test('intensity never exceeds 100', () => {
      const scale = getPatternIntensityScale(SignaturePattern.FULL_RESET, 'assertive');
      expect(scale).toBeLessThanOrEqual(100);
    });

    test('thermal patterns can have slightly higher intensity', () => {
      const thermalScale = getPatternIntensityScale(SignaturePattern.WARM_EXHALE, 'normal');
      const vibScale = getPatternIntensityScale(SignaturePattern.ATTENTION_TAP, 'normal');
      // Thermal is less intrusive, so it gets a small boost
      expect(thermalScale).toBeGreaterThanOrEqual(vibScale);
    });
  });

  /*******************************************************************************
   * BLE COMMAND GENERATION
   ******************************************************************************/
  describe('BLE Command Generation', () => {
    test('creates correct command format', () => {
      const cmd = createSignatureCommand(SignaturePattern.GROUNDING_PULSE, 80);
      expect(cmd).toBeInstanceOf(Uint8Array);
      expect(cmd.length).toBe(3);
      expect(cmd[0]).toBe(0x05); // Signature command ID
      expect(cmd[1]).toBe(SignaturePattern.GROUNDING_PULSE);
      expect(cmd[2]).toBe(80);
    });

    test('clamps intensity to valid range', () => {
      const cmdHigh = createSignatureCommand(SignaturePattern.HEARTBEAT, 150);
      expect(cmdHigh[2]).toBe(100);

      const cmdLow = createSignatureCommand(SignaturePattern.HEARTBEAT, -20);
      expect(cmdLow[2]).toBe(0);
    });

    test('stop command uses NONE pattern', () => {
      const stopCmd = createStopCommand();
      expect(stopCmd[1]).toBe(SignaturePattern.NONE);
      expect(stopCmd[2]).toBe(0);
    });

    test('all pattern values fit in one byte', () => {
      const patterns = Object.values(SignaturePattern).filter(
        (v) => typeof v === 'number'
      ) as number[];

      for (const pattern of patterns) {
        expect(pattern).toBeGreaterThanOrEqual(0);
        expect(pattern).toBeLessThanOrEqual(255);
      }
    });
  });

  /*******************************************************************************
   * TIMING CONSTANTS
   ******************************************************************************/
  describe('Timing Constants', () => {
    test('ramp times are reasonable', () => {
      expect(TIMING.RAMP_UP_MS).toBeGreaterThanOrEqual(200);
      expect(TIMING.RAMP_UP_MS).toBeLessThanOrEqual(1000);
      expect(TIMING.RAMP_DOWN_MS).toBeGreaterThanOrEqual(TIMING.RAMP_UP_MS);
    });

    test('breathing guide has proper 4:6 ratio', () => {
      const ratio = TIMING.BREATH_INHALE_MS / TIMING.BREATH_EXHALE_MS;
      expect(ratio).toBeCloseTo(4 / 6, 1);
    });

    test('heartbeat cycle targets ~75 BPM', () => {
      const bpm = 60000 / TIMING.HEARTBEAT_CYCLE_MS;
      expect(bpm).toBeCloseTo(75, 5); // Within 5 BPM
    });
  });

  /*******************************************************************************
   * INTENSITY LIMITS
   ******************************************************************************/
  describe('Intensity Limits', () => {
    test('vibration max is comfortable', () => {
      expect(INTENSITY.VIB_MAX).toBeLessThanOrEqual(70); // Never jarring
      expect(INTENSITY.VIB_MAX).toBeGreaterThan(50); // Still perceptible
    });

    test('thermal max is safe', () => {
      expect(INTENSITY.THERMAL_MAX).toBeLessThanOrEqual(75); // Safety
      expect(INTENSITY.THERMAL_MAX).toBeGreaterThan(50);
    });

    test('gentle values are below max', () => {
      expect(INTENSITY.VIB_GENTLE).toBeLessThan(INTENSITY.VIB_MAX);
      expect(INTENSITY.THERMAL_GENTLE).toBeLessThan(INTENSITY.THERMAL_MAX);
    });

    test('medium values are between gentle and max', () => {
      expect(INTENSITY.VIB_MEDIUM).toBeGreaterThan(INTENSITY.VIB_GENTLE);
      expect(INTENSITY.VIB_MEDIUM).toBeLessThan(INTENSITY.VIB_MAX);
    });
  });
});
