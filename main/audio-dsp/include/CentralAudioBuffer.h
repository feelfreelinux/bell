#pragma once

#include <atomic>
#include <cmath>
#include <memory>

#include "CircularBuffer.h"
#include "StreamInfo.h"
#include "BellUtils.h"

typedef std::function<void(std::string)> shutdownEventHandler;

namespace bell {
class CentralAudioBuffer {
  private:
	std::mutex accessMutex;

	std::atomic<bool> isLocked = false;

  public:
	CentralAudioBuffer(size_t size) {
		audioBuffer = std::make_shared<CircularBuffer>(size);
	}
	
	std::shared_ptr<bell::CircularBuffer> audioBuffer;
	uint32_t sampleRate		   = 44100;

	/**
	 * Sends an event which reconfigures current audio output
	 * @param format incoming sample format
	 * @param sampleRate data's sample rate
	 */
	void configureOutput(bell::BitWidth format, uint32_t sampleRate) {
		if (this->sampleRate != sampleRate) {
			this->sampleRate = sampleRate;
		}
	}

	/**
	 * Returns current sample rate
	 * @return sample rate
	 */
	uint32_t getSampleRate() {
		return sampleRate;
	}

	/**
	 * Clears input buffer, to be called for track change and such
	 */
	void clearBuffer() {
		audioBuffer->emptyExcept(this->sampleRate);
	}

	/**
	 * Locks access to audio buffer. Call after starting playback
	 */
	void lockAccess() {
		if (!isLocked) {
			clearBuffer();
			this->accessMutex.lock();
			isLocked = true;
		}
	}

	/**
	 * Frees access to the audio buffer. Call during shutdown
	 */
	void unlockAccess() {
		if (isLocked) {
			clearBuffer();
			this->accessMutex.unlock();
			isLocked = false;
		}
	}

	size_t read(uint8_t* dst, size_t bytes) {
		size_t readBytes = this->audioBuffer->read(dst, bytes);
		return readBytes;
	}

	/**
	 * Write audio data to the main buffer
	 * @param data pointer to raw PCM data
	 * @param bytes number of bytes to be read from provided pointer
	 * @return number of bytes read
	 */
	size_t write(const uint8_t *data, size_t bytes) {
		size_t bytesWritten = 0;
		while (bytesWritten < bytes) {
			auto write = audioBuffer->write(data + bytesWritten, bytes - bytesWritten);
			bytesWritten += write;

			if (write == 0) {
                //audioBuffer->dataSemaphore->wait();
				BELL_SLEEP_MS(10);
			}
		}

		return bytesWritten;
	}

	bool hasAtLeast(size_t bytes) {
		return audioBuffer->size() >= bytes;
	}
};

} // namespace bell