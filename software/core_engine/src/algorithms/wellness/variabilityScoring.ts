/**
 * Parasympathetic tone assessment via heart rate variability signatures.
 * 
 * High variability = healthy parasympathetic activity
 * Low variability = autonomic rigidity / sympathetic dominance
 * 
 * Maps to the "micro-variability" concept from the core engine's relaxation features.
 * 
 * References:
 * - Kubios HRV thresholds (adult resting)
 * - Task Force of ESC & NASPE (1996)
 */

export type VariabilityInput = {
	rrIntervalsMs: number[];     // RR intervals in milliseconds
	timeDomainMetrics: {
		sdnn: number;               // standard deviation of NN intervals (ms)
		rmssd: number;              // root mean square of successive differences (ms)
	};
	frequencyDomainMetrics: {
		hfPower: number;            // 0.15-0.4 Hz or adaptive (parasympathetic band)
		lfPower: number;            // 0.04-0.15 Hz or adaptive (sympathetic/mixed band)
	};
};

export type VariabilityScore = {
	parasympatheticTone: number;  // 0-1, proxy for vagal activity
	variabilityIndex: number;     // 0-100, overall HRV richness
	autonomicBalance: number;     // -1 to +1, -1=sympathetic, 0=balanced, +1=parasympathetic
	interpretation: string;       // human-readable HRV state
	riskLevel: 'normal' | 'elevated' | 'attention'; // wellness advisory
};

/**
 * Assess parasympathetic (vagal) tone via HRV variability signatures.
 * 
 * Healthy parasympathetic tone manifests as:
 * - High RMSSD (beat-to-beat variation, vagal influence)
 * - High HF power (respiratory sinus arrhythmia)
 * - Low LF/HF ratio (parasympathetic dominance)
 *
 * Clinical thresholds (adult resting, Kubios reference):
 * - RMSSD > 50ms: good vagal tone
 * - RMSSD 25-50ms: moderate
 * - RMSSD < 25ms: low / sympathetic dominance
 * 
 * @param input - HRV metrics from analyzeHRV()
 * @returns Variability score with parasympathetic assessment
 */
export function scoreVariability(input: VariabilityInput): VariabilityScore {
	const { sdnn, rmssd } = input.timeDomainMetrics;
	const { hfPower, lfPower } = input.frequencyDomainMetrics;

	// Parasympathetic tone from RMSSD (0-1 scale, normalized to 50ms ceiling)
	// RMSSD is the gold-standard marker for vagal tone
	const parasympatheticTone = Math.min(1, rmssd / 50);

	// Variability index (0-100) combines SDNN and RMSSD for overall HRV richness
	// SDNN measures long-term variability, RMSSD measures beat-to-beat
	const sdnnNorm = Math.min(1, sdnn / 150); // ceiling ~150ms for resting
	const rmssdNorm = Math.min(1, rmssd / 50);
	const variabilityIndex = (sdnnNorm * 0.4 + rmssdNorm * 0.6) * 100; // weight RMSSD higher (vagal)

	// Autonomic balance: -1=sympathetic (low HF, high LF), +1=parasympathetic (high HF, low LF)
	// Uses the CORRECT formula: HF / (LF + HF) mapped to [-1, +1]
	const totalPower = hfPower + lfPower;
	const hfFraction = totalPower > 0 ? hfPower / totalPower : 0.5;
	const autonomicBalance = (hfFraction * 2) - 1; // maps [0, 1] -> [-1, +1]

	// Risk level based on variability index
	let riskLevel: VariabilityScore['riskLevel'];
	if (variabilityIndex >= 50) {
		riskLevel = 'normal';
	} else if (variabilityIndex >= 25) {
		riskLevel = 'elevated';
	} else {
		riskLevel = 'attention';
	}

	// Interpretation
	let interpretation: string;
	if (parasympatheticTone > 0.6) {
		interpretation = "Strong parasympathetic tone: relaxed, vagally-dominant ANS.";
	} else if (parasympatheticTone > 0.3) {
		interpretation = "Moderate parasympathetic tone: balanced ANS activity.";
	} else {
		interpretation = "Low parasympathetic tone: sympathetic dominance, stress state.";
	}

	// Add autonomic balance context
	if (autonomicBalance > 0.3) {
		interpretation += " Autonomic balance favors relaxation.";
	} else if (autonomicBalance < -0.3) {
		interpretation += " Autonomic balance favors alertness.";
	}

	return {
		parasympatheticTone,
		variabilityIndex,
		autonomicBalance,
		interpretation,
		riskLevel,
	};
}