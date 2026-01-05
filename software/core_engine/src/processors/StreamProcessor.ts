/**
 * Real-time RR interval stream processor.
 * 
 * Buffers incoming RR intervals from BLE/sensor, enforces minimum window size,
 * detects artifacts, and signals when ready for HRV processing.
 * 
 * Features:
 * - Sliding window with configurable min/max size
 * - Automatic artifact detection (ectopy, dropped beats)
 * - Debounce to prevent over-processing
 * - Statistics tracking (sample count, reset events, gaps)
 * 
 * @example
 * ```typescript
 * const stream = new StreamProcessor({ minWindowSize: 64 });
 * 
 * bleService.onRRInterval((rr) => {
 *   if (stream.pushRR(rr)) {
 *     const buffer = stream.drain();
 *     const result = await engine.processRRIntervals(buffer);
 *   }
 * });
 * ```
 */

import { VALIDATED_CONSTANTS } from '../../wellnessEngine.v1.0';

export type StreamConfig = {
	/** Minimum RR samples before processing (default: 64) */
	minWindowSize: number;
	/** Maximum buffered samples before FIFO trim (default: 256) */
	maxWindowSize: number;
	/** RR > this triggers reset - likely ectopy/dropped beat (default: 2000ms) */
	resetThresholdMs: number;
	/** RR < this triggers reset - likely noise/artifact (default: 300ms) */
	minRRThresholdMs: number;
	/** Minimum time between process calls (default: 100ms) */
	debounceMs: number;
	/** Max beat-to-beat change ratio before flagging artifact (default: 0.20) */
	maxRateChange: number;
};

export type StreamState = {
	buffer: number[];
	lastProcessTime: number;
	sampleCount: number;
	resetCount: number;
	artifactCount: number;
	lastRR: number | null;
	gaps: number[];  // timestamps of detected gaps
};

export type StreamSnapshot = {
	bufferedSamples: number;
	readyForProcess: boolean;
	lastProcessAge: number;
	resetEvents: number;
	artifactRate: number;
	meanRR: number | null;
	instantHR: number | null;
};

export type StreamEvent = 
	| { type: 'ready'; samples: number }
	| { type: 'reset'; reason: string }
	| { type: 'artifact'; rr: number; reason: string };

export type StreamEventHandler = (event: StreamEvent) => void;

/**
 * Stateful RR stream accumulator with windowing and artifact detection.
 * 
 * Handles real-time RR intervals from wearable sensors with:
 * - Physiological validation (min/max RR, rate of change)
 * - Automatic buffer management (sliding window)
 * - Event callbacks for monitoring
 */
export class StreamProcessor {
	private config: StreamConfig;
	private state: StreamState;
	private eventHandler: StreamEventHandler | null = null;

	constructor(config: Partial<StreamConfig> = {}) {
		const { PHYSIOLOGICAL_LIMITS, SAMPLING } = VALIDATED_CONSTANTS;
		
		this.config = {
			minWindowSize: config.minWindowSize ?? SAMPLING.MIN_SAMPLES,
			maxWindowSize: config.maxWindowSize ?? 256,
			resetThresholdMs: config.resetThresholdMs ?? PHYSIOLOGICAL_LIMITS.MAX_RR_MS,
			minRRThresholdMs: config.minRRThresholdMs ?? PHYSIOLOGICAL_LIMITS.MIN_RR_MS,
			debounceMs: config.debounceMs ?? 100,
			maxRateChange: config.maxRateChange ?? PHYSIOLOGICAL_LIMITS.MAX_RATE_CHANGE,
		};

		this.state = {
			buffer: [],
			lastProcessTime: Date.now(),
			sampleCount: 0,
			resetCount: 0,
			artifactCount: 0,
			lastRR: null,
			gaps: [],
		};
	}

	/**
	 * Register event handler for stream events (ready, reset, artifact).
	 */
	onEvent(handler: StreamEventHandler): void {
		this.eventHandler = handler;
	}

	/**
	 * Add a single RR interval to the buffer.
	 * 
	 * @param rrMs - RR interval in milliseconds
	 * @returns true if buffer is ready for processing
	 */
	pushRR(rrMs: number): boolean {
		// Validate: too long (dropped beat, ectopy)
		if (rrMs > this.config.resetThresholdMs) {
			this.emitEvent({ type: 'reset', reason: `RR too long: ${rrMs}ms` });
			this.reset();
			return false;
		}

		// Validate: too short (noise, artifact)
		if (rrMs < this.config.minRRThresholdMs) {
			this.emitEvent({ type: 'artifact', rr: rrMs, reason: 'RR too short' });
			this.state.artifactCount++;
			return false; // Skip but don't reset
		}

		// Validate: rate of change (sudden jump suggests artifact)
		if (this.state.lastRR !== null) {
			const change = Math.abs(rrMs - this.state.lastRR) / this.state.lastRR;
			if (change > this.config.maxRateChange) {
				this.emitEvent({ type: 'artifact', rr: rrMs, reason: `Rate change ${(change * 100).toFixed(1)}%` });
				this.state.artifactCount++;
				// Don't add to buffer, but don't reset either
				return false;
			}
		}

		// Add to buffer
		this.state.buffer.push(rrMs);
		this.state.sampleCount++;
		this.state.lastRR = rrMs;

		// FIFO trim if exceeds max
		if (this.state.buffer.length > this.config.maxWindowSize) {
			this.state.buffer.shift();
		}

		// Check if ready
		const age = Date.now() - this.state.lastProcessTime;
		const ready = this.state.buffer.length >= this.config.minWindowSize &&
		              age >= this.config.debounceMs;

		if (ready) {
			this.emitEvent({ type: 'ready', samples: this.state.buffer.length });
		}

		return ready;
	}

