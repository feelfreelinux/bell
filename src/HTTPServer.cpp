#include "HTTPServer.h"

bell::HTTPServer::HTTPServer(int serverPort)
{
    this->serverPort = serverPort;
}

unsigned char bell::HTTPServer::h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

std::string bell::HTTPServer::urlDecode(std::string str)
{
    std::string encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str[i];
      if (c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str[i];
        i++;
        code1=str[i];
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
    }
    
   return encodedString;
}

std::vector<std::string> bell::HTTPServer::splitUrl(const std::string &url, char delimiter)
{
    std::stringstream ssb(url);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(ssb, segment, delimiter))
    {
        seglist.push_back(segment);
    }
    return seglist;
}

void bell::HTTPServer::registerHandler(RequestType requestType, const std::string &routeUrl, httpHandler handler)
{
    BELL_LOG(info, "http", routeUrl.c_str());
    if (routes.find(routeUrl) == routes.end())
    {
        routes.insert({routeUrl, std::vector<HTTPRoute>()});
    }
    this->routes[routeUrl].push_back(HTTPRoute{
        .requestType = requestType,
        .handler = handler,
    });
}

void bell::HTTPServer::listen()
{
    BELL_LOG(info, "http", "Starting configuration server at port %d", this->serverPort);

    int fdMax;
    socklen_t addrlen;

    // setup address
    struct addrinfo hints, *server;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, std::to_string(serverPort).c_str(), &hints, &server);

    int sockfd = socket(server->ai_family,
                        server->ai_socktype, server->ai_protocol);
    pipe(pipeFd);
    struct sockaddr_in clientname;
    socklen_t incomingSockSize;
    int i;
    int yes = true;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    bind(sockfd, server->ai_addr, server->ai_addrlen);
    ::listen(sockfd, 10);

    FD_ZERO(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);
    FD_SET(pipeFd[0], &activeFdSet);

    for (;;)
    {
        /* Block until input arrives on one or more active sockets. */
        readFdSet = activeFdSet;
        if (select(FD_SETSIZE, &readFdSet, NULL, NULL, NULL) < 0)
        {
            BELL_LOG(error, "http", "Error in select");
            perror("select");
            exit(EXIT_FAILURE);
        }

        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET(i, &readFdSet))
            {
                if (i == sockfd)
                {
                    /* Connection request on original socket. */
                    int newFd;
                    incomingSockSize = sizeof(clientname);
                    newFd = accept(sockfd, (struct sockaddr *)&clientname, &incomingSockSize);
                    if (newFd < 0)
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }

                    FD_SET(newFd, &activeFdSet);

                    HTTPConnection conn = {
                        .buffer = std::vector<uint8_t>(128)};

                    this->connections.insert({newFd, conn});
                }
                else if (i == pipeFd[0])
                {
                    char dummy;
                    read(pipeFd[0], &dummy, 1);

                    if (responseQueue.size() > 0)
                    {
                        auto response = responseQueue.front();
                        responseQueue.pop();
                        writeResponse(response);
                    }
                }
                else
                {
                    /* Data arriving on an already-connected socket. */
                    readFromClient(i);
                }
            }
    }
}

void bell::HTTPServer::readFromClient(int clientFd)
{
    HTTPConnection &conn = this->connections[clientFd];

    int nbytes = recv(clientFd, &conn.buffer[0], conn.buffer.size(), 0);
    if (nbytes < 0)
    {
        BELL_LOG(error, "http", "Error reading from client");
        perror("recv");
        exit(EXIT_FAILURE);
    }
    else if (nbytes == 0)
    {
        this->closeConnection(clientFd);
    }
    else
    {
        conn.currentLine += std::string(conn.buffer.data(), conn.buffer.data() + nbytes);
        if (!conn.isReadingBody)
        {
            while (conn.currentLine.find("\r\n") != std::string::npos)
            {
                auto line = conn.currentLine.substr(0, conn.currentLine.find("\r\n"));
                conn.currentLine = conn.currentLine.substr(conn.currentLine.find("\r\n") + 2, conn.currentLine.size());
                if (line.find("GET ") != std::string::npos || line.find("POST ") != std::string::npos)
                {
                    BELL_LOG(info, "http", "Got following request");
                    BELL_LOG(info, "http", line.c_str());
                    conn.httpMethod = line;
                }

                if (line.find("Content-Length: ") != std::string::npos)
                {
                    conn.contentLength = std::stoi(line.substr(16, line.size() - 1));
                    BELL_LOG(info, "http", "Content-Length: %d", conn.contentLength);
                }
                if (line.size() == 0)
                {
                    if (conn.contentLength != 0) {
                        conn.isReadingBody = true;
                    } else {
                        BELL_LOG(info, "http", conn.httpMethod.c_str());
                        findAndHandleRoute(conn.httpMethod, conn.currentLine, clientFd);
                    }
                }
            }
        }
        else
        {
            BELL_LOG(info, "http", "Got %d bytes of body", conn.contentLength);
            if (conn.currentLine.size() >= conn.contentLength)
            {
                findAndHandleRoute(conn.httpMethod, conn.currentLine, clientFd);
                closeConnection(clientFd);
            }
        }
    }
}

