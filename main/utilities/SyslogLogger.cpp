#include "SyslogLogger.h"
#include <chrono>
#include <memory>
#include <string>
#include "BellSocket.h"
#include "SocketStream.h"
#include "TCPSocket.h"
#include "UDPSocket.h"

using namespace bell;

SyslogLogger::SyslogLogger(const std::string& hostname,
                           const std::string& appName)
    : hostname(hostname), appName(appName) {}

void SyslogLogger::connect(const std::string& url, int port,
                           Transport transport, Protocol protocol) {
  this->protocol = protocol;
  this->transport = transport;

  auto currentTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();

  if (lastConnectionTimestamp + reconnectIntervalMs > currentTimeMs) {
    return;  // only allow for initial connection, and reconnection every reconnectIntervals
  }

  std::unique_ptr<bell::Socket> socket;

  // Open either TCP or UDP connection, depending on the transport
  if (transport == Transport::UDP) {
    socket = std::make_unique<UDPSocket>();
    socket->open(url, port);
  } else if (transport == Transport::TCP) {
    socket = std::make_unique<TCPSocket>();
    socket->open(url, port);
  }

  // open syslogsocketstream
  syslogSocketStream.open(std::move(socket));
}

void SyslogLogger::sendLog(int severity, const std::string& submodule,
                           const std::string& filename,
                           std::string_view logMessage) {
  if (!syslogSocketStream.isOpen()) {
    // attempt to reconnect to the syslog server
    connect(serverUrl, serverPort, transport, protocol);
    return;  // invalid syslog connection
  }

  // Write the priority value, defined as facility * 8 + severity
  syslogSocketStream << "<" << (facilityCode * 8) + severity << ">";

  // Write the version
  syslogSocketStream << "1"
                     << " ";

  // Write the timestamp value
  if (enableTimestamp) {
    // Get the current time
    auto now = std::chrono::system_clock::now();
    time_t now_time = std::chrono::system_clock::to_time_t(now);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()) %
                       1000;

    syslogSocketStream << std::put_time(gmtime(&now_time), "%Y-%m-%dT%H:%M:%S")
                       << "." << std::setfill('0') << std::setw(3)
                       << nowMs.count() << "Z " << std::endl;
  } else {
    syslogSocketStream << "- ";
  }

  // Write hostname and appname info
  syslogSocketStream << hostname << " ";
  syslogSocketStream << appName << " ";

  // Write submodule as process id, filename as message id
  if (enableSubmodule) {
    syslogSocketStream << submodule << " ";
    syslogSocketStream << filename << " ";
  }

  syslogSocketStream << "- ";
  syslogSocketStream << "\"" << logMessage << "\"" << std::endl;
}

void SyslogLogger::debug(std::string filename, int line, std::string submodule,
                         const char* format, ...) {
  va_list args;
  va_start(args, format);
  int res = vsnprintf(stringBuffer.data(), stringBuffer.size(), format, args);
  va_end(args);

  if (res > 0) {
    std::string basenameStr(filename.substr(filename.rfind("/") + 1));
    sendLog(debugSeverity, submodule, basenameStr, stringBuffer.data());
  }
};

void SyslogLogger::error(std::string filename, int line, std::string submodule,
                         const char* format, ...) {
  va_list args;
  va_start(args, format);
  int res = vsnprintf(stringBuffer.data(), stringBuffer.size(), format, args);
  va_end(args);

  if (res > 0) {
    std::string basenameStr(filename.substr(filename.rfind("/") + 1));
    sendLog(errorSeverity, submodule, basenameStr, stringBuffer.data());
  }
};

void SyslogLogger::info(std::string filename, int line, std::string submodule,
                        const char* format, ...) {
  va_list args;
  va_start(args, format);
  int res = vsnprintf(stringBuffer.data(), stringBuffer.size(), format, args);
  va_end(args);

  if (res > 0) {
    std::string basenameStr(filename.substr(filename.rfind("/") + 1));
    sendLog(infoSeverity, submodule, basenameStr, stringBuffer.data());
  }
};
