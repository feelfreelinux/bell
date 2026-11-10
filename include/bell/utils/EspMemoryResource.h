#pragma once

#include "bell/Logger.h"
#ifdef ESP_PLATFORM

#include <memory>
#include <memory_resource>
#include <new>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace bell::utils {

// std::pmr implementation for ESP-IDF using heap capabilities
template <uint32_t Caps>
class EspMemoryResource : public std::pmr::memory_resource {
 public:
  EspMemoryResource() = default;
  ~EspMemoryResource() override = default;

  EspMemoryResource(const EspMemoryResource&) = delete;
  EspMemoryResource& operator=(const EspMemoryResource&) = delete;

 protected:
  void* do_allocate(size_t bytes, size_t alignment) override {
    void* ptr = heap_caps_aligned_alloc(alignment, bytes, Caps);

    // Could not allocate
    if (ptr == nullptr) {
      BELL_LOG(error, "EspMemoryResource",
               "OOM! Failed to allocate {} bytes (align {}) with caps 0x{}",
               bytes, alignment, (unsigned long)Caps);
      abort();
    }

    return ptr;
  }

  void do_deallocate(void* p, size_t bytes, size_t alignment) override {
    (void)bytes;
    (void)alignment;
    heap_caps_free(p);
  }

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
};

using DmaMemoryResource =
    EspMemoryResource<MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT>;

using InternalMemoryResource =
    EspMemoryResource<MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT>;

using PsramMemoryResource =
    EspMemoryResource<MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT>;

// Static instance of the DMA memory resource
static DmaMemoryResource dmaMemoryResource{};

// Static instance of the internal memory resource
static InternalMemoryResource internalMemoryResource{};

// Static instance of the PSRAM memory resource
static PsramMemoryResource psramMemoryResource{};

// For objects an ISR touches, which must sit in internal RAM, as a default
// malloc above the SPIRAM threshold goes to PSRAM first
template <typename T>
struct InternalDeleter {
  void operator()(T* p) const {
    p->~T();
    internalMemoryResource.deallocate(p, sizeof(T), alignof(T));
  }
};

template <typename T>
using InternalPtr = std::unique_ptr<T, InternalDeleter<T>>;

// Cpp allocation helper that goes to the internal memory
template <typename T, typename... Args>
InternalPtr<T> makeInternal(Args&&... args) {
  void* memory = internalMemoryResource.allocate(sizeof(T), alignof(T));
  return InternalPtr<T>(new (memory) T(std::forward<Args>(args)...));
}

}  // namespace bell::utils

#endif  // ESP_PLATFORM
