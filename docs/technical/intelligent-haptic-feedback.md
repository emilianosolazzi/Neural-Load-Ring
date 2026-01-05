# Intelligent Haptic Feedback System

> **The Intelligence Layer** — Connecting HRV analysis to physical feedback

This document describes how the Neural Load Ring converts autonomic nervous system metrics into subtle, evidence-based haptic cues that help users regulate their stress response.

---

## Overview

The ring doesn't just measure—it responds. When the HRV engine detects changes in your autonomic state, the haptic feedback system decides *if*, *when*, and *how* to intervene.

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   PPG/ECG   │───▶│  Wellness   │───▶│    Cue      │───▶│  Actuators  │
│   Sensor    │    │   Engine    │    │  Generator  │    │  (Thermal/  │
│             │    │  (HRV)      │    │             │    │  Vibration) │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                          │                  │
                          │    Metrics       │    Commands
                          ▼                  ▼
                   ┌─────────────────────────────────┐
                   │  • Micro-variability            │
                   │  • Phase coherence              │
                   │  • Coherence stability          │
                   │  • Confidence score             │
                   │  • Stress level                 │
                   │  • Trend (improving/declining)  │
                   └─────────────────────────────────┘
```

---

## Design Philosophy

### The Ring Whispers, Doesn't Shout

- **Subtle interventions** — Gentle warmth or soft pulses, not jarring alerts
- **Evidence-based** — Thresholds derived from HRV research literature
- **Respects autonomy** — Quiet hours, intensity caps, user preferences
- **Prevents habituation** — Adaptive cooldowns between cues
- **Confidence-gated** — No action on unreliable data

---

## HRV-to-Haptic Mapping

| Autonomic Signal | Haptic Response | Rationale |
|------------------|-----------------|-----------|
| **High micro-variability** | Vibration pulse | Neural chaos → grounding through tactile anchor |
| **Low coherence** | Thermal warmth | Dysregulation → comfort through warmth |
| **Unstable coherence** | Breathing pattern | Erratic rhythm → guided breathing cue |
| **Critical state** | Combined alert | Immediate attention needed |

### Threshold Values

Based on published research (Pincus 1991, McCraty & Shaffer 2015, Goldberger 2002):

```typescript
MICROVAR_THRESHOLDS = {
  HEALTHY: 0.02,      // < 2% = no action
  ELEVATED: 0.05,     // 2-5% = gentle vibration
  HIGH: 0.08,         // 5-8% = stronger pulse
  CRITICAL: 0.12,     // > 12% = alert pattern
}

COHERENCE_THRESHOLDS = {
  HIGH: 0.75,         // > 75% = optimal, no intervention
  MEDIUM: 0.50,       // 50-75% = light thermal
  LOW: 0.30,          // 30-50% = stronger warmth
  CRITICAL: 0.15,     // < 15% = combined intervention
}

STABILITY_THRESHOLDS = {
  STABLE: 0.70,       // > 70% = no guidance needed
  UNSTABLE: 0.40,     // < 40% = breathing cue
  CHAOTIC: 0.20,      // < 20% = stronger guidance
}
```

---

## Decision Cascade

The cue generator evaluates conditions in priority order:

```
1. MASTER SWITCH
   └─ Cues disabled? → No action

2. QUIET HOURS
   └─ In quiet period? → No action

3. RATE LIMIT
   └─ >12 cues this hour? → No action

4. CONFIDENCE GATE
   └─ Confidence < 60%? → Track streak
      └─ 3+ consecutive? → Check-fit nudge
      └─ Otherwise → No action

5. ARTIFACT GATE
   └─ Artifact rate > 25%? → No action

6. ALERT CHECK
   └─ Critical state OR micro-var > 12%? → Alert cue

7. COMBINED CHECK
   └─ Coherence < 30% AND micro-var > 5%? → Combined cue

8. BREATHING CHECK
   └─ Stability < 40%? → Breathing pattern

9. VIBRATION CHECK
   └─ Micro-var > 5%? → Grounding pulse

10. THERMAL CHECK
    └─ Coherence < 50%? → Comfort warmth

11. TREND CHECK
    └─ Deteriorating trend? → Preventive warmth

12. DEFAULT
    └─ Optimal state → No action
