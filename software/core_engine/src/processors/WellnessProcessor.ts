/**
 * Integrated wellness processing pipeline.
 * 
 * Orchestrates the full Neural Load Ring processing flow:
 * 1. Stream buffering (RR intervals from BLE)
 * 2. HRV analysis (time + frequency domain)
 * 3. Stress classification (with adaptive bands)
 * 4. Multi-modal prescription generation (thermal + vibration)
 * 
 * Key Features:
 * - Adaptive frequency bands for athletes/meditators
 * - User profile-based personalization
 * - Safety-capped prescriptions (thermal: 60%, vibration: 50%)
 * - Event-driven processing with configurable callbacks
 * 
 * @example
 * ```typescript
 * const processor = new WellnessProcessor({
 *   userProfileManager: new UserProfileManager(),
 *   minSamplesPerWindow: 64,
 * });
 * 
 * processor.setUser('user-123');
 * processor.onWellnessUpdate((snapshot) => {
 *   updateUI(snapshot);
 * });
 * 
 * bleService.onRRInterval(async (rr) => {
 *   if (processor.pushRR(rr)) {
 *     await processor.process();
 *   }
 * });
 * ```
 */

import { NeuralStress_awarenessProcessor } from '../../wellnessEngine.v1.0';
import { StreamProcessor, StreamSnapshot, StreamEvent } from './StreamProcessor';
import { UserProfileManager } from './UserProfileManager';
import { analyzeHRV } from '../algorithms/biometrics/hrvAnalysis';
import { signalQuality, SignalQualityResult } from '../algorithms/biometrics/signalQuality';
import { generateThermalPattern, ThermalPrescription } from '../algorithms/features/thermalPatterns';
import { generateVibrationPattern, VibrationPrescription } from '../algorithms/features/vibrationPatterns';

type ProcessedResult = Awaited<ReturnType<NeuralStress_awarenessProcessor['processRRIntervals']>>;

// ============================================================================
// Types
// ============================================================================

export type StressLevel = 'optimal' | 'low' | 'moderate' | 'high' | 'needs_attention';

export type AdaptiveBandInfo = {
  isAdapted: boolean;
  hf: { low: number; high: number };
  lf: { low: number; high: number };
  detectedRespHz: number | null;
  respiratoryFrequency?: number | null;
  prominence?: number;
  confidence?: number;
};

export type WellnessSnapshot = {
  /** Processing timestamp (Unix ms) */
  processedAt: number;
  
  /** Stream buffer state */
  streamState: {
    bufferedSamples: number;
    readyForProcess: boolean;
    artifactRate: number;
  };
  
  /** Core biometric measurements */
  biometrics: {
    /** Neural load score (0-100, higher = more stress) */
    loadScore: number;
    /** Stress awareness level */
    stressLevel: StressLevel;
    /** Phase coherence (0-1, higher = more regulated) */
    coherence: number;
    /** Measurement confidence (0-1) */
    confidence: number;
    /** Heart rate variability (RMSSD in ms) */
    rmssd?: number;
    /** Mean heart rate (BPM) */
    meanHR?: number;
  };
  
  /** Adaptive frequency band information */
  adaptiveBands?: AdaptiveBandInfo;
  
  /** Multi-modal prescriptions */
  prescription: {
    thermal: ThermalPrescription;
    vibration: VibrationPrescription;
  };
  
  /** Active user ID */
  userId?: string;
  
  /** Processing diagnostics */
  diagnostics?: {
    processingTimeMs: number;
    signalQuality: number;
    windowSamples: number;
  };
};

export type WellnessConfig = {
  /** User profile manager instance */
  userProfileManager: UserProfileManager;
  /** Minimum samples before processing (default: 64) */
  minSamplesPerWindow?: number;
  /** Debounce between process calls (default: 500ms) */
  debounceMs?: number;
  /** Minimum signal quality to generate prescriptions (default: 0.5) */
  minSignalQuality?: number;
  /** Enable adaptive frequency bands (default: true) */
  enableAdaptiveBands?: boolean;
  /** Sliding window overlap (default: 0.5) */
  windowOverlap?: number;
};

export type WellnessEventHandler = (snapshot: WellnessSnapshot) => void;

