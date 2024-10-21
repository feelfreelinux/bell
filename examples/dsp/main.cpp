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
#include "AudioPipeline.h"
#include "AudioResampler.h"
#include "BellHTTPServer.h"
#include "BellTask.h"
#include "BellUtils.h"
#include "CentralAudioBuffer.h"
#include "Compressor.h"
#include "DecoderGlobals.h"
#include "EncodedAudioStream.h"
#include "HTTPClient.h"
#include "NamedPipeAudioSink.h"
#include "PortAudioSink.h"
#include "StreamInfo.h"
#include "SyslogLogger.h"
#include "TCPSocket.h"
#include "TLSSocket.h"

#include <BellDSP.h>
#include <BellLogger.h>

std::shared_ptr<bell::CentralAudioBuffer> audioBuffer;
std::atomic<bool> isPaused = false;

class AudioPlayer : bell::Task {
 public:
  std::unique_ptr<NamedPipeAudioSink> audioSink;
  std::unique_ptr<bell::BellDSP> dsp;

  AudioPlayer() : bell::Task("player", 1024, 0, 0) {
    this->audioSink = std::make_unique<NamedPipeAudioSink>();
    this->audioSink->setParams(48000, 2, 16);

    auto pipeline = std::make_shared<AudioPipeline>();
    auto resampler = std::make_shared<AudioResampler>();
    resampler->configure(2, bell::SampleRate::SR_44100,
                         bell::SampleRate::SR_48000);
    pipeline->addTransform(resampler);
    this->dsp = std::make_unique<bell::BellDSP>();
    this->dsp->applyPipeline(pipeline);
    startTask();
  }

  void runTask() override {
    uint8_t outputChunk[4096 * 2];
    while (true) {
      if (audioBuffer->hasAtLeast(64) || isPaused) {
        auto chunk = audioBuffer->readChunk();

        if (chunk != nullptr && chunk->pcmSize > 0) {
          auto res = this->dsp->process(
              chunk->pcmData, chunk->pcmSize, outputChunk, sizeof(outputChunk),
              2, SampleRate::SR_44100, bell::BitWidth::BW_16);

          this->audioSink->feedPCMFrames(outputChunk, res->numSamples * 4);
        }
      }
    }
  }
};

int main() {
  bell::setDefaultLogger();


  bell::createDecoders();
  audioBuffer = std::make_shared<bell::CentralAudioBuffer>(512);
  auto task = AudioPlayer();

  auto url = "https://s2.radio.co/s2b2b68744/listen";
  // std::ifstream file("aactest.aac", std::ios::binary);

  auto req = bell::HTTPClient::get(url);
  auto container = AudioContainers::guessAudioContainer(req->stream());
  auto codec = AudioCodecs::getCodec(container.get());

  uint32_t dataLen;
  while (true) {
    uint8_t* data = codec->decode(container.get(), dataLen);

    if (!data) {
      continue;
    }

    size_t toWrite = dataLen;
    while (toWrite > 0) {
      toWrite -= audioBuffer->writePCM(data + dataLen - toWrite, toWrite, 0);
    }

    // std::cout << dataLen << std::endl;
  }
  return 0;
}