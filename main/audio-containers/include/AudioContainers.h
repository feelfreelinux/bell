#pragma once

#include <iostream>
#include <memory>
#include "AACContainer.h"
#include "AudioContainer.h"
#include "MP3Container.h"

namespace bell::AudioContainers {
std::unique_ptr<bell::AudioContainer> guessAudioContainer(std::istream& istr);
}  // namespace bell::AudioContainers
