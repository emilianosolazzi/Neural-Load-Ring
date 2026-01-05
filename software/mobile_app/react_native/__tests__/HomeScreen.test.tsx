import React from 'react';
import { render } from '@testing-library/react-native';
import { HomeScreen } from '../src/screens/HomeScreen';

jest.mock('../src/services/engineBridge', () => {
  const mockSubscribe = jest.fn(() => jest.fn());
  const mockGetStats = jest.fn(() => ({ totalCuesGenerated: 5, totalCuesSent: 3 }));
  return {
    engineBridge: {
      subscribe: mockSubscribe,
      getStats: mockGetStats,
    },
    EngineResult: {},
  };
});

jest.mock('../src/services/bleService', () => ({
  isConnected: jest.fn(() => false),
}));

jest.mock('../src/components/WellnessDashboard', () => ({
  WellnessDashboard: () => null,
}));

jest.mock('../src/components/StressAwarenessChart', () => ({
  StressAwarenessChart: () => null,
}));

jest.mock('../src/components/RelaxationFeatures', () => ({
  RelaxationFeatures: () => null,
}));

describe('HomeScreen', () => {
  it('renders status and stats', () => {
    const { getByText } = render(<HomeScreen />);
    expect(getByText(/Settings/)).toBeTruthy();
    expect(getByText(/Engine Stats/)).toBeTruthy();
  });

  it('displays demo mode indicator', () => {
    const { getByText } = render(<HomeScreen />);
    expect(getByText(/Demo Mode/)).toBeTruthy();
  });
});
