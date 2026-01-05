/**
 * @file StressAwarenessChart.tsx
 * @brief Time-series chart for coherence and stress trends
 */

import React from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';

const { width: screenWidth } = Dimensions.get('window');

interface DataPoint {
  timestamp: number;
  coherence: number;
  microVariability: number;
}

interface StressAwarenessChartProps {
  data: DataPoint[];
  windowMinutes?: number;
}

export const StressAwarenessChart: React.FC<StressAwarenessChartProps> = ({
  data,
  windowMinutes = 5,
}) => {
  // Simple bar visualization (replace with react-native-chart-kit for production)
  const recentData = data.slice(-20);
  const maxCoherence = Math.max(...recentData.map(d => d.coherence), 0.01);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Coherence Timeline</Text>
      <Text style={styles.subtitle}>Last {windowMinutes} minutes</Text>
      
      <View style={styles.chartContainer}>
        {recentData.length === 0 ? (
          <Text style={styles.noData}>Collecting data...</Text>
        ) : (
          <View style={styles.barsContainer}>
            {recentData.map((point, index) => (
              <View key={index} style={styles.barWrapper}>
                <View 
                  style={[
                    styles.barFill,
                    { 
                      height: `${(point.coherence / maxCoherence) * 100}%`,
                      backgroundColor: getCoherenceColor(point.coherence),
                    }
                  ]} 
                />
              </View>
            ))}
          </View>
        )}
      </View>

      <View style={styles.legend}>
        <View style={styles.legendItem}>
          <View style={[styles.legendDot, { backgroundColor: '#4ade80' }]} />
          <Text style={styles.legendText}>High (&gt;70%)</Text>
        </View>
        <View style={styles.legendItem}>
          <View style={[styles.legendDot, { backgroundColor: '#fbbf24' }]} />
          <Text style={styles.legendText}>Medium (40-70%)</Text>
        </View>
        <View style={styles.legendItem}>
          <View style={[styles.legendDot, { backgroundColor: '#ef4444' }]} />
          <Text style={styles.legendText}>Low (&lt;40%)</Text>
        </View>
      </View>
    </View>
  );
};

const getCoherenceColor = (coherence: number): string => {
  if (coherence >= 0.7) return '#4ade80';
  if (coherence >= 0.4) return '#fbbf24';
  return '#ef4444';
};

const styles = StyleSheet.create({
  container: {
    padding: 16,
    backgroundColor: '#fff',
    borderRadius: 12,
    margin: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  title: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#1e293b',
  },
  subtitle: {
    fontSize: 12,
    color: '#64748b',
    marginBottom: 12,
  },
  chartContainer: {
    height: 120,
    backgroundColor: '#f1f5f9',
    borderRadius: 8,
    padding: 8,
    marginBottom: 12,
  },
  noData: {
    textAlign: 'center',
    color: '#94a3b8',
    marginTop: 40,
  },
  barsContainer: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'flex-end',
    justifyContent: 'space-evenly',
  },
  barWrapper: {
    flex: 1,
    height: '100%',
    justifyContent: 'flex-end',
    marginHorizontal: 1,
  },
  barFill: {
    width: '100%',
    borderRadius: 2,
    minHeight: 2,
  },
  legend: {
    flexDirection: 'row',
    justifyContent: 'space-around',
  },
  legendItem: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  legendDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 4,
  },
  legendText: {
    fontSize: 10,
    color: '#64748b',
  },
});