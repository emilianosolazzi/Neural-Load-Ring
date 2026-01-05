/**
 * Processors Module
 * 
 * Real-time stream processing, wellness orchestration, and user profile management.
 * 
 * @module processors
 * 
 * Components:
 * - StreamProcessor: Buffers RR intervals from BLE, handles artifacts, signals readiness
 * - WellnessProcessor: Orchestrates full pipeline (stream → HRV → stress → prescriptions)
 * - UserProfileManager: Stores baselines, preferences, and personalization data
 * 
 * @example
 * ```typescript
 * import { WellnessProcessor, UserProfileManager, StreamProcessor } from './processors';
 * 
 * // Set up processing pipeline
 * const profileManager = new UserProfileManager();
 * const processor = new WellnessProcessor({ userProfileManager: profileManager });
 * 
 * // Create user profile
 * profileManager.createDefaultProfile('user-123', {
 *   age: 28,
 *   sex: 'F',
 *   activityLevel: 'active',
 * });
 * 
 * processor.setUser('user-123');
 * 
 * // Process incoming RR intervals
 * bleService.onRRInterval(async (rr) => {
 *   if (processor.pushRR(rr)) {
 *     const snapshot = await processor.process();
 *     if (snapshot) {
 *       updateUI(snapshot);
 *     }
 *   }
 * });
 * ```
 */

// ============================================================================
// Stream Processing
// ============================================================================

export {
  StreamProcessor,
  type StreamConfig,
  type StreamState,
  type StreamSnapshot,
  type StreamEvent,
  type StreamEventHandler,
} from './StreamProcessor';

// ============================================================================
// Wellness Processing
// ============================================================================

export {
  WellnessProcessor,
  type WellnessSnapshot,
  type WellnessConfig,
  type WellnessEventHandler,
  type StressLevel,
  type AdaptiveBandInfo,
} from './WellnessProcessor';

// ============================================================================
// User Profile Management
// ============================================================================

export {
  UserProfileManager,
  type UserProfile,
  type UserBaseline,
  type UserDemographic,
  type UserPreferences,
  type CuePreferences,
  type CalibrationResult,
} from './UserProfileManager';
