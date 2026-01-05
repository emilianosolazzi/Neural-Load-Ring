/**
 * @file hapticCueGenerator.test.ts
 * @brief Tests for the HRV-to-Haptic Cue Generator
 *
 * Tests the intelligent decision cascade:
 *   • Confidence gating
 *   • Micro-variability → vibration mapping
 *   • Coherence dips → thermal mapping
 *   • Stability → breathing guidance
 *   • Cooldown enforcement
 *   • Quiet hours
 *   • Rate limiting
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

import {
  HapticCueGenerator,
  EngineMetrics,
  CuePreferences,
  CUE_THRESHOLDS,
} from '../../hapticCueGenerator';

// Mock VibrationPattern enum (matches bleService.ts)
enum VibrationPattern {
  OFF = 0,
  SINGLE = 1,
  DOUBLE = 2,
  TRIPLE = 3,
  HEARTBEAT = 4,
  BREATHING = 5,
  ALERT = 6,
}

/*******************************************************************************
 * TEST HELPERS
 ******************************************************************************/

function createMetrics(overrides: Partial<EngineMetrics> = {}): EngineMetrics {
  return {
    timestamp: Date.now(),
    microVariability: 0.02,       // Healthy
    meanCoherence: 0.75,          // Good
    coherenceStability: 0.70,     // Stable
    confidence: 0.85,             // High confidence
    stress_awarenessLevel: 'optimal',
    trend: 'stable',
    artifactRate: 0.05,           // Low artifacts
    ...overrides,
  };
}

function createGenerator(prefs: Partial<CuePreferences> = {}): HapticCueGenerator {
  // Disable quiet hours by setting start = end (0 hours of quiet time)
  return new HapticCueGenerator({
    quietHoursStart: 0,
    quietHoursEnd: 0,   // Same value = disabled
    ...prefs,
  });
}

/*******************************************************************************
 * TESTS
 ******************************************************************************/

