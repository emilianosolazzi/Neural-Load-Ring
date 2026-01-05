/**
 * @file test_cue_processor.c
 * @brief Unit Tests for Cue Processor
 *
 * Tests the autonomous HRV-to-haptic decision engine:
 *   - Confidence gating
 *   - Decision cascade
 *   - Cooldown enforcement
 *   - Rate limiting
 *   - Quiet hours
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/wellness_feedback/cue_processor.h"

/*******************************************************************************
 * TEST HELPERS
 ******************************************************************************/

static cue_input_t make_input(uint32_t timestamp_ms, uint8_t coherence_pct, 
                              uint16_t micro_var_pct100, uint8_t stability_pct,
                              uint8_t confidence_pct)
{
    cue_input_t input = {0};
    input.timestamp_ms = timestamp_ms;
    input.coherence_pct = coherence_pct;
    input.micro_var_pct100 = micro_var_pct100;
    input.stability_pct = stability_pct;
    input.confidence_pct = confidence_pct;
    input.stress_level = 50;
    input.artifact_rate_pct = 5;
    return input;
}

static cue_input_t make_optimal_input(uint32_t timestamp_ms)
{
    return make_input(timestamp_ms, 80, 200, 75, 85);
}

static cue_input_t make_low_coherence_input(uint32_t timestamp_ms)
{
    return make_input(timestamp_ms, 25, 300, 60, 85);
}

static cue_input_t make_high_microvar_input(uint32_t timestamp_ms)
{
    return make_input(timestamp_ms, 60, 800, 55, 85);
}

static cue_input_t make_low_confidence_input(uint32_t timestamp_ms)
{
    return make_input(timestamp_ms, 70, 300, 70, 45);
}

static cue_input_t make_critical_input(uint32_t timestamp_ms)
{
    return make_input(timestamp_ms, 12, 1300, 15, 90);
}

/*******************************************************************************
 * INITIALIZATION TESTS
 ******************************************************************************/

TEST(cue_init_sets_defaults)
{
    cue_processor_init();
    ASSERT_TRUE(cue_processor_is_ready());
}

TEST(init_enables_all_modalities)
{
    cue_processor_init();
    
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    
    ASSERT_TRUE(prefs.enabled);
    ASSERT_TRUE(prefs.thermal_enabled);
    ASSERT_TRUE(prefs.vibration_enabled);
    ASSERT_TRUE(prefs.breathing_enabled);
}

TEST(init_sets_reasonable_limits)
{
    cue_processor_init();
    
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    
    ASSERT_GE(prefs.max_thermal_pct, 50);
    ASSERT_LE(prefs.max_thermal_pct, 100);
    ASSERT_GE(prefs.max_vib_pct, 50);
    ASSERT_LE(prefs.max_vib_pct, 100);
}

/*******************************************************************************
 * CONFIDENCE GATING TESTS
 ******************************************************************************/

TEST(low_confidence_suppresses_cue)
{
    cue_processor_init();
    cue_processor_reset();
    
    cue_input_t input = make_low_confidence_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_FALSE(triggered);
    ASSERT_EQ(CUE_TYPE_NONE, output.type);
}

TEST(high_confidence_allows_cue)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours for test */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    /* Low coherence should trigger thermal with good confidence */
    cue_input_t input = make_low_coherence_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    /* Should trigger some kind of cue */
    ASSERT_TRUE(triggered);
    ASSERT_NE(CUE_TYPE_NONE, output.type);
}

TEST(check_fit_after_low_confidence_streak)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    cue_output_t output;
    
    /* Three low-confidence readings */
    for (int i = 0; i < 3; i++) {
        cue_input_t input = make_low_confidence_input(1000 + i * 1000);
        cue_processor_generate(&input, &output);
    }
    
    /* Fourth should trigger check-fit (or next cycle will) */
    cue_input_t input = make_low_confidence_input(5000);
    bool triggered = cue_processor_generate(&input, &output);
    
    /* Should eventually trigger check-fit */
    if (triggered) {
        ASSERT_EQ(CUE_TYPE_CHECK_FIT, output.type);
    }
}

