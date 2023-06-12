#include "ADTSContainer.h"

#include <cstring>  // for memmove

#include "StreamInfo.h"  // for BitWidth, BitWidth::BW_16, SampleRate, Sampl...
#include "sbr_crc_check.h"
// #include "aacdec.h"      // for AACFindSyncWord

using namespace bell;

#define SYNC_WORLD_LEN 4
#define SYNCWORDH 0xff
#define SYNCWORDL 0xf0

static int AACFindSyncWord(unsigned char* buf, int nBytes) {
  int i;

  /* find byte-aligned syncword (12 bits = 0xFFF) */
  for (i = 0; i < nBytes - 1; i++) {
    if ((buf[i + 0] & SYNCWORDH) == SYNCWORDH &&
        (buf[i + 1] & SYNCWORDL) == SYNCWORDL)
      return i;
  }

  return -1;
}

ADTSContainer::ADTSContainer(std::istream& istr) : bell::AudioContainer(istr) {}

bool ADTSContainer::fillBuffer() {
  if (this->bytesInBuffer < AAC_MAX_FRAME_SIZE * 2) {
    this->istr.read((char*)buffer.data() + bytesInBuffer,
                    buffer.size() - bytesInBuffer);
    this->bytesInBuffer += istr.gcount();
  }
  return this->bytesInBuffer >= AAC_MAX_FRAME_SIZE;
}

std::byte* ADTSContainer::readSample(uint32_t& len) {
  // Align the data if previous read was offseted
  if (toConsume > 0) {
    size_t consumedAmount = min(toConsume, bytesInBuffer);
    memmove(buffer.data(), buffer.data() + consumedAmount,
            buffer.size() - consumedAmount);
    bytesInBuffer = bytesInBuffer - consumedAmount;
    toConsume -= consumedAmount;
  }
  
  if (!this->fillBuffer()) {
    len = 0;
    return nullptr;
  }

  // int startOffset =
  //     AACFindSyncWord((uint8_t*)this->buffer.data(), bytesInBuffer);

  // if (startOffset < 0) {
  //   // Discard word
  //   toConsume = AAC_MAX_FRAME_SIZE;
  //   return nullptr;
  // }

  // toConsume = startOffset;
  len = bytesInBuffer;

  return this->buffer.data();
}

void ADTSContainer::parseSetupData() {
  channels = 2;
  sampleRate = bell::SampleRate::SR_44100;
  bitWidth = bell::BitWidth::BW_16;
}
