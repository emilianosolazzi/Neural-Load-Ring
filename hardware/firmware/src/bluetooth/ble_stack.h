/**
 * @file ble_stack.h
 * @brief Neural Load Ring BLE Stack - Nordic SoftDevice S140 Integration
 *
 * Custom Wellness GATT Service for:
 *   - RR interval streaming (ring → phone)
 *   - Coherence/stress metrics (ring → phone)
 *   - Actuator control commands (phone → ring)
 *   - Device state reporting (battery, connection)
 *
 * Hardware: nRF52833 with SoftDevice S140 v7.x
 * 
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef BLE_STACK_H
#define BLE_STACK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * CUSTOM SERVICE UUIDs
 * Base UUID: 6E4C0000-B5A3-F393-E0A9-E50E24DCCA9E (NLR = 0x4E4C52)
 ******************************************************************************/

/** Base UUID for Neural Load Ring services */
#define NLR_UUID_BASE \
    { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
      0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x4C, 0x6E }

/** Wellness Service UUID: 6E4C0001-xxxx */
#define NLR_UUID_WELLNESS_SERVICE       0x0001

/** Characteristic UUIDs */
#define NLR_UUID_CHAR_RR_INTERVAL       0x0002  /**< RR intervals (notify, 2 bytes each) */
#define NLR_UUID_CHAR_COHERENCE         0x0003  /**< Coherence packet (notify) */
#define NLR_UUID_CHAR_ACTUATOR_CTRL     0x0004  /**< Actuator commands (write) */
#define NLR_UUID_CHAR_DEVICE_STATE      0x0005  /**< Battery, state (read/notify) */
#define NLR_UUID_CHAR_CONFIG            0x0006  /**< Configuration (read/write) */

/*******************************************************************************
 * ADVERTISING PARAMETERS
 ******************************************************************************/

#define NLR_ADV_INTERVAL_MIN_MS         100     /**< Fast advertising (100ms) */
#define NLR_ADV_INTERVAL_MAX_MS         200     /**< Slow down to 200ms */
#define NLR_ADV_TIMEOUT_S               180     /**< Stop advertising after 3 min */

/*******************************************************************************
 * CONNECTION PARAMETERS (optimized for HRV streaming)
 ******************************************************************************/

#define NLR_CONN_INTERVAL_MIN_MS        15      /**< 15ms = 66.67 Hz max throughput */
#define NLR_CONN_INTERVAL_MAX_MS        30      /**< 30ms for power saving */
#define NLR_CONN_SLAVE_LATENCY          0       /**< No latency for real-time data */
#define NLR_CONN_SUP_TIMEOUT_MS         4000    /**< 4s supervision timeout */

/*******************************************************************************
 * DATA STRUCTURES
 ******************************************************************************/

/** Coherence notification packet (12 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  stress_level;          /**< 0-100 awareness level */
    uint8_t  coherence_pct;         /**< 0-100 phase coherence */
    uint8_t  confidence_pct;        /**< 0-100 measurement confidence */
    uint8_t  variability_level;     /**< 0-100 micro-variability */
    uint16_t mean_rr_ms;            /**< Mean RR interval */
    uint16_t rmssd_ms;              /**< RMSSD (HRV metric) */
    uint16_t respiratory_rate_cpm;  /**< Breaths per minute × 10 */
    uint16_t reserved;              /**< Future use */
} nlr_coherence_packet_t;

/** Actuator control command (4 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t thermal_intensity;      /**< 0-100 thermal PWM */
    uint8_t thermal_duration_s;     /**< Duration in seconds (0 = off) */
    uint8_t vibration_pattern;      /**< Pattern ID (0 = off) */
    uint8_t vibration_intensity;    /**< 0-100 vibration strength */
} nlr_actuator_cmd_t;

/** Device state notification (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  battery_pct;           /**< Battery percentage 0-100 */
    uint8_t  charging_state;        /**< 0=not charging, 1=charging, 2=full */
    uint8_t  connection_state;      /**< 0=idle, 1=advertising, 2=connected */
    uint8_t  streaming_active;      /**< Bit flags: 0x01=RR, 0x02=coherence */
    int8_t   skin_temp_c;           /**< Skin temperature °C (signed) */
    uint8_t  error_flags;           /**< Error bit flags */
    uint16_t uptime_min;            /**< Uptime in minutes */
} nlr_device_state_t;

