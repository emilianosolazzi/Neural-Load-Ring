/**
 * @file ble_stack.c
 * @brief Neural Load Ring BLE Stack Implementation
 *
 * Full Nordic SoftDevice S140 integration with custom Wellness GATT service.
 * Handles advertising, connections, and bidirectional data streaming.
 *
 * Hardware: nRF52833 @ 64MHz, SoftDevice S140 v7.2.0
 * 
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "ble_stack.h"
#include <string.h>

/* Nordic SDK includes - these would come from nRF5 SDK */
#ifdef NRF_SDK_PRESENT
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#else
/* Stub definitions for compilation without SDK */
#define NRF_LOG_INFO(...)
#define NRF_LOG_WARNING(...)
#define NRF_LOG_ERROR(...)
#define APP_ERROR_CHECK(x) (void)(x)
#define BLE_CONN_HANDLE_INVALID 0xFFFF
#define BLE_GAP_EVT_CONNECTED 0x10
#define BLE_GAP_EVT_DISCONNECTED 0x11
#define BLE_GATTS_EVT_WRITE 0x50
#define BLE_GATTS_EVT_HVN_TX_COMPLETE 0x51
#define BLE_GATT_HVX_NOTIFICATION 0x01
#define NRF_SUCCESS 0
typedef uint32_t ret_code_t;
#endif

/*******************************************************************************
 * CONFIGURATION
 ******************************************************************************/

#define DEVICE_NAME                 "NLR"
#define MANUFACTURER_NAME           "NeuralLoadRing"

/** Advertising interval conversion (0.625ms units) */
#define MSEC_TO_UNITS(ms, unit)     (((ms) * 1000) / (unit))
#define ADV_INTERVAL_UNITS          625

/** Connection parameter units (1.25ms) */
#define CONN_INTERVAL_UNITS         1250

/** Maximum characteristics */
#define NLR_MAX_CHARACTERISTICS     5

/** TX queue depth for notifications */
#define NLR_TX_QUEUE_SIZE           8

/*******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/

/** Service handles */
typedef struct {
    uint16_t service_handle;
    uint16_t rr_interval_handle;
    uint16_t rr_interval_cccd;
    uint16_t coherence_handle;
    uint16_t coherence_cccd;
    uint16_t actuator_ctrl_handle;
    uint16_t device_state_handle;
    uint16_t device_state_cccd;
    uint16_t config_handle;
} nlr_service_handles_t;

/** Module state */
typedef struct {
    bool initialized;
    bool advertising;
    uint16_t conn_handle;
    uint16_t mtu_size;
    uint8_t uuid_type;
    nlr_service_handles_t handles;
    nlr_ble_evt_handler_t evt_handler;
    nlr_config_t config;
    nlr_device_state_t device_state;
    
    /* Notification state */
    bool rr_notifications_enabled;
    bool coherence_notifications_enabled;
    bool device_state_notifications_enabled;
    
    /* TX queue for flow control */
    uint8_t tx_queue_count;
} nlr_ble_state_t;

static nlr_ble_state_t m_state = {
    .initialized = false,
    .advertising = false,
    .conn_handle = 0xFFFF,  /* BLE_CONN_HANDLE_INVALID */
    .mtu_size = 23,         /* Default BLE 4.0 MTU */
    .config = {
        .streaming_rate_hz = 4,
        .coherence_update_s = 15,
        .thermal_max_pct = 80,
        .vibration_max_pct = 100,
        .quiet_hours_start = 22,
        .quiet_hours_end = 7,
        .led_brightness = 50,
    },
};

/*******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

static int softdevice_init(void);
static int gap_params_init(void);
static int gatt_init(void);
static int services_init(void);
static int advertising_init(void);
static int conn_params_init(void);
static void on_ble_evt(uint16_t evt_id, void *p_evt_data);
static void on_write_evt(uint16_t handle, const uint8_t *data, uint16_t len);
static void dispatch_event(nlr_ble_evt_type_t type, const void *data);

/*******************************************************************************
 * PUBLIC API IMPLEMENTATION
 ******************************************************************************/

