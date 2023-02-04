#include "MP3Container.h"

using namespace bell;

#define SYNC_WORLD_LEN 4

MP3Container::MP3Container(std::istream& istr) : bell::AudioContainer(istr) {}

bool MP3Container::fillBuffer() {
  if (this->bytesInBuffer < MP3_MAX_FRAME_SIZE * 2) {
    this->istr.read((char*)buffer.data() + bytesInBuffer,
                    buffer.size() - bytesInBuffer);
    this->bytesInBuffer += istr.gcount();
  }
  return this->bytesInBuffer >= MP3_MAX_FRAME_SIZE * 2;
}

std::byte* MP3Container::readSample(uint32_t& len) {
  // Align the data if previous read was offseted
  if (dataOffset > 0) {
    memmove(buffer.data(), buffer.data() + dataOffset,
            bytesInBuffer - dataOffset);
    bytesInBuffer = bytesInBuffer - dataOffset;
    dataOffset = 0;
  }

  if (!this->fillBuffer()) {
    len = 0;
    return nullptr;
  }

  int startOffset =
      MP3FindSyncWord((uint8_t*)this->buffer.data(), bytesInBuffer);

  if (startOffset < 0) {
    // Discard buffer
    this->bytesInBuffer = 0;
    len = 0;
    return nullptr;
  }

  dataOffset = MP3FindSyncWord(
      (uint8_t*)this->buffer.data() + startOffset + SYNC_WORLD_LEN,
      bytesInBuffer - startOffset - SYNC_WORLD_LEN);
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

void MP3Container::parseSetupData() {
  channels = 2;
  sampleRate = bell::SampleRate::SR_44100;
  bitWidth = bell::BitWidth::BW_16;
}
