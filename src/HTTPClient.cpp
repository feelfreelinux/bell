#include "HTTPClient.h"
#include "TCPSocket.h"

void bell::HTTPClient::close() {
	if (status != ClientStatus::CLOSED) {
        dataSocket->close();
        status = ClientStatus::CLOSED;
    }
}

void bell::HTTPClient::executeGET(const std::string &constUrl) {
    dataSocket = std::make_shared<TLSSocket>();
    dataSocket->open(constUrl);

    std::string url = constUrl;
    // remove https or http from url
    url.erase(0, url.find("://") + 3);

    // split by first "/" in url
    std::string hostUrl = url.substr(0, url.find('/'));
    std::string pathUrl = url.substr(url.find('/'));
    std::string portString = "443";

    // check if hostUrl contains ':'
    if (hostUrl.find(':') != std::string::npos)
    {
        // split by ':'
        std::string host = hostUrl.substr(0, hostUrl.find(':'));
        portString = hostUrl.substr(hostUrl.find(':') + 1);
        hostUrl = host;
    }

    // Prepare HTTP get header
    std::stringstream ss;
    ss << "GET " << pathUrl << " HTTP/1.1\r\n"
       << "Host: " << hostUrl << ":" << portString << "\r\n"
       << "Accept: */*\r\n"
       << "\r\n\r\n";

    std::string request = ss.str();

    // Send the request
    if (dataSocket->write((uint8_t*)request.c_str(), request.length()) != (int)request.length())
    {
        dataSocket->close();
        BELL_LOG(error, "http", "Can't send request");
        throw std::runtime_error("Resolve failed");
    }

    status = ClientStatus::READING_HEADERS;
    readHeaders();
}

void bell::HTTPClient::executePOST(const std::string &constUrl, const std::string& body, const std::string& contentType) {
    dataSocket = std::make_shared<TLSSocket>();
    dataSocket->open(constUrl);

    std::string url = constUrl;
    // remove https or http from url
    url.erase(0, url.find("://") + 3);

    // split by first "/" in url
    std::string hostUrl = url.substr(0, url.find('/'));
    std::string pathUrl = url.substr(url.find('/'));
    std::string portString = "443";

    // check if hostUrl contains ':'
    if (hostUrl.find(':') != std::string::npos)
    {
        // split by ':'
        std::string host = hostUrl.substr(0, hostUrl.find(':'));
        portString = hostUrl.substr(hostUrl.find(':') + 1);
        hostUrl = host;
    }

    // Prepare HTTP get header
    std::stringstream ss;
    ss << "POST " << pathUrl << " HTTP/1.1\r\n"
       << "Host: " << hostUrl << ":" << portString << "\r\n"
       << "Accept: */*\r\n"
       << "Content-type: " << contentType << "\r\n"
       << "Content-length: " << body.length() << "\r\n"
       << "\r\n" << body;

    std::string request = ss.str();

    // Send the request
    if (dataSocket->write((uint8_t*)request.c_str(), request.length()) != (int)request.length())
    {
        close();
        BELL_LOG(error, "http", "Can't send request");
        throw std::runtime_error("Resolve failed");
    }

    status = ClientStatus::READING_HEADERS;
    readHeaders();
}

void bell::HTTPClient::readHeaders() {
    auto buffer = std::vector<uint8_t>(128);
    auto currentLine = std::string();
    auto statusOkay = false;
    auto readingData = false;
    // Read data on socket sockFd line after line
    int nbytes;

    while (status == ClientStatus::READING_HEADERS)
    {
        nbytes = dataSocket->read(&buffer[0], buffer.size());
        if (nbytes < 0)
        {
            BELL_LOG(error, "http", "Error reading from client");
            perror("recv");
            exit(EXIT_FAILURE);
        }
        else if (nbytes == 0)
        {
            BELL_LOG(error, "http", "Client disconnected");
            dataSocket->close();
        }
        else
        {
            currentLine += std::string(buffer.data(), buffer.data() + nbytes);
            while (currentLine.find("\r\n") != std::string::npos && status == ClientStatus::READING_HEADERS)
            {
                auto line = currentLine.substr(0, currentLine.find("\r\n"));
                currentLine = currentLine.substr(currentLine.find("\r\n") + 2, currentLine.size());

                // handle redirects:
                if (line.find("Location:") != std::string::npos)
                {
                    auto newUrl = line.substr(10);
                    BELL_LOG(info, "http", "Redirecting to %s", newUrl.c_str());

                    dataSocket->close();
                    return;
                    //return connectToUrl(newUrl);
                }
                // handle content-length
                if (line.find("Content-Length:") != std::string::npos)
                {
                    auto contentLengthStr = line.substr(16);
                    BELL_LOG(info, "http", "Content size %s", contentLengthStr.c_str());

                    // convert contentLengthStr to size_t
                    this->contentLength = std::stoi(contentLengthStr);
                    hasFixedSize = true;
                }
                else if (line.find("Transfer-Encoding: chunked") != std::string::npos)
                {
                    BELL_LOG(info, "http", "Transfer encoding chunked");
                    this->isChunked = true;
                } 
                else if (line.find("200 OK") != std::string::npos)
                {
                    statusOkay = true;
                }
                else if (line.size() == 0 && statusOkay)
                {
                    BELL_LOG(info, "http", "Ready to receive data! %d", currentLine.size());
                    if (currentLine.size() > 0) {
                        remainingData = std::vector<uint8_t>(currentLine.size());
                        remainingData.assign(currentLine.begin(), currentLine.end());
                    }
                    status = ClientStatus::READING_DATA;
                }
            }
        }
    }
}

size_t bell::HTTPClient::readFromSocket(uint8_t * data, size_t len) {
    if (remainingData.size() > 0) {
        if (len >= remainingData.size()) {
            auto sizeToCopy = remainingData.size();
            std::copy(remainingData.data(), remainingData.data() + sizeToCopy, data);
            remainingData.clear();
            return sizeToCopy;
        } else {
            std::copy(remainingData.data(), remainingData.data() + len, data);
            remainingData.erase(remainingData.begin(), remainingData.begin() + len);
            return len;
        }
    } else {
        return dataSocket->read(data, len);
    }
}

std::string bell::HTTPClient::readToString() {
    std::string result;
    size_t read;
    std::vector<char> buffer = std::vector<char>(128);

    if (this->isChunked) {
        size_t chunkSize;
        size_t chunkRemaining = 0;
        size_t bufRemaining;
        char *bufPos;
        do {
            read = readFromSocket((uint8_t *)buffer.data(), 128);
            bufPos = buffer.data();
            bufPos[read] = '\0';
            bufRemaining = read;
            while (bufRemaining) {
                if (chunkRemaining) {
                    auto count = std::min(bufRemaining, chunkRemaining);
                    result += std::string(bufPos, bufPos + count);
                    bufPos += count;
                    chunkRemaining -= count;
                    bufRemaining -= count;
                } else if (bufRemaining > 2) {
                    auto *chunkSizePos = bufPos;
                    bufPos = strstr(bufPos, "\r\n") + 2;
                    bufRemaining -= bufPos - chunkSizePos;
                    auto chunkSizeHex = std::string(chunkSizePos, bufPos - 2);
                    chunkSize = std::stoul(chunkSizeHex, nullptr, 16);
                    if (!chunkSize) {
                        break;
                    }
                    chunkRemaining = chunkSize;
                }
            }
        } while (chunkSize);
        return result;
    }

    std::vector<uint8_t> buffer = std::vector<uint8_t>(128);
    do {
        read = readFromSocket(buffer.data(), 128);
        result += std::string(buffer.data(), buffer.data() + read); 
    } while (result.find("\r\n\r\n") == std::string::npos);


    return result.substr(4, result.size() - 10);
}
