/**
 * @file cue_to_signature.h
 * @brief Neural Load Ring - Cue to Signature Feel Mapping
 *
 * Bridges the cue processor's decisions to the signature feel patterns,
 * ensuring all haptic feedback follows the ring's personality and vocabulary.
 *
 * Copyright (c) 2024-2026 Neural Load Ring Project
 * SPDX-License-Identifier: MIT
 */

#ifndef CUE_TO_SIGNATURE_H
#define CUE_TO_SIGNATURE_H

#include "cue_processor.h"
#include "signature_feel.h"

/**
 * @brief Map a cue output to the appropriate signature pattern
 *
 * Translates cue processor decisions into the ring's consistent haptic
 * vocabulary, ensuring all feedback has the same emotional quality.
 *
 * @param cue       Output from cue_processor_generate()
 * @return          Mapped signature pattern
 */
signature_pattern_t cue_to_signature_pattern(const cue_output_t *cue);

/**
 * @brief Execute a cue using the signature feel system
 *
 * All-in-one function that takes a cue output, maps it to the appropriate
 * signature pattern, and begins playback with proper intensity scaling.
 *
 * @param cue       Output from cue_processor_generate()
 */
void cue_execute_as_signature(const cue_output_t *cue);

/**
 * @brief Mapping table definition (for testing/customization)
 */
typedef struct {
    cue_type_t     cue_type;
    cue_priority_t priority;
    signature_pattern_t pattern;
} cue_signature_mapping_t;

/**
 * @brief Get the mapping table for inspection/testing
 */
const cue_signature_mapping_t* cue_get_signature_mappings(uint8_t *count);

#endif /* CUE_TO_SIGNATURE_H */
