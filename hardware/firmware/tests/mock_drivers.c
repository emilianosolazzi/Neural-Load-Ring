/**
 * @file mock_drivers.c
 * @brief Mock Hardware Driver Implementations for Tests
 *
 * This file provides mock implementations of the vibration and thermal
 * driver functions. It links against the test_mocks.h tracking variables.
 *
 * Include this file INSTEAD of the real driver .c files when building tests.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "vibration_feature.h"
#include "thermal_feature.h"

/* External tracking variables from test_mocks.h */
extern uint8_t g_mock_vib_intensity;
extern uint8_t g_mock_thermal_intensity;
extern bool g_mock_vib_on;
extern bool g_mock_thermal_on;

/*******************************************************************************
 * VIBRATION MOCK IMPLEMENTATIONS
 ******************************************************************************/

void vibration_feature_init(void)
{
    /* No-op in test */
}

void vibration_feature_play(vibration_pattern_t pattern, uint8_t intensity_pct)
{
    (void)pattern;
    g_mock_vib_on = (intensity_pct > 0);
    g_mock_vib_intensity = intensity_pct;
}

void vibration_feature_stop(void)
{
    g_mock_vib_on = false;
    g_mock_vib_intensity = 0;
}

void vibration_feature_on(uint8_t intensity_pct)
{
    g_mock_vib_on = true;
    g_mock_vib_intensity = intensity_pct;
}

void vibration_feature_off(void)
{
    g_mock_vib_on = false;
    g_mock_vib_intensity = 0;
}

void vibration_feature_tick(uint32_t now_ms)
{
    (void)now_ms;
    /* No-op in test */
}

bool vibration_feature_is_active(void)
{
    return g_mock_vib_on;
}

/*******************************************************************************
 * THERMAL MOCK IMPLEMENTATIONS
 ******************************************************************************/

void thermal_feature_init(void)
{
    /* No-op in test */
}

void thermal_feature_set(uint8_t intensity_pct)
{
    g_mock_thermal_on = (intensity_pct > 0);
    g_mock_thermal_intensity = intensity_pct;
}

void thermal_feature_set_timed(uint8_t intensity_pct, uint8_t duration_s)
{
    (void)duration_s;
    g_mock_thermal_on = (intensity_pct > 0);
    g_mock_thermal_intensity = intensity_pct;
}

void thermal_feature_play(thermal_pattern_t pattern, uint8_t intensity_pct, uint8_t duration_s)
{
    (void)pattern;
    (void)duration_s;
    g_mock_thermal_on = (intensity_pct > 0);
    g_mock_thermal_intensity = intensity_pct;
}

void thermal_feature_stop(void)
{
    g_mock_thermal_on = false;
    g_mock_thermal_intensity = 0;
}

void thermal_feature_tick(uint32_t now_ms)
{
    (void)now_ms;
    /* No-op in test */
}

void thermal_feature_update_skin_temp(int8_t temp_c)
{
    (void)temp_c;
    /* No-op in test */
}
