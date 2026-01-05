# Neural Load Ring: Physical Action Pathways

## Sensor Layer
- **PPG (photoplethysmography):** Infrared/green LEDs and photodiodes sense blood volume pulse. The optical path length changes with vasodilation/vasoconstriction driven by autonomic tone.
- **Skin temperature:** NTC/thermistor on the inner band tracks distal temperature shifts caused by sympathetic vs parasympathetic dominance (peripheral vasoconstriction vs dilation).
- **Accelerometer/IMU:** Captures motion to reject artifacts and infer posture/activity; motion-induced optical noise is filtered in the signal-quality stage.
- **EDA (if present):** Measures skin conductance from sweat gland activation, a direct sympathetic marker.

## Signal Conditioning
- **Optics:** LED current is modulated; photodiode current is amplified and filtered (analog front end). Ambient light cancellation uses differential sampling.
- **Sampling:** PPG is digitized; RR intervals are derived from pulse peaks. Artifacts (motion, ectopy, saturation) are clipped or dropped before HRV calculations.
- **Thermal sensing:** Slow-sampling (~1 Hz) averages skin temperature; sudden drops often indicate sympathetic vasoconstriction.

## Physiological Metrics
- **RR intervals & HRV:** Beat-to-beat timing drives time/frequency domain HRV. High HF power and RMSSD reflect parasympathetic activity; elevated LF/HF and reduced variability indicate sympathetic load.
- **Micro-variability & coherence:** Short-window variability and respiratory-band phase coherence describe vagal tone and cardiorespiratory coupling.
- **Signal quality:** Artifact rate and stability scores down-weight noisy segments to avoid false feedback.

## Actuation Pathways
- **Thermal cues (warming/cooling):** Ring heaters or Peltier elements modulate cutaneous thermoreceptors. Gentle warming encourages vasodilation and parasympathetic bias; light cooling can downshift over-excitation. Intensity and duration are capped for skin safety.
- **Vibrotactile cues:** Eccentric or linear resonant actuators deliver low-frequency patterns. Slow ramps and rhythmic pulses are aimed at entraining slower breathing and reducing sympathetic drive.

## Closed-Loop Effects on the Body
- **Autonomic modulation:** HRV-derived stress/load scores trigger cues that encourage slower respiration and vasodilation, nudging the balance toward parasympathetic dominance.
- **Peripheral vascular response:** Thermal cues increase local blood flow; reduced sympathetic vasoconstriction supports higher HF variability and lower perceived stress.
- **Respiratory coupling:** Tactile pacing encourages longer exhales; this increases vagal afferent activity and lowers heart rate and LF/HF ratio.
- **Feedback suppression of artifacts:** If signal quality drops (motion, ectopy), the system pauses cues to avoid mis-driving responses.

## Safety and Dose Controls
- **Temperature limits:** Firmware caps surface temperature and duty cycle to prevent burns; skin-contact sensors can cut off heating when contact is lost.
- **Vibration limits:** Amplitude and frequency stay below discomfort thresholds; off-ramps are smoothed to avoid startle responses.
- **Rest intervals:** Actuation includes refractory windows to prevent sensory fatigue and habituation.

## Expected Physical Outcomes
- Increased distal skin temperature and blood flow during calming cues.
- Reduced heart rate and higher beat-to-beat variability after paced breathing and warming sequences.
- Lower sweat gland activity (EDA) and reduced vasoconstriction after sustained parasympathetic shift.
- More stable PPG waveforms and fewer motion artifacts when cues are not delivered during movement.

## Everyday Explanation
- The ring listens to your pulse and skin signals to sense when your nervous system is tense or relaxed.
- When you are wound up, it can gently warm or buzz to nudge slower breathing and better blood flow, which helps your body shift out of “fight or flight.”
- When you are already calm, it stays quiet, simply tracking and avoiding false alerts if you are moving.
- Safety limits keep heat and vibration in a comfortable range, with breaks so your skin and nerves do not get overstimulated.
- The goal is to make you feel steadier day to day: warmer fingers, steadier pulse, easier breathing, and fewer stress spikes.
