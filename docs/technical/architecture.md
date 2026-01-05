# Architecture

High level architecture notes.

## Hardware Specifications
```js
const hardwareSpec = {
  // Minimum viable hardware (tested):
  processor: 'ARM Cortex-M4 @ 80MHz',
  memory: '64KB RAM, 256KB Flash',
  sensors: 'Single-channel PPG @ 100Hz',
  battery: '40mAh LiPo (7+ days operation)',
  connectivity: 'BLE 5.2 for data sync',
  
  // Production hardware (recommended):
  processor: 'ARM Cortex-M33 @ 120MHz',
  memory: '128KB RAM, 512KB Flash',
  sensors: 'Dual-channel PPG + temperature',
  battery: '60mAh LiPo (14+ days operation)',
  connectivity: 'BLE 5.2 + NFC for pairing',
};
```
