/**
 * @file App.tsx
 * @brief Neural Load Ring Mobile App - Main Component
 * 
 * Root component with navigation and intelligent feedback system.
 */

import React, { useEffect, useState } from 'react';
import { StatusBar } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { SafeAreaProvider } from 'react-native-safe-area-context';

import { HomeScreen } from './screens/HomeScreen';
import { SettingsScreen } from './components/SettingsScreen';
import { engineBridge } from './services/engineBridge';

const Stack = createNativeStackNavigator();

function App(): React.JSX.Element {
  const [isEngineReady, setIsEngineReady] = useState(false);

  useEffect(() => {
    // Initialize engine bridge with intelligent feedback
    console.log('[App] Starting intelligent mode...');
    
    const cleanup = engineBridge.subscribe((result) => {
      // Engine results available for debugging/monitoring
      if (__DEV__) {
        console.log('[App] HRV:', {
          coherence: result.meanCoherence.toFixed(3),
          microVar: result.microVariability.toFixed(3),
          stress: result.stress_awarenessLevel,
        });
      }
    });

    engineBridge.start({ 
      useMock: __DEV__, 
      enableCues: true,
      debugLogging: __DEV__
    });
    
    setIsEngineReady(true);

    return () => {
      cleanup();
      engineBridge.stop();
    };
  }, []);

  return (
    <SafeAreaProvider>
      <StatusBar barStyle="dark-content" />
      <NavigationContainer>
        <Stack.Navigator
          initialRouteName="Home"
          screenOptions={{
            headerStyle: {
              backgroundColor: '#667eea',
            },
            headerTintColor: '#fff',
            headerTitleStyle: {
              fontWeight: 'bold',
            },
          }}
        >
          <Stack.Screen 
            name="Home" 
            component={HomeScreen}
            options={{ title: 'Neural Load Ring' }}
          />
          <Stack.Screen 
            name="Settings" 
            component={SettingsScreen}
            options={{ title: 'Settings' }}
          />
        </Stack.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}

export default App;
