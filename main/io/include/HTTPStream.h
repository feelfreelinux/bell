
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "BellSocket.h"
#include "ByteStream.h"
#include "TCPSocket.h"
#include "TLSSocket.h"
#include "URLParser.h"
#include "fmt/core.h"
#include "picohttpparser.h"

namespace bell {
class HTTPStream {
 public:
  // most basic header type, represents by a key-val
  typedef std::pair<std::string, std::string> ValueHeader;

  typedef std::vector<ValueHeader> Headers;

  // Helper over ValueHeader, formatting a HTTP bytes range
  struct RangeHeader {
    static ValueHeader range(int32_t from, int32_t to) {
      return ValueHeader{"Range", fmt::format("bytes={}-{}", from, to)};
    }

    static ValueHeader last(int32_t nbytes) {
      return ValueHeader{"Range", fmt::format("bytes=-{}", nbytes)};
    }
  };

  class Response : public bell::ByteStream {
   public:
    Response() {};
    ~Response();

    /**
    * Initializes a connection with a given url.
    */
    void connect(const std::string& url);

    void rawRequest(const std::string& method, const std::string& url,
                    const std::string& content, Headers& headers);
    void get(const std::string& url, Headers headers = {});

    std::string_view body();
    std::vector<uint8_t> bytes();

    std::string_view getHeader(const std::string& headerName);

    size_t readRaw(uint8_t* dst, size_t bytes);
    size_t readFull(uint8_t* dst, size_t bytes);
    size_t contentLength();
    size_t fullContentLength();

    // ByteStream implementation ---
    size_t read(uint8_t* dst, size_t bytes) override;
    size_t skip(size_t nbytes) override;
    size_t position() override;
    size_t size() override;
    void close() override;
    //

   private:
    bell::URLParser urlParser;
    std::unique_ptr<bell::Socket> socket;

    struct phr_header phResponseHeaders[32];
    const size_t HTTP_BUF_SIZE = 1024;

    std::vector<uint8_t> httpBuffer = std::vector<uint8_t>(HTTP_BUF_SIZE);
    std::vector<uint8_t> rawBody = std::vector<uint8_t>();

    size_t contentSize = 0;
    size_t bytesRead = 0;
    size_t httpBufferAvailable = 0;
    size_t initialBytesConsumed = 0;
    bool hasContentSize = false;


    Headers responseHeaders;

    void readResponseHeaders();
    void readRawBody();
  };

  enum class Method : uint8_t { GET = 0, POST = 1 };

  struct Request {
    std::string url;
    Method method;
    Headers headers;
  };

  // Per request input buffer
  static const size_t DEFAULT_USER_BUFFER_LEN = 1024 * 8;

  static std::unique_ptr<Response> get(
      const std::string& url, Headers headers = {},
      size_t bufSize = DEFAULT_USER_BUFFER_LEN) {
    auto response = std::make_unique<Response>();
    response->connect(url);
    response->get(url, headers);
    return response;
  }
};
}  // namespace bell