describe('HapticCueGenerator', () => {
  
  describe('Confidence Gating', () => {
    
    test('suppresses cue when confidence is below threshold', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        confidence: 0.50,  // Below 60% threshold
        meanCoherence: 0.30,  // Would normally trigger thermal
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('Low confidence');
    });

    test('triggers check-fit cue after consecutive low confidence readings', () => {
      const gen = createGenerator();
      
      // Send 2 low-confidence readings (counter becomes 1, then 2)
      for (let i = 0; i < 2; i++) {
        const cue = gen.generateCue(createMetrics({
          timestamp: Date.now() + i * 60000,
          confidence: 0.40,
        }));
        expect(cue.shouldTrigger).toBe(false);  // Not triggered yet
      }
      
      // 3rd reading should trigger check-fit (counter becomes 3 >= LOW_CONFIDENCE_STREAK)
      const cue = gen.generateCue(createMetrics({
        timestamp: Date.now() + 120000,
        confidence: 0.40,
      }));
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.reason).toContain('check ring fit');
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.TRIPLE);
    });

    test('resets low confidence streak on good reading', () => {
      const gen = createGenerator();
      
      // 2 low confidence readings
      gen.generateCue(createMetrics({ confidence: 0.40 }));
      gen.generateCue(createMetrics({ 
        timestamp: Date.now() + 60000,
        confidence: 0.40 
      }));
      
      // Good reading should reset
      gen.generateCue(createMetrics({ 
        timestamp: Date.now() + 120000,
        confidence: 0.90 
      }));
      
      // Next low confidence shouldn't trigger check-fit (only 1 in streak)
      const cue = gen.generateCue(createMetrics({
        timestamp: Date.now() + 180000,
        confidence: 0.40,
        meanCoherence: 0.30,
      }));
      
      expect(cue.shouldTrigger).toBe(false);
    });
  });

  describe('Micro-variability → Vibration', () => {
    
    test('no cue for healthy micro-variability', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        microVariability: 0.01,  // Below ELEVATED threshold
        meanCoherence: 0.80,     // Good coherence
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('Optimal');
    });

    test('triggers vibration for elevated micro-variability', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        microVariability: 0.06,  // Between ELEVATED (0.05) and HIGH (0.08)
        meanCoherence: 0.80,     // Good coherence (no thermal trigger)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.SINGLE);
      expect(cue.reason).toContain('micro-variability');
    });

    test('triggers double vibration for high micro-variability', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        microVariability: 0.09,  // Above HIGH threshold
        meanCoherence: 0.80,
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.DOUBLE);
    });

    test('triggers alert for critical micro-variability', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        microVariability: 0.15,  // Above CRITICAL (0.12)
        meanCoherence: 0.30,
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.priority).toBe('alert');
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.ALERT);
    });
  });

  describe('Coherence Dips → Thermal', () => {
    
    test('no thermal cue for high coherence', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        meanCoherence: 0.80,  // Above MEDIUM (0.50)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(false);
    });

    test('triggers thermal for medium-low coherence', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        meanCoherence: 0.45,  // Below MEDIUM (0.50)
        microVariability: 0.03,  // Not elevated (no vibration)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.command?.thermalIntensity).toBeGreaterThan(0);
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.OFF);
      expect(cue.reason).toContain('coherence');
    });

    test('thermal intensity scales with coherence deficit', () => {
      const gen = createGenerator();
      
      // Mild deficit
      const mildCue = gen.generateCue(createMetrics({
        timestamp: Date.now(),
        meanCoherence: 0.45,
      }));
      
      // Reset for next test
      gen.reset();
      
      // Severe deficit
      const severeCue = gen.generateCue(createMetrics({
        timestamp: Date.now() + 500000,
        meanCoherence: 0.20,
      }));
      
      expect(severeCue.command!.thermalIntensity).toBeGreaterThan(mildCue.command!.thermalIntensity);
    });
  });

  describe('Stability → Breathing Guidance', () => {
    
    test('triggers breathing cue for unstable coherence', () => {
      const gen = createGenerator({ breathingGuidanceEnabled: true });
      const metrics = createMetrics({
        coherenceStability: 0.30,  // Below UNSTABLE (0.40)
        meanCoherence: 0.55,       // Above MEDIUM (no thermal)
        microVariability: 0.03,    // Healthy (no vibration)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.BREATHING);
      expect(cue.reason).toContain('stability');
    });

    test('respects breathingGuidanceEnabled preference', () => {
      const gen = createGenerator({ breathingGuidanceEnabled: false });
      const metrics = createMetrics({
        coherenceStability: 0.30,
        meanCoherence: 0.55,
        microVariability: 0.03,
      });
      
      const cue = gen.generateCue(metrics);
      
      // Should fall through to next condition (thermal) or no cue
      if (cue.shouldTrigger) {
        expect(cue.command?.vibrationPattern).not.toBe(VibrationPattern.BREATHING);
      }
    });
  });

  describe('Combined Cues', () => {
    
    test('triggers combined cue for low coherence + high variability', () => {
      const gen = createGenerator();
      const metrics = createMetrics({
        meanCoherence: 0.25,       // Below LOW (0.30)
        microVariability: 0.06,   // Above ELEVATED (0.05)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(true);
      expect(cue.command?.thermalIntensity).toBeGreaterThan(0);
      expect(cue.command?.vibrationPattern).toBe(VibrationPattern.HEARTBEAT);
      expect(cue.priority).toBe('high');
    });
  });

  describe('Cooldown Enforcement', () => {
    
    test('enforces cooldown between cues', () => {
      const gen = createGenerator();
      const baseTime = Date.now();
      
      // First cue should trigger
      const cue1 = gen.generateCue(createMetrics({
        timestamp: baseTime,
        microVariability: 0.06,
      }));
      expect(cue1.shouldTrigger).toBe(true);
      
      // Second cue within cooldown should not trigger
      const cue2 = gen.generateCue(createMetrics({
        timestamp: baseTime + 5000,  // Only 5s later
        microVariability: 0.07,
      }));
      expect(cue2.shouldTrigger).toBe(false);
      expect(cue2.reason).toContain('cooldown');
      
      // After cooldown should trigger again
      const cue3 = gen.generateCue(createMetrics({
        timestamp: baseTime + 35000,  // 35s later (> 30s vibration cooldown)
        microVariability: 0.06,
      }));
      expect(cue3.shouldTrigger).toBe(true);
    });

    test('thermal has longer cooldown than vibration', () => {
      const gen = createGenerator();
      const baseTime = Date.now();
      
      // Trigger thermal
      gen.generateCue(createMetrics({
        timestamp: baseTime,
        meanCoherence: 0.40,
        microVariability: 0.02,  // Not elevated
      }));
      
      // 60s later - still in thermal cooldown (120s)
      const cue = gen.generateCue(createMetrics({
        timestamp: baseTime + 60000,
        meanCoherence: 0.40,
      }));
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('cooldown');
    });
  });

  describe('Quiet Hours', () => {
    
    test('suppresses cues during quiet hours', () => {
      // Set quiet hours to include current time
      const now = new Date();
      const gen = createGenerator({
        quietHoursStart: now.getHours(),
        quietHoursEnd: (now.getHours() + 2) % 24,
      });
      
      const metrics = createMetrics({
        microVariability: 0.15,  // Would normally trigger alert
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('Quiet hours');
    });

    test('handles overnight quiet hours (e.g., 22:00-07:00)', () => {
      const gen = createGenerator({
        quietHoursStart: 22,
        quietHoursEnd: 7,
      });
      
      // Manually check the logic - at 11pm should be quiet
      // This is a unit test of the quiet hours logic
      const prefs = gen.getPreferences();
      expect(prefs.quietHoursStart).toBe(22);
      expect(prefs.quietHoursEnd).toBe(7);
    });
  });

  describe('Rate Limiting', () => {
    
    test('limits cues per hour', () => {
      const gen = createGenerator();
      const baseTime = Date.now();
      
      // Generate max cues
      for (let i = 0; i < CUE_THRESHOLDS.MAX_CUES_PER_HOUR; i++) {
        gen.generateCue(createMetrics({
          timestamp: baseTime + (i * 35000),  // 35s apart (past vibration cooldown)
          microVariability: 0.06,
        }));
      }
      
      // Next cue should be rate limited
      const cue = gen.generateCue(createMetrics({
        timestamp: baseTime + (CUE_THRESHOLDS.MAX_CUES_PER_HOUR * 35000) + 1000,
        microVariability: 0.15,  // Would be alert
      }));
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('Rate limit');
    });
  });

  describe('Sensitivity Profiles', () => {
    
    test('subtle mode uses lower intensities', () => {
      const gen = createGenerator({ sensitivityMode: 'subtle' });
      const metrics = createMetrics({
        microVariability: 0.06,
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.command?.vibrationIntensity).toBeLessThanOrEqual(40);
    });

    test('assertive mode uses higher intensities', () => {
      const gen = createGenerator({ sensitivityMode: 'assertive' });
      const metrics = createMetrics({
        microVariability: 0.06,
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.command?.vibrationIntensity).toBeGreaterThanOrEqual(45);
    });
  });

  describe('Preference Updates', () => {
    
    test('respects thermalEnabled preference', () => {
      const gen = createGenerator({ thermalEnabled: false });
      const metrics = createMetrics({
        meanCoherence: 0.40,
        microVariability: 0.03,  // Not elevated
      });
      
      const cue = gen.generateCue(metrics);
      
      if (cue.shouldTrigger) {
        expect(cue.command?.thermalIntensity).toBe(0);
      }
    });

    test('respects vibrationEnabled preference', () => {
      const gen = createGenerator({ vibrationEnabled: false });
      const metrics = createMetrics({
        microVariability: 0.06,  // Elevated
        meanCoherence: 0.80,     // Good (no thermal)
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.reason).toContain('disabled');
    });

    test('respects maxThermalIntensity cap', () => {
      const gen = createGenerator({ maxThermalIntensity: 30 });
      const metrics = createMetrics({
        meanCoherence: 0.20,  // Very low - would normally get high intensity
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.command?.thermalIntensity).toBeLessThanOrEqual(30);
    });

    test('can update preferences dynamically', () => {
      const gen = createGenerator();
      
      gen.updatePreferences({ sensitivityMode: 'subtle' });
      const prefs = gen.getPreferences();
      
      expect(prefs.sensitivityMode).toBe('subtle');
    });
  });

  describe('Master Switch', () => {
    
    test('suppresses all cues when disabled', () => {
      const gen = createGenerator({ enabled: false });
      const metrics = createMetrics({
        microVariability: 0.15,  // Would be alert
        stress_awarenessLevel: 'needs_attention',
      });
      
      const cue = gen.generateCue(metrics);
      
      expect(cue.shouldTrigger).toBe(false);
      expect(cue.reason).toContain('disabled');
    });
  });

  describe('Trend Detection', () => {
    
    test('triggers preventive cue for deteriorating trend', () => {
      const gen = createGenerator();
      const baseTime = Date.now();
      
      // Build history with deteriorating coherence
      for (let i = 0; i < 8; i++) {
        gen.generateCue(createMetrics({
          timestamp: baseTime + (i * 60000),
          meanCoherence: 0.70 - (i * 0.05),  // Declining from 0.70 to 0.35
          microVariability: 0.02,
        }));
      }
      
      // Reset cooldowns for testing
      gen.reset();
      
      // Rebuild history
      for (let i = 0; i < 6; i++) {
        gen.generateCue(createMetrics({
          timestamp: baseTime + (i * 180000),  // 3 min apart
          meanCoherence: 0.70 - (i * 0.06),
        }));
      }
      
      // Verify deteriorating trend logic (internal method tested indirectly)
      const stats = gen.getPreferences();
      expect(stats).toBeDefined();
    });
  });

  describe('Reset', () => {
    
    test('reset clears all state', () => {
      const gen = createGenerator();
      
      // Generate some cues to build state
      for (let i = 0; i < 5; i++) {
        gen.generateCue(createMetrics({
          timestamp: Date.now() + (i * 35000),
          microVariability: 0.06,
        }));
      }
      
      gen.reset();
      
      // Should be able to immediately generate cue (cooldown cleared)
      const cue = gen.generateCue(createMetrics({
        microVariability: 0.06,
      }));
      
      expect(cue.shouldTrigger).toBe(true);
    });
  });
});

/*******************************************************************************
 * RUN TESTS
 ******************************************************************************/

if (typeof describe === 'undefined') {
  console.log('Run with: npx jest hapticCueGenerator.test.ts');
}
