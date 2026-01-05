/**
 * User profile and personalization manager.
 * 
 * Stores baseline HRV, coherence thresholds, and cue preferences per user.
 * Enables adaptive patterns and personalized stress classification.
 * 
 * Features:
 * - Per-user baseline tracking (HR, RMSSD, coherence)
 * - Respiratory rate baseline for adaptive frequency bands
 * - Cue intensity preferences (thermal, vibration)
 * - Calibration state tracking
 * - JSON serialization for persistence
 * 
 * @example
 * ```typescript
 * const profileManager = new UserProfileManager();
 * 
 * // Set up new user
 * profileManager.setProfile({
 *   userId: 'user-123',
 *   demographic: { age: 32, sex: 'F', timezone: 'America/New_York' },
 *   baseline: profileManager.getBaseline('user-123'),
 *   preferences: profileManager.getPreferences('user-123'),
 *   updatedAt: new Date().toISOString(),
 * });
 * 
 * // Adjust thermal sensitivity
 * profileManager.setPreferences('user-123', { thermalIntensityScale: 0.7 });
 * ```
 */


// ============================================================================
// Types
// ============================================================================

export type UserBaseline = {
  /** Baseline resting heart rate (bpm) */
  restingHR: number;
  /** Baseline resting RMSSD (ms) */
  restingRMSSD: number;
  /** Typical minimum coherence (0-1) */
  coherenceFloor: number;
  /** Typical maximum coherence (0-1) */
  coherenceCeiling: number;
  /** Load score that triggers intervention (0-100) */
  stressThreshold: number;
  /** Personal respiratory rate baseline (breaths/min) */
  respiratoryRate: number;
  /** ISO timestamp when baseline was created */
  createdAt: string;
  /** ISO timestamp when baseline was last updated */
  updatedAt?: string;
  /** Number of baseline recordings collected */
  samplesCollected: number;
  /** Calibration status */
  calibrationStatus: 'uncalibrated' | 'partial' | 'complete';
};

export type CuePreferences = {
  /** Enable thermal cues */
  thermalEnabled: boolean;
  /** Multiply thermal intensity by this (0.5-2.0) */
  thermalIntensityScale: number;
  /** Enable vibration cues */
  vibrationEnabled: boolean;
  /** Multiply vibration intensity by this (0.5-2.0) */
  vibrationIntensityScale: number;
  /** Multiply cue duration by this (0.5-1.5) */
  cueDurationScale: number;
  /** Maximum cue events per day (rate limiting) */
  maxDailyActivations: number;
  /** Quiet hours start (0-23, null = disabled) */
  quietHoursStart: number | null;
  /** Quiet hours end (0-23, null = disabled) */
  quietHoursEnd: number | null;
};

export type UserPreferences = CuePreferences;

export type UserDemographic = {
  /** Age in years (18-100+) */
  age: number;
  /** Biological sex for baseline adjustments */
  sex: 'M' | 'F' | 'Other';
  /** User timezone (IANA format) */
  timezone: string;
  /** Activity level for adaptive band tuning */
  activityLevel?: 'sedentary' | 'moderate' | 'active' | 'athlete';
  /** Regular meditation practice affects respiratory patterns */
  meditationPractice?: boolean;
};

export type UserProfile = {
  /** Unique user identifier */
  userId: string;
  /** Demographic information */
  demographic: UserDemographic;
  /** HRV and coherence baselines */
  baseline: UserBaseline;
  /** Cue preferences and limits */
  preferences: CuePreferences;
  /** Last profile update timestamp */
  updatedAt: string;
  /** Profile version for migrations */
  version: number;
};

export type CalibrationResult = {
  status: 'success' | 'insufficient_data' | 'low_quality';
  message: string;
  samplesUsed: number;
  baseline?: Partial<UserBaseline>;
};

// ============================================================================
// Constants
// ============================================================================

const POPULATION_DEFAULTS: UserBaseline = {
  restingHR: 70,
  restingRMSSD: 35,
  coherenceFloor: 0.40,
  coherenceCeiling: 0.75,
  stressThreshold: 60,
  respiratoryRate: 15, // Average adult breathing rate
  createdAt: new Date().toISOString(),
  samplesCollected: 0,
  calibrationStatus: 'uncalibrated',
};

const DEFAULT_PREFERENCES: CuePreferences = {
  thermalEnabled: true,
  thermalIntensityScale: 1.0,
  vibrationEnabled: true,
  vibrationIntensityScale: 1.0,
  cueDurationScale: 1.0,
  maxDailyActivations: 100,
  quietHoursStart: null,
  quietHoursEnd: null,
};

