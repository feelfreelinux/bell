#include "HTTPStream.h"
#include <sstream>
#include "TCPSocket.h"
#include "picohttpparser.h"

using namespace bell;

void HTTPStream::Response::connect(const std::string& url) {
  urlParser = bell::URLParser::parse(url);

  // Create correct socket type
  if (urlParser.schema == "http") {
    this->socket = std::make_unique<bell::TCPSocket>();
  } else {
    this->socket = std::make_unique<bell::TLSSocket>();
  }

  // Open connection with socket
  this->socket->open(urlParser.host, urlParser.port);
}

HTTPStream::Response::~Response() {
  if (this->socket)
    this->socket->close();
}

size_t HTTPStream::Response::size() {
  return this->contentSize;
}

void HTTPStream::Response::close() {
  this->socket->close();
}

size_t HTTPStream::Response::position() {
  return this->bytesRead;
}

size_t HTTPStream::Response::skip(size_t nbytes) {
  size_t toSkip = nbytes;

  // Calculate amount of bytes to skip
  if (toSkip > (this->contentSize - bytesRead)) {
    toSkip = this->contentSize - bytesRead;
  }

  while (toSkip > 0) {
    size_t toRead = toSkip;
    if (toRead > this->httpBuffer.size()) {
      toRead = this->httpBuffer.size();
    }
    toSkip -= this->read(this->httpBuffer.data(), toRead);
  }

  return toSkip;
}

void HTTPStream::Response::rawRequest(const std::string& url,
                                      const std::string& method,
                                      const std::string& content,
                                      Headers& headers) {
  if (this->bytesRead < this->contentSize && hasContentSize) {
    this->skip(this->contentSize - this->bytesRead);
  }

  urlParser = bell::URLParser::parse(url);

  // Prepare a request
  std::stringstream httpRequest;
  const char* reqEnd = "\r\n";

  httpRequest << method << " " << urlParser.path << " HTTP/1.1" << reqEnd;
  httpRequest << "Host: " << urlParser.host << ":" << urlParser.port << reqEnd;
  httpRequest << "Connection: keep-alive" << reqEnd;
  httpRequest << "Accept: */*" << reqEnd;

  // Write content
  if (content.size() > 0) {
    httpRequest << "Content-Length: " << content.size() << reqEnd;
  }

  // Write headers
  for (auto& header : headers) {
    httpRequest << header.first << ": " << header.second << reqEnd;
  }

  httpRequest << reqEnd;
  std::string data = httpRequest.str();

  uint32_t len = socket->write((uint8_t*)data.c_str(), data.size());
  if (len != data.size()) {
    BELL_LOG(error, "http", "Writing failed: wrote %d of %d bytes", len,
             data.size());
    throw std::runtime_error("Cannot send the request");
  }

  // Parse response
  readResponseHeaders();
}

void HTTPStream::Response::readResponseHeaders() {
  char *method, *path;
  const char* msgPointer;

  size_t msgLen;
  int pret, minorVersion, status;

  size_t prevbuflen = 0, numHeaders;
  this->httpBufferAvailable = 0;
  this->bytesRead = 0;

  while (1) {
    size_t rret = socket->read(httpBuffer.data() + httpBufferAvailable,
                               httpBuffer.size() - httpBufferAvailable);

    if (rret <= 0)
      throw std::runtime_error("Cannot read response");
    prevbuflen = httpBufferAvailable;
    httpBufferAvailable += rret;

    // Parse the request
    numHeaders = sizeof(phResponseHeaders) / sizeof(phResponseHeaders[0]);

    pret =
        phr_parse_response((const char*)httpBuffer.data(), httpBufferAvailable,
                           &minorVersion, &status, &msgPointer, &msgLen,
                           phResponseHeaders, &numHeaders, prevbuflen);

    if (pret > 0) {
      initialBytesConsumed = pret;
      break; /* successfully parsed the request */
    } else if (pret == -1)
      throw std::runtime_error("Cannot parse http response");

    /* request is incomplete, continue the loop */
    assert(pret == -2);
    if (httpBufferAvailable == httpBuffer.size())
      throw std::runtime_error("Response too large");
  }

  this->responseHeaders = {};

  // Headers have benen read
  for (int headerIndex = 0; headerIndex < numHeaders; headerIndex++) {
    this->responseHeaders.push_back(
        ValueHeader{std::string(phResponseHeaders[headerIndex].name,
                                phResponseHeaders[headerIndex].name_len),
                    std::string(phResponseHeaders[headerIndex].value,
                                phResponseHeaders[headerIndex].value_len)});
  }

  std::string contentLengthValue = std::string(getHeader("content-length"));

  if (contentLengthValue.size() > 0) {
    this->hasContentSize = true;
    this->contentSize = std::stoi(contentLengthValue);
  }
}

void HTTPStream::Response::get(const std::string& url, Headers headers) {
  std::string method = "GET";
  return this->rawRequest(url, method, "", headers);
}

size_t HTTPStream::Response::contentLength() {
  return contentSize;
}

std::string_view HTTPStream::Response::getHeader(
    const std::string& headerName) {
  for (auto& header : this->responseHeaders) {
    std::string headerValue = header.first;
    std::transform(headerValue.begin(), headerValue.end(), headerValue.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (headerName == headerValue) {
      return header.second;
    }
  }

  return "";
}

size_t HTTPStream::Response::fullContentLength() {
  auto rangeHeader = getHeader("content-range");

  if (rangeHeader.find("/") != std::string::npos) {
    return std::stoi(
        std::string(rangeHeader.substr(rangeHeader.find("/") + 1)));
  }

  return this->contentLength();
}

size_t HTTPStream::Response::readRaw(uint8_t* dst, size_t bytes) {
  if (!socket)
    return 0;

  int32_t cachedAvailable =
      httpBufferAvailable - initialBytesConsumed - bytesRead;

  if (cachedAvailable > 0) {
    size_t toRead = bytes;
    if (toRead > cachedAvailable) {
      toRead = cachedAvailable;
    }

    memcpy(dst, httpBuffer.data() + initialBytesConsumed + bytesRead, toRead);
    bytesRead += toRead;
    return toRead;
  }

  size_t toRead = bytes;
  if (bytesRead + toRead > contentSize && hasContentSize) {
    toRead = contentSize - bytesRead;
  }

  size_t readAmount = socket->read(dst, toRead);
  if (hasContentSize)
    bytesRead += readAmount;

  return readAmount;
}

size_t HTTPStream::Response::readFull(uint8_t* dst, size_t bytes) {
  size_t toRead = bytes;
  while (toRead > 0) {
    toRead -= this->readRaw(dst + bytes - toRead, toRead);
  }

  return bytes;
}

size_t HTTPStream::Response::read(uint8_t* dst, size_t nbytes) {
  return readFull(dst, nbytes);
}

void HTTPStream::Response::readRawBody() {
  if (this->rawBody.size() == 0 && contentSize > 0) {
    this->rawBody = std::vector<uint8_t>(contentSize);

    size_t toRead = contentSize;

    while (toRead > 0) {
      toRead -= this->read(rawBody.data() + contentSize - toRead, toRead);
    }
  }
}

std::string_view HTTPStream::Response::body() {
  readRawBody();
  return std::string_view((char*)rawBody.data());
}

std::vector<uint8_t> HTTPStream::Response::bytes() {
  readRawBody();
  return rawBody;
}
