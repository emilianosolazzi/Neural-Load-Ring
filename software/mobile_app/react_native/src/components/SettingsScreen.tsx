/**
 * @file SettingsScreen.tsx
 * @brief User preferences and device configuration
 */

import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, Switch, ScrollView, TouchableOpacity } from 'react-native';
import { engineBridge } from '../services/engineBridge';
import { readConfig, writeConfig, RingConfig } from '../services/bleService';

export const SettingsScreen: React.FC = () => {
  const [preferences, setPreferences] = useState(engineBridge.getCuePreferences());
  const [ringConfig, setRingConfig] = useState<RingConfig | null>(null);

  useEffect(() => {
    loadRingConfig();
  }, []);

  const loadRingConfig = async () => {
    const config = await readConfig();
    if (config) setRingConfig(config);
  };

  const updatePreference = <K extends keyof typeof preferences>(
    key: K,
    value: typeof preferences[K]
  ) => {
    const updated = { ...preferences, [key]: value };
    setPreferences(updated);
    engineBridge.updateCuePreferences({ [key]: value });
  };

  const SectionHeader: React.FC<{ title: string }> = ({ title }) => (
    <Text style={styles.sectionHeader}>{title}</Text>
  );

  const SettingRow: React.FC<{
    label: string;
    description?: string;
    children: React.ReactNode;
  }> = ({ label, description, children }) => (
    <View style={styles.settingRow}>
      <View style={styles.settingInfo}>
        <Text style={styles.settingLabel}>{label}</Text>
        {description && <Text style={styles.settingDescription}>{description}</Text>}
      </View>
      {children}
    </View>
  );

  return (
    <ScrollView style={styles.container}>
      <SectionHeader title="Haptic Feedback" />
      
      <SettingRow label="Enable Intelligent Cues" description="Automatic haptic feedback based on HRV">
        <Switch
          value={preferences.enabled}
          onValueChange={(v) => updatePreference('enabled', v)}
          trackColor={{ true: '#667eea' }}
        />
      </SettingRow>

      <SettingRow label="Vibration" description="Motor feedback for attention cues">
        <Switch
          value={preferences.vibrationEnabled}
          onValueChange={(v) => updatePreference('vibrationEnabled', v)}
          trackColor={{ true: '#667eea' }}
        />
      </SettingRow>

      <SettingRow label="Thermal" description="Warming comfort for relaxation">
        <Switch
          value={preferences.thermalEnabled}
          onValueChange={(v) => updatePreference('thermalEnabled', v)}
          trackColor={{ true: '#667eea' }}
        />
      </SettingRow>

      <SettingRow label="Breathing Guidance" description="Haptic breathing patterns">
        <Switch
          value={preferences.breathingGuidanceEnabled}
          onValueChange={(v) => updatePreference('breathingGuidanceEnabled', v)}
          trackColor={{ true: '#667eea' }}
        />
      </SettingRow>

      <SectionHeader title="Sensitivity" />
      
      <View style={styles.sensitivityRow}>
        {(['subtle', 'normal', 'assertive'] as const).map((mode) => (
          <TouchableOpacity
            key={mode}
            style={[
              styles.sensitivityButton,
              preferences.sensitivityMode === mode && styles.sensitivityButtonActive,
            ]}
            onPress={() => updatePreference('sensitivityMode', mode)}
          >
            <Text
              style={[
                styles.sensitivityText,
                preferences.sensitivityMode === mode && styles.sensitivityTextActive,
              ]}
            >
              {mode.charAt(0).toUpperCase() + mode.slice(1)}
            </Text>
          </TouchableOpacity>
        ))}
      </View>

      <SectionHeader title="Intensity Limits" />

      <SettingRow label="Max Vibration" description={`${preferences.maxVibrationIntensity}%`}>
        <View style={styles.sliderPlaceholder}>
          <View style={[styles.sliderFill, { width: `${preferences.maxVibrationIntensity}%` }]} />
        </View>
      </SettingRow>

      <SettingRow label="Max Thermal" description={`${preferences.maxThermalIntensity}%`}>
        <View style={styles.sliderPlaceholder}>
          <View style={[styles.sliderFill, { width: `${preferences.maxThermalIntensity}%` }]} />
        </View>
      </SettingRow>

      <SectionHeader title="Quiet Hours" />
      
      <SettingRow 
        label="Do Not Disturb" 
        description={`${preferences.quietHoursStart}:00 - ${preferences.quietHoursEnd}:00`}
      >
        <Text style={styles.quietHoursText}>
          {preferences.quietHoursStart}:00 → {preferences.quietHoursEnd}:00
        </Text>
      </SettingRow>

      <SectionHeader title="Device Info" />
      
      {ringConfig ? (
        <>
          <SettingRow label="Firmware Version">
            <Text style={styles.infoText}>{ringConfig.firmwareVersion}</Text>
          </SettingRow>
          <SettingRow label="Battery">
            <Text style={styles.infoText}>{ringConfig.batteryLevel}%</Text>
          </SettingRow>
          <SettingRow label="Autonomous Mode">
            <Switch
              value={ringConfig.autonomousMode}
              onValueChange={async (v) => {
                const updated = { ...ringConfig, autonomousMode: v };
                await writeConfig(updated);
                setRingConfig(updated);
              }}
              trackColor={{ true: '#667eea' }}
            />
          </SettingRow>
        </>
      ) : (
        <Text style={styles.noDevice}>Ring not connected</Text>
      )}

      <View style={styles.bottomPadding} />
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f8fafc',
  },
  sectionHeader: {
    fontSize: 13,
    fontWeight: '600',
    color: '#64748b',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    paddingHorizontal: 16,
    paddingTop: 24,
    paddingBottom: 8,
  },
  settingRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: '#fff',
    paddingHorizontal: 16,
    paddingVertical: 14,
    borderBottomWidth: 1,
    borderBottomColor: '#f1f5f9',
  },
  settingInfo: {
    flex: 1,
    marginRight: 12,
  },
  settingLabel: {
    fontSize: 16,
    color: '#1e293b',
  },
  settingDescription: {
    fontSize: 12,
    color: '#94a3b8',
    marginTop: 2,
  },
  sensitivityRow: {
    flexDirection: 'row',
    backgroundColor: '#fff',
    padding: 16,
    justifyContent: 'space-between',
  },
  sensitivityButton: {
    flex: 1,
    paddingVertical: 10,
    paddingHorizontal: 12,
    borderRadius: 8,
    backgroundColor: '#f1f5f9',
    marginHorizontal: 4,
    alignItems: 'center',
  },
  sensitivityButtonActive: {
    backgroundColor: '#667eea',
  },
  sensitivityText: {
    fontSize: 14,
    fontWeight: '500',
    color: '#64748b',
  },
  sensitivityTextActive: {
    color: '#fff',
  },
  sliderPlaceholder: {
    width: 100,
    height: 6,
    backgroundColor: '#e2e8f0',
    borderRadius: 3,
  },
  sliderFill: {
    height: '100%',
    backgroundColor: '#667eea',
    borderRadius: 3,
  },
  quietHoursText: {
    fontSize: 14,
    color: '#64748b',
  },
  infoText: {
    fontSize: 14,
    color: '#1e293b',
  },
  noDevice: {
    padding: 16,
    textAlign: 'center',
    color: '#94a3b8',
    fontStyle: 'italic',
  },
  bottomPadding: {
    height: 40,
  },
});