```

---

## Cooldown Periods

To prevent habituation and user fatigue:

| Cue Type | Cooldown | Rationale |
|----------|----------|-----------|
| Vibration | 30 seconds | Quick recovery, can repeat |
| Thermal | 2 minutes | Heater needs cooldown, slower response |
| Combined | 3 minutes | Comprehensive intervention needs time |
| Breathing | 5 minutes | Full breathing cycle takes time |
| After Alert | 10 minutes | User needs recovery time |

---

## Sensitivity Profiles

Users can choose their preferred feedback intensity:

### Subtle Mode
- Thermal: 25-50% intensity
- Vibration: 15-40% intensity
- Duration: 70% of normal

### Normal Mode (Default)
- Thermal: 35-70% intensity
- Vibration: 30-60% intensity
- Duration: 100% of normal

### Assertive Mode
- Thermal: 45-85% intensity
- Vibration: 45-80% intensity
- Duration: 130% of normal

---

## Vibration Patterns

| Pattern | When Used | Description |
|---------|-----------|-------------|
| `SINGLE` | Elevated micro-variability | One short pulse |
| `DOUBLE` | High micro-variability | Two quick pulses |
| `TRIPLE` | Check ring fit | Three gentle taps |
| `HEARTBEAT` | Combined cue | Calming lub-dub rhythm |
| `BREATHING` | Stability guidance | Slow wave pattern |
| `ALERT` | Critical state | Attention-getting sequence |

---

## Implementation Files

### Phone-Side (TypeScript)

**`hapticCueGenerator.ts`** — Main decision engine
- `HapticCueGenerator` class with full state management
- `generateCue(metrics)` → returns `HapticCue`
- `generateSignatureCue(metrics)` → returns cue with signature pattern
- Preference management and reset functionality

**`signatureFeel.ts`** — Signature haptic vocabulary
- `SignaturePattern` enum with all pattern types
- `PATTERN_METADATA` with emotional intents and names
- `mapCueToPattern()` — maps HRV cue types to signature patterns
- `createSignatureCommand()` — generates BLE command bytes
- Easing curves and timing constants

**`engineBridge.ts`** — Orchestration layer
- Connects BLE → Engine → Cue Generator → BLE
- Automatic actuator command dispatch
- Debug logging for development

### Firmware-Side (C)

**`cue_processor.h/c`** — Autonomous mode
- Identical logic for when phone is disconnected
- RAM-efficient (8-sample rolling history)
- Battery-conscious triggering

**`signature_feel.h/c`** — Signature haptic personality
- Organic easing curves (sine-based, never linear)
- Pattern definitions with timing and intensity
- `signature_play()` / `signature_tick()` state machine
- Safety limits and graceful fade-out

**`cue_to_signature.h/c`** — Bridge layer
- Maps cue processor output to signature patterns
- Intensity scaling with perceptual correction

---

## Signature Feel: The Ring's Personality

> *"A calm, steady presence that helps you return to yourself."*

The Signature Feel system ensures every haptic interaction has a consistent emotional quality—like the same thoughtful friend reaching out each time.

### Emotional Vocabulary

| Modality | Role | Emotional Tone |
|----------|------|----------------|
| **Thermal** | Comfort, safety, grounding | "I've got you" |
| **Vibration** | Attention, awareness, guidance | "Notice this" |
| **Combined** | Comprehensive support | "Let's start fresh" |

### Signature Patterns

Each pattern has a name, a "verbal cue" (what the ring is saying), and specific contexts:

| Pattern | Verbal Cue | Used For |
|---------|------------|----------|
| **Grounding Pulse** | "Come back to your body" | Elevated micro-variability |
| **Attention Tap** | "Notice this" | Moderate stress signals |
| **Presence Check** | "I'm still here—are you?" | Low confidence streak |
| **Heartbeat** | "Let me steady you" | High stress with grounding |
| **Breathing Guide** | "Let's breathe together" | Unstable coherence |
| **Warm Exhale** | "You're safe. Slow down." | Low coherence |
| **Grounding Warmth** | "I've got you" | Preventive care |
| **Safety Embrace** | "Everything is okay" | Critical coherence dip |
| **Gentle Alert** | "Pause. Something changed." | Combined intervention |
| **Full Reset** | "Let's start fresh together" | Critical states |

### Timing Philosophy

All transitions follow organic easing—never linear, never jarring:

```
┌────────────────────────────────────────────────────────┐
│  Intensity                                              │
│     ▲                                                   │
│  60%│        ╭───────╮                                 │
│     │      ╱           ╲                               │
│  40%│    ╱               ╲                             │
│     │  ╱                   ╲                           │
│  20%│╱                       ╲                         │
│     │                          ╲                       │
│   0%└────────────────────────────╲──────▶ Time         │
│     │ Ramp Up │  Hold  │   Fade Out   │               │
│     │  400ms  │  150ms │    600ms     │               │
└────────────────────────────────────────────────────────┘
```

- **Ramp up**: `ease-in-sine` — gentle approach
- **Hold**: brief plateau at peak
- **Fade out**: `ease-out-sine` — natural decay

### Intensity Limits

Safety and comfort boundaries:

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Max vibration | 65% | Never jarring |
| Max thermal | 70% | Safety + comfort |
| Gentle vibration | 35% | Subtle presence |
| Gentle thermal | 45% | Warm hug |
| Min perceptible | 15% | Below is unnoticed |

### Cue → Pattern Mapping

The decision engine's raw cues map to signature patterns by type and priority:

```
┌─────────────────────────────────────────────────────────┐
│  Priority   │  Vibration      │  Thermal        │ Combined      │
├─────────────┼─────────────────┼─────────────────┼───────────────┤
│  ALERT      │  Full Reset     │  Safety Embrace │ Full Reset    │
│  HIGH       │  Heartbeat      │  Safety Embrace │ Heartbeat     │
│  NORMAL     │  Attention Tap  │  Warm Exhale    │ Gentle Alert  │
│  LOW        │  Grounding Pulse│  Grounding Warmth│ Grounding Pulse│
└─────────────────────────────────────────────────────────┘
```

---

## User Preferences

```typescript
interface CuePreferences {
  enabled: boolean;              // Master switch
  maxThermalIntensity: number;   // 0-100 cap
  maxVibrationIntensity: number; // 0-100 cap
  quietHoursStart: number;       // 0-23
  quietHoursEnd: number;         // 0-23
  sensitivityMode: 'subtle' | 'normal' | 'assertive';
  breathingGuidanceEnabled: boolean;
  thermalEnabled: boolean;
  vibrationEnabled: boolean;
}
```

---

## Integration Example

```typescript
import { engineBridge, startIntelligentMode } from './services/engineBridge';

