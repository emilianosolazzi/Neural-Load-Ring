# Neural Load Ring - BLE GATT Service Specification

## Overview

The Neural Load Ring uses a custom Bluetooth Low Energy GATT service for bidirectional communication with the mobile app. This document specifies the service UUIDs, characteristics, and data formats.

**Target Hardware:** nRF52833 with SoftDevice S140 v7.2.0  
**BLE Version:** 5.0 (2M PHY supported)  
**MTU:** 247 bytes (negotiated)

---

## Service UUID

| Name | UUID | Base |
|------|------|------|
| **Wellness Service** | `6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E` | Custom |

**Base UUID:** `6E4C0000-B5A3-F393-E0A9-E50E24DCCA9E`

The 16-bit UUID portion encodes "NLR" in ASCII (0x4E4C52).

---

## Characteristics

### 1. RR Interval (Notify)

| Property | Value |
|----------|-------|
| UUID | `6E4C0002-B5A3-F393-E0A9-E50E24DCCA9E` |
| Properties | Notify |
| Size | 2-20 bytes |

**Data Format:** Array of `uint16_t` (little-endian), each representing an RR interval in milliseconds.

```
Offset  Type      Description
------  --------  -----------
0       uint16_t  RR interval 1 (ms)
2       uint16_t  RR interval 2 (ms)
...     ...       Up to 10 values per notification
```

**Example:** Heart rate ~75 BPM with 3 RR intervals:
```
[0x20, 0x03, 0x18, 0x03, 0x28, 0x03]  // 800ms, 792ms, 808ms
```

**Streaming Rate:** Configurable 1-10 Hz (default: 4 Hz)

---

### 2. Coherence (Notify)

| Property | Value |
|----------|-------|
| UUID | `6E4C0003-B5A3-F393-E0A9-E50E24DCCA9E` |
| Properties | Notify |
| Size | 12 bytes |

**Data Format:**

```c
typedef struct __attribute__((packed)) {
    uint8_t  stress_level;          // 0-100 awareness level
    uint8_t  coherence_pct;         // 0-100 phase coherence
    uint8_t  confidence_pct;        // 0-100 measurement confidence
    uint8_t  variability_level;     // 0-100 micro-variability score
    uint16_t mean_rr_ms;            // Mean RR interval (little-endian)
    uint16_t rmssd_ms;              // RMSSD HRV metric (little-endian)
    uint16_t respiratory_rate_cpm;  // Breaths/min × 10 (e.g., 150 = 15.0)
    uint16_t reserved;              // Future use (set to 0)
} nlr_coherence_packet_t;
```

**Example:** Relaxed state
```
[35, 72, 90, 65, 0x54, 0x03, 0x28, 0x00, 0x96, 0x00, 0x00, 0x00]
// stress=35%, coherence=72%, confidence=90%, variability=65%
// meanRR=852ms, RMSSD=40ms, respRate=15.0/min
```

**Update Rate:** Configurable 5-60 seconds (default: 15s)

---

### 3. Actuator Control (Write)

| Property | Value |
|----------|-------|
| UUID | `6E4C0004-B5A3-F393-E0A9-E50E24DCCA9E` |
| Properties | Write, Write Without Response |
| Size | 4 bytes |

**Data Format:**

```c
typedef struct __attribute__((packed)) {
    uint8_t thermal_intensity;      // 0-100 thermal PWM (%)
    uint8_t thermal_duration_s;     // Duration (0 = off immediately)
    uint8_t vibration_pattern;      // Pattern ID (see below)
    uint8_t vibration_intensity;    // 0-100 vibration strength (%)
} nlr_actuator_cmd_t;
```

**Vibration Patterns:**

| ID | Name | Description |
|----|------|-------------|
| 0 | Off | Stop vibration |
| 1 | Single | Single short pulse |
| 2 | Double | Two pulses |
| 3 | Triple | Three pulses |
| 4 | Heartbeat | Lub-dub pattern |
| 5 | Breathing | Slow wave (inhale/exhale) |
| 6 | Alert | Rapid attention-getting |

**Example:** Gentle warmth with breathing vibration
```
[40, 30, 5, 50]  // 40% heat for 30s, breathing pattern at 50%
```

**Safety:** Values are clamped to configured maximums.

---

### 4. Device State (Read + Notify)

| Property | Value |
|----------|-------|
| UUID | `6E4C0005-B5A3-F393-E0A9-E50E24DCCA9E` |
| Properties | Read, Notify |
| Size | 8 bytes |

**Data Format:**

```c
typedef struct __attribute__((packed)) {
    uint8_t  battery_pct;           // 0-100 battery level
    uint8_t  charging_state;        // 0=not charging, 1=charging, 2=full
    uint8_t  connection_state;      // 0=idle, 1=advertising, 2=connected
    uint8_t  streaming_active;      // Bit 0: RR, Bit 1: coherence
    int8_t   skin_temp_c;           // Skin temperature °C (signed)
    uint8_t  error_flags;           // See error codes below
    uint16_t uptime_min;            // Uptime in minutes (little-endian)
} nlr_device_state_t;
```

**Error Flags:**

