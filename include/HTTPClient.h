#ifndef BELL_HTTP_CLIENT
#define BELL_HTTP_CLIENT

#include "BellSocket.h"
#include "TCPSocket.h"
#include "platform/TLSSocket.h"
#include <memory>
#include <string>

namespace bell {
class HTTPClient {
    enum class ClientStatus
    {
        OPENING,
        READING_HEADERS,
        READING_DATA,
        CLOSED
    };

    private:
        void readHeaders();
        std::vector<uint8_t> remainingData = std::vector<uint8_t>();
    public:
        ClientStatus status = ClientStatus::OPENING;
        std::shared_ptr<bell::Socket> dataSocket;
        int contentLength;
        bool hasFixedSize = false;
        void close();

        HTTPClient() {};
        ~HTTPClient() {
            close();
        };

        void executeGET(const std::string& url);
        void executePOST(const std::string& url, const std::string& body, const std::string& contentType);
        size_t readFromSocket(uint8_t* dataPtr, size_t len);

        std::string readToString();
};
}

#endif
