#pragma once

#include <string>
#include <string_view>
#include "BellLogger.h"
#include "BellSocket.h"
#include "SocketStream.h"

namespace bell {
class SyslogLogger : public bell::AbstractLogger {
 public:
  SyslogLogger() = default;

  enum class Protocol { IETF_RFC5424 };

  void setup(std::unique_ptr<bell::Socket> socket, const std::string& hostname,
             const std::string& appName,
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

  std::array<char, 512> stringBuffer;  // 512 being the max log length

  // For future proofing, currently we only supportf RFC5424 either way
  Protocol protocol;

  // Socket to the syslog server
  bell::SocketStream syslogSocketStream;

  // Attached to each syslog message
  std::string hostname;
  std::string appName;

  void sendLog(int severity, const std::string& submodule,
               const std::string& filename, std::string_view logMessage);
};
}  // namespace bell