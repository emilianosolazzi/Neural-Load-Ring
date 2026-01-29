#include "wellness_manager.h"
#include "wellness_processor.h"
#include "../sensors/ppg_driver.h"
#include "../wellness_feedback/cue_processor.h"
#include "../wellness_feedback/actuator_controller.h"
#include <stddef.h>

static struct {
    hr_metrics_t metrics;
    bool autonomous_enabled;
    uint32_t last_check_ms;
    
    /* Buffer for RR intervals to be consumed by other tasks (e.g. BLE) */
    float rr_buffer[16];
    uint8_t rr_head;
    uint8_t rr_tail;
    uint8_t rr_count;
} s_manager;

void wellness_manager_init(void) {
    biometrics_reset(&s_manager.metrics);
    s_manager.autonomous_enabled = true; /* Default to ON for "Local Awareness" */
    s_manager.last_check_ms = 0;
    s_manager.rr_head = 0;
    s_manager.rr_tail = 0;
    s_manager.rr_count = 0;
}

void wellness_manager_tick(uint32_t now_ms) {
    float rr_ms;
    bool has_new_data = false;

    /* 1. Poll all available RR intervals from the buffer */
    while (ppg_get_rr(&rr_ms)) {
        if (biometrics_process_rr(&s_manager.metrics, rr_ms)) {
            has_new_data = true;
            
            /* Add to internal buffer for external consumers */
            if (s_manager.rr_count < 16) {
                s_manager.rr_buffer[s_manager.rr_head] = rr_ms;
                s_manager.rr_head = (s_manager.rr_head + 1) % 16;
                s_manager.rr_count++;
            }
        }
    }

    if (!has_new_data) return;

    /* 2. Evaluate autonomous feedback logic (Rate limited) */
    if (s_manager.autonomous_enabled && (now_ms - s_manager.last_check_ms >= 15000)) {
        s_manager.last_check_ms = now_ms;

        /* If stress score is high (> 0.7) and we have enough data (at least ~30 seconds) */
        if (s_manager.metrics.valid_samples > 30) {
            
            /* Prepare input for the cue processor */
            cue_input_t cue_in = {
                .timestamp_ms = now_ms,
                .stress_level = (uint8_t)(s_manager.metrics.stress_score * 100.0f),
                .coherence_pct = (uint8_t)((1.0f - s_manager.metrics.stress_score) * 100.0f),
                .confidence_pct = (s_manager.metrics.valid_samples > 60) ? 90 : 70,
                .micro_var_pct100 = (uint16_t)(s_manager.metrics.rmssd * 10.0f), /* Scaled RMSSD */
                .artifact_rate_pct = (uint8_t)((1.0f - (float)s_manager.metrics.valid_samples / s_manager.metrics.total_samples) * 100.0f),
                .stability_pct = 80 /* Placeholder for coherence stability */
            };
            
            cue_output_t cue_out;
            if (cue_processor_generate(&cue_in, &cue_out)) {
                /* Apply the generated cue to the actuators */
                actuator_apply_ble(
                    cue_out.thermal_intensity,
                    cue_out.thermal_duration_s,
                    cue_out.vib_pattern,
                    cue_out.vib_intensity,
                    now_ms
                );
            }
        }
    }
}

void wellness_manager_set_autonomous(bool enabled) {
    s_manager.autonomous_enabled = enabled;
}

const hr_metrics_t* wellness_manager_get_metrics(void) {
    return &s_manager.metrics;
}

bool wellness_manager_pop_rr(float *out_rr_ms) {
    if (!out_rr_ms || s_manager.rr_count == 0) return false;
    
    *out_rr_ms = s_manager.rr_buffer[s_manager.rr_tail];
    s_manager.rr_tail = (s_manager.rr_tail + 1) % 16;
    s_manager.rr_count--;
    return true;
}
