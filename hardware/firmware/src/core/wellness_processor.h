// wellness_processor.h
// Lightweight PPG peak detector (100Hz) producing RR intervals in milliseconds.
// Implements a simplified Pan-Tompkins style pipeline: DC removal -> derivative ->
// squaring -> moving integration -> adaptive threshold with refractory guard.

#include <stdint.h>

// Process a single PPG sample. Returns 1 when a new RR interval is produced and
// writes it (ms) to out_rr_ms. Returns 0 otherwise.
int wellness_process_sample(float sample, uint32_t timestamp_ms, float *out_rr_ms);

// Pop the next available RR interval (ms) from the ring buffer. Returns 1 if a
// value was read, 0 if the buffer is empty.
int wellness_pop_rr(float *out_rr_ms);

// Reset detector state (clears buffers, thresholds, timers).
void wellness_reset(void);

// Legacy entry point (no-op placeholder to keep compatibility if needed).
void wellness_process(void);