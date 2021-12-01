#ifndef BELL_HTTP_SERVER_H
#define BELL_HTTP_SERVER_H

#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <regex>
#include <optional>
#include <set>
#include <iostream>
#include <queue>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <BellLogger.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sys/socket.h>
#include <string>
#include <netdb.h>
#include <mutex>
#include <fcntl.h>

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace bell
{
    class ResponseReader
    {
    public:
        ResponseReader(){};
        virtual ~ResponseReader() = default;

        virtual size_t getTotalSize() = 0;
        virtual size_t read(char *buffer, size_t size) = 0;
    };

    class FileResponseReader : public ResponseReader
    {
    public:
        FILE *file;
        size_t fileSize;
        FileResponseReader(std::string fileName)
        {
            file = fopen(fileName.c_str(), "r");
            fseek(file, 0, SEEK_END); // seek to end of file
            fileSize = ftell(file);       // get current file pointer
            fseek(file, 0, SEEK_SET); // seek back to beginning of file
        };
        ~FileResponseReader()
        {
            fclose(file);
        };

        size_t read(char *buffer, size_t size)
        {
            return fread(buffer, 1, size, file);
        }

        size_t getTotalSize()
        {
            return fileSize;
        }
    };

    enum class RequestType
    {
        GET,
        POST
    };

    struct HTTPRequest
    {
        std::map<std::string, std::string> urlParams;
        std::map<std::string, std::string> queryParams;
        std::string body;
        int handlerId;
        int connection;
    };

    struct HTTPResponse
    {
        int connectionFd;
        int status;
        bool useGzip = false;
        std::string body;
        std::string contentType;
        std::unique_ptr<ResponseReader> responseReader;
    };

    typedef std::function<void(HTTPRequest &)> httpHandler;
    struct HTTPRoute
    {
        RequestType requestType;
        httpHandler handler;
    };

    struct HTTPConnection
    {
        std::vector<uint8_t> buffer;
        std::string currentLine = "";
        int contentLength = 0;
        bool isReadingBody = false;
        std::string httpMethod;
        bool toBeClosed = false;
        bool isEventConnection = false;
    };

    class HTTPServer
    {
    private:
        std::regex routerPattern = std::regex(":([^\\/]+)?");
        fd_set master;
        fd_set readFds;
        fd_set activeFdSet, readFdSet;

        bool isClosed = true;
        bool writingResponse = false;

        std::map<std::string, std::vector<HTTPRoute>> routes;
        std::map<int, HTTPConnection> connections;
        void writeResponse(const HTTPResponse &);
        void writeResponseEvents(int connFd);
        void findAndHandleRoute(std::string &, std::string &, int connectionFd);

        std::vector<std::string> splitUrl(const std::string &url, char delimiter);
        std::mutex responseMutex;
        std::vector<char> responseBuffer = std::vector<char>(128);

        void readFromClient(int clientFd);
        std::map<std::string, std::string> parseQueryString(const std::string &queryString);
        unsigned char h2int(char c);
        std::string urlDecode(std::string str);

    public:
        HTTPServer(int serverPort);

        int serverPort;
        void registerHandler(RequestType requestType, const std::string &, httpHandler);
        void respond(const HTTPResponse &);
        void publishEvent(std::string eventName, std::string eventData);
        void closeConnection(int connection);
        void listen();
    };
}
#endif
