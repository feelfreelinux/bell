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
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "win32shim.h"
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#endif
#include <sstream>
#include <fstream>
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

        void open(std::string host, uint16_t port)
        {
			int err;
            int domain = AF_INET;
            int socketType = SOCK_STREAM;

            struct addrinfo hints{}, *addr;
            //fine-tune hints according to which socket you want to open
            hints.ai_family = domain;
            hints.ai_socktype = socketType;
            hints.ai_protocol = IPPROTO_IP; // no enum : possible value can be read in /etc/protocols
            hints.ai_flags = AI_CANONNAME | AI_ALL | AI_ADDRCONFIG;

            // BELL_LOG(info, "http", "%s %d", host.c_str(), port);

            char portStr[6];
			sprintf(portStr, "%u", port);
			err = getaddrinfo(host.c_str(), portStr, &hints, &addr);
            if (err != 0) {
                throw std::runtime_error("Resolve failed");
            }

            sockFd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

			err = connect(sockFd, addr->ai_addr, addr->ai_addrlen);
            if (err < 0)
            {
                close();
                BELL_LOG(error, "http", "Could not connect to %s. Error %d", host.c_str(), errno);
                throw std::runtime_error("Resolve failed");
            }

            int flag = 1;
            setsockopt(sockFd,        /* socket affected */
                       IPPROTO_TCP,   /* set option at TCP level */
                       TCP_NODELAY,   /* name of option */
                       (char *)&flag, /* the cast is historical cruft */
                       sizeof(int));  /* length of option value */

            freeaddrinfo(addr);
            std::cout << "Socket opened" << std::endl;
        }

        size_t read(uint8_t *buf, size_t len) {
            return recv(sockFd, (char*) buf, len, 0);
        }

        size_t write(uint8_t *buf, size_t len) {
            return send(sockFd, (char*) buf, len, 0);
        }

		size_t poll() {
#ifdef _WIN32
            unsigned long value;
			ioctlsocket(sockFd, FIONREAD, &value);
#else
			int value;
			ioctl(sockFd, FIONREAD, &value);
#endif
			return value;
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