	/**
	 * Add multiple RR intervals at once (batch processing).
	 * 
	 * @param rrArray - Array of RR intervals in milliseconds
	 * @returns true if buffer is ready for processing after batch
	 */
	pushBatch(rrArray: number[]): boolean {
		let ready = false;
		for (const rr of rrArray) {
			ready = this.pushRR(rr);
		}
		return ready;
	}

	/**
	 * Get current buffer snapshot without modifying state.
	 */
	peek(): number[] {
		return [...this.state.buffer];
	}

	/**
	 * Extract buffer for processing and reset for next window.
	 * Uses sliding window with 50% overlap by default.
	 * 
	 * @param overlap - Fraction of buffer to retain (0-0.9, default: 0.5)
	 */
	drain(overlap: number = 0.5): number[] {
		const result = [...this.state.buffer];
		
		// Sliding window: keep overlap fraction for continuity
		const keepCount = Math.floor(this.state.buffer.length * Math.min(0.9, Math.max(0, overlap)));
		
		// Handle overlap=0 case: clear the buffer completely
		if (keepCount === 0) {
			this.state.buffer = [];
		} else {
			this.state.buffer = this.state.buffer.slice(-keepCount);
		}
		
		this.state.lastProcessTime = Date.now();
		this.state.artifactCount = 0; // Reset artifact count per window
		
		return result;
	}

	/**
	 * Clear buffer and reset counters (e.g., on sensor reconnect).
	 */
	reset(): void {
		this.state.buffer = [];
		this.state.resetCount++;
		this.state.lastProcessTime = Date.now();
		this.state.lastRR = null;
		this.state.artifactCount = 0;
		this.state.gaps.push(Date.now());
		
		// Keep only last 10 gaps for memory
		if (this.state.gaps.length > 10) {
			this.state.gaps = this.state.gaps.slice(-10);
		}
	}

	/**
	 * Get processor state snapshot for diagnostics.
	 */
	snapshot(): StreamSnapshot {
		const now = Date.now();
		const buffer = this.state.buffer;
		
		// Calculate mean RR and instant HR if we have data
		let meanRR: number | null = null;
		let instantHR: number | null = null;
		
		if (buffer.length > 0) {
			meanRR = buffer.reduce((a, b) => a + b, 0) / buffer.length;
			instantHR = 60000 / meanRR;
		}

		const totalSamples = this.state.sampleCount || 1;
		
		return {
			bufferedSamples: buffer.length,
			readyForProcess: buffer.length >= this.config.minWindowSize &&
			                (now - this.state.lastProcessTime) >= this.config.debounceMs,
			lastProcessAge: now - this.state.lastProcessTime,
			resetEvents: this.state.resetCount,
			artifactRate: this.state.artifactCount / totalSamples,
			meanRR,
			instantHR,
		};
	}

	/**
	 * Get current buffer statistics.
	 */
	getStats(): {
		totalSamples: number;
		totalResets: number;
		totalArtifacts: number;
		recentGaps: number[];
		bufferFillPercent: number;
	} {
		return {
			totalSamples: this.state.sampleCount,
			totalResets: this.state.resetCount,
			totalArtifacts: this.state.artifactCount,
			recentGaps: [...this.state.gaps],
			bufferFillPercent: (this.state.buffer.length / this.config.maxWindowSize) * 100,
		};
	}

	/**
	 * Get config for inspection (read-only).
	 */
	getConfig(): Readonly<StreamConfig> {
		return Object.freeze({ ...this.config });
	}

	/**
	 * Update config at runtime.
	 */
	updateConfig(updates: Partial<StreamConfig>): void {
		this.config = { ...this.config, ...updates };
	}

	/**
	 * Check if processor has enough data for meaningful analysis.
	 */
	isReady(): boolean {
		return this.state.buffer.length >= this.config.minWindowSize;
	}

	/**
	 * Get time until next processing window is ready.
	 */
	timeUntilReady(): number {
		const age = Date.now() - this.state.lastProcessTime;
		const debounceRemaining = Math.max(0, this.config.debounceMs - age);
		const samplesNeeded = Math.max(0, this.config.minWindowSize - this.state.buffer.length);
		
		// Estimate ~850ms per RR interval (70 bpm average)
		const estimatedSampleTime = samplesNeeded * 850;
		
		return Math.max(debounceRemaining, estimatedSampleTime);
	}

	private emitEvent(event: StreamEvent): void {
		if (this.eventHandler) {
			try {
				this.eventHandler(event);
			} catch (e) {
				console.error('StreamProcessor event handler error:', e);
			}
		}
	}
}