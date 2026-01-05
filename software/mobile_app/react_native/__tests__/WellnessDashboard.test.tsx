import React from 'react';
import { render } from '@testing-library/react-native';
import { WellnessDashboard } from '../src/components/WellnessDashboard';

describe('WellnessDashboard', () => {
  const defaultProps = {
    coherence: 0.72,
    microVariability: 45.2,
    stressLevel: 'moderate',
    confidence: 0.85,
    trend: 'stable',
  };

  it('renders the dashboard title', () => {
    const { getByText } = render(<WellnessDashboard {...defaultProps} />);
    expect(getByText('Wellness Dashboard')).toBeTruthy();
  });

  it('displays coherence percentage', () => {
    const { getByText } = render(<WellnessDashboard {...defaultProps} />);
    expect(getByText(/72%/)).toBeTruthy();
  });

  it('displays the selected stress level', () => {
    const { getByText } = render(<WellnessDashboard {...defaultProps} />);
    expect(getByText(/moderate/i)).toBeTruthy();
  });

  it('updates when stress level changes', () => {
    const props = { ...defaultProps, stressLevel: 'optimal' };
    const { getByText } = render(<WellnessDashboard {...props} />);
    expect(getByText(/optimal/i)).toBeTruthy();
  });

  it('shows confidence indicator percent', () => {
    const { getByText } = render(<WellnessDashboard {...defaultProps} />);
    expect(getByText(/85%/)).toBeTruthy();
  });
});
