#pragma once

#include <memory_resource>

#ifdef ESP_PLATFORM
#include "bell/utils/EspMemoryResource.h"
#endif

namespace bell::dsp {

// Memory the audio thread reaches fast: internal DRAM on the ESP32, the
// default heap elsewhere
inline std::pmr::memory_resource* getInternalMemoryResource() {
#ifdef ESP_PLATFORM
  return &bell::utils::internalMemoryResource;
#else
  return std::pmr::get_default_resource();
#endif
}

}  // namespace bell::dsp
