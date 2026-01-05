/**
 * Core wellness types for the Neural Load Ring.
 * Shared type definitions for biometrics, patterns, and prescriptions.
 */

// ============================================================================
// STRESS & WELLNESS CLASSIFICATION
// ============================================================================

export type StressLevel = 'optimal' | 'low' | 'moderate' | 'high' | 'needs_attention';
export type Trend = 'improving' | 'stable' | 'deteriorating';
export type PrescriptionType = 'none' | 'thermal' | 'vibration' | 'combined';

// ============================================================================
// FREQUENCY BANDS (Standard & Adaptive)
// ============================================================================

export type FrequencyBand = {
	low: number;  // Hz
	high: number; // Hz
};

export type AdaptiveBandInfo = {
	lf: FrequencyBand;
	hf: FrequencyBand;
	isAdapted: boolean;
	detectedRespHz: number | null;
};

export type RespiratoryDetection = {
	respiratoryFrequency: number | null;  // Hz, or null if detection failed
	prominence: number;                    // 0-1, peak prominence
	confidence: number;                    // 0-1, detection confidence
};

// ============================================================================
// HRV METRICS
// ============================================================================

export type TimeDomainMetrics = {
	meanRR: number;   // ms
	sdnn: number;     // ms
	rmssd: number;    // ms
	nn50: number;     // count
	pnn50: number;    // percent
	hr: number;       // bpm
};

export type FrequencyDomainMetrics = {
	lf: number;         // ms² (low frequency power)
	hf: number;         // ms² (high frequency power)
	lfHfRatio: number;  // LF/HF ratio
	totalPower: number; // ms²
	peakLF: number;     // Hz (peak frequency in LF band)
	peakHF: number;     // Hz (peak frequency in HF band)
};

// ============================================================================
// PATTERN TYPES
// ============================================================================

export type ThermalPattern = {
	intensityPercent: number;   // 0-100, capped at 60% for safety
	durationMs: number;         // max 5000ms
	intervalMs?: number;        // repeat interval
};

export type VibrationPattern = {
	intensityPercent: number;   // 0-100, capped at 50% for safety
	durationMs: number;         // max 800ms
	frequencyHz?: number;       // pulse rate
	burstCount?: number;        // number of bursts
};

// ============================================================================
// PRESCRIPTION TYPES
// ============================================================================

export type Prescription<T> = {
	enabled: boolean;
	pattern: T;
	reason: string;
};

export type ThermalPrescription = Prescription<ThermalPattern>;
export type VibrationPrescription = Prescription<VibrationPattern>;

export type CombinedPrescription = {
	thermal: ThermalPrescription;
	vibration: VibrationPrescription;
};

// ============================================================================
// WELLNESS SNAPSHOT
// ============================================================================

export type WellnessMetrics = {
	loadScore: number;        // 0-100
	coherence: number;        // 0-1
	microVariability: number; // 0-1
	confidence: number;       // 0-1
	stressLevel: StressLevel;
	trend: Trend;
};

export type WellnessSnapshot = {
	timestamp: number;           // unix ms
	metrics: WellnessMetrics;
	timeDomain: TimeDomainMetrics;
	frequencyDomain: FrequencyDomainMetrics;
	adaptiveBands?: AdaptiveBandInfo;
	respiratoryDetection?: RespiratoryDetection;
	prescription: CombinedPrescription;
	userId?: string;
};

// ============================================================================
// SAFETY LIMITS
// ============================================================================

export const SAFETY_LIMITS = {
	thermal: {
		maxIntensity: 60,       // percent
		maxDuration: 5000,      // ms
		minInterval: 30000,     // ms (30s between cues)
		maxDeltaTemp: 2,        // °C
	},
	vibration: {
		maxIntensity: 50,       // percent
		maxDuration: 800,       // ms
		maxFrequency: 80,       // Hz
		maxBurstCount: 5,
	},
	dutyCycle: {
		thermal: 0.15,          // 15% max duty cycle
		vibration: 0.10,        // 10% max duty cycle
	},
} as const;

// ============================================================================
// CONFIDENCE THRESHOLDS
// ============================================================================

export const CONFIDENCE_THRESHOLDS = {
	minForPrescription: 0.5,    // Don't intervene below this
	minForClassification: 0.6,  // Use neutral classification below this
	highConfidence: 0.8,        // Full trust in metrics
} as const;