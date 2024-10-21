#include "BellLogger.h"

using namespace bell;

std::vector<std::unique_ptr<bell::AbstractLogger>> bell::bellRegisteredLoggers;

void AbstractLogger::enableSubmoduleLogging(bool enable) {
  this->enableSubmodule = enable;
}

void AbstractLogger::enableTimestampLogging(bool enable,
                                            bool localTimestampLogging) {
  this->enableTimestamp = enable;
  this->shortTime = localTimestampLogging;
}

void bell::registerLogger(std::unique_ptr<bell::AbstractLogger> logger) {
  bellRegisteredLoggers.push_back(std::move(logger));
}

void bell::setDefaultLogger(bool enableSubmoduleLogging,
                            bool enableTimestampLogging,
                            bool localTimestampLogging) {
  auto logger = std::make_unique<bell::StdoutLogger>();
  logger->enableSubmoduleLogging(enableSubmoduleLogging);
  logger->enableTimestampLogging(enableTimestampLogging, localTimestampLogging);

  bellRegisteredLoggers.push_back(std::move(logger));
}
