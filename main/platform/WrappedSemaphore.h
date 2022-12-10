#pragma once

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#elif __APPLE__
#include <dispatch/dispatch.h>
#elif _WIN32
#include <winsock2.h>
#else
#include <semaphore.h>
#include <time.h>
#endif

namespace bell {

class WrappedSemaphore {
  private:
#ifdef ESP_PLATFORM
	xSemaphoreHandle semaphoreHandle;
#elif __APPLE__
	dispatch_semaphore_t semaphoreHandle;
#elif _WIN32
	HANDLE semaphoreHandle;
#else
	sem_t semaphoreHandle;
#endif

  public:
	WrappedSemaphore(int maxVal = 200);
	~WrappedSemaphore();

	int wait();
	int twait(long milliseconds = 10);
	void give();
};

} // namespace bell