int nlr_ble_init(nlr_ble_evt_handler_t evt_handler)
{
    if (m_state.initialized) {
        return 0; /* Already initialized */
    }
    
    m_state.evt_handler = evt_handler;
    m_state.conn_handle = 0xFFFF;
    
    int err;
    
    /* Initialize SoftDevice */
    err = softdevice_init();
    if (err != 0) {
        NRF_LOG_ERROR("SoftDevice init failed: %d", err);
        return err;
    }
    
    /* Configure GAP (device name, appearance, connection params) */
    err = gap_params_init();
    if (err != 0) {
        NRF_LOG_ERROR("GAP init failed: %d", err);
        return err;
    }
    
    /* Initialize GATT module */
    err = gatt_init();
    if (err != 0) {
        NRF_LOG_ERROR("GATT init failed: %d", err);
        return err;
    }
    
    /* Register Wellness Service */
    err = services_init();
    if (err != 0) {
        NRF_LOG_ERROR("Services init failed: %d", err);
        return err;
    }
    
    /* Configure advertising */
    err = advertising_init();
    if (err != 0) {
        NRF_LOG_ERROR("Advertising init failed: %d", err);
        return err;
    }
    
    /* Configure connection parameter negotiation */
    err = conn_params_init();
    if (err != 0) {
        NRF_LOG_ERROR("Conn params init failed: %d", err);
        return err;
    }
    
    m_state.initialized = true;
    NRF_LOG_INFO("BLE stack initialized successfully");
    
    return 0;
}

int nlr_ble_advertising_start(void)
{
    if (!m_state.initialized) {
        return -1;
    }
    
    if (m_state.advertising) {
        return 0; /* Already advertising */
    }
    
#ifdef NRF_SDK_PRESENT
    ret_code_t err = ble_advertising_start(BLE_ADV_MODE_FAST);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("Advertising start failed: %d", err);
        return -2;
    }
#endif
    
    m_state.advertising = true;
    m_state.device_state.connection_state = 1; /* Advertising */
    
    NRF_LOG_INFO("Advertising started");
    return 0;
}

int nlr_ble_advertising_stop(void)
{
    if (!m_state.advertising) {
        return 0;
    }
    
#ifdef NRF_SDK_PRESENT
    sd_ble_gap_adv_stop(m_adv_handle);
#endif
    
    m_state.advertising = false;
    m_state.device_state.connection_state = 0; /* Idle */
    
    NRF_LOG_INFO("Advertising stopped");
    return 0;
}

int nlr_ble_disconnect(void)
{
    if (m_state.conn_handle == 0xFFFF) {
        return 0; /* Not connected */
    }
    
#ifdef NRF_SDK_PRESENT
    ret_code_t err = sd_ble_gap_disconnect(m_state.conn_handle, 
                                            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("Disconnect failed: %d", err);
        return -1;
    }
#endif
    
    return 0;
}