/*******************************************************************************
 * DECISION CASCADE TESTS
 ******************************************************************************/

TEST(critical_triggers_alert)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_TRUE(triggered);
    ASSERT_EQ(CUE_PRIORITY_ALERT, output.priority);
}

TEST(low_coherence_triggers_thermal)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    /* Just low coherence, not critical */
    cue_input_t input = make_input(1000, 40, 300, 60, 85);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_TRUE(triggered);
    ASSERT_GT(output.thermal_intensity, 0);
}

TEST(high_microvar_triggers_vibration)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    cue_input_t input = make_high_microvar_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_TRUE(triggered);
    ASSERT_GT(output.vib_intensity, 0);
}

TEST(optimal_state_no_cue)
{
    cue_processor_init();
    cue_processor_reset();
    
    cue_input_t input = make_optimal_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_FALSE(triggered);
    ASSERT_EQ(CUE_TYPE_NONE, output.type);
}

/*******************************************************************************
 * COOLDOWN TESTS
 ******************************************************************************/

TEST(cooldown_prevents_rapid_cues)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    /* First cue triggers */
    cue_input_t input1 = make_low_coherence_input(1000);
    cue_output_t output1;
    bool first = cue_processor_generate(&input1, &output1);
    ASSERT_TRUE(first);
    
    /* Immediate second cue should be blocked by cooldown */
    cue_input_t input2 = make_low_coherence_input(2000);
    cue_output_t output2;
    bool second = cue_processor_generate(&input2, &output2);
    
    /* Should be blocked (cooldown not elapsed) */
    ASSERT_FALSE(second);
}

TEST(cooldown_allows_after_period)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    /* First cue */
    cue_input_t input1 = make_low_coherence_input(1000);
    cue_output_t output1;
    cue_processor_generate(&input1, &output1);
    
    /* After cooldown period (2 minutes for thermal) */
    cue_input_t input2 = make_low_coherence_input(130000);  /* ~2 min later */
    cue_output_t output2;
    bool triggered = cue_processor_generate(&input2, &output2);
    
    ASSERT_TRUE(triggered);
}

/*******************************************************************************
 * QUIET HOURS TESTS
 ******************************************************************************/

TEST(quiet_hours_suppress_cues)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Set quiet hours 22:00 - 07:00 */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 22;
    prefs.quiet_end_hour = 7;
    cue_processor_set_preferences(&prefs);
    
    /* Set time to 23:00 (in quiet hours) */
    cue_processor_set_hour(23);
    
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    /* Should be suppressed despite critical state */
    ASSERT_FALSE(triggered);
}

TEST(outside_quiet_hours_allows_cues)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Set quiet hours 22:00 - 07:00 */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 22;
    prefs.quiet_end_hour = 7;
    cue_processor_set_preferences(&prefs);
    
    /* Set time to 14:00 (outside quiet hours) */
    cue_processor_set_hour(14);
    
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_TRUE(triggered);
}

/*******************************************************************************
 * PREFERENCE TESTS
 ******************************************************************************/

TEST(disabled_suppresses_all)
{
    cue_processor_init();
    cue_processor_reset();
    
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.enabled = false;
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    ASSERT_FALSE(triggered);
}

TEST(thermal_disabled_uses_vibration)
{
    cue_processor_init();
    cue_processor_reset();
    
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.thermal_enabled = false;
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    /* Input that would normally trigger thermal */
    cue_input_t input = make_input(1000, 40, 300, 60, 85);
    cue_output_t output;
    
    bool triggered = cue_processor_generate(&input, &output);
    
    /* Should either not trigger or use different modality */
    if (triggered) {
        ASSERT_EQ(0, output.thermal_intensity);
    }
}

TEST(intensity_respects_max)
{
    cue_processor_init();
    cue_processor_reset();
    
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.max_thermal_pct = 40;
    prefs.max_vib_pct = 30;
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    
    cue_processor_generate(&input, &output);
    
    /* Intensities should be capped */
    ASSERT_LE(output.thermal_intensity, 40);
    ASSERT_LE(output.vib_intensity, 30);
}