const MAX_THERMAL_INTENSITY = 60;
const MAX_VIBRATION_INTENSITY = 50;

const PROFILE_VERSION = 2;

// Calibration requirements
const MIN_CALIBRATION_SAMPLES = 50;  // ~1 minute of data
const FULL_CALIBRATION_SAMPLES = 300; // ~5 minutes of data

// ============================================================================
// UserProfileManager Class
// ============================================================================

/**
 * Persistent user profile store with lazy baseline estimation.
 * In production, integrate with database; here uses in-memory + JSON serialization.
 */
export class UserProfileManager {
  private profiles: Map<string, UserProfile> = new Map();
  private dailyActivations: Map<string, { date: string; count: number }> = new Map();

  /**
   * Create or update user profile.
   */
  setProfile(profile: UserProfile): void {
    profile.updatedAt = new Date().toISOString();
    profile.version = PROFILE_VERSION;
    this.profiles.set(profile.userId, profile);
  }

  /**
   * Retrieve profile by user ID.
   */
  getProfile(userId: string): UserProfile | null {
    return this.profiles.get(userId) ?? null;
  }

  /**
   * Delete user profile.
   */
  deleteProfile(userId: string): boolean {
    return this.profiles.delete(userId);
  }

  /**
   * List all user IDs.
   */
  listUsers(): string[] {
    return Array.from(this.profiles.keys());
  }

  /**
   * Check if user profile exists.
   */
  hasProfile(userId: string): boolean {
    return this.profiles.has(userId);
  }

  // ========================================================================
  // Baseline Management
  // ========================================================================

  /**
   * Get user baseline or return population defaults if not established.
   */
  getBaseline(userId: string): UserBaseline {
    const profile = this.getProfile(userId);
    if (profile?.baseline) {
      return { ...profile.baseline };
    }

    // Adjust population defaults based on demographics
    const demographic = profile?.demographic;
    const defaults = { ...POPULATION_DEFAULTS };

    if (demographic) {
      // Age-based adjustments (RMSSD decreases with age)
      if (demographic.age > 50) {
        defaults.restingRMSSD = 28;
        defaults.coherenceFloor = 0.35;
      } else if (demographic.age < 30) {
        defaults.restingRMSSD = 42;
        defaults.coherenceFloor = 0.45;
      }

      // Activity level adjustments
      if (demographic.activityLevel === 'athlete') {
        defaults.restingHR = 55;
        defaults.restingRMSSD = 50;
        defaults.respiratoryRate = 12; // Athletes often have slower breathing
      }

      // Meditation practice
      if (demographic.meditationPractice) {
        defaults.respiratoryRate = 10; // Slower breathing baseline
        defaults.coherenceCeiling = 0.85;
      }
    }

    defaults.createdAt = new Date().toISOString();
    return defaults;
  }

  /**
   * Update baseline with new calibration data.
   */
  updateBaseline(userId: string, update: Partial<UserBaseline>): void {
    let profile = this.getProfile(userId);
    if (!profile) {
      profile = this.createDefaultProfile(userId);
    }
    
    profile.baseline = {
      ...profile.baseline,
      ...update,
      updatedAt: new Date().toISOString(),
    };
    
    // Update calibration status based on samples
    if (profile.baseline.samplesCollected >= FULL_CALIBRATION_SAMPLES) {
      profile.baseline.calibrationStatus = 'complete';
    } else if (profile.baseline.samplesCollected >= MIN_CALIBRATION_SAMPLES) {
      profile.baseline.calibrationStatus = 'partial';
    }
    
    this.setProfile(profile);
  }