int nlr_ble_send_rr(const uint16_t *rr_ms, uint8_t count)
{
    if (!m_state.initialized || m_state.conn_handle == 0xFFFF) {
        return -1; /* Not connected */
    }
    
    if (!m_state.rr_notifications_enabled) {
        return -2; /* Notifications not enabled */
    }
    
    if (count == 0 || count > 10 || rr_ms == NULL) {
        return -3; /* Invalid parameters */
    }
    
    /* Check TX queue space */
    if (m_state.tx_queue_count >= NLR_TX_QUEUE_SIZE) {
        return -4; /* Queue full */
    }
    
    /* Prepare notification data (array of uint16_t, little-endian) */
    uint8_t data[20]; /* Max 10 RR values × 2 bytes */
    uint16_t len = count * 2;
    
    /* Ensure we don't exceed MTU */
    uint16_t max_len = m_state.mtu_size - 3; /* ATT header overhead */
    if (len > max_len) {
        count = max_len / 2;
        len = count * 2;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        data[i * 2]     = (uint8_t)(rr_ms[i] & 0xFF);
        data[i * 2 + 1] = (uint8_t)((rr_ms[i] >> 8) & 0xFF);
    }
    
#ifdef NRF_SDK_PRESENT
    ble_gatts_hvx_params_t hvx_params = {
        .handle = m_state.handles.rr_interval_handle,
        .type   = BLE_GATT_HVX_NOTIFICATION,
        .offset = 0,
        .p_len  = &len,
        .p_data = data,
    };
    
    ret_code_t err = sd_ble_gatts_hvx(m_state.conn_handle, &hvx_params);
    if (err == NRF_SUCCESS) {
        m_state.tx_queue_count++;
        m_state.device_state.streaming_active |= 0x01;
        return 0;
    } else if (err == NRF_ERROR_RESOURCES) {
        return -4; /* Queue full */
    } else {
        NRF_LOG_WARNING("RR notification failed: %d", err);
        return -5;
    }
#else
    (void)data;
    (void)len;
    m_state.device_state.streaming_active |= 0x01;
    return 0;
#endif
}

int nlr_ble_send_coherence(const nlr_coherence_packet_t *p_coherence)
{
    if (!m_state.initialized || m_state.conn_handle == 0xFFFF) {
        return -1;
    }
    
    if (!m_state.coherence_notifications_enabled) {
        return -2;
    }
    
    if (p_coherence == NULL) {
        return -3;
    }
    
    if (m_state.tx_queue_count >= NLR_TX_QUEUE_SIZE) {
        return -4;
    }
    
#ifdef NRF_SDK_PRESENT
    uint16_t len = sizeof(nlr_coherence_packet_t);
    
    ble_gatts_hvx_params_t hvx_params = {
        .handle = m_state.handles.coherence_handle,
        .type   = BLE_GATT_HVX_NOTIFICATION,
        .offset = 0,
        .p_len  = &len,
        .p_data = (const uint8_t *)p_coherence,
    };
    
    ret_code_t err = sd_ble_gatts_hvx(m_state.conn_handle, &hvx_params);
    if (err == NRF_SUCCESS) {
        m_state.tx_queue_count++;
        m_state.device_state.streaming_active |= 0x02;
        return 0;
    }
    return -5;
#else
    m_state.device_state.streaming_active |= 0x02;
    return 0;
#endif
}

int nlr_ble_update_device_state(const nlr_device_state_t *p_state)
{
    if (!m_state.initialized) {
        return -1;
    }
    
    if (p_state != NULL) {
        /* Update local copy, preserving connection_state */
        uint8_t conn_state = m_state.device_state.connection_state;
        memcpy(&m_state.device_state, p_state, sizeof(nlr_device_state_t));
        m_state.device_state.connection_state = conn_state;
    }
    
    /* Update characteristic value */
#ifdef NRF_SDK_PRESENT
    ble_gatts_value_t val = {
        .len     = sizeof(nlr_device_state_t),
        .offset  = 0,
        .p_value = (uint8_t *)&m_state.device_state,
    };
    
    sd_ble_gatts_value_set(m_state.conn_handle, 
                           m_state.handles.device_state_handle, 
                           &val);
    
    /* Send notification if enabled and connected */
    if (m_state.device_state_notifications_enabled && 
        m_state.conn_handle != 0xFFFF &&
        m_state.tx_queue_count < NLR_TX_QUEUE_SIZE) {
        
        uint16_t len = sizeof(nlr_device_state_t);
        ble_gatts_hvx_params_t hvx_params = {
            .handle = m_state.handles.device_state_handle,
            .type   = BLE_GATT_HVX_NOTIFICATION,
            .offset = 0,
            .p_len  = &len,
            .p_data = (uint8_t *)&m_state.device_state,
        };
        
        if (sd_ble_gatts_hvx(m_state.conn_handle, &hvx_params) == NRF_SUCCESS) {
            m_state.tx_queue_count++;
        }
    }
#endif
    
    return 0;
}