| Bit | Name | Description |
|-----|------|-------------|
| 0 | PPG_ERROR | PPG sensor communication failed |
| 1 | TEMP_ERROR | Temperature sensor error |
| 2 | BATTERY_LOW | Battery critically low (<10%) |
| 3 | THERMAL_FAULT | Thermal actuator over-temperature |
| 4 | STORAGE_FULL | Internal storage full |
| 5-7 | Reserved | |

**Example:** Healthy state, streaming RR
```
[85, 0, 2, 1, 32, 0, 0x3C, 0x00]
// 85% battery, not charging, connected, RR streaming
// skin 32°C, no errors, 60 min uptime
```

---

### 5. Configuration (Read + Write)

| Property | Value |
|----------|-------|
| UUID | `6E4C0006-B5A3-F393-E0A9-E50E24DCCA9E` |
| Properties | Read, Write |
| Size | 16 bytes |

**Data Format:**

```c
typedef struct __attribute__((packed)) {
    uint8_t  streaming_rate_hz;     // RR notification rate (1-10)
    uint8_t  coherence_update_s;    // Coherence update interval (5-60)
    uint8_t  thermal_max_pct;       // Maximum thermal intensity (0-100)
    uint8_t  vibration_max_pct;     // Maximum vibration intensity (0-100)
    uint8_t  quiet_hours_start;     // Quiet hours start (0-23)
    uint8_t  quiet_hours_end;       // Quiet hours end (0-23)
    uint8_t  led_brightness;        // Status LED brightness (0-100)
    uint8_t  reserved[9];           // Future use (set to 0)
} nlr_config_t;
```

**Default Values:**
```
streaming_rate_hz = 4
coherence_update_s = 15
thermal_max_pct = 80
vibration_max_pct = 100
quiet_hours_start = 22
quiet_hours_end = 7
led_brightness = 50
```

---

## Connection Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Advertising Interval (Fast) | 100-200 ms | First 3 minutes |
| Advertising Interval (Slow) | 200 ms | After timeout |
| Connection Interval | 15-30 ms | Optimized for streaming |
| Slave Latency | 0 | Real-time data |
| Supervision Timeout | 4000 ms | Allow brief dropouts |

---

## Mobile App Integration

### iOS (CoreBluetooth)

```swift
let NLR_SERVICE_UUID = CBUUID(string: "6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E")
let NLR_RR_CHAR_UUID = CBUUID(string: "6E4C0002-B5A3-F393-E0A9-E50E24DCCA9E")
let NLR_COHERENCE_CHAR_UUID = CBUUID(string: "6E4C0003-B5A3-F393-E0A9-E50E24DCCA9E")
let NLR_ACTUATOR_CHAR_UUID = CBUUID(string: "6E4C0004-B5A3-F393-E0A9-E50E24DCCA9E")
let NLR_STATE_CHAR_UUID = CBUUID(string: "6E4C0005-B5A3-F393-E0A9-E50E24DCCA9E")
let NLR_CONFIG_CHAR_UUID = CBUUID(string: "6E4C0006-B5A3-F393-E0A9-E50E24DCCA9E")
```

### Android (BLE GATT)

```kotlin
val NLR_SERVICE_UUID = UUID.fromString("6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E")
val NLR_RR_CHAR_UUID = UUID.fromString("6E4C0002-B5A3-F393-E0A9-E50E24DCCA9E")
// ... etc
```

### React Native (react-native-ble-plx)

```typescript
const NLR_SERVICE = '6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E';
const NLR_RR_CHAR = '6E4C0002-B5A3-F393-E0A9-E50E24DCCA9E';

// Enable notifications
await device.monitorCharacteristicForService(
  NLR_SERVICE,
  NLR_RR_CHAR,
  (error, characteristic) => {
    if (characteristic?.value) {
      const data = base64.decode(characteristic.value);
      // Parse RR intervals (uint16_t little-endian)
    }
  }
);
```

---

## Sequence Diagrams

### Connection + Streaming

```
Mobile App                              NLR Ring
    |                                      |
    |------- Scan for "NLR-XXXX" --------->|
    |<-------- Advertising Response -------|
    |                                      |
    |------- Connect Request ------------->|
    |<-------- Connected ------------------|
    |                                      |
    |------- MTU Exchange (247) ---------->|
    |<-------- MTU Response ---------------|
    |                                      |
    |------- Enable RR Notifications ----->|
    |<-------- Confirmation ---------------|
    |                                      |
    |<-------- RR Notification (4 Hz) -----|
    |<-------- RR Notification ------------|
    |<-------- Coherence (every 15s) ------|
    |                                      |
    |------- Actuator Command ------------>|
    |          (thermal/vibration)         |
    |                                      |
```

### Reconnection

The ring automatically restarts advertising after disconnect. The app should:
1. Cache the device address after first pairing
2. Attempt direct connection to cached address on app launch
3. Fall back to scanning if direct connection fails

---

## Power Optimization

1. **Advertising:** Transitions to slow mode after 3 minutes
2. **Connection Interval:** 30ms when idle, 15ms when streaming
3. **Notification Batching:** Up to 10 RR values per notification
4. **2M PHY:** Automatically negotiated for faster transfers

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-05 | Initial specification |