// ============================================================================
// Stress Level Mapping
// ============================================================================

/**
 * Map stress awareness level to numeric load score.
 */
function stressLevelToScore(level: StressLevel): number {
  switch (level) {
    case 'optimal': return 15;
    case 'low': return 35;
    case 'moderate': return 55;
    case 'high': return 75;
    case 'needs_attention': return 90;
    default: return 55;
  }
}

/**
 * Map numeric load score to stress level.
 */
function scoreToStressLevel(score: number): StressLevel {
  if (score < 25) return 'optimal';
  if (score < 45) return 'low';
  if (score < 65) return 'moderate';
  if (score < 85) return 'high';
  return 'needs_attention';
}

// ============================================================================
// WellnessProcessor Class
// ============================================================================

/**
 * Production wellness processor: orchestrates stream buffering, HRV analysis,
 * stress classification with adaptive bands, and multi-modal cue generation.
 */
export class WellnessProcessor {
  private streamProcessor: StreamProcessor;
  private engine: NeuralStress_awarenessProcessor;
  private profileManager: UserProfileManager;
  private config: Required<Omit<WellnessConfig, 'userProfileManager'>>;
  
  private lastSnapshot: WellnessSnapshot | null = null;
  private userId: string | null = null;
  private eventHandler: WellnessEventHandler | null = null;
  
  // Processing statistics
  private processCount: number = 0;
  private totalProcessingTimeMs: number = 0;

  constructor(config: WellnessConfig) {
    this.config = {
      minSamplesPerWindow: config.minSamplesPerWindow ?? 64,
      debounceMs: config.debounceMs ?? 500,
      minSignalQuality: config.minSignalQuality ?? 0.5,
      enableAdaptiveBands: config.enableAdaptiveBands ?? true,
      windowOverlap: config.windowOverlap ?? 0.5,
    };

    this.streamProcessor = new StreamProcessor({
      minWindowSize: this.config.minSamplesPerWindow,
      debounceMs: this.config.debounceMs,
    });
    
    this.engine = new NeuralStress_awarenessProcessor();
    this.profileManager = config.userProfileManager;
    
    // Forward stream events
    this.streamProcessor.onEvent(this.handleStreamEvent.bind(this));
  }

  // ========================================================================
  // Public API
  // ========================================================================

  /**
   * Set active user for personalized processing.
   */
  setUser(userId: string): void {
    this.userId = userId;
  }

  /**
   * Get current active user ID.
   */
  getUser(): string | null {
    return this.userId;
  }

  /**
   * Register callback for wellness updates.
   */
  onWellnessUpdate(handler: WellnessEventHandler): void {
    this.eventHandler = handler;
  }

  /**
   * Add RR interval to stream buffer.
   * @returns true if buffer is ready for processing
   */
  pushRR(rrMs: number): boolean {
    return this.streamProcessor.pushRR(rrMs);
  }

  /**
   * Add multiple RR intervals at once.
   * @returns true if buffer is ready for processing after batch
   */
  pushBatch(rrIntervals: number[]): boolean {
    return this.streamProcessor.pushBatch(rrIntervals);
  }

  /**
   * Process buffered RR intervals end-to-end if ready.
   * @returns wellness snapshot or null if not enough data
   */
  async process(): Promise<WellnessSnapshot | null> {
    const state = this.streamProcessor.snapshot();
    if (!state.readyForProcess) {
      return null;
    }

    const startTime = Date.now();
    const buffer = this.streamProcessor.drain(this.config.windowOverlap);
    const windowSamples = buffer.length;

    // Signal quality assessment
    const quality = signalQuality(buffer);
    
    // Guard: poor signal quality → return degraded snapshot
    if (quality.confidence < this.config.minSignalQuality) {
      const degradedSnapshot = this.createDegradedSnapshot(quality, state);
      this.lastSnapshot = degradedSnapshot;
      this.emitUpdate(degradedSnapshot);
      return degradedSnapshot;
    }

    // HRV analysis for additional metrics
    const hrv = analyzeHRV(buffer);

    // Main engine processing (includes adaptive bands)
    const engineResult = await this.engine.processRRIntervals(buffer);

    // Extract metrics from engine result
    // NOTE: Engine returns stress_awarenessLevel (string), meanCoherence (number), confidence (number)
    const loadScore = stressLevelToScore(engineResult.stress_awarenessLevel);
    const stressLevel = engineResult.stress_awarenessLevel as StressLevel;
    const coherence = engineResult.meanCoherence;
    const confidence = engineResult.confidence;

    // Build adaptive band info
    const adaptiveBands = this.extractAdaptiveBandInfo(engineResult);

    // Create full snapshot
    const snapshot = this.createSnapshot({
      loadScore,
      stressLevel,
      coherence,
      confidence,
      rmssd: hrv.timeDomain.rmssd,
      meanHR: hrv.timeDomain.hr,
      adaptiveBands,
      signalQuality: quality.confidence,
      windowSamples,
      processingTimeMs: Date.now() - startTime,
    });

    // Update statistics
    this.processCount++;
    this.totalProcessingTimeMs += snapshot.diagnostics?.processingTimeMs ?? 0;

    this.lastSnapshot = snapshot;
    this.emitUpdate(snapshot);
    return snapshot;
  }