void nlr_ble_get_config(nlr_config_t *p_config)
{
    if (p_config != NULL) {
        memcpy(p_config, &m_state.config, sizeof(nlr_config_t));
    }
}

bool nlr_ble_is_connected(void)
{
    return m_state.conn_handle != 0xFFFF;
}

uint16_t nlr_ble_get_conn_handle(void)
{
    return m_state.conn_handle;
}

uint16_t nlr_ble_get_mtu(void)
{
    return m_state.mtu_size;
}

void nlr_ble_process(void)
{
#ifdef NRF_SDK_PRESENT
    /* Process SoftDevice events */
    nrf_sdh_evts_poll();
#endif
}

/*******************************************************************************
 * PRIVATE FUNCTIONS - INITIALIZATION
 ******************************************************************************/

static int softdevice_init(void)
{
#ifdef NRF_SDK_PRESENT
    ret_code_t err;
    
    /* Enable SoftDevice */
    err = nrf_sdh_enable_request();
    if (err != NRF_SUCCESS) return -1;
    
    /* Configure BLE stack parameters */
    uint32_t ram_start = 0;
    err = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    if (err != NRF_SUCCESS) return -2;
    
    /* Enable BLE stack */
    err = nrf_sdh_ble_enable(&ram_start);
    if (err != NRF_SUCCESS) return -3;
    
    /* Register BLE event handler */
    NRF_SDH_BLE_OBSERVER(m_ble_observer, 3, on_ble_evt, NULL);
#endif
    
    NRF_LOG_INFO("SoftDevice S140 initialized");
    return 0;
}

static int gap_params_init(void)
{
#ifdef NRF_SDK_PRESENT
    ret_code_t err;
    ble_gap_conn_sec_mode_t sec_mode;
    
    /* No security (open link) */
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
    /* Generate device name with unique suffix from device address */
    char device_name[16];
    ble_gap_addr_t addr;
    sd_ble_gap_addr_get(&addr);
    snprintf(device_name, sizeof(device_name), "%s-%02X%02X", 
             DEVICE_NAME, addr.addr[1], addr.addr[0]);
    
    err = sd_ble_gap_device_name_set(&sec_mode, 
                                      (const uint8_t *)device_name, 
                                      strlen(device_name));
    if (err != NRF_SUCCESS) return -1;
    
    /* Set appearance: Generic Heart Rate Sensor */
    err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_HEART_RATE_SENSOR);
    if (err != NRF_SUCCESS) return -2;
    
    /* Set preferred connection parameters */
    ble_gap_conn_params_t conn_params = {
        .min_conn_interval = MSEC_TO_UNITS(NLR_CONN_INTERVAL_MIN_MS, CONN_INTERVAL_UNITS),
        .max_conn_interval = MSEC_TO_UNITS(NLR_CONN_INTERVAL_MAX_MS, CONN_INTERVAL_UNITS),
        .slave_latency     = NLR_CONN_SLAVE_LATENCY,
        .conn_sup_timeout  = MSEC_TO_UNITS(NLR_CONN_SUP_TIMEOUT_MS, 10000),
    };
    
    err = sd_ble_gap_ppcp_set(&conn_params);
    if (err != NRF_SUCCESS) return -3;
#endif
    
    NRF_LOG_INFO("GAP parameters configured");
    return 0;
}

static int gatt_init(void)
{
#ifdef NRF_SDK_PRESENT
    ret_code_t err;
    
    /* Initialize GATT module for MTU negotiation */
    err = nrf_ble_gatt_init(&m_gatt, NULL);
    if (err != NRF_SUCCESS) return -1;
    
    /* Request maximum MTU (247 bytes for BLE 5.0) */
    err = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, 247);
    if (err != NRF_SUCCESS) return -2;
