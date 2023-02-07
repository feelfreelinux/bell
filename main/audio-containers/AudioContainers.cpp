#include "AudioContainers.h"

using namespace bell;

std::unique_ptr<bell::AudioContainer> AudioContainers::guessAudioContainer(
    std::istream& istr) {
  std::byte tmp[14];
  istr.read((char*)tmp, sizeof(tmp));

  if (std::memcmp(tmp, "\xFF\xF1", 2) == 0 ||
      std::memcmp(tmp, "\xFF\xF9", 2) == 0) {
    // AAC found
    std::cout << "AAC" << std::endl;
    return std::make_unique<bell::AACContainer>(istr);
  } else if (std::memcmp(tmp, "\xFF\xFB", 2) == 0 ||
             std::memcmp(tmp, "\x49\x44\x33", 3) == 0) {
    // MP3 Found
    std::cout << "MP3" << std::endl;

    return std::make_unique<bell::MP3Container>(istr);
  }

  return nullptr;
}
