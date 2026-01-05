import React from 'react';
import { render, fireEvent, waitFor, act } from '@testing-library/react-native';
import { RelaxationFeatures } from '../src/components/RelaxationFeatures';

// Mock BLE service
jest.mock('../src/services/bleService', () => ({
  sendActuatorCommand: jest.fn().mockResolvedValue(undefined),
  VibrationPattern: {
    BREATHING: 'breathing',
    ALERT: 'alert',
    HEARTBEAT: 'heartbeat',
  },
}));

describe('RelaxationFeatures', () => {
  afterEach(() => {
    jest.runOnlyPendingTimers();
  });

  it('renders without crashing', () => {
    const { getByText } = render(<RelaxationFeatures />);
    expect(getByText(/Relaxation Tools/i)).toBeTruthy();
  });

  it('updates the breathing prompt after TAP TO START press', async () => {
    const { getByText } = render(<RelaxationFeatures />);
    const button = getByText(/TAP TO START/i);
    await act(async () => {
      fireEvent.press(button);
    });
    await waitFor(() => expect(getByText(/INHALE/i)).toBeTruthy());
    await act(async () => {
      fireEvent.press(getByText(/INHALE/i));
    });
  });

  it('renders quick haptic cues', () => {
    const { getByText } = render(<RelaxationFeatures />);
    expect(getByText('Calm')).toBeTruthy();
    expect(getByText('Ground')).toBeTruthy();
  });

  it('calls callback when calm cue pressed', async () => {
    const mockCallback = jest.fn();
    const { getByText } = render(<RelaxationFeatures onManualCue={mockCallback} />);
    await act(async () => {
      fireEvent.press(getByText('Calm'));
    });
    await waitFor(() => expect(mockCallback).toHaveBeenCalledWith('calm'));
  });
});