#endif
    
    NRF_LOG_INFO("GATT module initialized");
    return 0;
}

static int services_init(void)
{
#ifdef NRF_SDK_PRESENT
    ret_code_t err;
    
    /* Add vendor-specific base UUID */
    ble_uuid128_t base_uuid = { NLR_UUID_BASE };
    err = sd_ble_uuid_vs_add(&base_uuid, &m_state.uuid_type);
    if (err != NRF_SUCCESS) return -1;
    
    /* Add Wellness Service */
    ble_uuid_t service_uuid = {
        .uuid = NLR_UUID_WELLNESS_SERVICE,
        .type = m_state.uuid_type,
    };
    
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, 
                                    &service_uuid, 
                                    &m_state.handles.service_handle);
    if (err != NRF_SUCCESS) return -2;
    
    /*
     * Add characteristics with proper permissions
     */
    
    /* 1. RR Interval Characteristic (Notify only) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_md_t cccd_md = {0};
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_t    attr = {0};
        ble_uuid_t          char_uuid = { .uuid = NLR_UUID_CHAR_RR_INTERVAL, .type = m_state.uuid_type };
        
        /* CCCD (Client Characteristic Configuration Descriptor) for notifications */
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_md.char_props.notify = 1;
        char_md.p_cccd_md = &cccd_md;
        
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 0;
        attr.max_len = 20; /* Up to 10 RR values */
        
        ble_gatts_char_handles_t handles;
        err = sd_ble_gatts_characteristic_add(m_state.handles.service_handle, 
                                               &char_md, &attr, &handles);
        if (err != NRF_SUCCESS) return -3;
        
        m_state.handles.rr_interval_handle = handles.value_handle;
        m_state.handles.rr_interval_cccd = handles.cccd_handle;
    }
    
    /* 2. Coherence Characteristic (Notify only) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_md_t cccd_md = {0};
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_t    attr = {0};
        ble_uuid_t          char_uuid = { .uuid = NLR_UUID_CHAR_COHERENCE, .type = m_state.uuid_type };
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_md.char_props.notify = 1;
        char_md.p_cccd_md = &cccd_md;
        
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 0;
        attr.max_len = sizeof(nlr_coherence_packet_t);
        
        ble_gatts_char_handles_t handles;
        err = sd_ble_gatts_characteristic_add(m_state.handles.service_handle, 
                                               &char_md, &attr, &handles);
        if (err != NRF_SUCCESS) return -4;
        
        m_state.handles.coherence_handle = handles.value_handle;
        m_state.handles.coherence_cccd = handles.cccd_handle;
    }
    
    /* 3. Actuator Control Characteristic (Write only) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_t    attr = {0};
        ble_uuid_t          char_uuid = { .uuid = NLR_UUID_CHAR_ACTUATOR_CTRL, .type = m_state.uuid_type };
        
        char_md.char_props.write = 1;
        char_md.char_props.write_wo_resp = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 0;
        attr.max_len = sizeof(nlr_actuator_cmd_t);
        
        ble_gatts_char_handles_t handles;
        err = sd_ble_gatts_characteristic_add(m_state.handles.service_handle, 
                                               &char_md, &attr, &handles);
        if (err != NRF_SUCCESS) return -5;
        
        m_state.handles.actuator_ctrl_handle = handles.value_handle;
    }
    
    /* 4. Device State Characteristic (Read + Notify) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_md_t cccd_md = {0};
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_t    attr = {0};
        ble_uuid_t          char_uuid = { .uuid = NLR_UUID_CHAR_DEVICE_STATE, .type = m_state.uuid_type };
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_md.char_props.read = 1;
        char_md.char_props.notify = 1;
        char_md.p_cccd_md = &cccd_md;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.p_value = (uint8_t *)&m_state.device_state;
        attr.init_len = sizeof(nlr_device_state_t);
        attr.max_len = sizeof(nlr_device_state_t);
        
        ble_gatts_char_handles_t handles;
        err = sd_ble_gatts_characteristic_add(m_state.handles.service_handle, 
                                               &char_md, &attr, &handles);
        if (err != NRF_SUCCESS) return -6;
        
        m_state.handles.device_state_handle = handles.value_handle;
        m_state.handles.device_state_cccd = handles.cccd_handle;
    }
    
    /* 5. Configuration Characteristic (Read + Write) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_t    attr = {0};
        ble_uuid_t          char_uuid = { .uuid = NLR_UUID_CHAR_CONFIG, .type = m_state.uuid_type };
        
        char_md.char_props.read = 1;
        char_md.char_props.write = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.p_value = (uint8_t *)&m_state.config;
        attr.init_len = sizeof(nlr_config_t);
        attr.max_len = sizeof(nlr_config_t);
        
        ble_gatts_char_handles_t handles;
        err = sd_ble_gatts_characteristic_add(m_state.handles.service_handle, 
                                               &char_md, &attr, &handles);
        if (err != NRF_SUCCESS) return -7;
        
        m_state.handles.config_handle = handles.value_handle;
    }
#endif
    
    NRF_LOG_INFO("Wellness Service registered with 5 characteristics");
    return 0;
}

static int advertising_init(void)
{
#ifdef NRF_SDK_PRESENT
    ret_code_t err;
    
    /* Build advertising data */
    ble_uuid_t adv_uuids[] = {
        { .uuid = NLR_UUID_WELLNESS_SERVICE, .type = m_state.uuid_type },
    };
    
    ble_advdata_t advdata = {
        .name_type = BLE_ADVDATA_FULL_NAME,
        .include_appearance = true,
        .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        .uuids_complete = { .uuid_cnt = 1, .p_uuids = adv_uuids },
    };
    
    ble_advdata_t srdata = {
        .name_type = BLE_ADVDATA_NO_NAME,
    };
    
    ble_adv_modes_config_t options = {
        .ble_adv_fast_enabled  = true,
        .ble_adv_fast_interval = MSEC_TO_UNITS(NLR_ADV_INTERVAL_MIN_MS, ADV_INTERVAL_UNITS),
        .ble_adv_fast_timeout  = NLR_ADV_TIMEOUT_S,
        .ble_adv_slow_enabled  = true,
        .ble_adv_slow_interval = MSEC_TO_UNITS(NLR_ADV_INTERVAL_MAX_MS, ADV_INTERVAL_UNITS),
        .ble_adv_slow_timeout  = 0, /* No timeout for slow advertising */
    };
    
    err = ble_advertising_init(&m_advertising, &advdata, &srdata, &options, NULL);
    if (err != NRF_SUCCESS) return -1;
    
    ble_advertising_conn_cfg_tag_set(&m_advertising, 1);
