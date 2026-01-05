/**
 * @file bleService.ts
 * @brief Neural Load Ring BLE Service - React Native Integration
 *
 * Handles Bluetooth Low Energy communication with the NLR ring device.
 * Supports RR interval streaming, coherence notifications, and actuator control.
 *
 * Dependencies: react-native-ble-plx
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

import { BleManager, Device, Subscription, State } from 'react-native-ble-plx';
import { Platform, PermissionsAndroid } from 'react-native';
import { Buffer } from 'buffer';

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
	firmwareVersion?: string;
	batteryLevel?: number;
	autonomousMode?: boolean;
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

// BLE manager and device state
let manager: BleManager | null = null;
let device: Device | null = null;
let rrSubscription: Subscription | null = null;
let coherenceSubscription: Subscription | null = null;
let stateSubscription: Subscription | null = null;
let disconnectSubscription: Subscription | null = null;

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
 * BLE INITIALIZATION AND PERMISSIONS
 ******************************************************************************/

/**
 * Initialize the BLE manager
 * @returns Promise resolving to true when BLE is ready
 */
export async function initializeBle(): Promise<boolean> {
	if (manager) return true;

	manager = new BleManager();

	return new Promise((resolve) => {
		const subscription = manager!.onStateChange((state) => {
			console.log('[BLE] State changed:', state);
			if (state === State.PoweredOn) {
				subscription.remove();
				resolve(true);
			} else if (state === State.PoweredOff || state === State.Unauthorized) {
				subscription.remove();
				resolve(false);
			}
		}, true);

		// Timeout after 10 seconds
		setTimeout(() => {
			subscription.remove();
			resolve(manager !== null);
		}, 10000);
	});
}

/**
 * Request BLE permissions (required for Android)
 * @returns Promise resolving to true if permissions granted
 */
export async function requestBlePermissions(): Promise<boolean> {
	if (Platform.OS === 'ios') {
		// iOS handles permissions automatically via Info.plist
		return true;
	}

	try {
		if (typeof Platform.Version === 'number' && Platform.Version >= 31) {
			// Android 12+ requires BLUETOOTH_SCAN and BLUETOOTH_CONNECT
			const result = await PermissionsAndroid.requestMultiple([
				PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
				PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
			]);
			const scanGranted = result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN] === 'granted';
			const connectGranted = result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT] === 'granted';
			console.log('[BLE] Android 12+ permissions:', { scanGranted, connectGranted });
			return scanGranted && connectGranted;
		} else {
			// Android 11 and below requires ACCESS_FINE_LOCATION
			const result = await PermissionsAndroid.request(
				PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
				{
					title: 'Location Permission',
					message: 'Neural Load Ring needs location access to scan for BLE devices.',
					buttonPositive: 'OK',
				}
			);
			console.log('[BLE] Android location permission:', result);
			return result === 'granted';
		}
	} catch (error) {
		console.error('[BLE] Permission request failed:', error);
		return false;
	}
}

/**
 * Destroy BLE manager (cleanup)
 */
export function destroyBle(): void {
	if (manager) {
		manager.destroy();
		manager = null;
	}
}

/*******************************************************************************
 * REAL BLE IMPLEMENTATION
 ******************************************************************************/

/**
 * Scan for NLR devices
 * @param timeoutMs Scan duration
 * @returns Promise resolving to array of discovered devices
 */
