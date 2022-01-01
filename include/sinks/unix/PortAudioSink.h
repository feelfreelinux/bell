#pragma once

#include <vector>
#include "portaudio.h"
#include <stdint.h>
#include <iostream>
#include "AudioSink.h"

class PortAudioSink : public AudioSink
{
public:
    PortAudioSink();
    ~PortAudioSink();
    void feedPCMFrames(const uint8_t *buffer, size_t bytes);
    
private:
    PaStream *stream;
};
