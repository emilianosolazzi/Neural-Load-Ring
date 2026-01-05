// ppg_driver.c
#include "ppg_driver.h"
#include "../core/wellness_processor.h"

void ppg_init(void) {}

// Ingest a single PPG sample and forward to the peak detector
void ppg_on_sample(float sample, uint32_t timestamp_ms) {
	float rr_ms = 0.0f;
	(void)wellness_process_sample(sample, timestamp_ms, &rr_ms);
}

// Retrieve next RR interval (ms) from detector buffer
int ppg_get_rr(float *out_rr_ms) {
	return wellness_pop_rr(out_rr_ms);
}