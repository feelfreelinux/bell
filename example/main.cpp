#include <atomic>
#include <memory>
#include <string>
#include <type_traits>

#include "BellTask.h"
#include "CentralAudioBuffer.h"
#include "PortAudioSink.h"
#include "StreamInfo.h"

#define DEBUG_LEVEL 4
#include <BellDSP.h>
#include <BellLogger.h>

std::shared_ptr<bell::CentralAudioBuffer> audioBuffer;
std::atomic<bool> isPaused = false;

class AudioPlayer : bell::Task {
 public:
  std::unique_ptr<PortAudioSink> audioSink;
  std::unique_ptr<bell::BellDSP> dsp;

  AudioPlayer() : bell::Task("player", 1024, 0, 0) {
    this->audioSink = std::make_unique<PortAudioSink>();
    this->audioSink->setParams(44100, 2, 16);
    this->dsp = std::make_unique<bell::BellDSP>(audioBuffer);
    startTask();
  }

  void runTask() override {
    while (true) {
      if (audioBuffer->hasAtLeast(64) || isPaused) {
        auto chunk = audioBuffer->readChunk();

        if (chunk != nullptr && chunk->pcmSize > 0) {
          this->dsp->process(chunk->pcmData, chunk->pcmSize, 2, 44100,
                             bell::BitWidth::BW_16);

          this->audioSink->feedPCMFrames(chunk->pcmData, chunk->pcmSize);
        }
      }
    }
  }
};

int main() {
  bell::setDefaultLogger();


  BELL_LOG(info, "cock", "Published?");

  return 0;
}
