#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <fstream>

#include "AudioSink.h"

class NamedPipeAudioSink : public AudioSink
{
public:
    NamedPipeAudioSink();
    ~NamedPipeAudioSink();
    void feedPCMFrames(const uint8_t *buffer, size_t bytes);
    
private:
    std::ofstream namedPipeFile;
};
