import React from 'react';
import { render } from '@testing-library/react-native';
import { StressAwarenessChart } from '../src/components/StressAwarenessChart';

describe('StressAwarenessChart', () => {
  const mockData = [
    { timestamp: Date.now() - 3600000, coherence: 0.8, microVariability: 0.3 },
    { timestamp: Date.now() - 2700000, coherence: 0.6, microVariability: 0.5 },
    { timestamp: Date.now(), coherence: 0.75, microVariability: 0.35 },
  ];

  it('renders the title and subtitle', () => {
    const { getByText } = render(<StressAwarenessChart data={mockData} />);
    expect(getByText(/Coherence Timeline/i)).toBeTruthy();
    expect(getByText(/Last 5 minutes/i)).toBeTruthy();
  });

  it('shows no-data state when empty', () => {
    const { getByText } = render(<StressAwarenessChart data={[]} />);
    expect(getByText(/Collecting data/i)).toBeTruthy();
  });

  it('renders legend items for coherence zones', () => {
    const { getByText } = render(<StressAwarenessChart data={mockData} />);
    expect(getByText(/High/i)).toBeTruthy();
    expect(getByText(/Medium/i)).toBeTruthy();
    expect(getByText(/Low/i)).toBeTruthy();
  });
});