/** Configuration structure (16 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  streaming_rate_hz;     /**< RR notification rate (1-10 Hz) */
    uint8_t  coherence_update_s;    /**< Coherence update interval (5-60s) */
    uint8_t  thermal_max_pct;       /**< Maximum thermal intensity allowed */
    uint8_t  vibration_max_pct;     /**< Maximum vibration intensity allowed */
    uint8_t  quiet_hours_start;     /**< Quiet hours start (0-23) */
    uint8_t  quiet_hours_end;       /**< Quiet hours end (0-23) */
    uint8_t  led_brightness;        /**< Status LED brightness 0-100 */
    uint8_t  reserved[9];           /**< Future configuration */
} nlr_config_t;

/** BLE event types for application callbacks */
typedef enum {
    NLR_BLE_EVT_CONNECTED,          /**< Central connected */
    NLR_BLE_EVT_DISCONNECTED,       /**< Central disconnected */
    NLR_BLE_EVT_ACTUATOR_CMD,       /**< Actuator command received */
    NLR_BLE_EVT_CONFIG_CHANGED,     /**< Configuration updated */
    NLR_BLE_EVT_NOTIFICATIONS_ENABLED,  /**< Client enabled notifications */
    NLR_BLE_EVT_NOTIFICATIONS_DISABLED, /**< Client disabled notifications */
    NLR_BLE_EVT_MTU_UPDATED,        /**< MTU size changed */
} nlr_ble_evt_type_t;

/** BLE event structure */
typedef struct {
    nlr_ble_evt_type_t type;
    union {
        struct {
            uint16_t conn_handle;
            uint8_t  peer_addr[6];
        } connected;
        struct {
            uint16_t conn_handle;
            uint8_t  reason;
        } disconnected;
        struct {
            nlr_actuator_cmd_t cmd;
        } actuator;
        struct {
            nlr_config_t config;
        } config;
        struct {
            uint16_t mtu;
        } mtu;
    } data;
} nlr_ble_evt_t;

/** Application event handler callback */
typedef void (*nlr_ble_evt_handler_t)(const nlr_ble_evt_t *p_evt);

/*******************************************************************************
 * PUBLIC API
 ******************************************************************************/

/**
 * @brief Initialize BLE stack (SoftDevice, GAP, GATT, Services)
 * 
 * Must be called once at startup before any other BLE functions.
 * Configures SoftDevice S140, sets up GAP parameters, and registers
 * the custom Wellness Service.
 *
 * @param[in] evt_handler  Application callback for BLE events (can be NULL)
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_init(nlr_ble_evt_handler_t evt_handler);

/**
 * @brief Start BLE advertising
 * 
 * Begins advertising with device name "NLR-XXXX" where XXXX is
 * derived from device ID. Uses fast advertising initially, then
 * slows down for power savings.
 *
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_advertising_start(void);

/**
 * @brief Stop BLE advertising
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_advertising_stop(void);

/**
 * @brief Disconnect current connection gracefully
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_disconnect(void);

/**
 * @brief Send RR intervals via notification
 * 
 * Queues RR interval data for transmission. Data is batched
 * for efficiency (up to MTU size - 3 bytes).
 *
 * @param[in] rr_ms     Array of RR intervals in milliseconds
 * @param[in] count     Number of intervals (1-10)
 * @return 0 on success, -ENOCONN if not connected, -EBUSY if queue full
 */
int nlr_ble_send_rr(const uint16_t *rr_ms, uint8_t count);

/**
 * @brief Send coherence metrics via notification
 *
 * @param[in] p_coherence  Coherence packet to send
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_send_coherence(const nlr_coherence_packet_t *p_coherence);

/**
 * @brief Update device state characteristic
 *
 * @param[in] p_state  Device state to broadcast
 * @return 0 on success, negative error code on failure
 */
int nlr_ble_update_device_state(const nlr_device_state_t *p_state);

/**
 * @brief Get current configuration
 *
 * @param[out] p_config  Buffer to receive current config
 */
void nlr_ble_get_config(nlr_config_t *p_config);

/**
 * @brief Check if BLE is connected
 * @return true if connected to central
 */
bool nlr_ble_is_connected(void);

/**
 * @brief Get current connection handle
 * @return Connection handle or BLE_CONN_HANDLE_INVALID
 */
uint16_t nlr_ble_get_conn_handle(void);

/**
 * @brief Get negotiated MTU size
 * @return MTU size (23-247)
 */
uint16_t nlr_ble_get_mtu(void);

/**
 * @brief Process BLE events (call from main loop or scheduler)
 * 
 * Handles pending SoftDevice events. Must be called regularly
 * to maintain BLE connection and process notifications.
 */
void nlr_ble_process(void);

/*******************************************************************************
 * LEGACY COMPATIBILITY
 ******************************************************************************/

/** @deprecated Use nlr_ble_init() instead */
static inline void ble_init(void) { nlr_ble_init(NULL); }

#ifdef __cplusplus
}
#endif

#endif /* BLE_STACK_H */