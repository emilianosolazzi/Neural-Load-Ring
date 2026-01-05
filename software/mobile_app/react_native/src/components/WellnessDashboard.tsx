/**
 * @file WellnessDashboard.tsx
 * @brief Real-time HRV metrics visualization
 */

import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface WellnessDashboardProps {
  coherence: number;
  microVariability: number;
  stressLevel: string;
  confidence: number;
  trend: string;
}

export const WellnessDashboard: React.FC<WellnessDashboardProps> = ({
  coherence,
  microVariability,
  stressLevel,
  confidence,
  trend,
}) => {
  const getStressColor = (level: string) => {
    switch (level) {
      case 'optimal': return '#4ade80';
      case 'low': return '#86efac';
      case 'moderate': return '#fbbf24';
      case 'high': return '#f97316';
      case 'needs_attention': return '#ef4444';
      default: return '#9ca3af';
    }
  };

  const getTrendIcon = (t: string) => {
    switch (t) {
      case 'improving': return '↗️';
      case 'stable': return '→';
      case 'deteriorating': return '↘️';
      default: return '•';
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Wellness Dashboard</Text>
      
      <View style={styles.metricsRow}>
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Coherence</Text>
          <Text style={styles.metricValue}>{(coherence * 100).toFixed(0)}%</Text>
          <View style={[styles.bar, { width: `${coherence * 100}%`, backgroundColor: '#667eea' }]} />
        </View>
        
        <View style={styles.metricCard}>
          <Text style={styles.metricLabel}>Micro-Variability</Text>
          <Text style={styles.metricValue}>{(microVariability * 100).toFixed(1)}%</Text>
          <View style={[styles.bar, { width: `${Math.min(microVariability * 100, 100)}%`, backgroundColor: '#f59e0b' }]} />
        </View>
      </View>

      <View style={styles.statusRow}>
        <View style={[styles.statusCard, { backgroundColor: getStressColor(stressLevel) }]}>
          <Text style={styles.statusLabel}>Stress Awareness</Text>
          <Text style={styles.statusValue}>{stressLevel.replace('_', ' ')}</Text>
        </View>
        
        <View style={styles.statusCard}>
          <Text style={styles.statusLabel}>Trend</Text>
          <Text style={styles.trendValue}>{getTrendIcon(trend)} {trend}</Text>
        </View>
      </View>

      <View style={styles.confidenceRow}>
        <Text style={styles.confidenceLabel}>Data Confidence:</Text>
        <Text style={styles.confidenceValue}>{(confidence * 100).toFixed(0)}%</Text>
        {confidence < 0.6 && (
          <Text style={styles.warning}>⚠️ Low confidence - check ring fit</Text>
        )}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    padding: 16,
    backgroundColor: '#f8fafc',
    borderRadius: 12,
    margin: 8,
  },
  title: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#1e293b',
    marginBottom: 16,
  },
  metricsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 16,
  },
  metricCard: {
    flex: 1,
    backgroundColor: '#fff',
    padding: 12,
    borderRadius: 8,
    marginHorizontal: 4,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  metricLabel: {
    fontSize: 12,
    color: '#64748b',
    marginBottom: 4,
  },
  metricValue: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#1e293b',
    marginBottom: 8,
  },
  bar: {
    height: 4,
    borderRadius: 2,
  },
  statusRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 12,
  },
  statusCard: {
    flex: 1,
    padding: 12,
    borderRadius: 8,
    marginHorizontal: 4,
    backgroundColor: '#e2e8f0',
  },
  statusLabel: {
    fontSize: 12,
    color: '#374151',
    marginBottom: 4,
  },
  statusValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#fff',
    textTransform: 'capitalize',
  },
  trendValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#1e293b',
  },
  confidenceRow: {
    flexDirection: 'row',
    alignItems: 'center',
    flexWrap: 'wrap',
  },
  confidenceLabel: {
    fontSize: 12,
    color: '#64748b',
  },
  confidenceValue: {
    fontSize: 12,
    fontWeight: '600',
    color: '#1e293b',
    marginLeft: 4,
  },
  warning: {
    fontSize: 12,
    color: '#f59e0b',
    marginLeft: 8,
  },
});