// Copyright (c) Kuba Szczodrzyński 2022-1-7.

#include "BufferedStream.h"
#include <cstring>

BufferedStream::BufferedStream(
	const std::string &taskName,
	size_t bufferSize,
	size_t readThreshold,
	size_t readSize,
	size_t readyThreshold,
	size_t notReadyThreshold,
	bool waitForReady,
	bool endWithSource)
	: bell::Task(taskName, 1024, 5, 0) {
	this->bufferSize = bufferSize;
	this->readAt = bufferSize - readThreshold;
	this->readSize = readSize;
	this->readyThreshold = readyThreshold;
	this->notReadyThreshold = notReadyThreshold;
	this->waitForReady = waitForReady;
	this->endWithSource = endWithSource;
	this->buf = static_cast<uint8_t *>(malloc(bufferSize));
	this->bufEnd = buf + bufferSize;
	reset();
}

BufferedStream::~BufferedStream() {
	this->terminate = true;
	source = nullptr;
	free(buf);
}

void BufferedStream::reset() {
	this->bufReadPtr = this->buf;
	this->bufWritePtr = this->buf;
	this->readTotal = 0;
	this->readAvailable = 0;
	this->terminate = false;
}

bool BufferedStream::open(const std::shared_ptr<bell::ByteStream> &stream) {
	if (this->running)
		return false;
	this->source = stream;
	reset();
	startTask();
	return true;
}

void BufferedStream::close() {
	this->terminate = true;
	source = nullptr;
}

bool BufferedStream::isReady() const {
	return readAvailable >= readyThreshold;
}

bool BufferedStream::isNotReady() const {
	return readAvailable < notReadyThreshold;
}

size_t BufferedStream::skip(size_t len) {
	return read(nullptr, len);
}

size_t BufferedStream::position() {
	return readTotal;
}

size_t BufferedStream::size() {
	return source->size();
}

size_t BufferedStream::lengthBetween(uint8_t *me, uint8_t *other) {
	const std::lock_guard lock(readMutex);
	if (other <= me) {
		// buf .... other ...... me ........ bufEnd
		// buf .... me/other ........ bufEnd
		return bufEnd - me;
	} else {
		// buf ........ me ........ other .... bufEnd
		return other - me;
	}
}

size_t BufferedStream::read(uint8_t *dst, size_t len) {
	if (waitForReady && isNotReady()) {
		while (!isReady()) {}
	}
	if (!running && !readAvailable)
		return 0;
	size_t read = 0;
	size_t toReadTotal = std::min(readAvailable.load(), len);
	while (toReadTotal > 0) {
		size_t toRead = std::min(toReadTotal, lengthBetween(bufReadPtr, bufWritePtr));
		if (dst) {
			memcpy(dst, bufReadPtr, toRead);
			dst += toRead;
		}
		readAvailable -= toRead;
		bufReadPtr += toRead;
		if (bufReadPtr >= bufEnd)
			bufReadPtr = buf;
		toReadTotal -= toRead;
		read += toRead;
		readTotal += toRead;
	}
	this->readSem.give();
	return read;
}

void BufferedStream::runTask() {
	running = true;
	while (!terminate) {
		if (!source)
			break;
		if (isReady()) {
			// buffer ready, wait for any read operations
			this->readSem.wait();
		}
		if (readAvailable > readAt)
			continue;
		// here, the buffer needs re-filling
		size_t len;
		bool wasReady = isReady();
		do {
			size_t toRead = std::min(readSize, lengthBetween(bufWritePtr, bufReadPtr));
			if (!source)
				break;
			len = source->read(bufWritePtr, toRead);
			if (!len && endWithSource)
				terminate = true;
			readAvailable += len;
			bufWritePtr += len;
			if (bufWritePtr >= bufEnd) // TODO is == enough here?
				bufWritePtr = buf;
		} while (len && readSize < bufferSize - readAvailable); // loop until there's no more free space in the buffer
		// signal that buffer is ready for reading
		if (!wasReady && isReady()) {
			this->readySem.give();
		}
	}
	running = false;
	reset();
}
