// biometric_algorithms.c - renamed from hrv_algorithms
#include "biometric_algorithms.h"

// Placeholder
#include <math.h>
#include <string.h>

/* Physiological limits (verified in wellnessEngine.v1.0.ts) */
#define MIN_RR_MS           300.0f
#define MAX_RR_MS           2000.0f
#define MAX_RR_CHANGE_ALPHA 0.20f  /* 20% max beat-to-beat change */

#define BASELINE_ALPHA      0.005f /* Slow adaptation for baseline (approx 200 samples to shift significantly) */
#define MIN_BASELINE_SAMPLES 60    /* Require ~1 min of data before trusting baseline */
#define DEFAULT_BASELINE_RMSSD 40.0f /* Fallback starting point */

void biometrics_reset(hr_metrics_t *p_metrics) {
    if (p_metrics) {
        memset(p_metrics, 0, sizeof(hr_metrics_t));
        p_metrics->baseline_rmssd = DEFAULT_BASELINE_RMSSD;
        p_metrics->baseline_established = false;
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

    /* 4. Adaptive Baseline and Personalized Stress Scoring */
    if (p_metrics->rmssd > 0) {
        /* Update baseline: Only update if strictly valid and we are somewhat stable.
           We use a very slow moving average to learn the user's "normal". */
        if (p_metrics->valid_samples > 10) {
            p_metrics->baseline_rmssd = (BASELINE_ALPHA * p_metrics->rmssd) + ((1.0f - BASELINE_ALPHA) * p_metrics->baseline_rmssd);
        }
        
        if (p_metrics->valid_samples > MIN_BASELINE_SAMPLES) {
            p_metrics->baseline_established = true;
        }

        /* Calculate Stress Score relative to PERSONALIZED baseline */
        /* If RMSSD is at baseline, stress is 0.3 (relaxed alert). 
           If RMSSD is 50% of baseline, stress is high (0.8).
           If RMSSD is 150% of baseline, stress is low (0.0). */
           
        float ratio = p_metrics->rmssd / p_metrics->baseline_rmssd;
        float stress_raw;
        
        /* Sigmoid-like mapping tailored for HRV */
        if (ratio >= 1.5f) {
            stress_raw = 0.0f; /* Very relaxed / recovery */
        } else if (ratio <= 0.5f) {
            stress_raw = 1.0f; /* High acute stress */
        } else {
            /* Linear interpolation between 0.5 (1.0 stress) and 1.5 (0.0 stress) */
            stress_raw = 1.0f - (ratio - 0.5f); 
        }

        /* Clamp */
        if (stress_raw < 0.0f) stress_raw = 0.0f;
        if (stress_raw > 1.0f) stress_raw = 1.0f;
        
        /* Smooth the output score */
        if (p_metrics->valid_samples == 1) {
            p_metrics->stress_score = stress_raw;
        } else {
            p_metrics->stress_score = (0.2f * stress_raw) + (0.8f * p_metrics->stress_score);
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
