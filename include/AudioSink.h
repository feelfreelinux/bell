#ifndef AUDIOSINK_H
#define AUDIOSINK_H

#include <stdint.h>
#include <vector>

class AudioSink
{
  public:
	AudioSink() {}
	virtual ~AudioSink() {}
	virtual void feedPCMFrames(const uint8_t *buffer, size_t bytes) = 0;
	virtual void volumeChanged(uint16_t volume) {}
	bool softwareVolumeControl = true;
	bool usign = false;
};

#endif
