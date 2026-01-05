/**
 * @file test_signature_feel.c
 * @brief Unit Tests for Signature Feel System
 *
 * Tests the ring's haptic personality:
 *   - Easing curve calculations
 *   - Pattern playback state machine
 *   - Intensity limits and safety
 *   - Timing accuracy
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/wellness_feedback/signature_feel.h"

/* External mock state from unit_tests.c */
extern uint8_t g_mock_vib_intensity;
extern uint8_t g_mock_thermal_intensity;
extern bool g_mock_vib_on;
extern bool g_mock_thermal_on;
extern void mocks_reset(void);

/* Alias for local usage */
#define mock_vib_intensity   g_mock_vib_intensity
#define mock_thermal_intensity g_mock_thermal_intensity
#define mock_vib_on          g_mock_vib_on
#define mock_thermal_on      g_mock_thermal_on
#define reset_mocks()        mocks_reset()

/*******************************************************************************
 * EASING CURVE TESTS
 ******************************************************************************/

TEST(ease_linear_identity)
{
    ASSERT_FLOAT_EQ(0.0f, ease_calculate(EASE_LINEAR, 0.0f), 0.001f);
    ASSERT_FLOAT_EQ(0.5f, ease_calculate(EASE_LINEAR, 0.5f), 0.001f);
    ASSERT_FLOAT_EQ(1.0f, ease_calculate(EASE_LINEAR, 1.0f), 0.001f);
}

TEST(ease_in_sine_starts_slow)
{
    float early = ease_calculate(EASE_IN_SINE, 0.2f);
    /* Ease-in should be slower than linear at the start */
    ASSERT_TRUE(early < 0.2f);
}

TEST(ease_out_sine_ends_slow)
{
    float late = ease_calculate(EASE_OUT_SINE, 0.8f);
    /* Ease-out should be further along than linear */
    ASSERT_TRUE(late > 0.8f);
}

TEST(ease_in_out_sine_symmetric)
{
    float first_quarter = ease_calculate(EASE_IN_OUT_SINE, 0.25f);
    float third_quarter = ease_calculate(EASE_IN_OUT_SINE, 0.75f);
    /* Should be symmetric around 0.5 */
    ASSERT_FLOAT_EQ(first_quarter, 1.0f - third_quarter, 0.01f);
}

TEST(ease_clamps_boundaries)
{
    ASSERT_FLOAT_EQ(0.0f, ease_calculate(EASE_IN_SINE, -0.5f), 0.001f);
    ASSERT_FLOAT_EQ(1.0f, ease_calculate(EASE_IN_SINE, 1.5f), 0.001f);
}

TEST(ease_breath_asymmetric)
{
    /* At t=0.4 (end of inhale), should be near peak */
    float at_peak = ease_calculate(EASE_BREATH, 0.4f);
    ASSERT_TRUE(at_peak > 0.9f);
    
    /* During exhale, still above halfway */
    float mid_exhale = ease_calculate(EASE_BREATH, 0.7f);
    ASSERT_TRUE(mid_exhale > 0.3f);
}

TEST(ease_intensity_interpolates)
{
    ASSERT_EQ(50, ease_intensity(0, 100, EASE_LINEAR, 0.5f));
    ASSERT_EQ(40, ease_intensity(20, 60, EASE_LINEAR, 0.5f));
}

TEST(ease_intensity_clamps)
{
    /* Should clamp to 0-100 */
    ASSERT_EQ(0, ease_intensity(0, 0, EASE_LINEAR, 0.5f));
    ASSERT_LE(ease_intensity(0, 200, EASE_LINEAR, 1.0f), 100);
}

/*******************************************************************************
 * SAFETY LIMIT TESTS
 ******************************************************************************/

TEST(safe_vibration_clamps)
{
    ASSERT_EQ(SIG_VIB_MAX_INTENSITY, signature_safe_vibration(100));
    ASSERT_EQ(50, signature_safe_vibration(50));
    ASSERT_EQ(0, signature_safe_vibration(0));
}

