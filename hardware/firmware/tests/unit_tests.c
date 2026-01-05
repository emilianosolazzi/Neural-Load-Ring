/**
 * @file unit_tests.c
 * @brief Main Test Runner for Neural Load Ring Firmware
 *
 * Compiles and runs all unit tests for the wellness feedback system.
 *
 * Build (host simulation):
 *   gcc -o run_tests unit_tests.c mock_drivers.c -I../src -I../src/wellness_feedback -lm
 *   ./run_tests
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
#include "test_framework.h"

/* Mock tracking variables - defined here, externed everywhere else */
uint8_t g_mock_vib_intensity = 0;
uint8_t g_mock_thermal_intensity = 0;
bool g_mock_vib_on = false;
bool g_mock_thermal_on = false;

void mocks_reset(void)
{
    g_mock_vib_intensity = 0;
    g_mock_thermal_intensity = 0;
    g_mock_vib_on = false;
    g_mock_thermal_on = false;
}

/* Source files (compiled together for testing) */
#include "../src/wellness_feedback/signature_feel.c"
#include "../src/wellness_feedback/cue_processor.c"
#include "../src/wellness_feedback/cue_to_signature.c"

/* Test suites */
extern void run_signature_feel_tests(void);
extern void run_cue_processor_tests(void);
extern void run_cue_to_signature_tests(void);

/* Include test implementations */
#include "test_signature_feel.c"
#include "test_cue_processor.c"
#include "test_cue_to_signature.c"

/*******************************************************************************
 * MAIN
 ******************************************************************************/

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     NEURAL LOAD RING - FIRMWARE UNIT TESTS                   ║\n");
    printf("║     Wellness Feedback System                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* Reset counters */
    g_tests_passed = 0;
    g_tests_failed = 0;
    
    /* Run all test suites */
    run_signature_feel_tests();
    run_cue_processor_tests();
    run_cue_to_signature_tests();
    
    /* Print summary */
    test_print_summary();
    
    /* Return exit code */
    return (g_tests_failed > 0) ? 1 : 0;
}
