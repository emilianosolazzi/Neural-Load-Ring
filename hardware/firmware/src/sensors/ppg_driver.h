// ppg_driver.h
#include <stdint.h>

void ppg_init(void);

// Called by ISR or sampling loop with raw PPG sample (100Hz) and timestamp in ms.
void ppg_on_sample(float sample, uint32_t timestamp_ms);

// Pop next RR interval (ms) computed by the PPG peak detector. Returns 1 if a
// value was read, 0 otherwise.
int ppg_get_rr(float *out_rr_ms);