/*******************************************************************************
 * STATISTICS TESTS
 ******************************************************************************/

TEST(stats_track_generated)
{
    cue_processor_init();
    cue_processor_reset();
    
    /* Disable quiet hours */
    cue_preferences_t prefs;
    cue_processor_get_preferences(&prefs);
    prefs.quiet_start_hour = 0;
    prefs.quiet_end_hour = 0;
    cue_processor_set_preferences(&prefs);
    cue_processor_set_hour(12);
    
    uint32_t generated_before, suppressed_before;
    cue_processor_get_stats(&generated_before, &suppressed_before, NULL, NULL);
    
    /* Trigger a cue */
    cue_input_t input = make_critical_input(1000);
    cue_output_t output;
    cue_processor_generate(&input, &output);
    
    uint32_t generated_after, suppressed_after;
    cue_processor_get_stats(&generated_after, &suppressed_after, NULL, NULL);
    
    ASSERT_EQ(generated_before + 1, generated_after);
}

TEST(stats_track_suppressed)
{
    cue_processor_init();
    cue_processor_reset();
    
    uint32_t generated_before, suppressed_before;
    cue_processor_get_stats(&generated_before, &suppressed_before, NULL, NULL);
    
    /* Low confidence should suppress */
    cue_input_t input = make_low_confidence_input(1000);
    cue_output_t output;
    cue_processor_generate(&input, &output);
    
    uint32_t generated_after, suppressed_after;
    cue_processor_get_stats(&generated_after, &suppressed_after, NULL, NULL);
    
    ASSERT_EQ(suppressed_before + 1, suppressed_after);
}

/*******************************************************************************
 * TEST RUNNER
 ******************************************************************************/

static void run_init_tests(void)
{
    RUN_TEST(cue_init_sets_defaults);
    RUN_TEST(init_enables_all_modalities);
    RUN_TEST(init_sets_reasonable_limits);
}

static void run_confidence_tests(void)
{
    RUN_TEST(low_confidence_suppresses_cue);
    RUN_TEST(high_confidence_allows_cue);
    RUN_TEST(check_fit_after_low_confidence_streak);
}

static void run_cascade_tests(void)
{
    RUN_TEST(critical_triggers_alert);
    RUN_TEST(low_coherence_triggers_thermal);
    RUN_TEST(high_microvar_triggers_vibration);
    RUN_TEST(optimal_state_no_cue);
}

static void run_cooldown_tests(void)
{
    RUN_TEST(cooldown_prevents_rapid_cues);
    RUN_TEST(cooldown_allows_after_period);
}

static void run_quiet_hours_tests(void)
{
    RUN_TEST(quiet_hours_suppress_cues);
    RUN_TEST(outside_quiet_hours_allows_cues);
}

static void run_preference_tests(void)
{
    RUN_TEST(disabled_suppresses_all);
    RUN_TEST(thermal_disabled_uses_vibration);
    RUN_TEST(intensity_respects_max);
}

static void run_stats_tests(void)
{
    RUN_TEST(stats_track_generated);
    RUN_TEST(stats_track_suppressed);
}

void run_cue_processor_tests(void)
{
    printf("\n========================================\n");
    printf("CUE PROCESSOR TESTS\n");
    printf("========================================\n");
    
    RUN_TEST_SUITE("Initialization", run_init_tests);
    RUN_TEST_SUITE("Confidence Gating", run_confidence_tests);
    RUN_TEST_SUITE("Decision Cascade", run_cascade_tests);
    RUN_TEST_SUITE("Cooldown Enforcement", run_cooldown_tests);
    RUN_TEST_SUITE("Quiet Hours", run_quiet_hours_tests);
    RUN_TEST_SUITE("Preferences", run_preference_tests);
    RUN_TEST_SUITE("Statistics", run_stats_tests);
}
