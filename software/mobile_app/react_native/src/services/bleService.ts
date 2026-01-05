/**
 * @file bleService.ts
 * @brief Neural Load Ring BLE Service - React Native Integration
 *
 * Handles Bluetooth Low Energy communication with the NLR ring device.
 * Supports RR interval streaming, coherence notifications, and actuator control.
 *
 * Dependencies: react-native-ble-plx (or native bridge)
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * GATT SERVICE UUIDs (must match firmware ble_stack.h)
 ******************************************************************************/

export const NLR_SERVICE_UUID = '6E4C0001-B5A3-F393-E0A9-E50E24DCCA9E';

export const NLR_CHARACTERISTICS = {
	RR_INTERVAL: '6E4C0002-B5A3-F393-E0A9-E50E24DCCA9E',
	COHERENCE: '6E4C0003-B5A3-F393-E0A9-E50E24DCCA9E',
	ACTUATOR_CTRL: '6E4C0004-B5A3-F393-E0A9-E50E24DCCA9E',
	DEVICE_STATE: '6E4C0005-B5A3-F393-E0A9-E50E24DCCA9E',
	CONFIG: '6E4C0006-B5A3-F393-E0A9-E50E24DCCA9E',
} as const;

/*******************************************************************************
 * DATA STRUCTURES (mirrors firmware packed structs)
 ******************************************************************************/

/** Coherence notification packet from ring */
export interface CoherencePacket {
	stressLevel: number; // 0-100
	coherencePct: number; // 0-100
	confidencePct: number; // 0-100
	variabilityLevel: number; // 0-100
	meanRrMs: number;
	rmssdMs: number;
	respiratoryRateCpm: number; // actual = value / 10
}

/** Actuator command to ring */
export interface ActuatorCommand {
	thermalIntensity: number; // 0-100
	thermalDurationS: number; // 0 = off
	vibrationPattern: VibrationPattern;
	vibrationIntensity: number; // 0-100
}

export enum VibrationPattern {
	OFF = 0,
	SINGLE = 1,
	DOUBLE = 2,
	TRIPLE = 3,
	HEARTBEAT = 4,
	BREATHING = 5,
	ALERT = 6,
}

/** Device state from ring */
export interface DeviceState {
	batteryPct: number;
	chargingState: 'not_charging' | 'charging' | 'full';
	connectionState: 'idle' | 'advertising' | 'connected';
	streamingRR: boolean;
	streamingCoherence: boolean;
	skinTempC: number;
	errorFlags: number;
	uptimeMin: number;
}

/** Configuration for ring */
export interface RingConfig {
	streamingRateHz: number; // 1-10
	coherenceUpdateS: number; // 5-60
	thermalMaxPct: number; // 0-100
	vibrationMaxPct: number; // 0-100
	quietHoursStart: number; // 0-23
	quietHoursEnd: number; // 0-23
	ledBrightness: number; // 0-100
}

/*******************************************************************************
 * EVENT LISTENERS
 ******************************************************************************/

type RRListener = (rrMs: number) => void;
type CoherenceListener = (packet: CoherencePacket) => void;
type DeviceStateListener = (state: DeviceState) => void;
type ConnectionListener = (connected: boolean, deviceId?: string) => void;

const rrListeners = new Set<RRListener>();
const coherenceListeners = new Set<CoherenceListener>();
const deviceStateListeners = new Set<DeviceStateListener>();
const connectionListeners = new Set<ConnectionListener>();

/*******************************************************************************
 * INTERNAL STATE
 ******************************************************************************/

let mockTimer: ReturnType<typeof setInterval> | null = null;
let connectedDeviceId: string | null = null;
let isScanning = false;

/*******************************************************************************
 * MOCK IMPLEMENTATION (for development without hardware)
 ******************************************************************************/

function emitRR(rrMs: number) {
	rrListeners.forEach((cb) => cb(rrMs));
}

function emitCoherence(packet: CoherencePacket) {
	coherenceListeners.forEach((cb) => cb(packet));
}

function emitDeviceState(state: DeviceState) {
	deviceStateListeners.forEach((cb) => cb(state));
}

function emitConnection(connected: boolean, deviceId?: string) {
	connectionListeners.forEach((cb) => cb(connected, deviceId));
}

export function subscribeToRR(listener: RRListener): () => void {
	rrListeners.add(listener);
	return () => rrListeners.delete(listener);
}

export function subscribeToCoherence(listener: CoherenceListener): () => void {
	coherenceListeners.add(listener);
	return () => coherenceListeners.delete(listener);
}

export function subscribeToDeviceState(listener: DeviceStateListener): () => void {
	deviceStateListeners.add(listener);
	return () => deviceStateListeners.delete(listener);
}

export function subscribeToConnection(listener: ConnectionListener): () => void {
	connectionListeners.add(listener);
	return () => connectionListeners.delete(listener);
}