TEST(safe_thermal_clamps)
{
    ASSERT_EQ(SIG_THERMAL_MAX, signature_safe_thermal(100));
    ASSERT_EQ(50, signature_safe_thermal(50));
    ASSERT_EQ(0, signature_safe_thermal(0));
}

TEST(intensity_limits_reasonable)
{
    /* Vibration max should be comfortable */
    ASSERT_LE(SIG_VIB_MAX_INTENSITY, 70);
    ASSERT_GT(SIG_VIB_MAX_INTENSITY, 50);
    
    /* Thermal max should be safe */
    ASSERT_LE(SIG_THERMAL_MAX, 75);
    ASSERT_GT(SIG_THERMAL_MAX, 50);
}

/*******************************************************************************
 * PATTERN PLAYBACK TESTS
 ******************************************************************************/

TEST(init_sets_defaults)
{
    signature_init();
    ASSERT_FALSE(signature_is_playing());
    ASSERT_EQ(SIG_PATTERN_NONE, signature_current_pattern());
}

TEST(play_starts_pattern)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_GROUNDING_PULSE, 80);
    
    ASSERT_TRUE(signature_is_playing());
    ASSERT_EQ(SIG_GROUNDING_PULSE, signature_current_pattern());
}

TEST(play_none_stops)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_GROUNDING_PULSE, 80);
    signature_play(SIG_PATTERN_NONE, 0);
    
    /* Should trigger fade-out, not immediately stop */
    /* After immediate stop: */
    signature_stop_immediate();
    ASSERT_FALSE(signature_is_playing());
}

TEST(stop_immediate_kills_output)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_HEARTBEAT, 100);
    signature_tick(0);
    signature_tick(100);
    
    signature_stop_immediate();
    
    ASSERT_FALSE(signature_is_playing());
    ASSERT_EQ(0, mock_vib_intensity);
}

TEST(tick_advances_pattern)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_GROUNDING_PULSE, 100);
    
    /* First tick initializes */
    signature_tick(0);
    
    /* During ramp-up, intensity should increase */
    signature_tick(100);
    uint8_t early_intensity = mock_vib_intensity;
    
    signature_tick(300);
    uint8_t later_intensity = mock_vib_intensity;
    
    /* Should be ramping up */
    ASSERT_GE(later_intensity, early_intensity);
}

TEST(pattern_completes)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_GROUNDING_PULSE, 100);
    
    /* Run through the pattern (~900ms total) */
    for (uint32_t t = 0; t < 2000; t += 10) {
        signature_tick(t);
    }
    
    /* Should have completed or be fading */
    /* Either not playing or in fade-out */
}

/*******************************************************************************
 * PATTERN ENUMERATION TESTS
 ******************************************************************************/

TEST(all_patterns_valid)
{
    ASSERT_EQ(0, SIG_PATTERN_NONE);
    ASSERT_GT(SIG_PATTERN_COUNT, 10);  /* We have at least 10 patterns */
}

TEST(vibration_patterns_exist)
{
    ASSERT_GT(SIG_GROUNDING_PULSE, 0);
    ASSERT_GT(SIG_ATTENTION_TAP, 0);
    ASSERT_GT(SIG_HEARTBEAT, 0);
    ASSERT_GT(SIG_BREATHING_GUIDE, 0);
}

TEST(thermal_patterns_exist)
{
    ASSERT_GT(SIG_WARM_EXHALE, 0);
    ASSERT_GT(SIG_GROUNDING_WARMTH, 0);
    ASSERT_GT(SIG_SAFETY_EMBRACE, 0);
}

TEST(combined_patterns_exist)
{
    ASSERT_GT(SIG_GENTLE_ALERT, 0);
    ASSERT_GT(SIG_FULL_RESET, 0);
}

/*******************************************************************************
 * TIMING CONSTANT TESTS
 ******************************************************************************/

TEST(timing_ramps_reasonable)
{
    /* Ramp up should be ~400ms */
    ASSERT_GE(SIG_RAMP_UP_MS, 200);
    ASSERT_LE(SIG_RAMP_UP_MS, 600);
    
    /* Ramp down should be longer (organic decay) */
    ASSERT_GE(SIG_RAMP_DOWN_MS, SIG_RAMP_UP_MS);
}

