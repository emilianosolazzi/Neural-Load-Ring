// Vibration phase correction patterns for cutaneous autonomic timing cues.
// Uses brief, rhythmic pulses to reinforce parasympathetic tone without startling.
// Frequency and intensity map to systemic load; patterns preserve the "invisible" principle.

export type VibrationPattern = {
	intensityPercent: number;  // 0-100, clamped by actuator controller
	durationMs: number;        // pulse train duration
	frequencyHz?: number;      // pulse repetition rate (0Hz = no pulse, e.g., sustained buzz)
	burstCount?: number;       // number of pulse bursts (optional)
};

export type VibrationInput = {
	loadScore: number;         // 0-100 from HRV analysis
	coherence: number;         // 0-1 phase coherence
	confidence: number;        // 0-1 signal confidence
};

export type VibrationPrescription = {
	enabled: boolean;
	pattern: VibrationPattern;
	reason: string;
};

// Vibration patterns: sub-perceptual to noticeable, never jarring.
// Lower frequencies (20-40Hz) feel sustained; higher (60+Hz) feel sharp.
// Burst counts avoid tonic stimulation; keep pulses brief.
const VIBRATION_PATTERNS = {
	optimal: { intensityPercent: 0, durationMs: 0 },
	low: { intensityPercent: 10, durationMs: 200, frequencyHz: 35, burstCount: 2 }, // barely noticeable
	moderate: { intensityPercent: 20, durationMs: 300, frequencyHz: 40, burstCount: 3 }, // subtle cueing
	high: { intensityPercent: 35, durationMs: 400, frequencyHz: 50, burstCount: 4 }, // more salient
	needs_attention: { intensityPercent: 50, durationMs: 500, frequencyHz: 65, burstCount: 5 }, // rhythmic alert
} as const;

const VIBRATION_SAFETY = {
	MAX_INTENSITY: 50,
	MAX_DURATION_MS: 800,
	MAX_FREQUENCY_HZ: 80,
	MAX_BURST_COUNT: 5,
} as const;

export function generateVibrationPattern(input: VibrationInput): VibrationPrescription {
	if (input.confidence < 0.5) {
		return {
			enabled: false,
			pattern: VIBRATION_PATTERNS.optimal,
			reason: "Low signal confidence; no intervention.",
		};
	}

	const coherence_boost = input.coherence > 0.7 ? 20 : input.coherence > 0.5 ? 10 : 0;
	const adjusted_load = Math.max(0, input.loadScore - coherence_boost);

	let pattern: (typeof VIBRATION_PATTERNS)[keyof typeof VIBRATION_PATTERNS];
	let reason: string;

	if (adjusted_load < 25) {
		pattern = VIBRATION_PATTERNS.optimal;
		reason = "Optimal autonomic state; no vibration cue needed.";
	} else if (adjusted_load < 45) {
		pattern = VIBRATION_PATTERNS.low;
		reason = "Low load; sub-perceptual vibration for subtle phase cueing.";
	} else if (adjusted_load < 65) {
		pattern = VIBRATION_PATTERNS.moderate;
		reason = "Moderate load; rhythmic vibration reinforces parasympathetic tone.";
	} else if (adjusted_load < 80) {
		pattern = VIBRATION_PATTERNS.high;
		reason = "High load; increased vibration salience for autonomic re-phasing.";
	} else {
		pattern = VIBRATION_PATTERNS.needs_attention;
		reason = "Critical load; maximal vibration alerting (5s cap enforced).";
	}

	const frequencyHz = 'frequencyHz' in pattern && pattern.frequencyHz !== undefined
		? Math.min(pattern.frequencyHz, VIBRATION_SAFETY.MAX_FREQUENCY_HZ)
		: undefined;

	const burstCount = 'burstCount' in pattern && pattern.burstCount !== undefined
		? Math.min(pattern.burstCount, VIBRATION_SAFETY.MAX_BURST_COUNT)
		: undefined;

	const safePattern: VibrationPattern = {
		intensityPercent: Math.min(pattern.intensityPercent, VIBRATION_SAFETY.MAX_INTENSITY),
		durationMs: Math.min(pattern.durationMs, VIBRATION_SAFETY.MAX_DURATION_MS),
		frequencyHz,
		burstCount,
	};

	return {
		enabled: safePattern.durationMs > 0,
		pattern: safePattern,
		reason,
	};
}