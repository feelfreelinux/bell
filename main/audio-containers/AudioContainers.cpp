#include "AudioContainers.h"

#include <string.h>  // for memcmp
#include <cstddef>   // for byte

#include "AACContainer.h"  // for AACContainer
#include "CodecType.h"     // for bell
#include "MP3Container.h"  // for MP3Container

namespace bell {
class AudioContainer;
}  // namespace bell

using namespace bell;

std::unique_ptr<bell::AudioContainer> AudioContainers::guessAudioContainer(
    std::istream& istr) {
  std::byte tmp[14];
  istr.read((char*)tmp, sizeof(tmp));

  if (memcmp(tmp, "\xFF\xF1", 2) == 0 || memcmp(tmp, "\xFF\xF9", 2) == 0) {
    // AAC found
    std::cout << "AAC" << std::endl;
    return std::make_unique<bell::AACContainer>(istr);
  } else if (memcmp(tmp, "\xFF\xFB", 2) == 0 ||
             memcmp(tmp, "\x49\x44\x33", 3) == 0) {
    // MP3 Found
    std::cout << "MP3" << std::endl;

    return std::make_unique<bell::MP3Container>(istr);
  }

  return nullptr;
}
