#include "BellLogger.h"

bell::AbstractLogger* bell::bellGlobalLogger;

void bell::setDefaultLogger() {
    bell::bellGlobalLogger = new bell::BellLogger();
}

void bell::enableSubmoduleLogging() {
    bell::bellGlobalLogger->enableSubmodule = true;
}