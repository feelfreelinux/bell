#include "SyslogLogger.h"
#include <memory>
#include "BellSocket.h"
#include "SocketStream.h"

using namespace bell;

void SyslogLogger::setup(std::unique_ptr<bell::Socket> socket,
                         const std::string& hostname,
                         const std::string& appName, Protocol protocol) {
  this->hostname = hostname;
  this->appName = appName;
  this->protocol = protocol;

  // Open the socket stream
  syslogSocketStream.open(std::move(socket));
}

void SyslogLogger::sendLog(int severity, const std::string& submodule,
                           const std::string& filename,
                           std::string_view logMessage) {
  if (!syslogSocketStream.isOpen()) {
    std::cout << "Invalid syslog connection" << std::endl;
    return;  // invalid syslog connection
  }

  // Write the priority value, defined as facility * 8 + severity
  syslogSocketStream << "<" << (facilityCode * 8) + severity << ">"; 

  // Write the version
  syslogSocketStream << "1" << " ";

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