#endif
    
    NRF_LOG_INFO("Advertising configured");
    return 0;
}

static int conn_params_init(void)
{
#ifdef NRF_SDK_PRESENT
    ble_conn_params_init_t cp_init = {
        .p_conn_params                  = NULL,
        .first_conn_params_update_delay = APP_TIMER_TICKS(5000),
        .next_conn_params_update_delay  = APP_TIMER_TICKS(30000),
        .max_conn_params_update_count   = 3,
        .start_on_notify_cccd_handle    = m_state.handles.rr_interval_cccd,
        .disconnect_on_fail             = false,
        .evt_handler                    = NULL,
        .error_handler                  = NULL,
    };
    
    ret_code_t err = ble_conn_params_init(&cp_init);
    if (err != NRF_SUCCESS) return -1;
#endif
    
    NRF_LOG_INFO("Connection parameters module initialized");
    return 0;
}

/*******************************************************************************
 * PRIVATE FUNCTIONS - EVENT HANDLING
 ******************************************************************************/

#ifdef NRF_SDK_PRESENT
static void on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    (void)p_context;
    
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
        {
            m_state.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            m_state.advertising = false;
            m_state.device_state.connection_state = 2; /* Connected */
            m_state.tx_queue_count = 0;
            
            NRF_LOG_INFO("Connected: handle=%d", m_state.conn_handle);
            
            /* Notify application */
            nlr_ble_evt_t evt = {
                .type = NLR_BLE_EVT_CONNECTED,
                .data.connected.conn_handle = m_state.conn_handle,
            };
            memcpy(evt.data.connected.peer_addr, 
                   p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr, 6);
            dispatch_event(NLR_BLE_EVT_CONNECTED, &evt);
            break;
        }
        
        case BLE_GAP_EVT_DISCONNECTED:
        {
            uint16_t old_handle = m_state.conn_handle;
            uint8_t reason = p_ble_evt->evt.gap_evt.params.disconnected.reason;
            
            m_state.conn_handle = BLE_CONN_HANDLE_INVALID;
            m_state.rr_notifications_enabled = false;
            m_state.coherence_notifications_enabled = false;
            m_state.device_state_notifications_enabled = false;
            m_state.device_state.streaming_active = 0;
            m_state.device_state.connection_state = 0;
            
            NRF_LOG_INFO("Disconnected: handle=%d, reason=0x%02X", old_handle, reason);
            
            /* Notify application */
            nlr_ble_evt_t evt = {
                .type = NLR_BLE_EVT_DISCONNECTED,
                .data.disconnected.conn_handle = old_handle,
                .data.disconnected.reason = reason,
            };
            dispatch_event(NLR_BLE_EVT_DISCONNECTED, &evt);
            
            /* Auto-restart advertising */
            nlr_ble_advertising_start();
            break;
        }
        
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            /* Accept PHY update request (prefer 2M PHY for throughput) */
            ble_gap_phys_t phys = { .rx_phys = BLE_GAP_PHY_AUTO, .tx_phys = BLE_GAP_PHY_AUTO };
            sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            break;
        }
        
        case BLE_GATTC_EVT_TIMEOUT:
        case BLE_GATTS_EVT_TIMEOUT:
        {
            /* GATT timeout - disconnect */
            sd_ble_gap_disconnect(m_state.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            break;
        }
        
        case BLE_GATTS_EVT_WRITE:
        {
            ble_gatts_evt_write_t const *p_write = &p_ble_evt->evt.gatts_evt.params.write;
            on_write_evt(p_write->handle, p_write->data, p_write->len);
            break;
        }
        
        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
        {
            /* TX complete - decrement queue count */
            uint8_t count = p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count;
            if (m_state.tx_queue_count >= count) {
                m_state.tx_queue_count -= count;
            } else {
                m_state.tx_queue_count = 0;
            }
            break;
        }
        
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
        {
            /* Respond with our max MTU */
            sd_ble_gatts_exchange_mtu_reply(m_state.conn_handle, 247);
            break;
        }
        
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        {
            /* Accept data length updates */
            break;
        }
        
        default:
            break;
    }
}
#else
static void on_ble_evt(uint16_t evt_id, void *p_evt_data)
{
    (void)evt_id;
    (void)p_evt_data;
}
#endif

