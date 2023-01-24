#include "BellHTTPServer.h"
#include <regex>
#include "CivetServer.h"
#include "civetweb.h"

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

std::vector<std::string> BellHTTPServer::Router::split(
    const std::string str, const std::string regex_str) {
  std::regex regexz(regex_str);
  return {std::sregex_token_iterator(str.begin(), str.end(), regexz, -1),
          std::sregex_token_iterator()};
}

void BellHTTPServer::Router::insert(const std::string& route,
                                    HTTPHandler& value) {
  auto parts = split(route, "/");
  auto currentNode = &root;

  for (int index = 0; index < parts.size(); index++) {
    auto part = parts[index];
    if (part[0] == ':') {
      currentNode->isParam = true;
      currentNode->paramName = part.substr(1);
      part = "";
    } else if (part[0] == '*') {
      currentNode->isCatchAll = true;
      currentNode->value = value;
      return;
    }

    if (!currentNode->children.count(part)) {
      currentNode->children[part] = std::make_unique<RouterNode>();
    }
    currentNode = currentNode->children[part].get();
  }
  currentNode->value = value;
}

BellHTTPServer::Router::HandlerAndParams BellHTTPServer::Router::find(
    const std::string& route) {
  auto parts = split(route, "/");
  auto currentNode = &root;
  std::unordered_map<std::string, std::string> params;

  for (int index = 0; index < parts.size(); index++) {
    auto part = parts[index];

    if (currentNode->children.count(part)) {
      currentNode = currentNode->children[part].get();
    } else if (currentNode->isParam) {
      params[currentNode->paramName] = part;
      if (currentNode->children.count("")) {
        currentNode = currentNode->children[""].get();
      } else {
        return {nullptr, Params()};
      }
    } else if (currentNode->isCatchAll) {
      params["**"] = '*';
      return {currentNode->value, params};
    } else {
      return {nullptr, Params()};
    }
  }

  if (currentNode->value != nullptr) {
    return {currentNode->value, params};
  }

  return {nullptr, Params()};
}

bool BellHTTPServer::handleGet(CivetServer* server,
                               struct mg_connection* conn) {
  auto requestInfo = mg_get_request_info(conn);
  auto handler = getRequestsRouter.find(requestInfo->local_uri);

  if (handler.first == nullptr) {
    if (this->notFoundHandler != nullptr) {
      this->notFoundHandler(conn); 
      return true;
    }

    return false;
  }

  mg_set_user_connection_data(conn, &handler.second);

  try {
    auto reply = handler.first(conn);
    if (reply.body == nullptr) {
      return true;
    }

    mg_printf(
        conn,
        "HTTP/1.1 %d OK\r\nContent-Type: "
        "%s\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        reply.status, reply.headers["Content-Type"].c_str());
    mg_write(conn, reply.body, reply.bodySize);

    return true;
  } catch (std::exception& e) {
    BELL_LOG(error, "HttpServer", "Exception occured in handler: %s", e.what());
    return false;
  }
}

bool BellHTTPServer::handlePost(CivetServer* server,
                                struct mg_connection* conn) {
  auto requestInfo = mg_get_request_info(conn);
  auto handler = postRequestsRouter.find(requestInfo->local_uri);

  if (handler.first == nullptr) {
    return false;
  }

  mg_set_user_connection_data(conn, &handler.second);

  try {
    auto reply = handler.first(conn);
    if (reply.body == nullptr) {
      return true;
    }

    mg_printf(
        conn,
        "HTTP/1.1 %d OK\r\nContent-Type: "
        "%s\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
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
  server->addHandler(url, this);
  getRequestsRouter.insert(url, handler);
}

void BellHTTPServer::registerPost(const std::string& url,
                                  BellHTTPServer::HTTPHandler handler) {
  server->addHandler(url, this);
  postRequestsRouter.insert(url, handler);
}

void BellHTTPServer::registerWS(const std::string& url,
                                BellHTTPServer::WSDataHandler dataHandler,
                                BellHTTPServer::WSStateHandler stateHandler) {
  server->addWebSocketHandler(url,
                              new WebSocketHandler(dataHandler, stateHandler));
}

void BellHTTPServer::registerNotFound(HTTPHandler handler) {
  this->notFoundHandler = handler;
}

std::unordered_map<std::string, std::string> BellHTTPServer::extractParams(
    struct mg_connection* conn) {
  std::unordered_map<std::string, std::string>& params =
      *(std::unordered_map<std::string, std::string>*)
          mg_get_user_connection_data(conn);
  return params;
}
