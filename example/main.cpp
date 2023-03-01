#include <memory.h>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include "AudioCodecs.h"
#include "AudioContainers.h"
#include "BellHTTPServer.h"
#include "BellTar.h"
#include "BellTask.h"
#include "CentralAudioBuffer.h"
#include "Compressor.h"
#include "DecoderGlobals.h"
#include "EncodedAudioStream.h"
#include "HTTPClient.h"
#include "PortAudioSink.h"
#define DEBUG_LEVEL 4
#include "X509Bundle.h"
#include "mbedtls/debug.h"

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

  std::fstream file("system.tar", std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    std::cout << "file not open" << std::endl;
    return 1;
  }

  BellTar::reader reader(file);
  reader.extract_all_files("./dupa2");
  return 0;
}
