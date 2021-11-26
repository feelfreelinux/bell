#include "HTTPStream.h"

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <netinet/tcp.h>

bell::HTTPStream::HTTPStream()
{
}

bell::HTTPStream::~HTTPStream()
{
    close();
}

void bell::HTTPStream::close()
{
    if (status != StreamStatus::CLOSED)
    {
        status = StreamStatus::CLOSED;
        BELL_LOG(info, "httpStream", "Closing socket");
        ::close(this->sockFd);
    }
}

void bell::HTTPStream::connectToUrl(std::string url)
{
    // remove https or http from url
    url.erase(0, url.find("://") + 3);

    // split by first "/" in url
    std::string hostUrl = url.substr(0, url.find('/'));
    std::string pathUrl = url.substr(url.find('/'));

    std::string portString = "80";
    // check if hostUrl contains ':'
    if (hostUrl.find(':') != std::string::npos)
    {
        // split by ':'
        std::string host = hostUrl.substr(0, hostUrl.find(':'));
        portString = hostUrl.substr(hostUrl.find(':') + 1);
        hostUrl = host;
    }

    int domain = AF_INET;         // IP_v4
    int socketType = SOCK_STREAM; // Sequenced, reliable, connection-based byte streams.

    addrinfo hints, *addr;
    //fine-tune hints according to which socket you want to open
    hints.ai_family = domain;
    hints.ai_socktype = socketType;
    hints.ai_protocol = IPPROTO_IP; // no enum : possible value can be read in /etc/protocols
    hints.ai_flags = AI_CANONNAME | AI_ALL | AI_ADDRCONFIG;

    BELL_LOG(info, "http", "%s %s", hostUrl.c_str(), portString.c_str());

    if (getaddrinfo(hostUrl.c_str(), portString.c_str(), &hints, &addr) != 0)
    {
        BELL_LOG(error, "webradio", "DNS lookup error");
        throw std::runtime_error("Resolve failed");
    }

    sockFd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (connect(sockFd, addr->ai_addr, addr->ai_addrlen) < 0)
    {
        close();
        BELL_LOG(error, "http", "Could not connect to %s", url.c_str());
        throw std::runtime_error("Resolve failed");
    }

    int flag = 1;
    setsockopt(sockFd,  /* socket affected */
               IPPROTO_TCP,   /* set option at TCP level */
               TCP_NODELAY,   /* name of option */
               (char *)&flag, /* the cast is historical cruft */
               sizeof(int));  /* length of option value */

    freeaddrinfo(addr);

    // Prepare HTTP get header
    std::stringstream ss;
    ss << "GET " << pathUrl << " HTTP/1.1\r\n"
       << "Host: " << hostUrl << ":" << portString << "\r\n"
       << "Accept: */*\r\n"
       << "\r\n\r\n";

    std::string request = ss.str();

    // Send the request
    if (send(sockFd, request.c_str(), request.length(), 0) != (int)request.length())
    {
        close();
        BELL_LOG(error, "http", "Can't send request");
        throw std::runtime_error("Resolve failed");
    }

    status = StreamStatus::READING_HEADERS;
    auto buffer = std::vector<uint8_t>(128);
    auto currentLine = std::string();
    auto statusOkay = false;
    auto readingData = false;
    // Read data on socket sockFd line after line
    int nbytes;

    while (status == StreamStatus::READING_HEADERS)
    {
        nbytes = recv(sockFd, &buffer[0], buffer.size(), 0);
        if (nbytes < 0)
        {
            BELL_LOG(error, "http", "Error reading from client");
            perror("recv");
            exit(EXIT_FAILURE);
        }
        else if (nbytes == 0)
        {
            BELL_LOG(error, "http", "Client disconnected");
            close();
        }
        else
        {
            currentLine += std::string(buffer.data(), buffer.data() + nbytes);
            while (currentLine.find("\r\n") != std::string::npos)
            {
                auto line = currentLine.substr(0, currentLine.find("\r\n"));
                currentLine = currentLine.substr(currentLine.find("\r\n") + 2, currentLine.size());
                BELL_LOG(info, "http", "Line: %s", line.c_str());

                // handle redirects:
                if (line.find("Location:") != std::string::npos)
                {
                    auto newUrl = line.substr(10);
                    BELL_LOG(info, "http", "Redirecting to %s", newUrl.c_str());

                    close();
                    return connectToUrl(newUrl);
                }
                else if (line.find("200 OK") != std::string::npos)
                {
                    statusOkay = true;
                }
                else if (line.size() == 0 && statusOkay)
                {
                    BELL_LOG(info, "http", "Ready to receive data!");
                    status = StreamStatus::READING_DATA;
                }
            }
        }
    }
}

size_t bell::HTTPStream::read(uint8_t *buf, size_t nbytes)
{
    if (status != StreamStatus::READING_DATA)
    {
        BELL_LOG(error, "http", "Not ready to read data");
        return 0;
    }

    int nread = recv(sockFd, buf, nbytes, 0);
    if (nread < 0)
    {
        BELL_LOG(error, "http", "Error reading from client %d", sockFd);
        close();

        perror("recv");
        exit(EXIT_FAILURE);
    }
    else if (nread == 0)
    {
        BELL_LOG(error, "http", "Client disconnected");
    }
    return nread;
}

size_t bell::HTTPStream::skip(size_t nbytes)
{
    return 0;
}