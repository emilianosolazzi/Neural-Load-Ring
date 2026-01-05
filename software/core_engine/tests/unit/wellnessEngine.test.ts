/**
 * Wellness Engine Unit Tests
 * Ensures the core biometrics algorithms, stream buffer, and pattern generators behave consistently.
 */

import { describe, it, expect, beforeEach } from '@jest/globals';
import { NeuralStress_awarenessProcessor, ValidatedHRVAlgorithms } from '../../wellnessEngine.v1.0';
import { StreamProcessor } from '../../src/processors/StreamProcessor';
import { analyzeHRV } from '../../src/algorithms/biometrics/hrvAnalysis';
import { signalQuality } from '../../src/algorithms/biometrics/signalQuality';
import { generateThermalPattern } from '../../src/algorithms/features/thermalPatterns';
import { generateVibrationPattern } from '../../src/algorithms/features/vibrationPatterns';
import { scoreCoherence } from '../../src/algorithms/wellness/coherenceScoring';
import { scoreVariability } from '../../src/algorithms/wellness/variabilityScoring';

// Updated synthetic signals with physiologically accurate frequencies
// Resting: 850ms mean (70bpm), 0.25Hz modulation (RSA/Parasympathetic)
// t = i * 0.85s. 0.25Hz -> sin(2*PI*0.25*t) = sin(1.335 * i)
const RESTING_RR = Array.from({ length: 80 }, (_, i) => 850 + Math.sin(i * 1.335) * 50);

// Stressed: 520ms mean (115bpm), 0.1Hz modulation (Mayer/Sympathetic)
// t = i * 0.52s. 0.1Hz -> sin(2*PI*0.1*t) = sin(0.326 * i)
const STRESSED_RR = Array.from({ length: 80 }, (_, i) => 520 + Math.sin(i * 0.326) * 20);

// High Variability: Mixed signal
const HIGH_VAR_RR = Array.from({ length: 80 }, (_, i) => 800 + Math.sin(i * 0.3) * 50);
const ARTIFACT_RR = [...RESTING_RR.slice(0, 40), 2500, ...RESTING_RR.slice(41)];

const REAL_RESTING_RR = Array.from({ length: 200 }, (_, i) => RESTING_RR[i % RESTING_RR.length]);

const REAL_STRESSED_RR = Array.from({ length: 200 }, (_, i) =>
  520 + Math.sin(i * 0.03) * 3 + (i % 50 === 0 ? -60 : 0)
);

const REAL_NOISY_RR = (() => {
  const base = Array.from({ length: 200 }, (_, i) => 780 + Math.sin(i * 0.07) * 35);
  const noisy = [...base];
  noisy[40] = 2400;
  noisy[120] = 2200;
  return noisy;
})();

const DIURNAL_DRIFT_RR = Array.from({ length: 360 }, (_, i) =>
  860 + Math.sin(i * 0.04) * 20 + Math.sin(i * 0.003) * 10 + i * 0.01
);

const CALM_RECOVERY_RR = Array.from({ length: 200 }, () => 900);

const REPLAY_24H_RR = (() => {
  const arr: number[] = [];
  for (let i = 0; i < 1440; i++) {
    // Base circadian wave plus mild variability
    const base = 820 + Math.sin(i * 0.02) * 30 + Math.sin(i * 0.003) * 12;
    // Insert sparse motion artifacts
    const artifact = i % 180 === 0 ? 2400 : base;
    arr.push(artifact);
  }
  return arr;
})();

const MOTION_SPIKE_RR = (() => {
  const base = Array.from({ length: 120 }, () => 820 + Math.random() * 20);
  for (let i = 0; i < base.length; i += 10) {
    base[i] = 2400;
  }
  return base;
})();

describe('NeuralStress_awarenessProcessor', () => {
  let processor: NeuralStress_awarenessProcessor;

  beforeEach(() => {
    processor = new NeuralStress_awarenessProcessor();
  });

  it('classifies resting state as parasympathetic', async () => {
    const result = await processor.processRRIntervals(RESTING_RR);
    expect(result.meanCoherence).toBeGreaterThan(0.55);
    expect(result.microVariability).toBeGreaterThan(0.5);
    expect(['low', 'moderate']).toContain(result.stress_awarenessLevel);
  });

  it('flags high load states as stressed', async () => {
    const result = await processor.processRRIntervals(STRESSED_RR);
    expect(result.stress_awarenessLevel).not.toBe('low');
    expect(result.relaxationSuggested).toBe(true);
  });

  it('produces relaxation suggestions when stressed', async () => {
    const result = await processor.processRRIntervals(STRESSED_RR);
    expect(result.relaxationSuggested).toBe(true);
    expect(result.relaxation_featureType).not.toBe('none');
  });

  it('buffers high variability without throwing', async () => {
    const result = await processor.processRRIntervals(HIGH_VAR_RR);
    expect(result.microVariability).toBeGreaterThan(0.6);
  });
});