  /**
   * Process calibration samples and update baseline.
   * Call this during user calibration flow.
   */
  processCalibrationSamples(
    userId: string,
    rrIntervals: number[],
    coherenceValues: number[]
  ): CalibrationResult {
    if (rrIntervals.length < MIN_CALIBRATION_SAMPLES) {
      return {
        status: 'insufficient_data',
        message: `Need at least ${MIN_CALIBRATION_SAMPLES} samples, got ${rrIntervals.length}`,
        samplesUsed: rrIntervals.length,
      };
    }

    // Calculate baseline metrics
    const meanRR = rrIntervals.reduce((a, b) => a + b, 0) / rrIntervals.length;
    const restingHR = 60000 / meanRR;

    // Calculate RMSSD
    let sumSquaredDiffs = 0;
    for (let i = 1; i < rrIntervals.length; i++) {
      const diff = rrIntervals[i] - rrIntervals[i - 1];
      sumSquaredDiffs += diff * diff;
    }
    const restingRMSSD = Math.sqrt(sumSquaredDiffs / (rrIntervals.length - 1));

    // Coherence floor/ceiling from calibration
    const sortedCoherence = [...coherenceValues].sort((a, b) => a - b);
    const coherenceFloor = sortedCoherence[Math.floor(sortedCoherence.length * 0.1)] ?? 0.4;
    const coherenceCeiling = sortedCoherence[Math.floor(sortedCoherence.length * 0.9)] ?? 0.75;

    const baseline: Partial<UserBaseline> = {
      restingHR: Math.round(restingHR),
      restingRMSSD: Math.round(restingRMSSD * 10) / 10,
      coherenceFloor: Math.round(coherenceFloor * 100) / 100,
      coherenceCeiling: Math.round(coherenceCeiling * 100) / 100,
      samplesCollected: rrIntervals.length,
    };

    this.updateBaseline(userId, baseline);

    return {
      status: 'success',
      message: `Baseline updated with ${rrIntervals.length} samples`,
      samplesUsed: rrIntervals.length,
      baseline,
    };
  }

  /**
   * Update respiratory rate baseline (detected during adaptive band processing).
   */
  updateRespiratoryBaseline(userId: string, detectedRate: number): void {
    const current = this.getBaseline(userId);
    
    // Exponential moving average for smooth updates
    const alpha = 0.1;
    const newRate = current.respiratoryRate * (1 - alpha) + detectedRate * alpha;
    
    this.updateBaseline(userId, {
      respiratoryRate: Math.round(newRate * 10) / 10,
    });
  }

  // ========================================================================
  // Preferences Management
  // ========================================================================

  /**
   * Get user's cue preferences or return defaults.
   */
  getPreferences(userId: string): CuePreferences {
    const profile = this.getProfile(userId);
    return profile?.preferences ?? { ...DEFAULT_PREFERENCES };
  }

  /**
   * Update cue preferences (e.g., reduce thermal intensity for sensitive users).
   */
  setPreferences(userId: string, prefs: Partial<CuePreferences>): void {
    let profile = this.getProfile(userId);
    if (!profile) {
      profile = this.createDefaultProfile(userId);
    }
    
    // Validate and clamp preference values
    if (prefs.thermalIntensityScale !== undefined) {
      prefs.thermalIntensityScale = Math.max(0.5, Math.min(2.0, prefs.thermalIntensityScale));
    }
    if (prefs.vibrationIntensityScale !== undefined) {
      prefs.vibrationIntensityScale = Math.max(0.5, Math.min(2.0, prefs.vibrationIntensityScale));
    }
    if (prefs.cueDurationScale !== undefined) {
      prefs.cueDurationScale = Math.max(0.5, Math.min(1.5, prefs.cueDurationScale));
    }
    
    profile.preferences = { ...profile.preferences, ...prefs };
    this.setProfile(profile);
  }

  /**
   * Scale prescription intensity by user preference.
   */
  scalePrescription(
    userId: string,
    baseIntensity: number,
    cueType: 'thermal' | 'vibration'
  ): number {
    const prefs = this.getPreferences(userId);
    const scale = cueType === 'thermal' 
      ? prefs.thermalIntensityScale 
      : prefs.vibrationIntensityScale;

    // Apply scale with safety bounds
    const maxIntensity = cueType === 'thermal' 
      ? MAX_THERMAL_INTENSITY 
      : MAX_VIBRATION_INTENSITY;

    return Math.min(maxIntensity, Math.max(5, baseIntensity * scale));
  }

  /**
   * Scale prescription duration by user preference.
   */
  scaleDuration(userId: string, baseDurationMs: number): number {
    const prefs = this.getPreferences(userId);
    return Math.round(baseDurationMs * prefs.cueDurationScale);
  }

  // ========================================================================
  // Rate Limiting
  // ========================================================================

