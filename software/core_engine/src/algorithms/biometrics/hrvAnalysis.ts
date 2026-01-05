// HRV analysis focused on upstream systemic timing noise for the Neural Load Ring.
// Uses adaptive frequency bands for accurate classification across all breathing patterns.

import { ValidatedHRVAlgorithms, VALIDATED_CONSTANTS } from '../../../wellnessEngine.v1.0';

export type HRVAnalysisResult = {
	cleanSamples: number;
	artifactRate: number;
	coherence: number;         // respiratory coherence (0-1), using adaptive bands
	microVariability: number;  // surrogate micro-variability (0-1)
	loadScore: number;         // 0-100 higher = more systemic load
	timeDomain: ReturnType<typeof ValidatedHRVAlgorithms.calculateTimeDomainHRV>;
	frequencyDomain: {
		lf: number;
		hf: number;
		lfHfRatio: number;
		totalPower: number;
		peakLF: number;
		peakHF: number;
	};
	sampleEntropy: number;
	// Adaptive band metadata
	adaptiveBands?: {
		lf: { low: number; high: number };
		hf: { low: number; high: number };
		isAdapted: boolean;
		detectedRespHz: number | null;
	};
	respiratoryDetection?: {
		respiratoryFrequency: number | null;
		prominence: number;
		confidence: number;
	};
};

const clamp01 = (v: number) => Math.min(1, Math.max(0, v));

/**
 * Analyze HRV with adaptive frequency bands.
 * Detects user's actual respiratory rate and adjusts HF/LF bands accordingly.
 * Fixes misclassification of slow breathers (athletes, meditators) as "stressed".
 */
export function analyzeHRV(rrMs: number[]): HRVAnalysisResult {
	if (!Array.isArray(rrMs) || rrMs.length < VALIDATED_CONSTANTS.SAMPLING.MIN_SAMPLES) {
		throw new Error(`Need at least ${VALIDATED_CONSTANTS.SAMPLING.MIN_SAMPLES} RR intervals`);
	}

	const artifact = ValidatedHRVAlgorithms.kubiosArtifactCorrection(rrMs);
	const clean = artifact.cleaned;
	if (clean.length < rrMs.length * VALIDATED_CONSTANTS.PHYSIOLOGICAL_LIMITS.MIN_VALID_DATA_PERCENT) {
		throw new Error('Insufficient clean data after artifact correction');
	}

	const timeDomain = ValidatedHRVAlgorithms.calculateTimeDomainHRV(clean);
	
	// Use adaptive frequency domain analysis (detects breathing rate automatically)
	const frequencyDomainAdaptive = ValidatedHRVAlgorithms.calculateFrequencyDomainHRVAdaptive(
		clean,
		VALIDATED_CONSTANTS.SAMPLING.OPTIMAL_FS
	);
	
	const sampleEntropy = ValidatedHRVAlgorithms.calculateSampleEntropy(clean);

	// Coherence using CORRECT formula: HF / (LF + HF)
	// This excludes VLF/DC noise and uses adaptive bands for accurate classification
	const denominator = frequencyDomainAdaptive.lf + frequencyDomainAdaptive.hf;
	const coherence = denominator > 0 
		? clamp01(frequencyDomainAdaptive.hf / denominator) 
		: 0.5;

	// Micro-variability surrogate: dampened LF/HF ratio (higher ratio => more instability)
	const microVariability = clamp01(frequencyDomainAdaptive.lfHfRatio / 4);

	// Systemic load score (0-100): higher when coherence is low and micro-variability is high
	const loadScore = Math.round(
		clamp01(microVariability * 0.6 + (1 - coherence) * 0.4) * 100
	);

	return {
		cleanSamples: clean.length,
		artifactRate: artifact.artifacts.length / rrMs.length,
		coherence,
		microVariability,
		loadScore,
		timeDomain,
		frequencyDomain: {
			lf: frequencyDomainAdaptive.lf,
			hf: frequencyDomainAdaptive.hf,
			lfHfRatio: frequencyDomainAdaptive.lfHfRatio,
			totalPower: frequencyDomainAdaptive.totalPower,
			peakLF: frequencyDomainAdaptive.peakLF,
			peakHF: frequencyDomainAdaptive.peakHF,
		},
		sampleEntropy,
		adaptiveBands: frequencyDomainAdaptive.adaptiveBands,
		respiratoryDetection: frequencyDomainAdaptive.respiratoryDetection,
	};
}