// Copyright (c) Kuba Szczodrzyński 2022-1-12.

#include "AudioCodecs.h"

#ifdef BELL_CODEC_AAC
#include "AACDecoder.h"
static std::shared_ptr<AACDecoder> codecAac;
#endif

#ifdef BELL_CODEC_MP3
#include "MP3Decoder.h"
static std::shared_ptr<MP3Decoder> codecMp3;
#endif

#ifdef BELL_CODEC_VORBIS
#include "VorbisDecoder.h"
static std::shared_ptr<VorbisDecoder> codecVorbis;
#endif

#ifdef BELL_CODEC_OPUS
#include "OPUSDecoder.h"
static std::shared_ptr<OPUSDecoder> codecOpus;
#endif

std::shared_ptr<BaseCodec> AudioCodecs::getCodec(AudioCodec type) {
	switch (type) {
#ifdef BELL_CODEC_AAC
		case AudioCodec::AAC:
			if (codecAac)
				return codecAac;
			codecAac = std::make_shared<AACDecoder>();
			return codecAac;
#endif
#ifdef BELL_CODEC_MP3
		case AudioCodec::MP3:
			if (codecMp3)
				return codecMp3;
			codecMp3 = std::make_shared<MP3Decoder>();
			return codecMp3;
#endif
#ifdef BELL_CODEC_VORBIS
		case AudioCodec::VORBIS:
			if (codecVorbis)
				return codecVorbis;
			codecVorbis = std::make_shared<VorbisDecoder>();
			return codecVorbis;
#endif
#ifdef BELL_CODEC_OPUS
		case AudioCodec::OPUS:
			if (codecOpus)
				return codecOpus;
			codecOpus = std::make_shared<OPUSDecoder>();
			return codecOpus;
#endif
		default:
			return nullptr;
	}
}

std::shared_ptr<BaseCodec> AudioCodecs::getCodec(BaseContainer *container) {
	return getCodec(container->codec);
}
