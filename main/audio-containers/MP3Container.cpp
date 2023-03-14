#include "MP3Container.h"

using namespace bell;

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
  if (!this->fillBuffer()) {
    len = 0;
    return nullptr;
  }

  // Align the data if previous read was offseted
  if (toConsume > 0 && toConsume <= bytesInBuffer) {
    memmove(buffer.data(), buffer.data() + toConsume,
            buffer.size() - toConsume);
    bytesInBuffer = bytesInBuffer - toConsume;
    toConsume = 0;
  }

  int startOffset =
      MP3FindSyncWord((uint8_t*)this->buffer.data(), bytesInBuffer);

  if (startOffset < 0) {
    // Discard word
    toConsume = MP3_MAX_FRAME_SIZE;
    return nullptr;
  }

  len = bytesInBuffer - startOffset;

  return this->buffer.data() + startOffset;
}

void MP3Container::parseSetupData() {
  channels = 2;
  sampleRate = bell::SampleRate::SR_44100;
  bitWidth = bell::BitWidth::BW_16;
}
