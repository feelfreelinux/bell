#include "BellLogger.h"

std::shared_ptr<bell::AbstractLogger> bell::bellGlobalLogger;

void bell::setDefaultLogger() {
    bell::bellGlobalLogger = std::make_shared<bell::BellLogger>();
}