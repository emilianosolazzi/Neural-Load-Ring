/**
 * @file test_mocks.h
 * @brief Shared Mock Hardware for Firmware Tests
 *
 * Simulates vibration and thermal drivers for host-based testing.
 * 
 * NOTE: This file only provides TRACKING VARIABLES and a reset function.
 * The actual mock function implementations are provided separately.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_MOCKS_H
#define TEST_MOCKS_H

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * MOCK STATE - These track what the hardware drivers were called with
 ******************************************************************************/

static uint8_t g_mock_vib_intensity = 0;
static uint8_t g_mock_thermal_intensity = 0;
static bool g_mock_vib_on = false;
static bool g_mock_thermal_on = false;

/*******************************************************************************
 * HELPERS
 ******************************************************************************/

static void mocks_reset(void)
{
    g_mock_vib_intensity = 0;
    g_mock_thermal_intensity = 0;
    g_mock_vib_on = false;
    g_mock_thermal_on = false;
}

#endif /* TEST_MOCKS_H */
