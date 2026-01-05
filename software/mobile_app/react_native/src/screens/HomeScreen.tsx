/**
 * @file HomeScreen.tsx
 * @brief Main dashboard with live HRV metrics and controls
 */

import React, { useState, useEffect } from 'react';
import { View, ScrollView, StyleSheet, TouchableOpacity, Text } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { WellnessDashboard } from '../components/WellnessDashboard';
import { StressAwarenessChart } from '../components/StressAwarenessChart';
import { RelaxationFeatures } from '../components/RelaxationFeatures';
import { engineBridge, EngineResult } from '../services/engineBridge';
import { isConnected } from '../services/bleService';

interface DataPoint {
  timestamp: number;
  coherence: number;
  microVariability: number;
}

export const HomeScreen: React.FC = () => {
  const navigation = useNavigation();
  const [latestResult, setLatestResult] = useState<EngineResult | null>(null);
  const [chartData, setChartData] = useState<DataPoint[]>([]);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    // Subscribe to engine results
    const unsubscribe = engineBridge.subscribe((result) => {
      setLatestResult(result);
      setChartData(prev => [
        ...prev.slice(-49),
        {
          timestamp: result.timestamp,
          coherence: result.meanCoherence,
          microVariability: result.microVariability,
        }
      ]);
    });

    // Check connection status periodically
    const connectionCheck = setInterval(() => {
      setConnected(isConnected());
    }, 1000);

    return () => {
      unsubscribe();
      clearInterval(connectionCheck);
    };
  }, []);

  return (
    <View style={styles.container}>
      {/* Connection Status */}
      <View style={[styles.statusBar, connected ? styles.connected : styles.disconnected]}>
        <Text style={styles.statusText}>
          {connected ? '● Connected' : '○ Demo Mode'}
        </Text>
        <TouchableOpacity onPress={() => (navigation as any).navigate('Settings')}>
          <Text style={styles.settingsLink}>Settings ⚙️</Text>
        </TouchableOpacity>
      </View>

      <ScrollView style={styles.scrollView} showsVerticalScrollIndicator={false}>
        {/* Main Dashboard */}
        {latestResult ? (
          <WellnessDashboard
            coherence={latestResult.meanCoherence}
            microVariability={latestResult.microVariability}
            stressLevel={latestResult.stress_awarenessLevel}
            confidence={latestResult.confidence}
            trend={latestResult.trend}
          />
        ) : (
          <View style={styles.loadingCard}>
            <Text style={styles.loadingText}>Initializing HRV engine...</Text>
            <Text style={styles.loadingSubtext}>Collecting data</Text>
          </View>
        )}

        {/* Chart */}
        <StressAwarenessChart data={chartData} windowMinutes={5} />

        {/* Relaxation Tools */}
        <RelaxationFeatures 
          onManualCue={(type) => {
            console.log('[HomeScreen] Manual cue triggered:', type);
          }}
        />

        {/* Stats Footer */}
        <View style={styles.statsFooter}>
          <Text style={styles.statsText}>
            Engine Stats: {engineBridge.getStats().totalCuesGenerated} cues generated, {engineBridge.getStats().totalCuesSent} sent
          </Text>
        </View>
      </ScrollView>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f1f5f9',
  },
  statusBar: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 8,
  },
  connected: {
    backgroundColor: '#dcfce7',
  },
  disconnected: {
    backgroundColor: '#fef3c7',
  },
  statusText: {
    fontSize: 12,
    fontWeight: '600',
    color: '#374151',
  },
  settingsLink: {
    fontSize: 12,
    color: '#667eea',
    fontWeight: '600',
  },
  scrollView: {
    flex: 1,
  },
  loadingCard: {
    backgroundColor: '#fff',
    margin: 8,
    padding: 32,
    borderRadius: 12,
    alignItems: 'center',
  },
  loadingText: {
    fontSize: 16,
    color: '#64748b',
  },
  loadingSubtext: {
    fontSize: 12,
    color: '#94a3b8',
    marginTop: 4,
  },
  statsFooter: {
    padding: 16,
    alignItems: 'center',
  },
  statsText: {
    fontSize: 11,
    color: '#94a3b8',
  },
});