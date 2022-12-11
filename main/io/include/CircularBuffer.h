#pragma once

#include <algorithm>
#include <memory>
#include <cstring>
#include <mutex>
#include <vector>
#include "WrappedSemaphore.h"


namespace bell {
class CircularBuffer {
  public:
	CircularBuffer(size_t dataCapacity);

    std::unique_ptr<bell::WrappedSemaphore> dataSemaphore;

	size_t size() const {
		return dataSize;
	}

	size_t capacity() const {
		return dataCapacity;
	}

	size_t write(const uint8_t *data, size_t bytes);
	size_t read(uint8_t *data, size_t bytes);
	void emptyBuffer();
	void emptyExcept(size_t size);

  private:
	std::mutex bufferMutex;
	size_t begIndex		= 0;
	size_t endIndex		= 0;
	size_t dataSize		= 0;
	size_t dataCapacity = 0;
	std::vector<uint8_t> buffer;
};
} // namespace bell