/**
 * Start mock BLE stream for development/testing
 */
export function startMockStream() {
	if (mockTimer) return;

	emitConnection(true, 'MOCK-NLR-0000');
	connectedDeviceId = 'MOCK-NLR-0000';

	let t = 0;
	let coherenceCounter = 0;

	mockTimer = setInterval(() => {
		// Synthetic RR stream: ~75 bpm with respiratory modulation
		const respiratoryPhase = Math.sin((t / 15) * 2 * Math.PI);
		const base = 800 + respiratoryPhase * 40; // RSA modulation
		const noise = (Math.random() - 0.5) * 20;
		const rr = Math.max(300, Math.min(2000, base + noise));
		emitRR(rr);

		// Send coherence packet every ~15 seconds (60 ticks at 4Hz)
		coherenceCounter++;
		if (coherenceCounter >= 60) {
			coherenceCounter = 0;
			emitCoherence({
				stressLevel: Math.round(30 + Math.random() * 20),
				coherencePct: Math.round(60 + respiratoryPhase * 15),
				confidencePct: Math.round(85 + Math.random() * 10),
				variabilityLevel: Math.round(50 + Math.random() * 20),
				meanRrMs: Math.round(base),
				rmssdMs: Math.round(35 + Math.random() * 15),
				respiratoryRateCpm: Math.round(140 + Math.random() * 20), // 14.0-16.0
			});
		}

		// Send device state every ~30 seconds
		if (t % 120 === 0) {
			emitDeviceState({
				batteryPct: 85,
				chargingState: 'not_charging',
				connectionState: 'connected',
				streamingRR: true,
				streamingCoherence: true,
				skinTempC: 32,
				errorFlags: 0,
				uptimeMin: Math.floor(t / 240),
			});
		}

		t += 1;
	}, 250); // 4 Hz RR events
}

export function stopMockStream() {
	if (!mockTimer) return;
	clearInterval(mockTimer);
	mockTimer = null;
	emitConnection(false);
	connectedDeviceId = null;
}

/*******************************************************************************
 * REAL BLE IMPLEMENTATION STUBS
 * Replace with react-native-ble-plx or native module calls
 ******************************************************************************/

/**
 * Scan for NLR devices
 * @param timeoutMs Scan duration
 * @returns Promise resolving to array of discovered devices
 */
export async function scanForDevices(timeoutMs = 10000): Promise<Array<{ id: string; name: string; rssi: number }>> {
	if (isScanning) return [];
	isScanning = true;

	// TODO: Replace with actual BLE scanning
	// const manager = new BleManager();
	// manager.startDeviceScan([NLR_SERVICE_UUID], null, (error, device) => {
	//   if (device?.name?.startsWith('NLR-')) {
	//     // Handle discovered device
	//   }
	// });

	console.log(`[BLE] Scanning for ${timeoutMs}ms...`);

	return new Promise((resolve) => {
		setTimeout(() => {
			isScanning = false;
			// Return mock device for development
			resolve([{ id: 'MOCK-NLR-0000', name: 'NLR-0000', rssi: -45 }]);
		}, Math.min(timeoutMs, 2000));
	});
}

/**
 * Connect to a specific NLR device
 * @param deviceId Device identifier from scan
 */
export async function connectToDevice(deviceId: string): Promise<boolean> {
	console.log(`[BLE] Connecting to ${deviceId}...`);

	// TODO: Replace with actual BLE connection
	// const device = await manager.connectToDevice(deviceId);
	// await device.discoverAllServicesAndCharacteristics();
	// Enable notifications on RR, Coherence, DeviceState characteristics

	if (deviceId.startsWith('MOCK')) {
		startMockStream();
		return true;
	}

	return false;
}

/**
 * Disconnect from current device
 */
export async function disconnect(): Promise<void> {
	console.log('[BLE] Disconnecting...');

	if (connectedDeviceId?.startsWith('MOCK')) {
		stopMockStream();
	}

	// TODO: Replace with actual BLE disconnect
	// await manager.cancelDeviceConnection(connectedDeviceId);
}

/**
 * Send actuator command to ring
 */
export async function sendActuatorCommand(cmd: ActuatorCommand): Promise<boolean> {
	if (!connectedDeviceId) {
		console.warn('[BLE] Not connected, cannot send actuator command');
		return false;
	}

	console.log('[BLE] Sending actuator command:', cmd);

	// Pack command into 4 bytes (little-endian)
	const data = new Uint8Array([
		Math.min(100, Math.max(0, cmd.thermalIntensity)),
		Math.min(255, Math.max(0, cmd.thermalDurationS)),
		cmd.vibrationPattern,
		Math.min(100, Math.max(0, cmd.vibrationIntensity)),
	]);

	// TODO: Replace with actual BLE write
	// await device.writeCharacteristicWithResponseForService(
	//   NLR_SERVICE_UUID,
	//   NLR_CHARACTERISTICS.ACTUATOR_CTRL,
	//   base64.encode(data)
	// );

	return true;
}

