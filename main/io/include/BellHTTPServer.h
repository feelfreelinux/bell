#pragma once

#include <BellLogger.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include "CivetServer.h"

using namespace bell;
namespace bell {
class BellHTTPServer {
 public:
  BellHTTPServer(int serverPort);

  enum class WSState { CONNECTED, READY, CLOSED };

  struct HTTPResponse {
    uint8_t* body;
    size_t bodySize;
    std::map<std::string, std::string> headers;

    int status;

    HTTPResponse() {
      body = nullptr;
      bodySize = 0;
      status = 200;
    }

    ~HTTPResponse() {
      if (body != nullptr) {
        free(body);
      }
    }
  };

  typedef std::function<HTTPResponse(struct mg_connection* conn)> HTTPHandler;
  typedef std::function<void(struct mg_connection* conn, WSState)>
      WSStateHandler;
  typedef std::function<void(struct mg_connection* conn, char*, size_t)>
      WSDataHandler;

  class RequestHandler : public CivetHandler {
		private:
    BellHTTPServer::HTTPHandler getReqHandler = nullptr;
		BellHTTPServer::HTTPHandler postReqHandler = nullptr;

   public:
    enum class RequestType { GET, POST, PUT, DELETE };

    RequestType requestType;

    RequestHandler();

		void setReqHandler(RequestType type, BellHTTPServer::HTTPHandler handler);

    bool handlePost(CivetServer* server, struct mg_connection* conn);

    bool handleGet(CivetServer* server, struct mg_connection* conn);
  };

  HTTPResponse makeJsonResponse(const std::string& json, int status = 200);
  HTTPResponse makeEmptyResponse();

  void registerGet(const std::string&, HTTPHandler handler);
  void registerPost(const std::string&, HTTPHandler handler);
  void registerWS(const std::string&, WSDataHandler dataHandler,
                  WSStateHandler stateHandler);
 private:
  std::unique_ptr<CivetServer> server;
	int serverPort = 8080;
  std::unordered_map<std::string, RequestHandler*> handlers;
};

}  // namespace bell