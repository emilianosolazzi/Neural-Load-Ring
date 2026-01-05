/**
 * @file system_init.h
 * @brief Neural Load Ring System Initialization Header
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all system components
 * 
 * Must be called first in main(). Configures:
 *   - Clocks (HFCLK 64MHz, LFCLK 32.768kHz)
 *   - Power management (DC/DC converter)
 *   - GPIO pin directions
 *   - App timer
 *   - Watchdog
 */
void system_init(void);

/**
 * @brief Feed the watchdog timer
 * 
 * Call periodically from main loop to prevent reset.
 * Watchdog timeout: 8 seconds.
 */
void system_watchdog_feed(void);

/**
 * @brief Enter low-power idle until next event
 * 
 * Uses WFE (Wait For Event) to sleep CPU while
 * peripherals continue operation.
 */
void system_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */
