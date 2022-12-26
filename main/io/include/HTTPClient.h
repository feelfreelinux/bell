#pragma once

#include <core_http_client.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <string_view>

#include "plaintext_posix.h"
#include "mbedtls_posix.h"
#include "sockets_posix.h"

#include "transport_interface.h"

#include "core_http_client.h"

#include "URLParser.h"

struct NetworkContext {
  void* pParams;
};

namespace bell {
class HTTPClient {
 public:
  typedef std::pair<std::string, std::string> ValueHeader;

  struct RangeHeader {
    int32_t from;
    int32_t to;

    static RangeHeader range(int32_t from, int32_t to) {
      return {.from = from, .to = to};
    }

    static RangeHeader last(int32_t nbytes) {
      return {.from = -nbytes, .to = HTTP_RANGE_REQUEST_END_OF_FILE};
    }
  };

  typedef std::variant<ValueHeader, RangeHeader> Header;
  typedef std::vector<Header> Headers;

  class Response {
   public:
    ~Response();
    void connect(const std::string& url, size_t bufSize);

    void get(const std::string& url, Headers headers = {});

    std::string_view body();
    std::vector<uint8_t> bytes();

    std::string_view getHeader(const std::string& headerName);

    size_t read(uint8_t* dst, size_t bytes);
    size_t contentLength();
    size_t fullContentLength();

   private:
    bell::URLParser urlParser;

    std::vector<uint8_t> userBuffer;

    // CoreHTTP specific
    PlaintextParams_t plaintextParams = {0};
    MbedTLSParams_t mbedtlsParams = {};
    NetworkContext_t networkContext{};
    HTTPResponse_t response = {0};

    TransportInterface_t transportInterface = {
        .pNetworkContext = &networkContext};
  };

  enum class Method : uint8_t { GET = 0, POST = 1 };

  struct Request {
    std::string url;
    Method method;
    Headers headers;
  };

  // Per request input buffer
  static const size_t DEFAULT_USER_BUFFER_LEN = 1024 * 8;

  static std::unique_ptr<Response> get(const std::string& url, Headers headers = {}, size_t bufSize = DEFAULT_USER_BUFFER_LEN) {
    auto response = std::make_unique<Response>();
    response->connect(url, bufSize);
    response->get(url, headers);
    return response;
  }
};
}  // namespace bell2