/**
 * Read current device configuration
 */
export async function readConfig(): Promise<RingConfig | null> {
	if (!connectedDeviceId) return null;

	// TODO: Replace with actual BLE read
	// const characteristic = await device.readCharacteristicForService(
	//   NLR_SERVICE_UUID,
	//   NLR_CHARACTERISTICS.CONFIG
	// );
	// Parse 16 bytes into RingConfig

	// Return mock config
	return {
		streamingRateHz: 4,
		coherenceUpdateS: 15,
		thermalMaxPct: 80,
		vibrationMaxPct: 100,
		quietHoursStart: 22,
		quietHoursEnd: 7,
		ledBrightness: 50,
	};
}

/**
 * Write device configuration
 */
export async function writeConfig(config: RingConfig): Promise<boolean> {
	if (!connectedDeviceId) return false;

	console.log('[BLE] Writing config:', config);

	// Pack config into 16 bytes
	const data = new Uint8Array(16);
	data[0] = Math.min(10, Math.max(1, config.streamingRateHz));
	data[1] = Math.min(60, Math.max(5, config.coherenceUpdateS));
	data[2] = Math.min(100, Math.max(0, config.thermalMaxPct));
	data[3] = Math.min(100, Math.max(0, config.vibrationMaxPct));
	data[4] = Math.min(23, Math.max(0, config.quietHoursStart));
	data[5] = Math.min(23, Math.max(0, config.quietHoursEnd));
	data[6] = Math.min(100, Math.max(0, config.ledBrightness));
	// bytes 7-15 reserved

	// TODO: Replace with actual BLE write

	return true;
}

/**
 * Check if currently connected
 */
export function isConnected(): boolean {
	return connectedDeviceId !== null;
}

/**
 * Get connected device ID
 */
export function getConnectedDeviceId(): string | null {
	return connectedDeviceId;
}

/*******************************************************************************
 * DATA PARSING UTILITIES
 ******************************************************************************/

/**
 * Parse RR interval notification (array of uint16_t little-endian)
 */
export function parseRRNotification(data: Uint8Array): number[] {
	const intervals: number[] = [];
	for (let i = 0; i + 1 < data.length; i += 2) {
		const rr = data[i] | (data[i + 1] << 8);
		if (rr >= 200 && rr <= 2500) {
			// Physiological range
			intervals.push(rr);
		}
	}
	return intervals;
}

/**
 * Parse coherence notification (12 bytes)
 */
export function parseCoherenceNotification(data: Uint8Array): CoherencePacket | null {
	if (data.length < 12) return null;

	return {
		stressLevel: data[0],
		coherencePct: data[1],
		confidencePct: data[2],
		variabilityLevel: data[3],
		meanRrMs: data[4] | (data[5] << 8),
		rmssdMs: data[6] | (data[7] << 8),
		respiratoryRateCpm: data[8] | (data[9] << 8),
	};
}

/**
 * Parse device state notification (8 bytes)
 */
export function parseDeviceStateNotification(data: Uint8Array): DeviceState | null {
	if (data.length < 8) return null;

	const chargingStates = ['not_charging', 'charging', 'full'] as const;
	const connectionStates = ['idle', 'advertising', 'connected'] as const;

	return {
		batteryPct: data[0],
		chargingState: chargingStates[Math.min(data[1], 2)],
		connectionState: connectionStates[Math.min(data[2], 2)],
		streamingRR: (data[3] & 0x01) !== 0,
		streamingCoherence: (data[3] & 0x02) !== 0,
		skinTempC: data[4] > 127 ? data[4] - 256 : data[4], // signed int8
		errorFlags: data[5],
		uptimeMin: data[6] | (data[7] << 8),
	};
}

/*******************************************************************************
 * EXPORTED SERVICE OBJECT
 ******************************************************************************/

export const bleService = {
	// UUIDs
	SERVICE_UUID: NLR_SERVICE_UUID,
	CHARACTERISTICS: NLR_CHARACTERISTICS,

	// Connection
	scanForDevices,
	connectToDevice,
	disconnect,
	isConnected,
	getConnectedDeviceId,

	// Data subscriptions
	subscribeToRR,
	subscribeToCoherence,
	subscribeToDeviceState,
	subscribeToConnection,

	// Commands
	sendActuatorCommand,
	readConfig,
	writeConfig,

	// Mock for development
	startMockStream,
	stopMockStream,

	// Native bridge (call from native BLE callbacks)
	emitRR,
	emitCoherence,
	emitDeviceState,
	emitConnection,

	// Parsing utilities
	parseRRNotification,
	parseCoherenceNotification,
	parseDeviceStateNotification,
};
