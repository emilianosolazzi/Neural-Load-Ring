/**
 * @file engineBridge.ts
 * @brief Neural Load Ring - Engine-to-Haptic Bridge
 *
 * This is the orchestration layer that creates the intelligent feedback loop:
 *   Ring (BLE) → Engine (HRV Analysis) → Cue Generator → Ring (Actuators)
 *
 * Key integrations:
 *   • micro‑variability → vibration (neural chaos → grounding pulse)
 *   • coherence dips → thermal (low coherence → warming comfort)
 *   • stability → breathing patterns (unstable → guided breathing)
 *   • confidence gating (suppress cues when data quality is poor)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

import NeuralStress_awarenessProcessor, { VALIDATED_CONSTANTS } from '../../../../core_engine/wellnessEngine.v1.0';
import { 
  HapticCueGenerator, 
  EngineMetrics, 
  HapticCue, 
  CuePreferences 
} from '../../../../core_engine/hapticCueGenerator';
import { 
  subscribeToRR, 
  startMockStream, 
  stopMockStream, 
  sendActuatorCommand,
  ActuatorCommand 
} from './bleService';

// Type alias for processed results from the engine
export type EngineResult = Awaited<ReturnType<NeuralStress_awarenessProcessor['processRRIntervals']>>;

type EngineListener = (result: EngineResult) => void;
type CueListener = (cue: HapticCue, metrics: EngineMetrics) => void;

/*******************************************************************************
 * ENGINE BRIDGE CLASS
 ******************************************************************************/

class EngineBridge {
  private processor = new NeuralStress_awarenessProcessor();
  private cueGenerator = new HapticCueGenerator();
  
  private rrBuffer: number[] = [];
  private unsubBle: (() => void) | null = null;
  private engineListeners = new Set<EngineListener>();
  private cueListeners = new Set<CueListener>();
  private processing = false;
  
  private readonly minSamples = VALIDATED_CONSTANTS.SAMPLING.MIN_SAMPLES;
  private readonly maxWindow = VALIDATED_CONSTANTS.SAMPLING.WINDOW_SECONDS * VALIDATED_CONSTANTS.SAMPLING.OPTIMAL_FS;
  
  // Intelligent cue configuration
  private autoSendCues = true;
  private cuesEnabled = true;
  private debugLogging = false;
  
  // Statistics
  private totalCuesGenerated = 0;
  private totalCuesSent = 0;
  private lastCue: HapticCue | null = null;

  /**
   * Start the engine bridge
   * @param options Configuration options
   */
  start({ useMock = false, enableCues = true, debugLogging = false } = {}) {
    if (this.unsubBle) return;
    
    this.cuesEnabled = enableCues;
    this.debugLogging = debugLogging;
    
    this.unsubBle = subscribeToRR((rrMs) => this.onRR(rrMs));
    if (useMock) startMockStream();
    
    this.log('Engine bridge started', { useMock, enableCues });
  }

  /**
   * Stop the engine bridge
   */
  stop() {
    if (this.unsubBle) {
      this.unsubBle();
      this.unsubBle = null;
    }
    stopMockStream();
    this.rrBuffer = [];
    this.log('Engine bridge stopped');
  }

  /**
   * Subscribe to engine results (HRV analysis)
   */
  subscribe(listener: EngineListener): () => void {
    this.engineListeners.add(listener);
    return () => this.engineListeners.delete(listener);
  }

  /**
   * Subscribe to generated haptic cues
   */
  onCue(listener: CueListener): () => void {
    this.cueListeners.add(listener);
    return () => this.cueListeners.delete(listener);
  }

  /**
   * Update cue generator preferences
   */
  updateCuePreferences(prefs: Partial<CuePreferences>): void {
    this.cueGenerator.updatePreferences(prefs);
    this.log('Cue preferences updated:', prefs);
  }

  /**
   * Get current cue preferences
   */
  getCuePreferences(): CuePreferences {
    return this.cueGenerator.getPreferences();
  }

  /**
   * Enable/disable automatic cue sending to ring
   */
  setAutoSendCues(enabled: boolean): void {
    this.autoSendCues = enabled;
    this.log('Auto-send cues:', enabled);
  }

  /**
   * Enable/disable cue generation entirely
   */
  setCuesEnabled(enabled: boolean): void {
    this.cuesEnabled = enabled;
    this.log('Cues enabled:', enabled);
  }

