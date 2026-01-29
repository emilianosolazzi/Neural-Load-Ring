// biometric_algorithms.c - renamed from hrv_algorithms
#include "biometric_algorithms.h"

// Placeholder
#include <math.h>
#include <string.h>

/* Physiological limits (verified in wellnessEngine.v1.0.ts) */
#define MIN_RR_MS           300.0f
#define MAX_RR_MS           2000.0f
#define MAX_RR_CHANGE_ALPHA 0.20f  /* 20% max beat-to-beat change */

void biometrics_reset(hr_metrics_t *p_metrics) {
    if (p_metrics) {
        memset(p_metrics, 0, sizeof(hr_metrics_t));
    }
}

bool biometrics_process_rr(hr_metrics_t *p_metrics, float rr_ms) {
    if (!p_metrics) return false;

    /* 1. Artifact Rejection: Level 1 - Absolute limits */
    if (rr_ms < MIN_RR_MS || rr_ms > MAX_RR_MS) {
        return false;
    }

    /* 2. Artifact Rejection: Level 2 - Relative change (Malik et al., 1996) */
    if (p_metrics->valid_samples > 0) {
        float diff = fabsf(rr_ms - p_metrics->last_rr_ms);
        float max_allowed = p_metrics->last_rr_ms * MAX_RR_CHANGE_ALPHA;
        if (diff > max_allowed) {
            return false;
        }
    }

    /* 3. Update Metrics (Incremental RMSSD and Mean) */
    if (p_metrics->valid_samples > 0) {
        float diff = rr_ms - p_metrics->last_rr_ms;
        float diff_sq = diff * diff;
        
        /* Incremental RMSSD calculation (using EMA for firmware efficiency) */
        float alpha = 0.1f; /* Smoothing factor */
        if (p_metrics->valid_samples == 1) {
            p_metrics->mean_diff_sq = diff_sq;
        } else {
            p_metrics->mean_diff_sq = (alpha * diff_sq) + ((1.0f - alpha) * p_metrics->mean_diff_sq);
        }
        p_metrics->rmssd = sqrtf(p_metrics->mean_diff_sq);
    }

    /* Update mean RR */
    if (p_metrics->valid_samples == 0) {
        p_metrics->mean_rr_ms = rr_ms;
    } else {
        float alpha_mean = 0.05f;
        p_metrics->mean_rr_ms = (alpha_mean * rr_ms) + ((1.0f - alpha_mean) * p_metrics->mean_rr_ms);
    }

    /* Update stress score (Simplified: lower RMSSD relative to baseline = higher stress) */
    if (p_metrics->rmssd > 0) {
        /* Goal: Map 10ms-100ms RMSSD to 1.0-0.0 stress score.
           We use a more robust sigmoid or linear mapping. */
        float stress;
        if (p_metrics->rmssd < 10.0f) {
            stress = 1.0f;
        } else if (p_metrics->rmssd > 100.0f) {
            stress = 0.0f;
        } else {
            stress = (100.0f - p_metrics->rmssd) / 90.0f;
        }
        
        /* Apply some smoothing to the stress score itself */
        if (p_metrics->valid_samples == 1) {
            p_metrics->stress_score = stress;
        } else {
            p_metrics->stress_score = (0.2f * stress) + (0.8f * p_metrics->stress_score);
        }
    }

    p_metrics->last_rr_ms = rr_ms;
    p_metrics->valid_samples++;
    p_metrics->total_samples++;

    return true;
}

void compute_biometrics(void) {
    /* Legacy placeholder - metrics are now updated per RR interval */
}
