// wellness_processor.c - renamed from neural_load_core
// PPG peak detection for RR interval extraction at 100Hz.

#include "wellness_processor.h"
#include <stdbool.h>

// Tunable parameters for 100Hz PPG
#define PPG_FS_HZ              100U          // sample rate
#define MA_DC_WINDOW           5U            // 50 ms DC removal window
#define MA_INTEGRATOR_WINDOW   12U           // 120 ms moving integration
#define REFRACTORY_MS          300U          // ignore peaks within 300 ms
#define INITIAL_THRESHOLD      0.05f         // starting adaptive threshold
#define THRESH_DECAY           0.995f        // slow decay when no peaks
#define THRESH_BOOST_ALPHA     0.10f         // how fast threshold follows peaks

typedef struct {
	float dc_buffer[MA_DC_WINDOW];
	float integ_buffer[MA_INTEGRATOR_WINDOW];
	uint8_t dc_idx;
	uint8_t integ_idx;
	float dc_sum;
	float integ_sum;
	float prev_dc_removed;
	float threshold;
	uint32_t last_peak_ts_ms;
	bool initialized;
} ppg_peak_state_t;

static ppg_peak_state_t g_state = {0};

// RR ring buffer for downstream consumers (e.g., BLE telemetry)
#define RR_BUFFER_SIZE 32U
static float g_rr_buffer[RR_BUFFER_SIZE];
static uint8_t g_rr_head = 0U;
static uint8_t g_rr_tail = 0U;
static uint8_t g_rr_count = 0U;

static inline float moving_average_update(float *buffer, uint8_t *idx, uint8_t size, float *accum, float sample) {
	*accum -= buffer[*idx];
	buffer[*idx] = sample;
	*accum += sample;
	*idx = (uint8_t)((*idx + 1U) % size);
	return *accum / (float)size;
}

int wellness_process_sample(float sample, uint32_t timestamp_ms, float *out_rr_ms) {
	if (!out_rr_ms) return 0;

	// Initialize threshold and buffers on first call
	if (!g_state.initialized) {
		for (uint8_t i = 0; i < MA_DC_WINDOW; i++) g_state.dc_buffer[i] = sample;
		for (uint8_t i = 0; i < MA_INTEGRATOR_WINDOW; i++) g_state.integ_buffer[i] = 0.0f;
		g_state.dc_sum = sample * (float)MA_DC_WINDOW;
		g_state.integ_sum = 0.0f;
		g_state.prev_dc_removed = sample;
		g_state.threshold = INITIAL_THRESHOLD;
		g_state.last_peak_ts_ms = 0U;
		g_state.dc_idx = 0U;
		g_state.integ_idx = 0U;
		g_state.initialized = true;
	}

	// 1) DC removal via short moving average
	float dc_mean = moving_average_update(g_state.dc_buffer, &g_state.dc_idx, MA_DC_WINDOW, &g_state.dc_sum, sample);
	float dc_removed = sample - dc_mean;

	// 2) Derivative (emphasize rising edge) and 3) Squaring
	float diff = dc_removed - g_state.prev_dc_removed;
	g_state.prev_dc_removed = dc_removed;
	float squared = diff * diff;

	// 4) Moving integration (approximate energy over ~120 ms)
	float integ_avg = moving_average_update(g_state.integ_buffer, &g_state.integ_idx, MA_INTEGRATOR_WINDOW, &g_state.integ_sum, squared);

	// 5) Adaptive thresholding with refractory period
	bool refractory_ok = (g_state.last_peak_ts_ms == 0U) || ((timestamp_ms - g_state.last_peak_ts_ms) > REFRACTORY_MS);
	bool is_peak = refractory_ok && (integ_avg > g_state.threshold);

	if (is_peak) {
		// Update threshold toward current peak energy
		g_state.threshold = (1.0f - THRESH_BOOST_ALPHA) * g_state.threshold + THRESH_BOOST_ALPHA * integ_avg;

		// Emit RR if we have a previous peak
		if (g_state.last_peak_ts_ms > 0U) {
			*out_rr_ms = (float)(timestamp_ms - g_state.last_peak_ts_ms);

			// Push into RR ring buffer (drop oldest on overflow)
			if (g_rr_count == RR_BUFFER_SIZE) {
				g_rr_tail = (uint8_t)((g_rr_tail + 1U) % RR_BUFFER_SIZE);
				g_rr_count--;
			}
			g_rr_buffer[g_rr_head] = *out_rr_ms;
			g_rr_head = (uint8_t)((g_rr_head + 1U) % RR_BUFFER_SIZE);
			g_rr_count++;
		}

		g_state.last_peak_ts_ms = timestamp_ms;
		return (g_state.last_peak_ts_ms > 0U); // returns 1 when RR computed
	}

	// Slowly decay threshold to follow lower amplitudes
	g_state.threshold *= THRESH_DECAY;
	return 0;
}

void wellness_reset(void) {
	g_state = (ppg_peak_state_t){0};
	g_rr_head = g_rr_tail = g_rr_count = 0U;
}

int wellness_pop_rr(float *out_rr_ms) {
	if (!out_rr_ms || g_rr_count == 0U) return 0;
	*out_rr_ms = g_rr_buffer[g_rr_tail];
	g_rr_tail = (uint8_t)((g_rr_tail + 1U) % RR_BUFFER_SIZE);
	g_rr_count--;
	return 1;
}

// Legacy entry point kept for compatibility; does nothing in this implementation.
void wellness_process(void) {}