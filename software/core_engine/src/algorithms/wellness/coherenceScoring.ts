// Autonomic phase coherence scoring.
// Measures respiratory-cardiac synchronization via HF/(LF+HF) ratio.
// High coherence = relaxed (parasympathetic); low coherence = stressed (sympathetic).

export type CoherenceInput = {
	frequencyDomain: {
		hfPower: number;    // high-frequency power (0.15-0.4 Hz or adaptive)
		lfPower: number;    // low-frequency power (0.04-0.15 Hz or adaptive)
		totalPower: number; // total spectral power
	};
	sampleCount: number;    // number of RR intervals processed
	adaptiveBands?: {
		isAdapted: boolean;
		detectedRespHz: number | null;
	};
};

export type CoherenceScore = {
	phaseCoherence: number;     // 0-1, HF/(LF+HF) ratio
	lfhfRatio: number;          // LF/HF ratio (sympathetic/parasympathetic balance)
	spectralQuality: number;    // 0-1, confidence in spectral estimate
	interpretation: string;     // human-readable state description
	bandType: 'standard' | 'adaptive'; // which bands were used
};

/**
 * Calculate autonomic phase coherence from frequency-domain HRV.
 * 
 * CORRECT Formula: coherence = HF / (LF + HF)
 * - Excludes VLF and DC components (noise, slow trends)
 * - Uses only acute autonomic balance (LF vs HF)
 * 
 * Interpretation:
 * - > 0.6: coherent (parasympathetic dominance, relaxed/flow state)
 * - 0.3-0.6: mixed (transitional, variable)
 * - < 0.3: incoherent (sympathetic dominance, stressed)
 * 
 * Note: With adaptive bands, slow breathers (athletes, meditators) are
 * correctly classified as relaxed instead of falsely showing as stressed.
 */
export function scoreCoherence(input: CoherenceInput): CoherenceScore {
	const { hfPower, lfPower, totalPower } = input.frequencyDomain;

	// Guard: insufficient signal
	if (totalPower < 1 || input.sampleCount < 32) {
		return {
			phaseCoherence: 0.5, // neutral default
			lfhfRatio: 1.0,
			spectralQuality: 0,
			interpretation: "Insufficient signal for coherence assessment.",
			bandType: 'standard',
		};
	}

	// CORRECT: Phase coherence = HF / (LF + HF)
	// This formula excludes VLF/DC and focuses on acute autonomic balance
	const denominator = lfPower + hfPower;
	const phaseCoherence = denominator > 0 
		? Math.min(1, hfPower / denominator) 
		: 0;
	
	const lfhfRatio = hfPower > 0.001 ? lfPower / hfPower : 999; // avoid division by zero

	// Spectral quality: confidence based on power distribution and sample adequacy
	const powerBalance = Math.min(hfPower, lfPower) / Math.max(hfPower, lfPower, 0.001);
	const sampleBoost = Math.min(1, input.sampleCount / 256); // full confidence at 256+ samples
	const spectralQuality = Math.min(1, powerBalance * 0.5 + sampleBoost * 0.5);

	// Interpretation with updated thresholds
	let interpretation: string;
	if (phaseCoherence > 0.6) {
		interpretation = "Coherent: parasympathetic dominance, relaxed/flow state.";
	} else if (phaseCoherence > 0.3) {
		interpretation = "Mixed: transitional ANS state, variable coherence.";
	} else {
		interpretation = "Incoherent: sympathetic dominance, stressed state.";
	}

	// Add adaptive band note if applicable
	const bandType = input.adaptiveBands?.isAdapted ? 'adaptive' : 'standard';
	if (bandType === 'adaptive' && input.adaptiveBands?.detectedRespHz) {
		const bpm = Math.round(input.adaptiveBands.detectedRespHz * 60);
		interpretation += ` (Breathing: ~${bpm} breaths/min)`;
	}

	return {
		phaseCoherence,
		lfhfRatio,
		spectralQuality,
		interpretation,
		bandType,
	};
}