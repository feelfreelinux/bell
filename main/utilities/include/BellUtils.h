#ifndef EUPHONIUM_BELL_UTILS
#define EUPHONIUM_BELL_UTILS

#include <stdint.h>  // for int32_t, int64_t
#include <string.h>  // for NULL
#include <cmath>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/time.h>  // for tv, gettimeofday
#include <unistd.h>    // for usleep
#endif
#include <chrono>
#include <cstdlib>  // for floor
#include <string>   // for string

#ifdef ESP_PLATFORM
#include "esp_system.h"
#endif

namespace bell {

std::string generateRandomUUID();
void freeAndNull(void*& ptr);
std::string getMacAddress();

struct tv {
  int64_t sec;   // seconds
  int64_t usec;  // microseconds

  // Default constructor
  tv() : sec(0), usec(0) {}

  // Constructor with seconds and microseconds
  tv(int64_t sec, int64_t usec) : sec(sec), usec(usec) { normalize(); }

  // Normalize the tv to ensure usec is within [-1,000,000, 1,000,000]
  void normalize() {
    if (usec >= 1000000) {
      sec += usec / 1000000;
      usec %= 1000000;
    } else if (usec < 0) {
      long borrow_sec = std::abs(usec / 1000000) + 1;
      sec -= borrow_sec;
      usec += borrow_sec * 1000000;
    }
  }
  // Overloading addition operator
  tv operator+(const tv& other) const {
    tv result(sec + other.sec, usec + other.usec);
    return result;
  }

  // Overloading subtraction operator
  tv operator-(const tv& other) const {
    tv result(sec - other.sec, usec - other.usec);
    return result;
  }

  // Overloading division operator
  tv operator/(long divisor) const {
    tv result(sec / divisor, usec / divisor);
    return result;
  }

  uint64_t ms() { return (sec * 1000) + (usec / 1000); }

  // Static method to get the current time
  static tv now() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    auto microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(duration)
            .count() %
        1000000;
    return tv(seconds, microseconds);
  }
};
}  // namespace bell

#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>

#define BELL_SLEEP_MS(ms) vTaskDelay(ms / portTICK_PERIOD_MS)
#define BELL_YIELD() taskYIELD()

#elif defined(_WIN32)
#define BELL_SLEEP_MS(ms) Sleep(ms)
#define BELL_YIELD() ;
#else

#define BELL_SLEEP_MS(ms) usleep(ms * 1000)
#define BELL_YIELD() ;

#endif
#endif
