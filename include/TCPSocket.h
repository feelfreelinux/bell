#ifndef BELL_BASIC_SOCKET_H
#define BELL_BASIC_SOCKET_H

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
#include <BellLogger.h>

namespace bell
{
    class TCPSocket : public bell::Socket
    {
    private:
        int sockFd;
        bool isClosed = false;

    public:
        TCPSocket() {};
        ~TCPSocket() {
            close();
        };

        void open(std::string url)
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

            int domain = AF_INET;
            int socketType = SOCK_STREAM;

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
            setsockopt(sockFd,        /* socket affected */
                       IPPROTO_TCP,   /* set option at TCP level */
                       TCP_NODELAY,   /* name of option */
                       (char *)&flag, /* the cast is historical cruft */
                       sizeof(int));  /* length of option value */

            freeaddrinfo(addr);
        }

        size_t read(uint8_t *buf, size_t len) {
            return recv(sockFd, buf, len, 0);
        }

        size_t write(uint8_t *buf, size_t len) {
            return send(sockFd, buf, len, 0);
        }

        void close() {
            if (!isClosed) {
                ::close(sockFd);
                isClosed = true;
            }
        }
    };

}

#endif