  /**
   * Force process regardless of ready state (for testing/debugging).
   */
  async forceProcess(): Promise<WellnessSnapshot | null> {
    const buffer = this.streamProcessor.peek();
    if (buffer.length < 8) {
      return null; // Absolute minimum
    }

    // Temporarily drain without overlap
    const data = this.streamProcessor.drain(0);
    
    const quality = signalQuality(data);
    const hrv = analyzeHRV(data);
    const engineResult = await this.engine.processRRIntervals(data);

    const loadScore = stressLevelToScore(engineResult.stress_awarenessLevel);
    const snapshot = this.createSnapshot({
      loadScore,
      stressLevel: engineResult.stress_awarenessLevel as StressLevel,
      coherence: engineResult.meanCoherence,
      confidence: engineResult.confidence,
      rmssd: hrv.timeDomain.rmssd,
      meanHR: hrv.timeDomain.hr,
      signalQuality: quality.confidence,
      windowSamples: data.length,
      processingTimeMs: 0,
    });

    this.lastSnapshot = snapshot;
    return snapshot;
  }

  /**
   * Get last computed wellness snapshot.
   */
  lastResult(): WellnessSnapshot | null {
    return this.lastSnapshot;
  }

  /**
   * Get stream buffer state without triggering processing.
   */
  getStreamState(): StreamSnapshot {
    return this.streamProcessor.snapshot();
  }

  /**
   * Check if processor is ready to process.
   */
  isReady(): boolean {
    return this.streamProcessor.isReady();
  }

  /**
   * Reset stream and state (e.g., on sensor reconnect).
   */
  reset(): void {
    this.streamProcessor.reset();
    this.lastSnapshot = null;
  }

  /**
   * Get processing statistics.
   */
  getStats(): {
    processCount: number;
    avgProcessingTimeMs: number;
    streamStats: ReturnType<StreamProcessor['getStats']>;
  } {
    return {
      processCount: this.processCount,
      avgProcessingTimeMs: this.processCount > 0 
        ? this.totalProcessingTimeMs / this.processCount 
        : 0,
      streamStats: this.streamProcessor.getStats(),
    };
  }

  // ========================================================================
  // Private Helpers
  // ========================================================================

  /**
   * Extract adaptive band information from engine result.
   */
  private extractAdaptiveBandInfo(result: ProcessedResult): AdaptiveBandInfo | undefined {
    if (!result.adaptiveBands) {
      return undefined;
    }

    return {
      isAdapted: result.adaptiveBands.isAdapted,
      hf: { low: result.adaptiveBands.hf.low, high: result.adaptiveBands.hf.high },
      lf: { low: result.adaptiveBands.lf.low, high: result.adaptiveBands.lf.high },
      detectedRespHz: result.adaptiveBands.detectedRespHz,
      respiratoryFrequency: result.respiratoryDetection?.respiratoryFrequency ?? null,
      prominence: result.respiratoryDetection?.prominence,
      confidence: result.respiratoryDetection?.confidence,
    };
  }

