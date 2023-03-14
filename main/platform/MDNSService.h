#pragma once

#include <map>
#include <string>

namespace bell {

class MDNSService {
  public:
	static void* registerService(
		const std::string &serviceName,
		const std::string &serviceType,
		const std::string &serviceProto,
		const std::string &serviceHost,
		int servicePort,
		const std::map<std::string, std::string> txtData
	);
	static void unregisterService(void* service);
};

} // namespace bell