/**
 * @file sdk_config.h
 * @brief Nordic SDK Configuration for Neural Load Ring
 *
 * This file configures the Nordic nRF5 SDK modules.
 * Generated base config should be customized for NLR requirements.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef SDK_CONFIG_H
#define SDK_CONFIG_H

/*******************************************************************************
 * SDK VERSION
 ******************************************************************************/

#define NRF_SDK_VERSION                 "17.1.0"

/*******************************************************************************
 * CLOCK CONFIGURATION
 ******************************************************************************/

#define NRF_CLOCK_ENABLED               1
#define CLOCK_CONFIG_LF_SRC             1  /**< XTAL */
#define CLOCK_CONFIG_LF_CAL_ENABLED     1

/*******************************************************************************
 * POWER MANAGEMENT
 ******************************************************************************/

#define NRF_PWR_MGMT_ENABLED            1
#define NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED 1
#define NRF_PWR_MGMT_CONFIG_AUTO_SHUTDOWN_RETRY 1

/*******************************************************************************
 * BLE STACK CONFIGURATION
 ******************************************************************************/

#define NRF_BLE_GATT_ENABLED            1
#define NRF_BLE_QWR_ENABLED             1
#define BLE_ADVERTISING_ENABLED         1
#define BLE_NUS_ENABLED                 0  /**< Nordic UART Service - optional */

/** BLE GAP Configuration */
#define NRF_BLE_GATT_MAX_MTU_SIZE       247
#define BLE_GAP_DATA_LENGTH             251

/** Connection parameters */
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(40, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)

/*******************************************************************************
 * TWI/I2C CONFIGURATION (for sensors)
 ******************************************************************************/

#define TWI_ENABLED                     1
#define TWI0_ENABLED                    1
#define TWI0_USE_EASY_DMA               1

/** TWI0 pins (I2C for PPG, temp sensor) */
#define TWI0_CONFIG_SCL                 27  /**< P0.27 - SCL */
#define TWI0_CONFIG_SDA                 26  /**< P0.26 - SDA */
#define TWI0_CONFIG_FREQUENCY           3   /**< 250 kbps */

/*******************************************************************************
 * SPI CONFIGURATION
 ******************************************************************************/

#define SPI_ENABLED                     0  /**< Not used in current design */

/*******************************************************************************
 * PWM CONFIGURATION (for actuators)
 ******************************************************************************/

#define PWM_ENABLED                     1

/** PWM0 - Vibration motor */
#define PWM0_ENABLED                    1
#define PWM0_CONFIG_OUT0_PIN            13  /**< P0.13 - Motor PWM */
#define PWM0_CONFIG_BASE_CLOCK          4   /**< 1 MHz */
#define PWM0_CONFIG_COUNT_MODE          0   /**< Up counter */
#define PWM0_CONFIG_TOP_VALUE           1000 /**< 1 kHz PWM */

/** PWM1 - Thermal heater */
#define PWM1_ENABLED                    1
#define PWM1_CONFIG_OUT0_PIN            14  /**< P0.14 - Heater PWM */
#define PWM1_CONFIG_BASE_CLOCK          4   /**< 1 MHz */
#define PWM1_CONFIG_COUNT_MODE          0   /**< Up counter */
#define PWM1_CONFIG_TOP_VALUE           1000 /**< 1 kHz PWM */

/*******************************************************************************
 * TIMER CONFIGURATION
 ******************************************************************************/

#define TIMER_ENABLED                   1
#define TIMER0_ENABLED                  0   /**< Reserved for SoftDevice */
#define TIMER1_ENABLED                  1   /**< App timers */
#define TIMER2_ENABLED                  1   /**< PPG sampling */

/*******************************************************************************
 * RTC CONFIGURATION
 ******************************************************************************/

#define RTC_ENABLED                     1
#define NRF_DRV_RTC_ENABLED             1

/*******************************************************************************
 * SAADC CONFIGURATION (battery monitoring)
 ******************************************************************************/

#define SAADC_ENABLED                   1
#define SAADC_CONFIG_RESOLUTION         2   /**< 12-bit */
#define SAADC_CONFIG_OVERSAMPLE         4   /**< 16x oversampling */

/*******************************************************************************
 * UART CONFIGURATION (debug logging)
 ******************************************************************************/

#define UART_ENABLED                    1
#define UART0_ENABLED                   1
#define UART0_CONFIG_HWFC               0   /**< No hardware flow control */
#define UART0_CONFIG_PARITY             0   /**< No parity */
#define UART0_CONFIG_BAUDRATE           30801920 /**< 115200 baud */

/** UART pins */
#define UART0_CONFIG_TXD_PIN            6   /**< P0.06 - TX */
#define UART0_CONFIG_RXD_PIN            8   /**< P0.08 - RX */

/*******************************************************************************
 * LOGGING CONFIGURATION
 ******************************************************************************/

#define NRF_LOG_ENABLED                 1
#define NRF_LOG_BACKEND_RTT_ENABLED     1
#define NRF_LOG_BACKEND_UART_ENABLED    0
#define NRF_LOG_DEFAULT_LEVEL           3   /**< Info */
#define NRF_LOG_DEFERRED                1

/*******************************************************************************
 * FLASH/FDS CONFIGURATION (persistent storage)
 ******************************************************************************/

#define FDS_ENABLED                     1
#define FDS_VIRTUAL_PAGES               3
#define FDS_VIRTUAL_PAGE_SIZE           1024

/*******************************************************************************
 * WATCHDOG CONFIGURATION
 ******************************************************************************/

#define WDT_ENABLED                     1
#define WDT_CONFIG_BEHAVIOUR            1   /**< Run in sleep */
#define WDT_CONFIG_RELOAD_VALUE         5000 /**< 5 seconds */

/*******************************************************************************
 * BOOTLOADER/DFU CONFIGURATION
 ******************************************************************************/

#define NRF_DFU_SETTINGS_VERSION        2
#define DFU_APP_DATA_RESERVED           0x4000  /**< 16 KB for DFU */

/*******************************************************************************
 * MEMORY LAYOUT
 ******************************************************************************/

/** Flash layout (nRF52840 has 1 MB) */
#define BOOTLOADER_START_ADDR           0x000F8000  /**< 992 KB */
#define BOOTLOADER_SIZE                 0x00006000  /**< 24 KB */
#define APP_CODE_BASE                   0x00027000  /**< After SoftDevice */

/** RAM layout (nRF52840 has 256 KB) */
#define RAM_START                       0x20002000
#define RAM_SIZE                        0x0003E000  /**< ~248 KB */

#endif /* SDK_CONFIG_H */