  /**
   * Create degraded snapshot for low signal quality scenarios.
   */
  private createDegradedSnapshot(
    quality: SignalQualityResult,
    streamState: StreamSnapshot
  ): WellnessSnapshot {
    // Conservative defaults when signal is poor
    const defaultMetrics = {
      loadScore: 50,
      stressLevel: 'moderate' as StressLevel,
      coherence: 0.5,
      confidence: quality.confidence,
    };

    return {
      processedAt: Date.now(),
      streamState: {
        bufferedSamples: streamState.bufferedSamples,
        readyForProcess: streamState.readyForProcess,
        artifactRate: streamState.artifactRate,
      },
      biometrics: defaultMetrics,
      prescription: {
        thermal: { enabled: false, pattern: { intensityPercent: 0, durationMs: 0 }, reason: 'Low signal quality' },
        vibration: { enabled: false, pattern: { intensityPercent: 0, durationMs: 0 }, reason: 'Low signal quality' },
      },
      userId: this.userId ?? undefined,
      diagnostics: {
        processingTimeMs: 0,
        signalQuality: quality.confidence,
        windowSamples: 0,
      },
    };
  }

  /**
   * Create wellness snapshot with personalization applied.
   */
  private createSnapshot(metrics: {
    loadScore: number;
    stressLevel: StressLevel;
    coherence: number;
    confidence: number;
    rmssd?: number;
    meanHR?: number;
    adaptiveBands?: AdaptiveBandInfo;
    signalQuality: number;
    windowSamples: number;
    processingTimeMs: number;
  }): WellnessSnapshot {
    const prescriptionInput = {
      loadScore: metrics.loadScore,
      coherence: metrics.coherence,
      confidence: metrics.confidence,
    };

    let thermal = generateThermalPattern(prescriptionInput);
    let vibration = generateVibrationPattern(prescriptionInput);

    // Apply user personalization
    if (this.userId) {
      const prefs = this.profileManager.getPreferences(this.userId);

      // Disable modalities if user prefers
      if (!prefs.thermalEnabled) {
        thermal = { 
          enabled: false, 
          pattern: { intensityPercent: 0, durationMs: 0 },
          reason: 'Thermal disabled by preference',
        };
      }
      if (!prefs.vibrationEnabled) {
        vibration = { 
          enabled: false, 
          pattern: { intensityPercent: 0, durationMs: 0 },
          reason: 'Vibration disabled by preference',
        };
      }

      // Scale intensities based on user sensitivity
      if (thermal.enabled && thermal.pattern) {
        thermal.pattern.intensityPercent = this.profileManager.scalePrescription(
          this.userId,
          thermal.pattern.intensityPercent,
          'thermal'
        );
      }
      if (vibration.enabled && vibration.pattern) {
        vibration.pattern.intensityPercent = this.profileManager.scalePrescription(
          this.userId,
          vibration.pattern.intensityPercent,
          'vibration'
        );
      }
    }

    const streamState = this.streamProcessor.snapshot();

    return {
      processedAt: Date.now(),
      streamState: {
        bufferedSamples: streamState.bufferedSamples,
        readyForProcess: streamState.readyForProcess,
        artifactRate: streamState.artifactRate,
      },
      biometrics: {
        loadScore: metrics.loadScore,
        stressLevel: metrics.stressLevel,
        coherence: metrics.coherence,
        confidence: metrics.confidence,
        rmssd: metrics.rmssd,
        meanHR: metrics.meanHR,
      },
      adaptiveBands: metrics.adaptiveBands,
      prescription: { thermal, vibration },
      userId: this.userId ?? undefined,
      diagnostics: {
        processingTimeMs: metrics.processingTimeMs,
        signalQuality: metrics.signalQuality,
        windowSamples: metrics.windowSamples,
      },
    };
  }

  /**
   * Handle stream processor events.
   */
  private handleStreamEvent(event: StreamEvent): void {
    // Could log, emit metrics, or trigger UI updates
    if (event.type === 'reset') {
      console.debug(`WellnessProcessor: Stream reset - ${event.reason}`);
    }
  }

  /**
   * Emit wellness update to registered handler.
   */
  private emitUpdate(snapshot: WellnessSnapshot): void {
    if (this.eventHandler) {
      try {
        this.eventHandler(snapshot);
      } catch (e) {
        console.error('WellnessProcessor event handler error:', e);
      }
    }
  }
}