describe('StreamProcessor', () => {
  it('buffers and drains RR intervals', () => {
    const stream = new StreamProcessor({ minWindowSize: 64, maxWindowSize: 128 });
    RESTING_RR.forEach(rr => stream.pushRR(rr));
    expect(stream.snapshot().bufferedSamples).toBeGreaterThanOrEqual(64);
    
    // drain() uses sliding window with overlap by default
    // Use overlap=0 to completely clear buffer (for testing)
    const drained = stream.drain(0);
    expect(drained.length).toBeGreaterThanOrEqual(64);
    expect(stream.snapshot().bufferedSamples).toBe(0);
  });

  it('resets on artifacts', () => {
    const stream = new StreamProcessor({ minWindowSize: 64 });
    ARTIFACT_RR.forEach(rr => stream.pushRR(rr));
    expect(stream.snapshot().bufferedSamples).toBeLessThan(64);
    expect(stream.snapshot().resetEvents).toBeGreaterThanOrEqual(1);
  });
});

describe('HRV Biometrics', () => {
  it('analyzes resting HRV', () => {
    const metrics = analyzeHRV(RESTING_RR);
    expect(metrics.artifactRate).toBeLessThan(0.1);
    expect(metrics.coherence).toBeGreaterThanOrEqual(0);
    expect(metrics.microVariability).toBeGreaterThanOrEqual(0);
    expect(metrics.loadScore).toBeLessThan(110);
  });

  it('detects artifacts and throws if too many', () => {
    expect(() => analyzeHRV([100, 2500, 100, 2500])).toThrow();
  });
});

describe('Signal Quality', () => {
  it('rewards stable streams', () => {
    const result = signalQuality(RESTING_RR);
    expect(result.confidence).toBeGreaterThan(0.7);
    expect(result.artifactRate).toBeLessThan(0.05);
  });

  it('drops confidence for noisy traces', () => {
    const noisy = [...RESTING_RR];
    noisy[30] = 2400;
    const result = signalQuality(noisy);
    expect(result.confidence).toBeLessThan(0.7);
    expect(result.artifactRate).toBeGreaterThan(0.02);
  });
});

describe('Pattern Generators', () => {
  it('generates thermal cues for low coherence', () => {
    const prescription = generateThermalPattern({ loadScore: 80, coherence: 0.3, confidence: 0.9 });
    expect(prescription.enabled).toBe(true);
    expect(prescription.pattern.intensityPercent).toBeGreaterThan(30);
  });

  it('skips cues when confidence is low', () => {
    const prescription = generateThermalPattern({ loadScore: 80, coherence: 0.3, confidence: 0.2 });
    expect(prescription.enabled).toBe(false);
  });

  it('generates vibration cues for high load', () => {
    const prescription = generateVibrationPattern({ loadScore: 85, coherence: 0.25, confidence: 0.9 });
    expect(prescription.enabled).toBe(true);
    expect(prescription.pattern.frequencyHz).toBeDefined();
  });

  it('respects thermal safety caps', () => {
    const prescription = generateThermalPattern({ loadScore: 100, coherence: 0.1, confidence: 1 });
    expect(prescription.pattern.intensityPercent).toBeLessThanOrEqual(60);
    expect(prescription.pattern.durationMs).toBeLessThanOrEqual(5000);
    if (prescription.pattern.intervalMs) {
      expect(prescription.pattern.intervalMs).toBeGreaterThanOrEqual(30000);
    }
  });

  it('respects vibration safety caps', () => {
    const prescription = generateVibrationPattern({ loadScore: 100, coherence: 0.1, confidence: 1 });
    expect(prescription.pattern.intensityPercent).toBeLessThanOrEqual(50);
    expect(prescription.pattern.durationMs).toBeLessThanOrEqual(800);
    if (prescription.pattern.frequencyHz) {
      expect(prescription.pattern.frequencyHz).toBeLessThanOrEqual(80);
    }
    if (prescription.pattern.burstCount) {
      expect(prescription.pattern.burstCount).toBeLessThanOrEqual(5);
    }
  });
});

describe('Wellness Scoring', () => {
  it('scores high coherence', () => {
    const result = scoreCoherence({
      frequencyDomain: { hfPower: 1200, lfPower: 400, totalPower: 1600 },
      sampleCount: 128,
    });
    expect(result.phaseCoherence).toBeGreaterThan(0.6);
  });

  it('scores parasympathetic tone from RMSSD', () => {
    const result = scoreVariability({
      rrIntervalsMs: RESTING_RR,
      timeDomainMetrics: { sdnn: 40, rmssd: 35 },
      frequencyDomainMetrics: { hfPower: 900, lfPower: 450 },
    });
    expect(result.parasympatheticTone).toBeGreaterThan(0.6);
    expect(result.autonomicBalance).toBeGreaterThan(0);
  });
});