export async function scanForDevices(timeoutMs = 10000): Promise<Array<{ id: string; name: string; rssi: number }>> {
	if (isScanning) return [];

	// Request permissions first
	const hasPermission = await requestBlePermissions();
	if (!hasPermission) {
		console.warn('[BLE] Permissions not granted');
		return [];
	}

	// Initialize BLE manager
	const bleReady = await initializeBle();
	if (!bleReady || !manager) {
		console.warn('[BLE] BLE not available');
		// Return mock device for development
		return [{ id: 'MOCK-NLR-0000', name: 'NLR-0000 (Mock)', rssi: -45 }];
	}

	isScanning = true;
	const devices: Array<{ id: string; name: string; rssi: number }> = [];
	const seenIds = new Set<string>();

	console.log(`[BLE] Scanning for ${timeoutMs}ms...`);

	return new Promise((resolve) => {
		const timeout = setTimeout(() => {
			manager!.stopDeviceScan();
			isScanning = false;
			console.log(`[BLE] Scan complete. Found ${devices.length} devices.`);
			// Always include mock device for development
			if (devices.length === 0) {
				devices.push({ id: 'MOCK-NLR-0000', name: 'NLR-0000 (Mock)', rssi: -45 });
			}
			resolve(devices);
		}, timeoutMs);

		manager!.startDeviceScan(
			[NLR_SERVICE_UUID],
			{ allowDuplicates: false },
			(error, scannedDevice) => {
				if (error) {
					console.warn('[BLE] Scan error:', error.message);
					return;
				}

				if (scannedDevice && scannedDevice.name?.startsWith('NLR-')) {
					if (!seenIds.has(scannedDevice.id)) {
						seenIds.add(scannedDevice.id);
						console.log(`[BLE] Found device: ${scannedDevice.name} (${scannedDevice.id})`);
						devices.push({
							id: scannedDevice.id,
							name: scannedDevice.name,
							rssi: scannedDevice.rssi ?? -100,
						});
					}
				}
			}
		);
	});
}

/**
 * Enable BLE notifications for all characteristics
 */
async function enableNotifications(): Promise<void> {
	if (!device) return;

	console.log('[BLE] Enabling notifications...');

	// RR Interval notifications
	rrSubscription = device.monitorCharacteristicForService(
		NLR_SERVICE_UUID,
		NLR_CHARACTERISTICS.RR_INTERVAL,
		(error, characteristic) => {
			if (error) {
				console.warn('[BLE] RR notification error:', error.message);
				return;
			}
			if (!characteristic?.value) return;

			try {
				const data = Buffer.from(characteristic.value, 'base64');
				const intervals = parseRRNotification(new Uint8Array(data));
				intervals.forEach((rr) => emitRR(rr));
			} catch (e) {
				console.warn('[BLE] RR parse error:', e);
			}
		}
	);

	// Coherence notifications
	coherenceSubscription = device.monitorCharacteristicForService(
		NLR_SERVICE_UUID,
		NLR_CHARACTERISTICS.COHERENCE,
		(error, characteristic) => {
			if (error) {
				console.warn('[BLE] Coherence notification error:', error.message);
				return;
			}
			if (!characteristic?.value) return;

			try {
				const data = Buffer.from(characteristic.value, 'base64');
				const packet = parseCoherenceNotification(new Uint8Array(data));
				if (packet) emitCoherence(packet);
			} catch (e) {
				console.warn('[BLE] Coherence parse error:', e);
			}
		}
	);

	// Device state notifications
	stateSubscription = device.monitorCharacteristicForService(
		NLR_SERVICE_UUID,
		NLR_CHARACTERISTICS.DEVICE_STATE,
		(error, characteristic) => {
			if (error) {
				console.warn('[BLE] State notification error:', error.message);
				return;
			}
			if (!characteristic?.value) return;

			try {
				const data = Buffer.from(characteristic.value, 'base64');
				const state = parseDeviceStateNotification(new Uint8Array(data));
				if (state) emitDeviceState(state);
			} catch (e) {
				console.warn('[BLE] State parse error:', e);
			}
		}
	);

	console.log('[BLE] Notifications enabled');
}

/**
 * Remove all BLE subscriptions
 */
function removeSubscriptions(): void {
	rrSubscription?.remove();
	coherenceSubscription?.remove();
	stateSubscription?.remove();
	disconnectSubscription?.remove();
	rrSubscription = null;
	coherenceSubscription = null;
	stateSubscription = null;
	disconnectSubscription = null;
}

/**
 * Connect to a specific NLR device
 * @param deviceId Device identifier from scan
 */
