/**
 * @file test_biometrics.c
 * @brief Unit tests for biometric algorithms
 */

#include "test_framework.h"
#include "../src/core/biometric_algorithms.h"
#include <math.h>

void run_biometric_tests(void) {
    TEST_CASE("Biometrics: Reset works");
    {
        hr_metrics_t metrics;
        metrics.rmssd = 100.0f;
        biometrics_reset(&metrics);
        ASSERT_FLOAT_EQ(metrics.rmssd, 0.0f);
        ASSERT_INT_EQ(metrics.valid_samples, 0);
    }

    TEST_CASE("Biometrics: Artifact rejection (Low)");
    {
        hr_metrics_t metrics;
        biometrics_reset(&metrics);
        bool ok = biometrics_process_rr(&metrics, 200.0f); /* Too low */
        ASSERT_FALSE(ok);
        ASSERT_INT_EQ(metrics.valid_samples, 0);
        ASSERT_INT_EQ(metrics.total_samples, 0); /* Still 0 if first sample rejected? Let's check impl */
    }

    TEST_CASE("Biometrics: Normal sequence calculation");
    {
        hr_metrics_t metrics;
        biometrics_reset(&metrics);
        
        /* Simulating 800ms base RR with some variability */
        biometrics_process_rr(&metrics, 800.0f);
        biometrics_process_rr(&metrics, 820.0f); /* diff = 20 */
        biometrics_process_rr(&metrics, 780.0f); /* diff = 40 */
        
        ASSERT_INT_EQ(metrics.valid_samples, 3);
        ASSERT_TRUE(metrics.rmssd > 0.0f);
        ASSERT_TRUE(metrics.mean_rr_ms > 700.0f && metrics.mean_rr_ms < 900.0f);
    }

    TEST_CASE("Biometrics: Relative artifact rejection");
    {
        hr_metrics_t metrics;
        biometrics_reset(&metrics);
        biometrics_process_rr(&metrics, 800.0f);
        bool ok = biometrics_process_rr(&metrics, 1200.0f); /* 50% jump, should be rejected */
        ASSERT_FALSE(ok);
        ASSERT_INT_EQ(metrics.valid_samples, 1);
    }
}