  /**
   * Check if user is in quiet hours.
   */
  isInQuietHours(userId: string): boolean {
    const prefs = this.getPreferences(userId);
    if (prefs.quietHoursStart === null || prefs.quietHoursEnd === null) {
      return false;
    }

    const profile = this.getProfile(userId);
    const tz = profile?.demographic.timezone ?? 'UTC';
    
    // Get current hour in user's timezone
    const now = new Date();
    const formatter = new Intl.DateTimeFormat('en-US', {
      hour: 'numeric',
      hour12: false,
      timeZone: tz,
    });
    const currentHour = parseInt(formatter.format(now), 10);

    // Handle overnight quiet hours (e.g., 22:00 - 07:00)
    if (prefs.quietHoursStart > prefs.quietHoursEnd) {
      return currentHour >= prefs.quietHoursStart || currentHour < prefs.quietHoursEnd;
    }
    
    return currentHour >= prefs.quietHoursStart && currentHour < prefs.quietHoursEnd;
  }

  /**
   * Check if user can receive cues (rate limiting + quiet hours).
   */
  canActivateCue(userId: string): { allowed: boolean; reason?: string } {
    // Check quiet hours
    if (this.isInQuietHours(userId)) {
      return { allowed: false, reason: 'quiet_hours' };
    }

    // Check daily activation limit
    const prefs = this.getPreferences(userId);
    const today = new Date().toISOString().slice(0, 10);
    const record = this.dailyActivations.get(userId);

    if (record && record.date === today && record.count >= prefs.maxDailyActivations) {
      return { allowed: false, reason: 'daily_limit_reached' };
    }

    return { allowed: true };
  }

  /**
   * Record a cue activation for rate limiting.
   */
  recordActivation(userId: string): void {
    const today = new Date().toISOString().slice(0, 10);
    const record = this.dailyActivations.get(userId);

    if (record && record.date === today) {
      record.count++;
    } else {
      this.dailyActivations.set(userId, { date: today, count: 1 });
    }
  }

  /**
   * Get remaining activations for today.
   */
  getRemainingActivations(userId: string): number {
    const prefs = this.getPreferences(userId);
    const today = new Date().toISOString().slice(0, 10);
    const record = this.dailyActivations.get(userId);

    if (!record || record.date !== today) {
      return prefs.maxDailyActivations;
    }

    return Math.max(0, prefs.maxDailyActivations - record.count);
  }

  // ========================================================================
  // Profile Creation & Serialization
  // ========================================================================

  /**
   * Create fresh profile with defaults.
   */
  createDefaultProfile(userId: string, demographic?: Partial<UserDemographic>): UserProfile {
    const profile: UserProfile = {
      userId,
      demographic: {
        age: demographic?.age ?? 35,
        sex: demographic?.sex ?? 'Other',
        timezone: demographic?.timezone ?? 'UTC',
        activityLevel: demographic?.activityLevel,
        meditationPractice: demographic?.meditationPractice,
      },
      baseline: this.getBaseline(userId),
      preferences: { ...DEFAULT_PREFERENCES },
      updatedAt: new Date().toISOString(),
      version: PROFILE_VERSION,
    };
    
    this.setProfile(profile);
    return profile;
  }

  /**
   * Serialize all profiles for persistence.
   */
  toJSON(): { profiles: Record<string, UserProfile>; version: number } {
    return {
      profiles: Object.fromEntries(this.profiles),
      version: PROFILE_VERSION,
    };
  }

  /**
   * Deserialize profiles from storage.
   */
  fromJSON(data: { profiles: Record<string, UserProfile>; version?: number }): void {
    this.profiles.clear();
    
    for (const [userId, profile] of Object.entries(data.profiles)) {
      // Migrate old profiles if needed
      const migrated = this.migrateProfile(profile, data.version ?? 1);
      this.profiles.set(userId, migrated);
    }
  }

  /**
   * Migrate profile from older version if needed.
   */
  private migrateProfile(profile: UserProfile, fromVersion: number): UserProfile {
    if (fromVersion >= PROFILE_VERSION) {
      return profile;
    }

    // Version 1 → 2: Add respiratory rate, quiet hours, activity level
    if (fromVersion < 2) {
      profile.baseline.respiratoryRate = profile.baseline.respiratoryRate ?? 15;
      profile.baseline.coherenceCeiling = profile.baseline.coherenceCeiling ?? 0.75;
      profile.baseline.calibrationStatus = profile.baseline.calibrationStatus ?? 'uncalibrated';
      profile.preferences.quietHoursStart = profile.preferences.quietHoursStart ?? null;
      profile.preferences.quietHoursEnd = profile.preferences.quietHoursEnd ?? null;
    }

    profile.version = PROFILE_VERSION;
    return profile;
  }

  /**
   * Clear all profiles (for testing).
   */
  clear(): void {
    this.profiles.clear();
    this.dailyActivations.clear();
  }
}
