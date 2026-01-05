/**
 * @file feature_config.h
 * @brief Neural Load Ring - Firmware Feature Configuration
 *
 * Compile-time feature flags and hardware configuration.
 * Modify these to enable/disable features for different builds.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef FEATURE_CONFIG_H
#define FEATURE_CONFIG_H

/*******************************************************************************
 * FEATURE ENABLES
 ******************************************************************************/

/** Enable wellness feedback system (HRV-to-haptic) */
#define ENABLE_WELLNESS_FEEDBACK        1

/** Enable thermal (heating) actuator */
#define ENABLE_THERMAL_FEATURE          1

/** Enable vibration (LRA/ERM motor) actuator */
#define ENABLE_VIBRATION_FEATURE        1

/** Enable signature feel pattern system */
#define ENABLE_SIGNATURE_FEEL           1

/** Enable autonomous cue processor (offline mode) */
#define ENABLE_CUE_PROCESSOR            1

/** Enable BLE communication */
#define ENABLE_BLE                      1

/** Enable PPG sensor (MAX30102) */
#define ENABLE_PPG_SENSOR               1

/** Enable temperature sensor (TMP117) */
#define ENABLE_TEMP_SENSOR              1

/** Enable accelerometer (LIS2DH12) */
#define ENABLE_ACCELEROMETER            1

/** Enable battery monitoring */
#define ENABLE_BATTERY_MONITOR          1

/** Enable low-power modes */
#define ENABLE_LOW_POWER_MODE           1

/** Enable debug UART logging */
#define ENABLE_DEBUG_UART               1

/** Enable over-the-air (OTA) firmware updates */
#define ENABLE_OTA_UPDATES              1

/*******************************************************************************
 * HARDWARE CONFIGURATION
 ******************************************************************************/

/** Target board */
#define NLR_BOARD_V1                    1

/** nRF52840 settings */
#define NRF52840_XXAA                   1
#define NRF_CLOCK_LFXO_PRESENT          1  /**< Has 32.768 kHz crystal */
#define NRF_CLOCK_HFXO_PRESENT          1  /**< Has 32 MHz crystal */

/*******************************************************************************
 * ACTUATOR SAFETY LIMITS
 ******************************************************************************/

/** Maximum thermal intensity (burn prevention) */
#define THERMAL_MAX_INTENSITY_PCT       80

/** Maximum thermal duration (seconds) */
#define THERMAL_MAX_DURATION_S          60

/** Maximum vibration intensity */
#define VIBRATION_MAX_INTENSITY_PCT     100

/** Maximum skin temperature (°C) */
#define THERMAL_MAX_SKIN_TEMP_C         42

/*******************************************************************************
 * POWER MANAGEMENT
 ******************************************************************************/

/** Battery low threshold (mV) */
#define BATTERY_LOW_MV                  3300

/** Battery critical threshold (mV) */
#define BATTERY_CRITICAL_MV             3000

/** Idle timeout before sleep (seconds) */
#define IDLE_TIMEOUT_S                  300

/** Deep sleep timeout when disconnected (seconds) */
#define DEEP_SLEEP_TIMEOUT_S            3600

/*******************************************************************************
 * SENSOR CONFIGURATION
 ******************************************************************************/

/** PPG sampling rate (Hz) */
#define PPG_SAMPLE_RATE_HZ              100

/** PPG LED current (mA) - range 0-51 mA */
#define PPG_LED_CURRENT_MA              25

/** Temperature sampling interval (seconds) */
#define TEMP_SAMPLE_INTERVAL_S          10

/** Accelerometer sampling rate (Hz) */
#define ACCEL_SAMPLE_RATE_HZ            25

/*******************************************************************************
 * BLE CONFIGURATION
 ******************************************************************************/

/** Device name for BLE advertising */
#define BLE_DEVICE_NAME                 "NeuralLoadRing"

/** BLE connection interval min (ms) */
#define BLE_CONN_INTERVAL_MIN_MS        20

/** BLE connection interval max (ms) */
#define BLE_CONN_INTERVAL_MAX_MS        40

/** BLE supervision timeout (ms) */
#define BLE_SUPERVISION_TIMEOUT_MS      4000

/*******************************************************************************
 * MEMORY CONFIGURATION
 ******************************************************************************/

/** RR interval buffer size (samples) */
#define RR_BUFFER_SIZE                  256

/** HRV analysis window (samples) */
#define HRV_WINDOW_SIZE                 120

/** Event log size (entries) */
#define EVENT_LOG_SIZE                  64

/*******************************************************************************
 * DEBUG OPTIONS
 ******************************************************************************/

/** Enable verbose logging */
#define DEBUG_VERBOSE                   0

/** Enable profiling timers */
#define DEBUG_PROFILING                 0

/** Enable assert checks (disable for production) */
#define DEBUG_ASSERT_ENABLED            1

/** Enable watchdog timer */
#define ENABLE_WATCHDOG                 1

/*******************************************************************************
 * VERSION INFO
 ******************************************************************************/

#define FIRMWARE_VERSION_MAJOR          1
#define FIRMWARE_VERSION_MINOR          0
#define FIRMWARE_VERSION_PATCH          0
#define FIRMWARE_BUILD_NUMBER           1

#endif /* FEATURE_CONFIG_H */