static void on_write_evt(uint16_t handle, const uint8_t *data, uint16_t len)
{
    /* Handle CCCD writes (notification enable/disable) */
    if (len == 2) {
        uint16_t cccd_value = (data[1] << 8) | data[0];
        bool notifications_enabled = (cccd_value & 0x0001) != 0;
        
        if (handle == m_state.handles.rr_interval_cccd) {
            m_state.rr_notifications_enabled = notifications_enabled;
            NRF_LOG_INFO("RR notifications %s", notifications_enabled ? "enabled" : "disabled");
        } else if (handle == m_state.handles.coherence_cccd) {
            m_state.coherence_notifications_enabled = notifications_enabled;
            NRF_LOG_INFO("Coherence notifications %s", notifications_enabled ? "enabled" : "disabled");
        } else if (handle == m_state.handles.device_state_cccd) {
            m_state.device_state_notifications_enabled = notifications_enabled;
            NRF_LOG_INFO("Device state notifications %s", notifications_enabled ? "enabled" : "disabled");
        }
        
        /* Notify application */
        dispatch_event(notifications_enabled ? 
                       NLR_BLE_EVT_NOTIFICATIONS_ENABLED : 
                       NLR_BLE_EVT_NOTIFICATIONS_DISABLED, NULL);
        return;
    }
    
    /* Handle Actuator Control writes */
    if (handle == m_state.handles.actuator_ctrl_handle && len == sizeof(nlr_actuator_cmd_t)) {
        nlr_actuator_cmd_t cmd;
        memcpy(&cmd, data, sizeof(nlr_actuator_cmd_t));
        
        /* Clamp to configured maximums */
        if (cmd.thermal_intensity > m_state.config.thermal_max_pct) {
            cmd.thermal_intensity = m_state.config.thermal_max_pct;
        }
        if (cmd.vibration_intensity > m_state.config.vibration_max_pct) {
            cmd.vibration_intensity = m_state.config.vibration_max_pct;
        }
        
        NRF_LOG_INFO("Actuator cmd: thermal=%d%% %ds, vib=%d pat=%d",
                     cmd.thermal_intensity, cmd.thermal_duration_s,
                     cmd.vibration_intensity, cmd.vibration_pattern);
        
        /* Notify application */
        nlr_ble_evt_t evt = {
            .type = NLR_BLE_EVT_ACTUATOR_CMD,
            .data.actuator.cmd = cmd,
        };
        dispatch_event(NLR_BLE_EVT_ACTUATOR_CMD, &evt);
        return;
    }
    
    /* Handle Configuration writes */
    if (handle == m_state.handles.config_handle && len == sizeof(nlr_config_t)) {
        nlr_config_t new_config;
        memcpy(&new_config, data, sizeof(nlr_config_t));
        
        /* Validate and clamp */
        if (new_config.streaming_rate_hz < 1) new_config.streaming_rate_hz = 1;
        if (new_config.streaming_rate_hz > 10) new_config.streaming_rate_hz = 10;
        if (new_config.coherence_update_s < 5) new_config.coherence_update_s = 5;
        if (new_config.coherence_update_s > 60) new_config.coherence_update_s = 60;
        if (new_config.thermal_max_pct > 100) new_config.thermal_max_pct = 100;
        if (new_config.vibration_max_pct > 100) new_config.vibration_max_pct = 100;
        if (new_config.quiet_hours_start > 23) new_config.quiet_hours_start = 23;
        if (new_config.quiet_hours_end > 23) new_config.quiet_hours_end = 23;
        if (new_config.led_brightness > 100) new_config.led_brightness = 100;
        
        memcpy(&m_state.config, &new_config, sizeof(nlr_config_t));
        
        NRF_LOG_INFO("Config updated: rate=%dHz, coherence=%ds",
                     new_config.streaming_rate_hz, new_config.coherence_update_s);
        
        /* Notify application */
        nlr_ble_evt_t evt = {
            .type = NLR_BLE_EVT_CONFIG_CHANGED,
            .data.config.config = new_config,
        };
        dispatch_event(NLR_BLE_EVT_CONFIG_CHANGED, &evt);
    }
}

static void dispatch_event(nlr_ble_evt_type_t type, const void *data)
{
    if (m_state.evt_handler != NULL) {
        if (data != NULL) {
            m_state.evt_handler((const nlr_ble_evt_t *)data);
        } else {
            nlr_ble_evt_t evt = { .type = type };
            m_state.evt_handler(&evt);
        }
    }
}