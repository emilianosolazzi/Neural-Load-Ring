// Thermal phase correction patterns for autonomic re-phasing.
// Maps systemic load to gentle, parasympathetic-reinforcing thermal cues.
// Does NOT stimulate; re-phases the autonomic clock through timing-dependent neural plasticity.

export type ThermalPattern = {
	intensityPercent: number;  // 0-100, clamped by actuator controller
	durationMs: number;         // cue duration (max 5000ms enforced by hardware)
	intervalMs?: number;        // repeat interval if patterned (optional)
};

export type ThermalInput = {
	loadScore: number;          // 0-100 from HRV analysis (coherence * confidence weighted)
	coherence: number;          // 0-1 phase coherence proxy (HF fraction of HRV)
	confidence: number;         // 0-1 signal confidence
};

export type ThermalPrescription = {
	enabled: boolean;
	pattern: ThermalPattern;
	reason: string; // for logging
};

// Lookup table: (adjusted_load) -> pattern.
// Adjusted load = observed_load - (coherence_boost if coherent).
// Rationale: if already coherent, reduce cue frequency; if incoherent, intervene more.
const THERMAL_PATTERNS = {
	optimal: { intensityPercent: 0, durationMs: 0 },
	low: { intensityPercent: 15, durationMs: 2000, intervalMs: 180000 }, // 3-min cycles, subtle warmth
	moderate: { intensityPercent: 30, durationMs: 3000, intervalMs: 120000 }, // 2-min cycles
	high: { intensityPercent: 50, durationMs: 4000, intervalMs: 60000 }, // frequent + warming
	needs_attention: { intensityPercent: 70, durationMs: 5000, intervalMs: 30000 }, // max cap
} as const;

const THERMAL_SAFETY = {
	MAX_INTENSITY: 60,
	MAX_DURATION_MS: 5000,
	MIN_INTERVAL_MS: 30000,
} as const;

export function generateThermalPattern(input: ThermalInput): ThermalPrescription {
	// Confidence threshold: if measurement is poor, don't intervene.
	if (input.confidence < 0.5) {
		return {
			enabled: false,
			pattern: THERMAL_PATTERNS.optimal,
			reason: "Low signal confidence; no intervention.",
		};
	}

	// Phase coherence boost: if already well-phased, reduce cue load.
	const coherence_boost = input.coherence > 0.7 ? 20 : input.coherence > 0.5 ? 10 : 0;
	const adjusted_load = Math.max(0, input.loadScore - coherence_boost);

	let pattern: (typeof THERMAL_PATTERNS)[keyof typeof THERMAL_PATTERNS];
	let reason: string;

	if (adjusted_load < 25) {
		pattern = THERMAL_PATTERNS.optimal;
		reason = "Optimal autonomic state; no thermal intervention needed.";
	} else if (adjusted_load < 45) {
		pattern = THERMAL_PATTERNS.low;
		reason = "Low systemic load; gentle parasympathetic reinforcement.";
	} else if (adjusted_load < 65) {
		pattern = THERMAL_PATTERNS.moderate;
		reason = "Moderate load; regular phase-correction warmth.";
	} else if (adjusted_load < 80) {
		pattern = THERMAL_PATTERNS.high;
		reason = "High load; increased thermal presence for re-phasing.";
	} else {
		pattern = THERMAL_PATTERNS.needs_attention;
		reason = "Critical load; maximal thermal intervention (5s cap enforced).";
	}

	const intervalMs = 'intervalMs' in pattern && pattern.intervalMs !== undefined
		? Math.max(pattern.intervalMs, THERMAL_SAFETY.MIN_INTERVAL_MS)
		: undefined;

	const safePattern: ThermalPattern = {
		intensityPercent: Math.min(pattern.intensityPercent, THERMAL_SAFETY.MAX_INTENSITY),
		durationMs: Math.min(pattern.durationMs, THERMAL_SAFETY.MAX_DURATION_MS),
		intervalMs,
	};

	return {
		enabled: safePattern.durationMs > 0,
		pattern: safePattern,
		reason,
	};
}