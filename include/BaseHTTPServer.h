#ifndef BELL_BASE_HTTP_SERV
#define BELL_BASE_HTTP_SERV

#include <cstdio>

namespace bell {

class ResponseReader {
  public:
    ResponseReader(){};
    virtual ~ResponseReader() = default;

    virtual size_t getTotalSize() = 0;
    virtual size_t read(char *buffer, size_t size) = 0;
};

class FileResponseReader : public ResponseReader {
  public:
    FILE *file;
    size_t fileSize;
    FileResponseReader(std::string fileName) {
        file = fopen(fileName.c_str(), "r");
        fseek(file, 0, SEEK_END); // seek to end of file
        fileSize = ftell(file);   // get current file pointer
        fseek(file, 0, SEEK_SET); // seek back to beginning of file
    };
    ~FileResponseReader() { fclose(file); };

    size_t read(char *buffer, size_t size) {
        return fread(buffer, 1, size, file);
    }

    size_t getTotalSize() { return fileSize; }
};

enum class RequestType { GET, POST };

struct HTTPRequest {
    std::map<std::string, std::string> urlParams;
    std::map<std::string, std::string> queryParams;
    std::string body;
    int handlerId;
    int connection;
};

struct HTTPResponse {
    int connectionFd;
    int status;
    bool useGzip = false;
    std::string body;
    std::string contentType;
    std::unique_ptr<ResponseReader> responseReader;
};

typedef std::function<void(HTTPRequest &)> httpHandler;
struct HTTPRoute {
    RequestType requestType;
    httpHandler handler;
};

struct HTTPConnection {
    std::vector<uint8_t> buffer;
    std::string currentLine = "";
    int contentLength = 0;
    bool isReadingBody = false;
    std::string httpMethod;
    bool toBeClosed = false;
    bool isEventConnection = false;
};

class BaseHTTPServer {
public:
    BaseHTTPServer() {};
    virtual ~BaseHTTPServer() = default;

    int serverPort;
    virtual void registerHandler(RequestType requestType, const std::string &,
                                 httpHandler) = 0;
    virtual void respond(const HTTPResponse &) = 0;
    virtual void listen() = 0;
};
} // namespace bell

#endif
