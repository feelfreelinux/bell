#include "HTTPClient.h"


using namespace bell;

void HTTPClient::Response::connect(const std::string& url, size_t bufSize) {
  this->userBuffer = std::vector<uint8_t>(bufSize);

  SocketStatus_t socketStatus;
  ServerInfo_t serverInfo = {0};

  urlParser = bell::URLParser::parse(url);

  serverInfo.pHostName = urlParser.host.c_str();
  serverInfo.hostNameLength = urlParser.host.size();
  serverInfo.port = urlParser.port;

  if (urlParser.schema == "http") {
    this->networkContext.pParams = &plaintextParams;
    this->transportInterface.recv = Plaintext_Recv;
    this->transportInterface.send = Plaintext_Send;
    socketStatus = Plaintext_Connect(&networkContext, &serverInfo, 5000, 5000);
  } else {
    this->networkContext.pParams = &mbedtlsParams;
    MbedTLS_initialize(&networkContext);
    this->transportInterface.recv = MbedTLS_Recv;
    this->transportInterface.send = MbedTLS_Send;

    socketStatus = MbedTLS_Connect(&networkContext, &serverInfo, 5000, 5000);
  }

  if (socketStatus != SOCKETS_SUCCESS) {
    throw std::runtime_error("Cannot establish connection with provided url");
  }
}

HTTPClient::Response::~Response() {
  if (urlParser.schema == "http") {
    Plaintext_Disconnect(&networkContext);
  } else {
    MbedTLS_Disconnect(&networkContext);
  }
}

void HTTPClient::Response::get(const std::string& url, Headers headers) {
  HTTPStatus_t httpStatus = HTTPSuccess;
  HTTPRequestInfo_t reqInfo = {0};

  response = {0};
  HTTPRequestHeaders_t requestHeaders = {0};

  reqInfo.pMethod = "GET";
  reqInfo.methodLen = 3;
  reqInfo.pHost = urlParser.host.c_str();
  reqInfo.hostLen = urlParser.host.size();
  reqInfo.pPath = urlParser.path.c_str();
  reqInfo.pathLen = urlParser.path.size();
  reqInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

  requestHeaders.pBuffer = userBuffer.data();
  requestHeaders.bufferLen = userBuffer.size();

  httpStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, &reqInfo);

  for (Header header : headers) {
    if (header.index() == 0) {
      auto valueHeader = std::get<ValueHeader>(header);
      HTTPClient_AddHeader(&requestHeaders, valueHeader.first.c_str(),
                           valueHeader.first.size(), valueHeader.second.c_str(),
                           valueHeader.second.size());
    } else {
      auto rangeHeader = std::get<RangeHeader>(header);
      HTTPClient_AddRangeHeader(&requestHeaders, rangeHeader.from,
                                rangeHeader.to);
    }
  }

  if (httpStatus == HTTPSuccess) {
    response.pBuffer = userBuffer.data();
    response.bufferLen = userBuffer.size();

    httpStatus = HTTPClient_Send(&transportInterface, &requestHeaders, 0, 0,
                                 &response, 0);
  } else {
    throw std::runtime_error("Failed to initialize HTTP request headers");
  }

  if (httpStatus != HTTPSuccess) {
    std::cout << reqInfo.pHost << std::endl;
    std::cout << reqInfo.pPath << std::endl;
    std::cout << reqInfo.pathLen << std::endl;
    std::cout << httpStatus << std::endl;
    throw std::runtime_error("Failed to send HTTP request");
  }
}

size_t HTTPClient::Response::contentLength() {

  return response.contentLength;
}

std::string_view HTTPClient::Response::getHeader(
    const std::string& headerName) {
  char* headerLoc = NULL;
  size_t headerLen = 0;

  HTTPClient_ReadHeader(&response, headerName.c_str(), headerName.size(),
                        (const char**)&headerLoc, &headerLen);
  return std::string_view(headerLoc, headerLen);
}

size_t HTTPClient::Response::fullContentLength() {
  auto rangeHeader = getHeader("Content-Range");

  if (rangeHeader.find("/") != std::string::npos) {
    return std::stoi(
        std::string(rangeHeader.substr(rangeHeader.find("/") + 1)));
  }

  return this->contentLength();
}

size_t HTTPClient::Response::read(uint8_t* dst, size_t bytes) {
  size_t readLen = bytes > response.bodyLen ? response.bodyLen : bytes;
  memcpy(dst, response.pBody, readLen);
  return readLen;
}

std::string_view HTTPClient::Response::body() {
  return std::string_view((char*)response.pBody);
}

std::vector<uint8_t> HTTPClient::Response::bytes() {
  auto result = std::vector<uint8_t>(response.bodyLen);
  this->read(result.data(), result.size());
  return result;
}