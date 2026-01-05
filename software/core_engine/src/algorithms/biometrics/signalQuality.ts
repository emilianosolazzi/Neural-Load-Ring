// Signal quality estimation for RR streams: artifacts, stability, amplitude proxy.

export type SignalQualityResult = {
	artifactRate: number;   // 0-1
	stability: number;      // 0-1 (lower beat-to-beat variance => higher stability)
	confidence: number;     // aggregate 0-1
	issues?: string[];      // optional notes/warnings
};

const clamp01 = (v: number) => Math.min(1, Math.max(0, v));

export function signalQuality(rrMs: number[]): SignalQualityResult {
	if (!Array.isArray(rrMs) || rrMs.length < 4) {
		return { artifactRate: 1, stability: 0, confidence: 0, issues: ['insufficient data'] };
	}

	const diffs = [] as number[];
	let artifacts = 0;
	for (let i = 1; i < rrMs.length; i++) {
		const prev = rrMs[i - 1];
		const curr = rrMs[i];
		const diff = Math.abs(curr - prev);
		diffs.push(diff);
		// Flag as artifact if jump >20% of mean of the pair
		const meanPair = (prev + curr) * 0.5;
		if (meanPair > 0 && diff / meanPair > 0.20) artifacts++;
	}

	const artifactRate = artifacts / Math.max(1, rrMs.length - 1);

	// Stability: inverse of normalized RMSSD surrogate
	const meanDiff = diffs.reduce((a, b) => a + b, 0) / diffs.length;
	const varDiff = diffs.reduce((a, b) => a + Math.pow(b - meanDiff, 2), 0) / diffs.length;
	const sdDiff = Math.sqrt(varDiff);
	// Normalize using a soft cap at 80 ms
	const stability = 1 - clamp01(sdDiff / 80);

	// Confidence combines low artifacts and high stability
	const confidence = clamp01((1 - artifactRate) * 0.6 + stability * 0.4);

	const tissues: string[] = [];
	if (artifactRate > 0.3) tissues.push('high artifact rate');
	if (stability < 0.4) tissues.push('unstable rhythm');

	return { artifactRate, stability, confidence, issues: tissues.length ? tissues : undefined };
}