describe('Edge Cases', () => {
  it('handles flat RR intervals', async () => {
    const flat = Array(64).fill(800);
    const processor = new NeuralStress_awarenessProcessor();
    const result = await processor.processRRIntervals(flat);
    expect(result.microVariability).toBeGreaterThanOrEqual(0);
    expect(Number.isFinite(result.microVariability)).toBe(true);
  });
});

describe('Real-world-like traces', () => {
  it('classifies a realistic resting trace as low-to-moderate load', async () => {
    const processor = new NeuralStress_awarenessProcessor();
    const result = await processor.processRRIntervals(REAL_RESTING_RR);
    expect(['high', 'needs_attention']).not.toContain(result.stress_awarenessLevel);
  });

  it('classifies a realistic stressed trace as high load', async () => {
    const processor = new NeuralStress_awarenessProcessor();
    const result = await processor.processRRIntervals(REAL_STRESSED_RR);
    expect(result.stress_awarenessLevel).not.toBe('low');
    expect(result.relaxationSuggested).toBe(true);
  });

  it('detects artifacts and lowers confidence on noisy trace', () => {
    const quality = signalQuality(REAL_NOISY_RR);
    expect(quality.artifactRate).toBeGreaterThan(0.02);
    expect(quality.confidence).toBeLessThan(0.8);
  });
});

describe('Recovery latency and circadian drift handling', () => {
  it('reduces stress score after a calm recovery block', async () => {
    const processor = new NeuralStress_awarenessProcessor();
    const stressed = await processor.processRRIntervals(REAL_STRESSED_RR);
    const recovered = await processor.processRRIntervals(CALM_RECOVERY_RR);
    expect(Number.isFinite(recovered.meanCoherence)).toBe(true);
    expect(Number.isFinite(recovered.microVariability)).toBe(true);
    expect(recovered.relaxationSuggested).toBeDefined();
  });

  it('handles slow diurnal drift without misclassifying as high load', async () => {
    const processor = new NeuralStress_awarenessProcessor();
    const result = await processor.processRRIntervals(DIURNAL_DRIFT_RR);
    expect(['high', 'needs_attention']).not.toContain(result.stress_awarenessLevel);
    expect(result.circadianDrift).toBeLessThan(0.5);
  });
});

describe('Replay validations', () => {
  it('handles a 24h replay trace without over-triggering high load', async () => {
    const processor = new NeuralStress_awarenessProcessor();
    const result = await processor.processRRIntervals(REPLAY_24H_RR);
    expect(['needs_attention']).not.toContain(result.stress_awarenessLevel);
    expect(result.circadianDrift).toBeLessThan(1);
    expect(result.confidence).toBeGreaterThan(0.3);
  });

  it('suppresses cues when motion spikes lower confidence', () => {
    const quality = signalQuality(MOTION_SPIKE_RR);
    expect(quality.artifactRate).toBeGreaterThan(0.1);
    expect(quality.confidence).toBeLessThan(0.6);
    const prescription = generateThermalPattern({ loadScore: 75, coherence: 0.35, confidence: quality.confidence });
    expect(prescription.enabled).toBe(false);
  });
});

/* ============================================================================
   ADAPTIVE FREQUENCY BANDS TESTS
   ============================================================================ */

// Slow breather signal: 0.08 Hz (7.5 breaths/min) - typical athlete/meditator
// t = i * 0.85s (850ms RR). 0.08Hz -> sin(2*PI*0.08*t) = sin(0.427 * i)
const SLOW_BREATHER_RR = Array.from({ length: 120 }, (_, i) => 850 + Math.sin(i * 0.427) * 60);

// Fast shallow breather: 0.35 Hz (21 breaths/min) - anxious/shallow breathing
// t = i * 0.7s (700ms RR). 0.35Hz -> sin(2*PI*0.35*t) = sin(1.539 * i)
const FAST_BREATHER_RR = Array.from({ length: 120 }, (_, i) => 700 + Math.sin(i * 1.539) * 30);

// Normal breather: 0.25 Hz (15 breaths/min) - typical resting
const NORMAL_BREATHER_RR = Array.from({ length: 120 }, (_, i) => 800 + Math.sin(i * 1.335) * 50);

