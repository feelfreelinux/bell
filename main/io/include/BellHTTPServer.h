#pragma once

#include "CivetServer.h"
#include <BellLogger.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>

using namespace bell;

namespace bell {
class BellHTTPServer {
  private:
	std::unique_ptr<CivetServer> server;
	int serverPort;

  public:
	BellHTTPServer(int serverPort);

	enum class WSState { CONNECTED, READY, CLOSED };

	struct HTTPResponse {
		uint8_t *body;
		size_t bodySize;
		std::map<std::string, std::string> headers;

		int status;

		HTTPResponse() {
			body	 = nullptr;
			bodySize = 0;
			status	 = 200;
		}

		~HTTPResponse() {
			if (body != nullptr) {
				free(body);
			}
		}
	};

	typedef std::function<HTTPResponse(struct mg_connection *conn)> HTTPHandler;
	typedef std::function<void(struct mg_connection *conn, WSState)> WSStateHandler;
	typedef std::function<void(struct mg_connection *conn, char *, size_t)> WSDataHandler;

	HTTPResponse makeJsonResponse(const std::string &json, int status = 200);
	HTTPResponse makeEmptyResponse();

	void registerGet(const std::string &, HTTPHandler handler);
	void registerPost(const std::string &, HTTPHandler handler);
	void registerWS(const std::string &, WSDataHandler dataHandler, WSStateHandler stateHandler);

	void listen();
};

} // namespace bell