void bell::HTTPServer::closeConnection(int connection)
{
    close(connection);
    FD_CLR(connection, &activeFdSet);
    this->connections.erase(connection);
}

void bell::HTTPServer::writeResponse(const HTTPResponse &response)
{
    std::stringstream stream;
    stream << "HTTP/1.1 " << response.status << " OK\r\n";
    stream << "Server: EUPHONIUM\r\n";
    stream << "Connection: close\r\n";
    stream << "Content-type: " << response.contentType << "\r\n";
    stream << "Content-length:" << response.body.size() << "\r\n";
    stream << "Access-Control-Allow-Origin: *\r\n";
    stream << "\r\n";
    stream << response.body;

    auto responseStr = stream.str();
    write(response.connectionFd, responseStr.c_str(), responseStr.size());
    this->closeConnection(response.connectionFd);
}

void bell::HTTPServer::respond(const HTTPResponse &response)
{
    // add response to response queue
    responseQueue.push(response);

    // write one byte to pipeFd[1]
    int dummy = 1;
    write(pipeFd[1], &dummy, 1);
}

std::map<std::string, std::string> bell::HTTPServer::parseQueryString(const std::string &queryString)
{
    std::map<std::string, std::string> query;
    auto prefixedString = "&" + queryString;
    while (prefixedString.find("&") != std::string::npos) {
        auto keyStart = prefixedString.find("&");
        auto keyEnd = prefixedString.find("=");
        // Find second occurence of "&" in prefixedString
        auto valueEnd = prefixedString.find("&", keyStart + 1);
        if (valueEnd == std::string::npos) {
            valueEnd = prefixedString.size();
        }

        auto key = prefixedString.substr(keyStart + 1, keyEnd - 1);
        auto value = prefixedString.substr(keyEnd + 1, valueEnd - keyEnd - 1);
        BELL_LOG(info, "http", "Key: %s, Value: %s", key.c_str(), value.c_str());
        query[key] = urlDecode(value);
        prefixedString = prefixedString.substr(valueEnd);
    }

    return query;
}

void bell::HTTPServer::findAndHandleRoute(std::string &url, std::string &body, int connectionFd)
{
    BELL_LOG(info, "http", url.c_str());
    std::map<std::string, std::string> pathParams;
    std::map<std::string, std::string> queryParams;

    for (const auto &routeSet : this->routes)
    {
        for (const auto &route : routeSet.second)
        {

            std::string path = url;
            if (url.find("GET ") != std::string::npos && route.requestType == RequestType::GET)
            {
                path = path.substr(4);
            }
            else if (url.find("POST ") != std::string::npos && route.requestType == RequestType::POST)
            {
                BELL_LOG(info, "http", "GOT POST, path");
                path = path.substr(5);
                BELL_LOG(info, "http", path.c_str());
            }
            else
            {
                continue;
            }

            path = path.substr(0, path.find(" "));

            BELL_LOG(info, "http", routeSet.first.c_str());
            BELL_LOG(info, "http", path.c_str());

            if (path.find("?") != std::string::npos)
            {
                auto urlEncodedSplit = splitUrl(path, '?');
                path = urlEncodedSplit[0];
                queryParams = this->parseQueryString(urlEncodedSplit[1]);
            }

            auto routeSplit = splitUrl(routeSet.first, '/');
            auto urlSplit = splitUrl(path, '/');
            bool matches = true;

            pathParams.clear();

            if (routeSplit.size() == urlSplit.size())
            {
                for (int x = 0; x < routeSplit.size(); x++)
                {
                    if (routeSplit[x] != urlSplit[x])
                    {
                        if (routeSplit[x][0] == ':')
                        {
                            pathParams.insert({routeSplit[x].substr(1), urlSplit[x]});
                        }
                        else
                        {
                            matches = false;
                        }
                    }
                }
            }
            else
            {
                matches = false;
            }

            if (matches)
            {
                if (body.find("&") != std::string::npos)
                {
                    queryParams = this->parseQueryString(body);
                }

                HTTPRequest req = {
                    .urlParams = pathParams,
                    .body = body,
                    .queryParams = queryParams,
                    .handlerId = 0,
                    .connection = connectionFd};

                route.handler(req);
                return;
            }
        }
    }
    writeResponse(HTTPResponse{
                .status = 404,
                .body = "Not found",
                .connectionFd = connectionFd
            });
}