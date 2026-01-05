# Neural Load Ring - Mobile App

React Native mobile application for the Neural Load Ring wearable device.

## Features

✅ **Real-time HRV Analysis** - Powered by validated wellness engine  
✅ **Intelligent Haptic Feedback** - Autonomous cue generation and pattern system  
✅ **Signature Feel** - Organic, personality-driven haptic patterns  
✅ **BLE Communication** - Full device control and data streaming  
✅ **Live Dashboard** - Real-time coherence, micro-variability, stress awareness  
✅ **Mock Mode** - Full development without hardware

## Architecture

```
App.tsx                     → Root navigation & engine initialization
├─ screens/
│  └─ HomeScreen.tsx        → Main dashboard with live metrics
├─ components/
│  ├─ WellnessDashboard.tsx → HRV visualization
│  ├─ StressAwarenessChart.tsx → Coherence & variability charts
│  ├─ RelaxationFeatures.tsx → Breathing guide, manual cues
│  └─ SettingsScreen.tsx    → Preferences & device config
└─ services/
   ├─ bleService.ts         → BLE connection & data streaming
   └─ engineBridge.ts       → HRV engine ↔ Haptic cue bridge
```

## Prerequisites

- **Node.js** ≥ 18.x
- **React Native CLI** (not Expo)
- **iOS**: Xcode 15+ (macOS only)
- **Android**: Android Studio, SDK 33+

## Setup

### 1. Install Dependencies

```bash
cd software/mobile_app/react_native
npm install
```

### 2. iOS Setup (macOS only)

```bash
cd ios
pod install
cd ..
```

### 3. Run Development

**Android:**
```bash
npm run android
```

**iOS:**
```bash
npm run ios
```

**Start Metro bundler separately:**
```bash
npm start
```

## Development Mode

The app automatically starts in **mock mode** when `__DEV__` is true:
- Simulated RR intervals (850ms baseline with realistic HRV)
- Full engine processing & cue generation
- No physical ring required

## Key Integrations

### Engine Bridge
The `engineBridge` connects the validated HRV engine to the haptic cue system:

```typescript
import { engineBridge, startIntelligentMode } from '@/services/engineBridge';

// Subscribe to HRV results
const unsubscribe = engineBridge.subscribe((result) => {
  console.log('Coherence:', result.meanCoherence);
  console.log('Stress:', result.stress_awarenessLevel);
});

// Subscribe to generated cues
engineBridge.onCue((cue, metrics) => {
  if (cue.shouldTrigger) {
    console.log(`Cue: ${cue.reason} (priority: ${cue.priority})`);
  }
});

// Configure preferences
engineBridge.updateCuePreferences({
  sensitivityMode: 'normal',
  maxThermalIntensity: 80,
  quietHoursStart: 22,
  quietHoursEnd: 7,
});
```

### BLE Service
Direct ring control and data streaming:

```typescript
import { 
  scanForDevices, 
  connectToDevice,
  subscribeToRR,
  sendActuatorCommand 
} from '@/services/bleService';

// Find and connect
const devices = await scanForDevices();
await connectToDevice(devices[0].id);

// Listen to RR intervals
subscribeToRR((rrMs) => {
  console.log('RR:', rrMs);
});

// Send haptic command
await sendActuatorCommand({
  vibPattern: VibrationPattern.HEARTBEAT,
  vibIntensity: 70,
  thermalIntensity: 50,
  duration: 10,
});
```

## Testing

```bash
# Type checking
npm run type-check

# Linting
npm run lint

# Unit tests (when implemented)
npm test
```

## Build for Production

**Android APK:**
```bash
cd android
./gradlew assembleRelease
```

**iOS Archive:**
```bash
cd ios
xcodebuild -workspace NeuralLoadRing.xcworkspace \
  -scheme NeuralLoadRing \
  -configuration Release \
  archive
```

## Permissions

### iOS (Info.plist)
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Connect to Neural Load Ring for HRV monitoring</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>Connect to Neural Load Ring for HRV monitoring</string>
```

### Android (AndroidManifest.xml)
```xml
<uses-permission android:name="android.permission.BLUETOOTH"/>
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN"/>
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"/>
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>
```

## Troubleshooting

**Metro bundler cache issues:**
```bash
npm start -- --reset-cache
```

**Android build fails:**
```bash
cd android
./gradlew clean
cd ..
rm -rf android/app/build
npm run android
```

**iOS build fails:**
```bash
cd ios
pod deintegrate
pod install
cd ..
npm run ios
```

## Architecture Highlights

- **Validated HRV Engine** - Research-grade coherence & micro-variability
- **Autonomous Cue System** - On-device decision making with confidence gating
- **Signature Feel Patterns** - Organic easing curves, not mechanical vibrations
- **Adaptive Timing** - Context-aware cooldowns and quiet hours
- **Mock Mode** - Full development without hardware

## License

MIT - See LICENSE file in project root
