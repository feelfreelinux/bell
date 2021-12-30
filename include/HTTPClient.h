#ifndef BELL_HTTP_CLIENT
#define BELL_HTTP_CLIENT

#include "BellSocket.h"
#include "TCPSocket.h"
#include "platform/TLSSocket.h"
#include <map>
#include <memory>
#include <string>

#define BUF_SIZE 128

namespace bell {
class HTTPClient {
  public:
	enum HTTPMethod {
		GET,
		POST,
	};

	struct HTTPRequest {
		HTTPMethod method = HTTPMethod::GET;
		std::string url;
		std::string body;
		std::map<std::string, std::string> headers;
		std::string contentType;
		int maxRedirects = -1;
	};

	struct HTTPResponse {
		std::shared_ptr<bell::Socket> socket;

		std::map<std::string, std::string> headers;

		uint16_t statusCode;
		size_t contentLength;
		std::string contentType;
		std::string location;
		bool isChunked = false;
		bool isGzip = false;
		bool isComplete = false;
		bool isRedirect = false;
		size_t redirectCount = 0;

		void close() {
			socket->close();
			free(buf);
			buf = nullptr;
			bufPtr = nullptr;
		}

		void readHeaders();
		size_t read(char *dst, size_t len);
		std::string readToString();

	  private:
		char *buf = nullptr;	// allocated buffer
		char *bufPtr = nullptr; // reading pointer within buf
		size_t bodyRead = 0;
		size_t bufRemaining = 0;
		size_t chunkRemaining = 0;
		bool isStreaming = false;
		size_t readRaw(char *dst);
		bool skip(size_t len, bool dontRead = false);
	};

  private:
	static void executeImpl(const struct HTTPRequest &request, const char *url, struct HTTPResponse *&response);
	static bool readHeader(const char *&header, const char *name);

  public:
	static struct HTTPResponse *execute(const struct HTTPRequest &request);
};
} // namespace bell

#endif
