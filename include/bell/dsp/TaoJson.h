#pragma once

#ifndef BELL_DISABLE_TAOJSON

// Library includes
#include "tao/json/forward.hpp"

// Bell includes
#include "bell/dsp/Error.h"
#include "bell/dsp/Spec.h"

namespace bell::dsp {

/**
 * @brief Read a pipeline description from JSON, for files and tools
 *
 *     { "version": 1,
 *       "nodes": [
 *         { "type": "gain", "channels": [0], "gain_db": -4 },
 *         { "type": "high_pass", "freq_hz": 80, "order": 4 }
 *       ] }
 *
 * The format is given by schema/dsp-pipeline.schema.json. Parsing is strict:
 * an unknown key, a missing one or two ways of stating the same thing is an
 * error naming the node. Whether values suit a stream is left to compile().
 */
Expected<PipelineSpec> parseTaoJson(const tao::json::value& json);

}  // namespace bell::dsp

#endif  // BELL_DISABLE_TAOJSON
