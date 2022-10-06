#include "BellLogger.h"

std::shared_ptr<bell::AbstractLogger> bell::bellGlobalLogger;
int (*bell::function_printf)(const char *, ...) = &printf;
int (*bell::function_vprintf)(const char*, va_list) = &vprintf;

void bell::setDefaultLogger() {
    bell::bellGlobalLogger = std::make_shared<bell::BellLogger>();
}

void bell::enableSubmoduleLogging() {
    bell::bellGlobalLogger->enableSubmodule = true;
}

void bell::disableColors() {
    bell::bellGlobalLogger->enableColors = false;
}