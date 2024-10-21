#pragma once

#include <string>
#include <string_view>
#include "BellLogger.h"
#include "SocketStream.h"

namespace bell {
class SyslogLogger : public bell::AbstractLogger {
 public:
  /**
  * @param hostname syslog hostname 
  * @param appName syslog app name
  */
  SyslogLogger(const std::string& hostname, const std::string& appName);

  // Syslog protocol type, currently only IETF / 5424 is supported
  enum class Protocol { IETF_RFC5424 };

  enum class Transport { UDP, TCP };
  /**
   * @brief Connect to the syslog server, under a given endpoint
   * 
   * @param url syslog server endpoint
   * @param port syslog server port
   * @param transport syslog transport type, either UDP or TCP
   * @param protocol syslog protocol to use
   */
  void connect(const std::string& url, int port, Transport transport,
               Protocol protocol = Protocol::IETF_RFC5424);

  // Implementation of AbstractLogger
  void debug(std::string filename, int line, std::string submodule,
             const char* format, ...) override;
  void error(std::string filename, int line, std::string submodule,
             const char* format, ...) override;
  void info(std::string filename, int line, std::string submodule,
            const char* format, ...) override;

 private:
  const int facilityCode = 1;  // Facility code 1, user

  const int debugSeverity = 7;  // Debug severity num
  const int infoSeverity = 6;   // Info severity num
  const int errorSeverity = 3;  // Error severity num

  const int reconnectIntervalMs =
      5000;  // Ms between reconnection attempts, used in case of tcp

  std::array<char, 512> stringBuffer;  // 512 being the max log length

  // For future proofing, currently we only supportf RFC5424 either way
  Protocol protocol;

  // Socket to the syslog server
  bell::SocketStream syslogSocketStream;

  // Attached to each syslog message
  std::string hostname;
  std::string appName;
  std::string serverUrl;
  int serverPort;

  // Unix timestamp at which the syslog client has last reconnected
  uint64_t lastConnectionTimestamp = 0;

  Transport transport;  // either udp or tcp

  void sendLog(int severity, const std::string& submodule,
               const std::string& filename, std::string_view logMessage);
};
}  // namespace bell