  /**
   * Get bridge statistics
   */
  getStats() {
    return {
      totalCuesGenerated: this.totalCuesGenerated,
      totalCuesSent: this.totalCuesSent,
      lastCue: this.lastCue,
      bufferSize: this.rrBuffer.length,
      isProcessing: this.processing,
    };
  }

  /**
   * Reset cue generator state
   */
  resetCueState(): void {
    this.cueGenerator.reset();
    this.log('Cue state reset');
  }

  /*******************************************************************************
   * INTERNAL PROCESSING
   ******************************************************************************/

  private async onRR(rrMs: number) {
    this.rrBuffer.push(rrMs);
    if (this.rrBuffer.length > this.maxWindow) {
      this.rrBuffer.shift();
    }

    if (this.processing) return;
    if (this.rrBuffer.length < this.minSamples) return;

    this.processing = true;
    try {
      // 1. Run HRV analysis
      const result = await this.processor.processRRIntervals([...this.rrBuffer]);
      this.emitEngineResult(result);
      
      // 2. Generate haptic cue if enabled
      if (this.cuesEnabled) {
        await this.processCue(result);
      }
    } catch (err) {
      console.warn('[EngineBridge] Processing error:', err);
    } finally {
      this.processing = false;
    }
  }

  /**
   * Convert engine result to metrics and generate cue
   */
  private async processCue(result: EngineResult): Promise<void> {
    // Convert engine result to cue generator format
    const metrics: EngineMetrics = {
      timestamp: result.timestamp,
      microVariability: result.microVariability,
      meanCoherence: result.meanCoherence,
      coherenceStability: result.coherenceStability,
      confidence: result.confidence,
      stress_awarenessLevel: result.stress_awarenessLevel,
      trend: result.trend,
      artifactRate: result.artifactRate,
      respiratoryDetection: result.respiratoryDetection ? {
        detected: result.respiratoryDetection.respiratoryFrequency !== null,
        frequencyHz: result.respiratoryDetection.respiratoryFrequency ?? 0,
        confidence: result.respiratoryDetection.confidence,
      } : undefined,
    };

    // Generate cue
    const cue = this.cueGenerator.generateCue(metrics);
    this.lastCue = cue;

    // Emit to listeners
    this.emitCue(cue, metrics);

    // Handle triggered cue
    if (cue.shouldTrigger) {
      this.totalCuesGenerated++;
      this.log(`Cue generated: ${cue.reason} (priority: ${cue.priority})`);

      // Send to ring if auto-send enabled
      if (this.autoSendCues && cue.command) {
        const success = await this.sendCueToRing(cue.command);
        if (success) {
          this.totalCuesSent++;
        }
      }
    }
  }

  /**
   * Send actuator command to ring
   */
  private async sendCueToRing(cmd: ActuatorCommand): Promise<boolean> {
    try {
      const success = await sendActuatorCommand(cmd);
      this.log(`Cue sent to ring:`, cmd, success ? '✓' : '✗');
      return success;
    } catch (err) {
      console.warn('[EngineBridge] Failed to send cue:', err);
      return false;
    }
  }

  private emitEngineResult(result: EngineResult) {
    this.engineListeners.forEach((cb) => {
      try {
        cb(result);
      } catch (err) {
        console.warn('[EngineBridge] Listener error:', err);
      }
    });
  }

  private emitCue(cue: HapticCue, metrics: EngineMetrics) {
    this.cueListeners.forEach((cb) => {
      try {
        cb(cue, metrics);
      } catch (err) {
        console.warn('[EngineBridge] Cue listener error:', err);
      }
    });
  }

  private log(...args: any[]): void {
    if (this.debugLogging) {
      console.log('[EngineBridge]', ...args);
    }
  }
}

/*******************************************************************************
 * EXPORTS
 ******************************************************************************/

export const engineBridge = new EngineBridge();

/**
 * Quick start for intelligent mode with haptic cues
 */
export function startIntelligentMode(options?: {
  cuePreferences?: Partial<CuePreferences>;
  onCue?: CueListener;
  onResult?: EngineListener;
  debug?: boolean;
}): () => void {
  if (options?.cuePreferences) {
    engineBridge.updateCuePreferences(options.cuePreferences);
  }
  
  if (options?.onCue) {
    engineBridge.onCue(options.onCue);
  }
  
  if (options?.onResult) {
    engineBridge.subscribe(options.onResult);
  }

  engineBridge.start({ 
    useMock: true, 
    enableCues: true, 
    debugLogging: options?.debug ?? false 
  });

  return () => engineBridge.stop();
}
