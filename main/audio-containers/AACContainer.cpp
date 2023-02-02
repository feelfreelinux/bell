#include "AACContainer.h"

using namespace bell;

#define SYNC_WORLD_LEN 4

AACContainer::AACContainer() {}
AACContainer::~AACContainer() {}

bool AACContainer::parse() {}

int32_t AACContainer::getLoadingOffset(uint32_t timeMs) {
  return SAMPLE_NOT_SEEKABLE;
}

bool AACContainer::seekTo(uint32_t timeMs) {
  return false;
}

int32_t AACContainer::getCurrentTimeMs() {
  return 0;
}

bool AACContainer::fillBuffer() {
  if (this->bytesInBuffer < AAC_MAX_FRAME_SIZE * 2) {
    this->bytesInBuffer +=
        this->source->read(buffer.data() + bytesInBuffer, buffer.size() - bytesInBuffer);
    std::cout << this->bytesInBuffer << std::endl;
  }
  return this->bytesInBuffer >= AAC_MAX_FRAME_SIZE * 2; 
}

uint8_t* AACContainer::readSample(uint32_t& len) {
  // Align the data if previous read was offseted
  if (dataOffset > 0) {
    memmove(buffer.data(), buffer.data() + dataOffset, bytesInBuffer - dataOffset);
    bytesInBuffer = bytesInBuffer - dataOffset;
    dataOffset = 0;
    std::cout << "le mem move" << std::endl;
  }

  if (!this->fillBuffer()) {
    len = 0;
    return nullptr;
  }

  int startOffset = AACFindSyncWord(this->buffer.data(), bytesInBuffer);

  if (pos < 0) {
    // Discard buffer
    this->bytesInBuffer = 0;
    len = 0;
    return nullptr;
  }

  dataOffset = AACFindSyncWord(this->buffer.data() + startOffset + SYNC_WORLD_LEN, bytesInBuffer - startOffset - SYNC_WORLD_LEN);
  if (dataOffset < 0) {
    // Discard buffer
    this->bytesInBuffer = 0;
    len = 0;
    return nullptr;
  }

  len = dataOffset + SYNC_WORLD_LEN;
  dataOffset += startOffset + SYNC_WORLD_LEN; 

  if (len == 0) {
    return nullptr;
  }

  return this->buffer.data() + startOffset;
}

uint8_t* AACContainer::getSetupData(uint32_t& len, AudioCodec matchCodec) {}

void AACContainer::feed(const std::shared_ptr<bell::ByteStream>& stream,
                        uint32_t position) {
  BaseContainer::feed(stream, position);
}
