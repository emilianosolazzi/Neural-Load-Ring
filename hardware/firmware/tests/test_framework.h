/**
 * @file test_framework.h
 * @brief Lightweight Unit Test Framework for Embedded C
 *
 * A minimal test framework suitable for embedded targets and host simulation.
 * Inspired by Unity/MinUnit but simplified for our use case.
 *
 * Usage:
 *   TEST(test_name) {
 *       ASSERT_EQ(expected, actual);
 *       ASSERT_TRUE(condition);
 *   }
 *
 *   RUN_TEST(test_name);
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*******************************************************************************
 * TEST MACROS
 ******************************************************************************/

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    test_##name(); \
    printf("PASS\n"); \
    g_tests_passed++; \
} while(0)

#define RUN_TEST_SUITE(name, tests) do { \
    printf("\n[%s]\n", name); \
    tests(); \
} while(0)

/*******************************************************************************
 * ASSERTION MACROS
 ******************************************************************************/

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n"); \
        printf("    Assertion failed: %s\n", #cond); \
        printf("    File: %s, Line: %d\n", __FILE__, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL\n"); \
        printf("    Expected: %d, Actual: %d\n", (int)(expected), (int)(actual)); \
        printf("    File: %s, Line: %d\n", __FILE__, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NE(val1, val2) ASSERT_TRUE((val1) != (val2))

#define ASSERT_GT(val1, val2) ASSERT_TRUE((val1) > (val2))
#define ASSERT_GE(val1, val2) ASSERT_TRUE((val1) >= (val2))
#define ASSERT_LT(val1, val2) ASSERT_TRUE((val1) < (val2))
#define ASSERT_LE(val1, val2) ASSERT_TRUE((val1) <= (val2))

#define ASSERT_FLOAT_EQ(expected, actual, epsilon) do { \
    float _diff = fabsf((float)(expected) - (float)(actual)); \
    if (_diff > (epsilon)) { \
        printf("FAIL\n"); \
        printf("    Expected: %.4f, Actual: %.4f (diff: %.6f > %.6f)\n", \
               (float)(expected), (float)(actual), _diff, (float)(epsilon)); \
        printf("    File: %s, Line: %d\n", __FILE__, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_IN_RANGE(val, min, max) do { \
    if ((val) < (min) || (val) > (max)) { \
        printf("FAIL\n"); \
        printf("    Value %d not in range [%d, %d]\n", (int)(val), (int)(min), (int)(max)); \
        printf("    File: %s, Line: %d\n", __FILE__, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

/*******************************************************************************
 * TEST COUNTERS
 ******************************************************************************/

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void test_print_summary(void)
{
    printf("\n========================================\n");
    printf("Tests Passed: %d\n", g_tests_passed);
    printf("Tests Failed: %d\n", g_tests_failed);
    printf("========================================\n");
    
    if (g_tests_failed == 0) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }
}

#endif /* TEST_FRAMEWORK_H */
