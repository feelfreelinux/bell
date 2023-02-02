#pragma once

#include <memory>
#include "BaseCodec.h"
#include "BaseContainer.h"

namespace bell {

enum class AudioCodec {
  UNKNOWN = 0,
  AAC = 1,
  MP3 = 2,
  VORBIS = 3,
  OPUS = 4,
  FLAC = 5,
};

class AudioCodecs {
 public:
  static std::shared_ptr<BaseCodec> getCodec(AudioCodec type);
  static std::shared_ptr<BaseCodec> getCodec(BaseContainer* container);
  static void addCodec(AudioCodec type,
                       const std::shared_ptr<BaseCodec>& codec);
};
}  // namespace bell
