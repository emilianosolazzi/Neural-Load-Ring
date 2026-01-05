/**
 * @file test_cue_to_signature.c
 * @brief Unit Tests for Cue to Signature Mapping
 *
 * Tests the bridge between cue processor and signature feel system.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/wellness_feedback/cue_to_signature.h"

/* External mock state from unit_tests.c */
extern uint8_t g_mock_vib_intensity;
extern uint8_t g_mock_thermal_intensity;
extern bool g_mock_vib_on;
extern bool g_mock_thermal_on;
extern void mocks_reset(void);

/*******************************************************************************
 * MAPPING TESTS
 ******************************************************************************/

TEST(alert_maps_to_full_reset)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_COMBINED;
    cue.priority = CUE_PRIORITY_ALERT;
    cue.vib_intensity = 80;
    cue.thermal_intensity = 70;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_FULL_RESET, pattern);
}

TEST(high_combined_maps_to_heartbeat)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_COMBINED;
    cue.priority = CUE_PRIORITY_HIGH;
    cue.vib_intensity = 60;
    cue.thermal_intensity = 50;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_HEARTBEAT, pattern);
}

TEST(thermal_alert_maps_to_safety_embrace)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_THERMAL;
    cue.priority = CUE_PRIORITY_ALERT;
    cue.thermal_intensity = 70;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_SAFETY_EMBRACE, pattern);
}

TEST(normal_vibration_maps_to_attention_tap)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_VIBRATION;
    cue.priority = CUE_PRIORITY_NORMAL;
    cue.vib_intensity = 40;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_ATTENTION_TAP, pattern);
}

TEST(normal_thermal_maps_to_warm_exhale)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_THERMAL;
    cue.priority = CUE_PRIORITY_NORMAL;
    cue.thermal_intensity = 50;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_WARM_EXHALE, pattern);
}

TEST(low_vibration_maps_to_grounding_pulse)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_VIBRATION;
    cue.priority = CUE_PRIORITY_LOW;
    cue.vib_intensity = 25;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_GROUNDING_PULSE, pattern);
}

TEST(low_thermal_maps_to_grounding_warmth)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_THERMAL;
    cue.priority = CUE_PRIORITY_LOW;
    cue.thermal_intensity = 35;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_GROUNDING_WARMTH, pattern);
}

TEST(check_fit_maps_to_presence_check)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_CHECK_FIT;
    cue.priority = CUE_PRIORITY_LOW;
    cue.vib_intensity = 20;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_PRESENCE_CHECK, pattern);
}

TEST(breathing_maps_to_breathing_guide)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_BREATHING;
    cue.priority = CUE_PRIORITY_NORMAL;
    cue.vib_intensity = 30;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_BREATHING_GUIDE, pattern);
}

TEST(none_type_returns_none)
{
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_NONE;
    
    signature_pattern_t pattern = cue_to_signature_pattern(&cue);
    
    ASSERT_EQ(SIG_PATTERN_NONE, pattern);
}

TEST(null_cue_returns_none)
{
    signature_pattern_t pattern = cue_to_signature_pattern(NULL);
    
    ASSERT_EQ(SIG_PATTERN_NONE, pattern);
}

/*******************************************************************************
 * EXECUTION TESTS
 ******************************************************************************/

TEST(execute_plays_pattern)
{
    signature_init();
    reset_mocks();
    
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_VIBRATION;
    cue.priority = CUE_PRIORITY_NORMAL;
    cue.vib_intensity = 60;
    
    cue_execute_as_signature(&cue);
    
    ASSERT_TRUE(signature_is_playing());
    ASSERT_EQ(SIG_ATTENTION_TAP, signature_current_pattern());
}

TEST(execute_none_does_nothing)
{
    signature_init();
    reset_mocks();
    
    cue_output_t cue = {0};
    cue.type = CUE_TYPE_NONE;
    
    cue_execute_as_signature(&cue);
    
    ASSERT_FALSE(signature_is_playing());
}

TEST(execute_null_does_nothing)
{
    signature_init();
    reset_mocks();
    
    cue_execute_as_signature(NULL);
    
    ASSERT_FALSE(signature_is_playing());
}

/*******************************************************************************
 * MAPPING TABLE TESTS
 ******************************************************************************/

TEST(mapping_table_exists)
{
    uint8_t count = 0;
    const cue_signature_mapping_t *mappings = cue_get_signature_mappings(&count);
    
    ASSERT_NOT_NULL(mappings);
    ASSERT_GT(count, 0);
}

TEST(mapping_table_covers_priorities)
{
    uint8_t count = 0;
    const cue_signature_mapping_t *mappings = cue_get_signature_mappings(&count);
    
    bool has_alert = false;
    bool has_high = false;
    bool has_normal = false;
    bool has_low = false;
    
    for (uint8_t i = 0; i < count; i++) {
        if (mappings[i].priority == CUE_PRIORITY_ALERT) has_alert = true;
        if (mappings[i].priority == CUE_PRIORITY_HIGH) has_high = true;
        if (mappings[i].priority == CUE_PRIORITY_NORMAL) has_normal = true;
        if (mappings[i].priority == CUE_PRIORITY_LOW) has_low = true;
    }
    
    ASSERT_TRUE(has_alert);
    ASSERT_TRUE(has_high);
    ASSERT_TRUE(has_normal);
    ASSERT_TRUE(has_low);
}

/*******************************************************************************
 * TEST RUNNER
 ******************************************************************************/

static void run_mapping_tests(void)
{
    RUN_TEST(alert_maps_to_full_reset);
    RUN_TEST(high_combined_maps_to_heartbeat);
    RUN_TEST(thermal_alert_maps_to_safety_embrace);
    RUN_TEST(normal_vibration_maps_to_attention_tap);
    RUN_TEST(normal_thermal_maps_to_warm_exhale);
    RUN_TEST(low_vibration_maps_to_grounding_pulse);
    RUN_TEST(low_thermal_maps_to_grounding_warmth);
    RUN_TEST(check_fit_maps_to_presence_check);
    RUN_TEST(breathing_maps_to_breathing_guide);
    RUN_TEST(none_type_returns_none);
    RUN_TEST(null_cue_returns_none);
}

static void run_execution_tests(void)
{
    RUN_TEST(execute_plays_pattern);
    RUN_TEST(execute_none_does_nothing);
    RUN_TEST(execute_null_does_nothing);
}

static void run_table_tests(void)
{
    RUN_TEST(mapping_table_exists);
    RUN_TEST(mapping_table_covers_priorities);
}

void run_cue_to_signature_tests(void)
{
    printf("\n========================================\n");
    printf("CUE TO SIGNATURE MAPPING TESTS\n");
    printf("========================================\n");
    
    RUN_TEST_SUITE("Pattern Mapping", run_mapping_tests);
    RUN_TEST_SUITE("Execution", run_execution_tests);
    RUN_TEST_SUITE("Mapping Table", run_table_tests);
}
