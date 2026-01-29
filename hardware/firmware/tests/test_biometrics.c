/**
 * @file test_biometrics.c
 * @brief Unit tests for biometric algorithms
 */

#include "test_framework.h"
#include "../src/core/biometric_algorithms.h"
#include <math.h>

TEST(biometrics_reset) {
    hr_metrics_t metrics;
    metrics.rmssd = 100.0f;
    biometrics_reset(&metrics);
    ASSERT_FLOAT_EQ(0.0f, metrics.rmssd, 0.0001f);
    ASSERT_EQ(0, metrics.valid_samples);
    /* Baseline should be initialized to default */
    ASSERT_TRUE(metrics.baseline_rmssd > 0.0f); 
}

TEST(biometrics_artifact_rejection_low) {
    hr_metrics_t metrics;
    biometrics_reset(&metrics);
    bool ok = biometrics_process_rr(&metrics, 200.0f); /* Too low */
    ASSERT_FALSE(ok);
    ASSERT_EQ(0, metrics.valid_samples);
    ASSERT_EQ(0, metrics.total_samples); 
}

TEST(biometrics_normal_sequence) {
    hr_metrics_t metrics;
    biometrics_reset(&metrics);
    
    /* Simulating 800ms base RR with some variability */
    biometrics_process_rr(&metrics, 800.0f);
    biometrics_process_rr(&metrics, 820.0f); /* diff = 20 */
    biometrics_process_rr(&metrics, 780.0f); /* diff = 40 */
    
    ASSERT_EQ(3, metrics.valid_samples);
    ASSERT_TRUE(metrics.rmssd > 0.0f);
    ASSERT_TRUE(metrics.mean_rr_ms > 700.0f && metrics.mean_rr_ms < 900.0f);
}

TEST(biometrics_relative_artifact) {
    hr_metrics_t metrics;
    biometrics_reset(&metrics);
    biometrics_process_rr(&metrics, 800.0f);
    bool ok = biometrics_process_rr(&metrics, 1200.0f); /* 50% jump, should be rejected */
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, metrics.valid_samples);
}

void run_biometric_tests(void) {
    RUN_TEST(biometrics_reset);
    RUN_TEST(biometrics_artifact_rejection_low);
    RUN_TEST(biometrics_normal_sequence);
    RUN_TEST(biometrics_relative_artifact);
}
