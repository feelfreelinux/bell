#include "BellHTTPServer.h"

using namespace bell;

class WebSocketHandler : public CivetWebSocketHandler {
  public:
	BellHTTPServer::WSDataHandler dataHandler;
	BellHTTPServer::WSStateHandler stateHandler;

	WebSocketHandler(BellHTTPServer::WSDataHandler dataHandler, BellHTTPServer::WSStateHandler stateHandler) {
		this->dataHandler  = dataHandler;
		this->stateHandler = stateHandler;
	}

	virtual bool handleConnection(CivetServer *server, struct mg_connection *conn) {
		this->stateHandler(conn, BellHTTPServer::WSState::CONNECTED);
		return true;
	}

	virtual void handleReadyState(CivetServer *server, struct mg_connection *conn) {
		this->stateHandler(conn, BellHTTPServer::WSState::READY);
	}

	virtual bool handleData(CivetServer *server, struct mg_connection *conn, int bits, char *data, size_t data_len) {
		this->dataHandler(conn, data, data_len);
		return true;
	}

	virtual void handleClose(CivetServer *server, struct mg_connection *conn) {
		stateHandler(conn, BellHTTPServer::WSState::CLOSED);
	}
};

class RequestHandler : public CivetHandler {
	BellHTTPServer::HTTPHandler rqHandler;

  public:
	enum class RequestType { GET, POST, PUT, DELETE };

	RequestType requestType;

	RequestHandler(BellHTTPServer::HTTPHandler handler, RequestType requestType = RequestType::GET) {
		rqHandler	= handler;
		requestType = requestType;
	};

	bool handlePost(CivetServer *server, struct mg_connection *conn) {
		try {
			auto reply = rqHandler(conn);

			if (reply.body == nullptr) {
				return true;
			}

			mg_printf(
				conn,
				"HTTP/1.1 %d OK\r\nContent-Type: "
				"%s\r\nConnection: close\r\n\r\n",
				reply.status,
				reply.headers["Content-Type"].c_str()
			);
			mg_write(conn, reply.body, reply.bodySize);
			return true;
		} catch (std::exception &e) {
			BELL_LOG(error, "HttpServer", "Exception occured in handler: %s", e.what());
			return false;
		}
	}

	bool handleGet(CivetServer *server, struct mg_connection *conn) {
		try {
			auto reply = rqHandler(conn);
			if (reply.body == nullptr) {
				return true;
			}

			mg_printf(
				conn,
				"HTTP/1.1 %d OK\r\nContent-Type: "
				"%s\r\nConnection: close\r\n\r\n",
				reply.status,
				reply.headers["Content-Type"].c_str()
			);
			mg_write(conn, reply.body, reply.bodySize);

			return true;
		} catch (std::exception &e) {
			BELL_LOG(error, "HttpServer", "Exception occured in handler: %s", e.what());
			return false;
		}
	}
};

BellHTTPServer::BellHTTPServer(int serverPort) {
	mg_init_library(0);
	BELL_LOG(info, "HttpServer", "Server listening on port %d", serverPort);
	this->serverPort	  = serverPort;
	auto port			  = std::to_string(this->serverPort);
	const char *options[] = {"listening_ports", port.c_str(), 0};
	server				  = std::make_unique<CivetServer>(options);
}

BellHTTPServer::HTTPResponse BellHTTPServer::makeJsonResponse(const std::string &json, int status) {
	BellHTTPServer::HTTPResponse response;

	response.body					 = (uint8_t *)malloc(json.size());
	response.bodySize				 = json.size();
	response.headers["Content-Type"] = "application/json";
	response.status					 = status;

	memcpy(response.body, json.c_str(), json.size());
	return response;
}

BellHTTPServer::HTTPResponse BellHTTPServer::makeEmptyResponse() {
	BellHTTPServer::HTTPResponse response;
	return response;
}

void BellHTTPServer::registerGet(const std::string &url, BellHTTPServer::HTTPHandler handler) {
	server->addHandler(url, new RequestHandler(handler, RequestHandler::RequestType::GET));
}

void BellHTTPServer::registerPost(const std::string &url, BellHTTPServer::HTTPHandler handler) {
	server->addHandler(url, new RequestHandler(handler, RequestHandler::RequestType::POST));
}

void BellHTTPServer::registerWS(
	const std::string &url, BellHTTPServer::WSDataHandler dataHandler, BellHTTPServer::WSStateHandler stateHandler
) {
	server->addWebSocketHandler(url, new WebSocketHandler(dataHandler, stateHandler));
}

// void CivetHTTPServer::registerHandler(
// 	RequestType requestType, const std::string &url, httpHandler handler, bool readDataToStr
// ) {
// 	server->addHandler(url, new RequestHandler(handler));
// }

// void CivetHTTPServer::listen() {
// }