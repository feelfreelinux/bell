#include <memory.h>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include "BellHTTPServer.h"
#include "BellTask.h"
#include "CentralAudioBuffer.h"
#include "DecoderGlobals.h"
#include "EncodedAudioStream.h"
#include "HTTPStream.h"
#include "PortAudioSink.h"
#include "Compressor.h"

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

        if (chunk.pcmSize > 0) {
          this->dsp->process(chunk.pcmData, chunk.pcmSize, 2,
                             bell::SampleRate::SR_44100, bell::BitWidth::BW_16);

          this->audioSink->feedPCMFrames(chunk.pcmData, chunk.pcmSize);
        }
      }
    }
  }
};

int main() {
  bell::setDefaultLogger();
  bell::createDecoders();

  auto dupa = std::make_unique<bell::BellHTTPServer>(8080);

  dupa->registerGet("/pause", [&dupa](struct mg_connection* conn) {
    isPaused = !isPaused;
    return dupa->makeJsonResponse("alo");
  });
  auto url = "http://193.222.135.71/378";

  auto req = bell::HTTPStream::get(url);
  audioBuffer = std::make_shared<bell::CentralAudioBuffer>(512);

  auto audioStream = std::make_shared<bell::EncodedAudioStream>();
  audioStream->openWithStream(std::move(req));
  return 0;
  auto task = AudioPlayer();

  std::vector<uint8_t> frameData(1024 * 10);

  while (true) {
    size_t bytes = audioStream->decodeFrame(frameData.data());
    std::cout << bytes <<std::endl;

    size_t toWrite = bytes;

    if (!isPaused) {
      while (toWrite > 0) {
        toWrite -= audioBuffer->writePCM(frameData.data() + bytes - toWrite,
                                         toWrite, 0);
      }
    }
  }
  return 0;
}