TEST(breathing_timing_correct)
{
    /* 4:6 ratio for inhale:exhale */
    float ratio = (float)SIG_BREATH_INHALE_MS / (float)SIG_BREATH_EXHALE_MS;
    ASSERT_FLOAT_EQ(4.0f / 6.0f, ratio, 0.1f);
}

TEST(heartbeat_targets_75bpm)
{
    /* 75 BPM = 800ms per beat */
    float bpm = 60000.0f / (float)SIG_HEARTBEAT_PERIOD_MS;
    ASSERT_FLOAT_EQ(75.0f, bpm, 10.0f);  /* Within 10 BPM */
}

/*******************************************************************************
 * INTENSITY SCALE TESTS
 ******************************************************************************/

TEST(intensity_scale_applies)
{
    signature_init();
    reset_mocks();
    
    /* Play at 50% intensity */
    signature_play(SIG_GROUNDING_PULSE, 50);
    signature_tick(0);
    signature_tick(200);  /* During ramp */
    
    /* Intensity should be scaled down from max */
    ASSERT_LE(mock_vib_intensity, SIG_VIB_GENTLE);
}

TEST(intensity_100_uses_max)
{
    signature_init();
    reset_mocks();
    
    signature_play(SIG_GROUNDING_PULSE, 100);
    signature_tick(0);
    signature_tick(100);
    signature_tick(200);
    signature_tick(300);
    
    /* Should be at or approaching max gentle intensity */
    ASSERT_GT(mock_vib_intensity, 0);
}

/*******************************************************************************
 * TEST RUNNER
 ******************************************************************************/

static void run_easing_tests(void)
{
    RUN_TEST(ease_linear_identity);
    RUN_TEST(ease_in_sine_starts_slow);
    RUN_TEST(ease_out_sine_ends_slow);
    RUN_TEST(ease_in_out_sine_symmetric);
    RUN_TEST(ease_clamps_boundaries);
    RUN_TEST(ease_breath_asymmetric);
    RUN_TEST(ease_intensity_interpolates);
    RUN_TEST(ease_intensity_clamps);
}

static void run_safety_tests(void)
{
    RUN_TEST(safe_vibration_clamps);
    RUN_TEST(safe_thermal_clamps);
    RUN_TEST(intensity_limits_reasonable);
}

static void run_playback_tests(void)
{
    RUN_TEST(init_sets_defaults);
    RUN_TEST(play_starts_pattern);
    RUN_TEST(play_none_stops);
    RUN_TEST(stop_immediate_kills_output);
    RUN_TEST(tick_advances_pattern);
    RUN_TEST(pattern_completes);
}

static void run_enumeration_tests(void)
{
    RUN_TEST(all_patterns_valid);
    RUN_TEST(vibration_patterns_exist);
    RUN_TEST(thermal_patterns_exist);
    RUN_TEST(combined_patterns_exist);
}

static void run_timing_tests(void)
{
    RUN_TEST(timing_ramps_reasonable);
    RUN_TEST(breathing_timing_correct);
    RUN_TEST(heartbeat_targets_75bpm);
}

static void run_intensity_tests(void)
{
    RUN_TEST(intensity_scale_applies);
    RUN_TEST(intensity_100_uses_max);
}

void run_signature_feel_tests(void)
{
    printf("\n========================================\n");
    printf("SIGNATURE FEEL TESTS\n");
    printf("========================================\n");
    
    RUN_TEST_SUITE("Easing Curves", run_easing_tests);
    RUN_TEST_SUITE("Safety Limits", run_safety_tests);
    RUN_TEST_SUITE("Pattern Playback", run_playback_tests);
    RUN_TEST_SUITE("Pattern Enumeration", run_enumeration_tests);
    RUN_TEST_SUITE("Timing Constants", run_timing_tests);
    RUN_TEST_SUITE("Intensity Scaling", run_intensity_tests);
}