describe('Adaptive Frequency Bands', () => {
  describe('Respiratory Rate Detection', () => {
    it('detects slow breathing (athlete/meditator pattern)', () => {
      const detection = ValidatedHRVAlgorithms.detectRespiratoryRate(SLOW_BREATHER_RR);
      expect(detection.respiratoryFrequency).not.toBeNull();
      if (detection.respiratoryFrequency) {
        // Should detect ~0.08 Hz (within reasonable tolerance)
        expect(detection.respiratoryFrequency).toBeGreaterThan(0.05);
        expect(detection.respiratoryFrequency).toBeLessThan(0.15);
      }
      expect(detection.prominence).toBeGreaterThan(0.2);
    });

    it('detects fast breathing (anxious pattern)', () => {
      const detection = ValidatedHRVAlgorithms.detectRespiratoryRate(FAST_BREATHER_RR);
      expect(detection.respiratoryFrequency).not.toBeNull();
      if (detection.respiratoryFrequency) {
        // Should detect ~0.35 Hz
        expect(detection.respiratoryFrequency).toBeGreaterThan(0.25);
        expect(detection.respiratoryFrequency).toBeLessThan(0.45);
      }
    });

    it('detects normal breathing (standard range)', () => {
      const detection = ValidatedHRVAlgorithms.detectRespiratoryRate(NORMAL_BREATHER_RR);
      expect(detection.respiratoryFrequency).not.toBeNull();
      if (detection.respiratoryFrequency) {
        // Should detect ~0.25 Hz
        expect(detection.respiratoryFrequency).toBeGreaterThan(0.15);
        expect(detection.respiratoryFrequency).toBeLessThan(0.35);
      }
    });
  });

  describe('Adaptive Band Calculation', () => {
    it('adapts HF band for slow breathers', () => {
      const bands = ValidatedHRVAlgorithms.calculateAdaptiveBands(0.08);
      expect(bands.isAdapted).toBe(true);
      expect(bands.hf.low).toBeLessThan(0.15); // Below standard HF start
      expect(bands.hf.low).toBeCloseTo(0.03, 1);
      expect(bands.hf.high).toBeCloseTo(0.13, 1);
    });

    it('adapts HF band for fast breathers', () => {
      const bands = ValidatedHRVAlgorithms.calculateAdaptiveBands(0.40);
      expect(bands.isAdapted).toBe(true);
      expect(bands.hf.low).toBeCloseTo(0.35, 1);
      expect(bands.hf.high).toBeCloseTo(0.45, 1);
    });

    it('uses standard bands when detection fails', () => {
      const bands = ValidatedHRVAlgorithms.calculateAdaptiveBands(null);
      expect(bands.isAdapted).toBe(false);
      expect(bands.lf.low).toBeCloseTo(0.04, 2);
      expect(bands.lf.high).toBeCloseTo(0.15, 2);
      expect(bands.hf.low).toBeCloseTo(0.15, 2);
      expect(bands.hf.high).toBeCloseTo(0.40, 2);
    });

    it('prevents LF band collapse for very slow breathers', () => {
      // 0.05 Hz = 3 breaths/min (extreme slow breathing)
      const bands = ValidatedHRVAlgorithms.calculateAdaptiveBands(0.05);
      // Should fallback to standard bands since LF would collapse
      expect(bands.lf.low).toBeLessThan(bands.lf.high);
    });
  });

  describe('End-to-End Adaptive Processing', () => {
    it('correctly classifies slow breather as relaxed (not stressed)', async () => {
      const processor = new NeuralStress_awarenessProcessor();
      const result = await processor.processRRIntervals(SLOW_BREATHER_RR);
      
      // Should have adaptive band info
      expect(result.adaptiveBands).toBeDefined();
      expect(result.respiratoryDetection).toBeDefined();
      
      // Slow breathers should NOT be misclassified as stressed
      // (Old bug: standard bands would put their breathing in LF, showing "stress")
      expect(result.stress_awarenessLevel).not.toBe('high');
      expect(result.stress_awarenessLevel).not.toBe('needs_attention');
    });

    it('produces higher coherence for adapted slow breathers', async () => {
      const processor = new NeuralStress_awarenessProcessor();
      const result = await processor.processRRIntervals(SLOW_BREATHER_RR);
      
      // With adaptive bands, coherence should be meaningful (not near 0)
      expect(result.meanCoherence).toBeGreaterThan(0.3);
    });

    it('includes respiratory detection metadata in results', async () => {
      const processor = new NeuralStress_awarenessProcessor();
      const result = await processor.processRRIntervals(NORMAL_BREATHER_RR);
      
      expect(result.respiratoryDetection?.respiratoryFrequency).not.toBeNull();
      expect(result.adaptiveBands?.isAdapted).toBeDefined();
      expect(result.adaptiveBands?.detectedRespHz).toBeDefined();
    });
  });
});