export async function connectToDevice(deviceId: string): Promise<boolean> {
	console.log(`[BLE] Connecting to ${deviceId}...`);

	// Handle mock device for development
	if (deviceId.startsWith('MOCK')) {
		startMockStream();
		return true;
	}

	// Initialize BLE if needed
	if (!manager) {
		const ready = await initializeBle();
		if (!ready) {
			console.error('[BLE] Failed to initialize BLE');
			return false;
		}
	}

	try {
		// Connect to device with MTU negotiation
		device = await manager!.connectToDevice(deviceId, {
			requestMTU: 247,
			timeout: 10000,
		});

		console.log('[BLE] Connected, discovering services...');
		await device.discoverAllServicesAndCharacteristics();

		// Set up disconnect listener
		disconnectSubscription = manager!.onDeviceDisconnected(deviceId, (error, disconnectedDevice) => {
			console.log('[BLE] Device disconnected:', disconnectedDevice?.id, error?.message);
			removeSubscriptions();
			device = null;
			connectedDeviceId = null;
			emitConnection(false);
		});

		// Enable notifications for all characteristics
		await enableNotifications();

		connectedDeviceId = deviceId;
		emitConnection(true, deviceId);

		console.log('[BLE] Connection complete');
		return true;
	} catch (error: any) {
		console.error('[BLE] Connection failed:', error?.message || error);
		device = null;
		return false;
	}
}

/**
 * Disconnect from current device
 */
export async function disconnect(): Promise<void> {
	console.log('[BLE] Disconnecting...');

	// Handle mock device
	if (connectedDeviceId?.startsWith('MOCK')) {
		stopMockStream();
		return;
	}

	// Remove subscriptions first
	removeSubscriptions();

	// Disconnect from real device
	if (device && manager) {
		try {
			await manager.cancelDeviceConnection(device.id);
		} catch (error: any) {
			// Device may already be disconnected
			console.warn('[BLE] Disconnect warning:', error?.message);
		}
	}

	device = null;
	connectedDeviceId = null;
	emitConnection(false);
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

	// Pack command into 4 bytes
	const data = new Uint8Array([
		Math.min(100, Math.max(0, cmd.thermalIntensity)),
		Math.min(255, Math.max(0, cmd.thermalDurationS)),
		cmd.vibrationPattern,
		Math.min(100, Math.max(0, cmd.vibrationIntensity)),
	]);

	// Mock device - just log
	if (connectedDeviceId.startsWith('MOCK')) {
		console.log('[BLE] Mock actuator command sent');
		return true;
	}

	// Real BLE write
	if (!device) {
		console.warn('[BLE] Device not available');
		return false;
	}

	try {
		await device.writeCharacteristicWithResponseForService(
			NLR_SERVICE_UUID,
			NLR_CHARACTERISTICS.ACTUATOR_CTRL,
			Buffer.from(data).toString('base64')
		);
		console.log('[BLE] Actuator command sent successfully');
		return true;
	} catch (error: any) {
		console.error('[BLE] Actuator write failed:', error?.message || error);
		return false;
	}
}

/**
 * Read current device configuration
 */
export async function readConfig(): Promise<RingConfig | null> {
	if (!connectedDeviceId) return null;

	// Mock device - return default config
	if (connectedDeviceId.startsWith('MOCK') || !device) {
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

	try {
		const characteristic = await device.readCharacteristicForService(
			NLR_SERVICE_UUID,
			NLR_CHARACTERISTICS.CONFIG
		);

		if (!characteristic?.value) {
			console.warn('[BLE] No config data received');
			return null;
		}

		const data = new Uint8Array(Buffer.from(characteristic.value, 'base64'));
		console.log('[BLE] Config read:', data);

		return {
			streamingRateHz: data[0] || 4,
			coherenceUpdateS: data[1] || 15,
			thermalMaxPct: data[2] || 80,
			vibrationMaxPct: data[3] || 100,
			quietHoursStart: data[4] || 22,
			quietHoursEnd: data[5] || 7,
			ledBrightness: data[6] || 50,
		};
	} catch (error: any) {
		console.error('[BLE] Config read failed:', error?.message || error);
		return null;
	}
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

	// Mock device - just log
	if (connectedDeviceId.startsWith('MOCK') || !device) {
		console.log('[BLE] Mock config write:', config);
		return true;
	}

	try {
		await device.writeCharacteristicWithResponseForService(
			NLR_SERVICE_UUID,
			NLR_CHARACTERISTICS.CONFIG,
			Buffer.from(data).toString('base64')
		);
		console.log('[BLE] Config written successfully');
		return true;
	} catch (error: any) {
		console.error('[BLE] Config write failed:', error?.message || error);
		return false;
	}
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

	// Initialization
	initializeBle,
	requestBlePermissions,
	destroyBle,

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
