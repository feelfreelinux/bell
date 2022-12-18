#include "BellHTTPServer.h"

using namespace bell;

class WebSocketHandler : public CivetWebSocketHandler {
 public:
  BellHTTPServer::WSDataHandler dataHandler;
  BellHTTPServer::WSStateHandler stateHandler;

  WebSocketHandler(BellHTTPServer::WSDataHandler dataHandler,
                   BellHTTPServer::WSStateHandler stateHandler) {
    this->dataHandler = dataHandler;
    this->stateHandler = stateHandler;
  }

  virtual bool handleConnection(CivetServer* server,
                                struct mg_connection* conn) {
    this->stateHandler(conn, BellHTTPServer::WSState::CONNECTED);
    return true;
  }

  virtual void handleReadyState(CivetServer* server,
                                struct mg_connection* conn) {
    this->stateHandler(conn, BellHTTPServer::WSState::READY);
  }

  virtual bool handleData(CivetServer* server, struct mg_connection* conn,
                          int bits, char* data, size_t data_len) {
    this->dataHandler(conn, data, data_len);
    return true;
  }

  virtual void handleClose(CivetServer* server, struct mg_connection* conn) {
    stateHandler(conn, BellHTTPServer::WSState::CLOSED);
  }
};

BellHTTPServer::RequestHandler::RequestHandler(){};

void BellHTTPServer::RequestHandler::setReqHandler(
    RequestType type, BellHTTPServer::HTTPHandler handler) {
  switch (type) {
    case RequestType::GET:
      getReqHandler = handler;
      break;
    case RequestType::POST:
      postReqHandler = handler;
      break;
    default:
      break;
  }
}

bool BellHTTPServer::RequestHandler::handleGet(CivetServer* server,
                                               struct mg_connection* conn) {
  if (getReqHandler == nullptr) {
    return false;
  }

  try {
    auto reply = getReqHandler(conn);
    if (reply.body == nullptr) {
      return true;
    }

    mg_printf(conn,
              "HTTP/1.1 %d OK\r\nContent-Type: "
              "%s\r\nConnection: close\r\n\r\n",
              reply.status, reply.headers["Content-Type"].c_str());
    mg_write(conn, reply.body, reply.bodySize);

    return true;
  } catch (std::exception& e) {
    BELL_LOG(error, "HttpServer", "Exception occured in handler: %s", e.what());
    return false;
  }
}

bool BellHTTPServer::RequestHandler::handlePost(CivetServer* server,
                                                struct mg_connection* conn) {
  if (postReqHandler == nullptr) {
    return false;
  }

  try {
    auto reply = postReqHandler(conn);

    if (reply.body == nullptr) {
      return true;
    }

    mg_printf(conn,
              "HTTP/1.1 %d OK\r\nContent-Type: "
              "%s\r\nConnection: close\r\n\r\n",
              reply.status, reply.headers["Content-Type"].c_str());
    mg_write(conn, reply.body, reply.bodySize);
    return true;
  } catch (std::exception& e) {
    BELL_LOG(error, "HttpServer", "Exception occured in handler: %s", e.what());
    return false;
  }
}

BellHTTPServer::BellHTTPServer(int serverPort) {
  mg_init_library(0);
  BELL_LOG(info, "HttpServer", "Server listening on port %d", serverPort);
  this->serverPort = serverPort;
  auto port = std::to_string(this->serverPort);
  const char* options[] = {"listening_ports", port.c_str(), 0};
  server = std::make_unique<CivetServer>(options);
}

BellHTTPServer::HTTPResponse BellHTTPServer::makeJsonResponse(
    const std::string& json, int status) {
  BellHTTPServer::HTTPResponse response;

  response.body = (uint8_t*)malloc(json.size());
  response.bodySize = json.size();
  response.headers["Content-Type"] = "application/json";
  response.status = status;

  memcpy(response.body, json.c_str(), json.size());
  return response;
}

BellHTTPServer::HTTPResponse BellHTTPServer::makeEmptyResponse() {
  BellHTTPServer::HTTPResponse response;
  return response;
}

void BellHTTPServer::registerGet(const std::string& url,
                                 BellHTTPServer::HTTPHandler handler) {
  if (this->handlers.find(url) == this->handlers.end()) {
    this->handlers[url] = new RequestHandler();
    server->addHandler(url, handlers[url]);
  }

  handlers[url]->setReqHandler(RequestHandler::RequestType::GET, handler);
}

void BellHTTPServer::registerPost(const std::string& url,
                                  BellHTTPServer::HTTPHandler handler) {
  if (this->handlers.find(url) == this->handlers.end()) {
    this->handlers[url] = new RequestHandler();
    server->addHandler(url, handlers[url]);
  }

  handlers[url]->setReqHandler(RequestHandler::RequestType::POST, handler);
}

void BellHTTPServer::registerWS(const std::string& url,
                                BellHTTPServer::WSDataHandler dataHandler,
                                BellHTTPServer::WSStateHandler stateHandler) {
  server->addWebSocketHandler(url,
                              new WebSocketHandler(dataHandler, stateHandler));
}