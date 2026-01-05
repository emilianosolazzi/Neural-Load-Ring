/**
 * @file RelaxationFeatures.tsx
 * @brief Breathing guide and manual haptic controls
 */

import React, { useState, useEffect, useRef } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, Animated } from 'react-native';
import { sendActuatorCommand, VibrationPattern } from '../services/bleService';

interface RelaxationFeaturesProps {
  onManualCue?: (type: string) => void;
}

export const RelaxationFeatures: React.FC<RelaxationFeaturesProps> = ({ onManualCue }) => {
  const [breathingActive, setBreathingActive] = useState(false);
  const [breathPhase, setBreathPhase] = useState<'inhale' | 'hold' | 'exhale'>('inhale');
  const breathAnim = useRef(new Animated.Value(0.3)).current;
  const breathInterval = useRef<NodeJS.Timeout | null>(null);

  // 4-7-8 breathing pattern
  const INHALE_MS = 4000;
  const HOLD_MS = 7000;
  const EXHALE_MS = 8000;

  useEffect(() => {
    if (breathingActive) {
      startBreathingCycle();
    } else {
      stopBreathingCycle();
    }
    return () => stopBreathingCycle();
  }, [breathingActive]);

  const startBreathingCycle = () => {
    const runCycle = () => {
      // Inhale
      setBreathPhase('inhale');
      Animated.timing(breathAnim, {
        toValue: 1,
        duration: INHALE_MS,
        useNativeDriver: true,
      }).start();

      setTimeout(() => {
        // Hold
        setBreathPhase('hold');
      }, INHALE_MS);

      setTimeout(() => {
        // Exhale
        setBreathPhase('exhale');
        Animated.timing(breathAnim, {
          toValue: 0.3,
          duration: EXHALE_MS,
          useNativeDriver: true,
        }).start();
      }, INHALE_MS + HOLD_MS);
    };

    runCycle();
    breathInterval.current = setInterval(runCycle, INHALE_MS + HOLD_MS + EXHALE_MS);
  };

  const stopBreathingCycle = () => {
    if (breathInterval.current) {
      clearInterval(breathInterval.current);
      breathInterval.current = null;
    }
    breathAnim.setValue(0.3);
  };

  const sendHapticCue = async (type: 'calm' | 'alert' | 'grounding') => {
    let pattern: VibrationPattern;
    let intensity: number;
    let thermal: number;

    switch (type) {
      case 'calm':
        pattern = VibrationPattern.BREATHING;
        intensity = 40;
        thermal = 60;
        break;
      case 'alert':
        pattern = VibrationPattern.ALERT;
        intensity = 80;
        thermal = 0;
        break;
      case 'grounding':
        pattern = VibrationPattern.HEARTBEAT;
        intensity = 50;
        thermal = 40;
        break;
    }

    await sendActuatorCommand({
      vibrationPattern: pattern,
      vibrationIntensity: intensity,
      thermalIntensity: thermal,
      thermalDurationS: 10,
    });

    onManualCue?.(type);
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Relaxation Tools</Text>

      {/* Breathing Guide */}
      <View style={styles.breathingSection}>
        <Text style={styles.sectionTitle}>Breathing Guide (4-7-8)</Text>
        
        <TouchableOpacity
          style={[styles.breathCircleContainer]}
          onPress={() => setBreathingActive(!breathingActive)}
        >
          <Animated.View 
            style={[
              styles.breathCircle,
              { transform: [{ scale: breathAnim }] }
            ]}
          >
            <Text style={styles.breathText}>
              {breathingActive ? breathPhase.toUpperCase() : 'TAP TO START'}
            </Text>
          </Animated.View>
        </TouchableOpacity>
        
        {breathingActive && (
          <Text style={styles.breathInstruction}>
            {breathPhase === 'inhale' && 'Breathe in slowly...'}
            {breathPhase === 'hold' && 'Hold your breath...'}
            {breathPhase === 'exhale' && 'Exhale slowly...'}
          </Text>
        )}
      </View>

      {/* Manual Haptic Cues */}
      <View style={styles.hapticSection}>
        <Text style={styles.sectionTitle}>Quick Haptic Cues</Text>
        
        <View style={styles.buttonRow}>
          <TouchableOpacity
            style={[styles.hapticButton, styles.calmButton]}
            onPress={() => sendHapticCue('calm')}
          >
            <Text style={styles.buttonEmoji}>🌊</Text>
            <Text style={styles.buttonText}>Calm</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.hapticButton, styles.groundingButton]}
            onPress={() => sendHapticCue('grounding')}
          >
            <Text style={styles.buttonEmoji}>💓</Text>
            <Text style={styles.buttonText}>Ground</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.hapticButton, styles.alertButton]}
            onPress={() => sendHapticCue('alert')}
          >
            <Text style={styles.buttonEmoji}>⚡</Text>
            <Text style={styles.buttonText}>Alert</Text>
          </TouchableOpacity>
        </View>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    padding: 16,
    backgroundColor: '#fff',
    borderRadius: 12,
    margin: 8,
  },
  title: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#1e293b',
    marginBottom: 16,
  },
  sectionTitle: {
    fontSize: 14,
    fontWeight: '600',
    color: '#64748b',
    marginBottom: 12,
  },
  breathingSection: {
    alignItems: 'center',
    marginBottom: 24,
  },
  breathCircleContainer: {
    width: 160,
    height: 160,
    justifyContent: 'center',
    alignItems: 'center',
  },
  breathCircle: {
    width: 140,
    height: 140,
    borderRadius: 70,
    backgroundColor: '#667eea',
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#667eea',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 8,
    elevation: 5,
  },
  breathText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 14,
    textAlign: 'center',
  },
  breathInstruction: {
    marginTop: 12,
    fontSize: 16,
    color: '#667eea',
    fontStyle: 'italic',
  },
  hapticSection: {
    marginTop: 8,
  },
  buttonRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  hapticButton: {
    flex: 1,
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
    marginHorizontal: 4,
  },
  calmButton: {
    backgroundColor: '#dbeafe',
  },
  groundingButton: {
    backgroundColor: '#fef3c7',
  },
  alertButton: {
    backgroundColor: '#fee2e2',
  },
  buttonEmoji: {
    fontSize: 24,
    marginBottom: 4,
  },
  buttonText: {
    fontSize: 12,
    fontWeight: '600',
    color: '#374151',
  },
});