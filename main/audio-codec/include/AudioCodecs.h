#pragma once

#include <memory>  // for shared_ptr

#include "CodecType.h"  // for AudioCodec

namespace bell {
class AudioContainer;
class BaseCodec;

class AudioCodecs {
 public:
  static std::shared_ptr<BaseCodec> getCodec(AudioCodec type);
  static std::shared_ptr<BaseCodec> getCodec(AudioContainer* container);
  static void addCodec(AudioCodec type,
                       const std::shared_ptr<BaseCodec>& codec);
};
}  // namespace bell