// Quick start with default settings
const stop = startIntelligentMode({
  cuePreferences: {
    sensitivityMode: 'subtle',
    quietHoursStart: 22,
    quietHoursEnd: 7,
  },
  onCue: (cue, metrics) => {
    console.log(`Cue: ${cue.reason}`);
    // Update UI, log analytics, etc.
  },
  debug: true,
});

// Later: stop the bridge
stop();
```

---

## Test Coverage

The haptic cue system has comprehensive test coverage:

- ✅ Confidence gating (low confidence suppression)
- ✅ Check-fit cue after confidence streak
- ✅ Micro-variability → vibration mapping
- ✅ Coherence → thermal mapping
- ✅ Stability → breathing guidance
- ✅ Combined cue triggering
- ✅ Cooldown enforcement
- ✅ Quiet hours handling
- ✅ Rate limiting
- ✅ Sensitivity profiles
- ✅ Preference updates
- ✅ State reset

**62 tests passing** (27 for haptic cues + 35 for wellness engine)

---

## Safety Considerations

1. **Thermal Safety Chain**
   - Max 42°C skin temperature
   - 80% duty cycle cap
   - 60-second max duration
   - 30-second cooldown between sessions
   - Thermal runaway detection (2°C/s)

2. **Vibration Safety**
   - Max 100% intensity cap (user-configurable lower)
   - Pattern-based limiting prevents continuous vibration

3. **Data Quality**
   - No action on low-confidence readings
   - High artifact rates suppress cues
   - Check-fit nudge prompts user to adjust ring

---

## References

- Pincus, S. M. (1991). Approximate entropy as a measure of system complexity
- McCraty, R., & Shaffer, F. (2015). Heart rate variability: New perspectives
- Goldberger, A. L., et al. (2002). Fractal dynamics in physiology
- Task Force of ESC and NASPE (1996). Heart rate variability standards

---

*Last updated: January 2026*
