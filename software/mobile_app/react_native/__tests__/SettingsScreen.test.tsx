import React from 'react';
import { render } from '@testing-library/react-native';
import { SettingsScreen } from '../src/components/SettingsScreen';

// Mock services
jest.mock('../src/services/engineBridge', () => ({
  engineBridge: {
    getCuePreferences: () => ({
      enabled: true,
      vibrationEnabled: true,
      thermalEnabled: true,
      breathingGuidanceEnabled: false,
      sensitivityMode: 'normal',
      maxVibrationIntensity: 70,
      maxThermalIntensity: 55,
      quietHoursStart: 22,
      quietHoursEnd: 6,
    }),
    updateCuePreferences: jest.fn(),
  },
}));

jest.mock('../src/services/bleService', () => ({
  readConfig: jest.fn().mockResolvedValue(null),
  writeConfig: jest.fn().mockResolvedValue(undefined),
}));

describe('SettingsScreen', () => {
  it('renders the haptic feedback section', () => {
    const { getByText } = render(<SettingsScreen />);
    expect(getByText('Haptic Feedback')).toBeTruthy();
  });

  it('shows the vibration toggle row', () => {
    const { getByText } = render(<SettingsScreen />);
    expect(getByText('Vibration')).toBeTruthy();
    expect(getByText('Motor feedback for attention cues')).toBeTruthy();
  });

  it('shows the thermal toggle row', () => {
    const { getByText } = render(<SettingsScreen />);
    expect(getByText('Thermal')).toBeTruthy();
    expect(getByText('Warming comfort for relaxation')).toBeTruthy();
  });
});
