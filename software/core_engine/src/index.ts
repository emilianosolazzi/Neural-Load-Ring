/**
 * Neural Load Ring - Core Engine SDK
 * 
 * Exports all public APIs for:
 * - HRV biometrics analysis (with adaptive frequency bands)
 * - Wellness scoring (coherence, variability)
 * - Pattern generation (thermal, vibration)
 * - Stream processing (real-time RR buffering)
 * - User profile management
 * 
 * @example
 * ```typescript
 * import { analyzeHRV, WellnessProcessor, generateThermalPattern } from '@nlr/core-engine';
 * 
 * const result = analyzeHRV(rrIntervals);
 * console.log(`Coherence: ${result.coherence}, Load: ${result.loadScore}`);
 * ```
 */

// ============================================================================
// TYPES
// ============================================================================
export * from './types/wellness.types';

// ============================================================================
// BIOMETRICS
// ============================================================================
export { 
	analyzeHRV, 
	type HRVAnalysisResult 
} from './algorithms/biometrics/hrvAnalysis';

export { 
	signalQuality, 
	type SignalQualityResult 
} from './algorithms/biometrics/signalQuality';

// ============================================================================
// WELLNESS SCORING
// ============================================================================
export { 
	scoreCoherence, 
	type CoherenceInput, 
	type CoherenceScore 
} from './algorithms/wellness/coherenceScoring';

export { 
	scoreVariability, 
	type VariabilityInput, 
	type VariabilityScore 
} from './algorithms/wellness/variabilityScoring';

// ============================================================================
// PATTERN GENERATION
// ============================================================================
export { 
	generateThermalPattern, 
	type ThermalInput, 
	type ThermalPrescription 
} from './algorithms/features/thermalPatterns';

export { 
	generateVibrationPattern, 
	type VibrationInput, 
	type VibrationPrescription 
} from './algorithms/features/vibrationPatterns';

// ============================================================================
// PROCESSORS (barrel export)
// ============================================================================
export {
	// Stream
	StreamProcessor,
	type StreamConfig,
	type StreamState,
	type StreamSnapshot,
	type StreamEvent,
	type StreamEventHandler,

	// Wellness
	WellnessProcessor,
	type WellnessConfig,
	type WellnessSnapshot as ProcessorSnapshot,
	type WellnessEventHandler,
	type StressLevel,
	type AdaptiveBandInfo,

	// Profiles
	UserProfileManager,
	type UserProfile,
	type UserBaseline,
	type UserDemographic,
	type UserPreferences,
	type CuePreferences,
	type CalibrationResult,
} from './processors';

// ============================================================================
// CORE ENGINE (re-export from parent)
// ============================================================================
export { 
	NeuralStress_awarenessProcessor,
	ValidatedHRVAlgorithms,
	VALIDATED_CONSTANTS 
} from '../wellnessEngine.v1.0';
