// Copyright (c) Kuba Szczodrzyński 2022-1-10.

#include "BellUtils.h"
#include "Mpeg4Container.h"
#include "Mpeg4Types.h"

using namespace bell;

/** Populate [chunks] using the Sample-to-chunk Table */
void Mpeg4Container::readStsc() {
	skipBytes(4); // skip version and flags
	chunksLen = readUint32();
	chunks = (Mpeg4ChunkRange *)malloc(chunksLen * sizeof(Mpeg4ChunkRange));
	for (uint32_t i = 0; i < chunksLen; i++) {
		chunks[i].count = readUint32();
		chunks[i].samples = readUint32();
		chunks[i].sampleDescriptionId = readUint32();
		if (i > 0) {
			chunks[i - 1].count = chunks[i].count - chunks[i - 1].count;
		}
	}
	if (chunkOffsetsLen) {
		chunks[chunksLen - 1].count = chunkOffsetsLen - chunks[chunksLen - 1].count + 1;
	}
}

/** Populate [chunkOffsets] using the Chunk Offset Table */
void Mpeg4Container::readStco() {
	skipBytes(4); // skip version and flags
	chunkOffsetsLen = readUint32();
	chunkOffsets = (Mpeg4ChunkOffset *)malloc(chunkOffsetsLen * sizeof(Mpeg4ChunkOffset));
	for (uint32_t i = 0; i < chunkOffsetsLen; i++) {
		chunkOffsets[i] = readUint32();
	}
	if (chunksLen) {
		chunks[chunksLen - 1].count = chunkOffsetsLen - chunks[chunksLen - 1].count + 1;
	}
}

/** Populate [samples] using the Time-to-sample Table */
void Mpeg4Container::readStts() {
	skipBytes(4); // skip version and flags
	samplesLen = readUint32();
	samples = (Mpeg4SampleRange *)malloc(samplesLen * sizeof(Mpeg4SampleRange));
	for (uint32_t i = 0; i < samplesLen; i++) {
		samples[i].count = readUint32();
		samples[i].duration = readUint32();
	}
}

/** Populate [sampleSizes] using the Sample Size Table */
void Mpeg4Container::readStsz() {
	skipBytes(4); // skip version and flags
	uint32_t sampleSize = readUint32();
	sampleSizesLen = readUint32();
	if (sampleSize) {
		sampleSizesLen = 1;
	}
	sampleSizes = (Mpeg4SampleSize *)malloc(sampleSizesLen * sizeof(Mpeg4SampleSize));
	if (sampleSize) {
		sampleSizes[0] = sampleSize;
		if (sampleSize > sampleSizeMax)
			sampleSizeMax = sampleSize;
		return;
	}
	for (uint32_t i = 0; i < sampleSizesLen; i++) {
		sampleSize = readUint32();
		if (sampleSize > sampleSizeMax)
			sampleSizeMax = sampleSize;
		sampleSizes[i] = sampleSize;
	}
	// reallocate sampleData if the max size changes
	allocSampleData();
}

/** Populate [sampleDesc] using the Sample Description Table */
void Mpeg4Container::readStsd() {
	freeAndNull((void *&)sampleDesc);
	skipBytes(4); // skip version and flags
	sampleDescLen = readUint32();
	sampleDesc = (SampleDescription *)malloc(sampleDescLen * sizeof(SampleDescription));
	for (SampleDescription *desc = sampleDesc; desc < sampleDesc + sampleDescLen; desc++) {
		uint32_t entryEnd = readUint32() - 4 + pos;
		desc->format = (AudioSampleFormat)readUint32();
		desc->mp4aObjectType = MP4AObjectType::UNDEFINED;
		desc->mp4aProfile = MP4AProfile::UNDEFINED;
		skipBytes(6); // reserved
		desc->dataReferenceIndex = readUint16();
		skipBytes(8); // reserved
		channelCount = readUint16();
		bitDepth = readUint16();
		skipBytes(4); // reserved
		sampleRate = readUint16();
		skipBytes(2); // decimal part of sample rate
		// read the child atom
		uint32_t atomSize;
		AtomType atomType;
		readAtomHeader(atomSize, (uint32_t &)atomType);
		if (atomType != AtomType::ATOM_ESDS) {
			desc->dataType = (uint32_t)atomType;
			desc->dataLength = atomSize;
			desc->data = (char *)malloc(desc->dataLength);
			readBytes(desc->data, desc->dataLength);
			continue;
		}
		// read ESDS
		skipBytes(4); // skip esds flags
		while (pos < entryEnd) {
			uint8_t tag = readUint8();
			uint32_t size = readVarint32();
			uint8_t flags;
			switch (tag) {
				case 0x03: // ES_Descriptor
					skipBytes(2);
					flags = readUint8();
					if (flags & 0b10000000)
						skipBytes(2);
					if (flags & 0b01000000)
						skipBytes(readUint8());
					if (flags & 0b00100000)
						skipBytes(2);
					break;
				case 0x04: // DecoderConfigDescriptor
					desc->mp4aObjectType = (MP4AObjectType)readUint8();
					skipBytes(12);
					break;
				case 0x05: // DecoderSpecificInfo
					if (desc->mp4aObjectType == MP4AObjectType::MP4A) {
						desc->mp4aProfile = (MP4AProfile)(readUint8() >> 3);
						skipBytes(size - 1);
					} else {
						desc->dataType = 0;
						desc->dataLength = size;
						desc->data = (char *)malloc(desc->dataLength);
						readBytes(desc->data, desc->dataLength);
					}
					break;
				default:
					skipBytes(size);
					break;
			}
		}
	}
}
