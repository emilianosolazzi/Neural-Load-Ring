// biometric_algorithms.h
#ifndef BIOMETRIC_ALGORITHMS_H
#define BIOMETRIC_ALGORITHMS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float rmssd;
    float sdnn;
    float mean_rr_ms;
    float stress_score;
    uint32_t valid_samples;
    float last_rr_ms;
    float mean_diff_sq;       /**< Internal state for incremental RMSSD */
    uint32_t total_samples;   /**< Total samples seen (including artifacts) */
    
    /* Adaptive Baseline Tracking */
    float baseline_rmssd;     /**< Long-term average RMSSD (User Normal) */
    bool baseline_established;/**< True if enough data collected to trust baseline */
} hr_metrics_t;

void biometrics_reset(hr_metrics_t *p_metrics);
bool biometrics_process_rr(hr_metrics_t *p_metrics, float rr_ms);
void compute_biometrics(void);

#endif
