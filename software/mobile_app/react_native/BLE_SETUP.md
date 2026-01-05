# BLE Platform Setup Guide

This document describes the platform-specific configuration required for Bluetooth Low Energy functionality in the Neural Load Ring mobile app.

## iOS Setup

### Info.plist Entries

Add the following entries to `ios/<AppName>/Info.plist`:

```xml
<!-- Bluetooth permissions -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Neural Load Ring needs Bluetooth access to connect to your wellness ring and stream HRV data.</string>

<key>NSBluetoothPeripheralUsageDescription</key>
<string>Neural Load Ring needs Bluetooth access to communicate with your wellness ring.</string>

<!-- Background BLE (optional - for continuous monitoring) -->
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
</array>
```

### Capabilities

In Xcode:
1. Open the project (`ios/<AppName>.xcworkspace`)
2. Select the app target
3. Go to "Signing & Capabilities"
4. Add "Background Modes" capability
5. Check "Uses Bluetooth LE accessories"

## Android Setup

### AndroidManifest.xml Permissions

Add the following permissions to `android/app/src/main/AndroidManifest.xml`:

```xml
<!-- Legacy Bluetooth permissions (Android 11 and below) -->
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30"/>
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30"/>

<!-- Android 12+ Bluetooth permissions -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation"/>
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>

<!-- Location permission (required for BLE scanning on Android 11 and below) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"/>

<!-- Foreground service for background BLE (optional) -->
<uses-permission android:name="android.permission.FOREGROUND_SERVICE"/>
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE"/>
```

### ProGuard Rules (if using ProGuard/R8)

Add to `android/app/proguard-rules.pro`:

```proguard
# react-native-ble-plx
-keep class com.polidea.multiplatformbleadapter.** { *; }
```

## Runtime Permission Flow

The `bleService.ts` module handles runtime permissions automatically:

```typescript
import { requestBlePermissions, initializeBle, scanForDevices } from './services/bleService';

// Before scanning
const hasPermission = await requestBlePermissions();
if (!hasPermission) {
  // Show UI explaining why BLE is needed
  return;
}

// Initialize BLE manager
const ready = await initializeBle();
if (!ready) {
  // BLE not available or powered off
  return;
}

// Now safe to scan
const devices = await scanForDevices(5000);
```

## Testing BLE Without Hardware

The BLE service includes a mock mode for development:

```typescript
// Scan will return a mock device if no real devices found
const devices = await scanForDevices();
// [{ id: 'MOCK-NLR-0000', name: 'NLR-0000 (Mock)', rssi: -45 }]

// Connecting to mock device starts synthetic data stream
await connectToDevice('MOCK-NLR-0000');
// Now you'll receive synthetic RR intervals at 4Hz
```

## GATT Service Specification

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Service | `6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E` | — | Wellness Service |
| RR Interval | `6E4C0002-...` | Notify | Array of uint16_t RR intervals |
| Coherence | `6E4C0003-...` | Notify | 12-byte coherence packet |
| Actuator Control | `6E4C0004-...` | Write | 4-byte actuator command |
| Device State | `6E4C0005-...` | Read, Notify | 8-byte device state |
| Configuration | `6E4C0006-...` | Read, Write | 16-byte config |

## Troubleshooting

### iOS
- **"Bluetooth permission not granted"**: Ensure Info.plist has the required usage descriptions
- **"Device not found"**: Check that the ring is advertising and not connected to another device
- **CoreBluetooth cache issues**: During development, you may need to forget the device in iOS Bluetooth settings

### Android
- **"Scan returns no devices"**: On Android 6-11, location permission is required for BLE scanning
- **"Connection times out"**: Ensure the device isn't bonded to another phone
- **Doze mode issues**: For reliable background operation, use a foreground service

### General
- **Mock device appears in scan**: This is intentional for development. Real devices take priority when found.
- **Data not streaming**: Verify notifications are enabled (happens automatically in `connectToDevice`)
