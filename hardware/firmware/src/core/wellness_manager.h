/**
 * @file wellness_manager.h
 * @brief Orchestrates local biometric analysis and haptic feedback
 */

#ifndef WELLNESS_MANAGER_H
#define WELLNESS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "biometric_algorithms.h"

/**
 * @brief Initialize the wellness manager
 */
void wellness_manager_init(void);

/**
 * @brief Primary processing tick for the wellness system
 * 
 * Called periodically (e.g. 1Hz or 4Hz) to:
 *   - Pull new RR intervals from the PPG driver
 *   - Update HRV metrics
 *   - Evaluate stress levels
 *   - Trigger haptic cues if autonomous mode is enabled
 */
void wellness_manager_tick(uint32_t now_ms);

/**
 * @brief Enable or disable autonomous feedback mode
 */
void wellness_manager_set_autonomous(bool enabled);

/**
 * @brief Get latest computed metrics
 */
const hr_metrics_t* wellness_manager_get_metrics(void);

/**
 * @brief Pop an RR interval that has been processed by the manager.
 *        Useful for streaming RR intervals to BLE.
 * 
 * @param[out] out_rr_ms Pointer to store the RR interval
 * @return true if an RR was returned, false if queue is empty
 */
bool wellness_manager_pop_rr(float *out_rr_ms);

#endif // WELLNESS_MANAGER_H
