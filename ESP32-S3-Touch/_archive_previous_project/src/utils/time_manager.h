#pragma once

#include <Arduino.h>

namespace TimeManager {
  // Initialize SNTP with a TZ string (e.g., "Europe/Sofia") and default servers
  void init(const char* tz);
  // Returns true if system time is set (> Jan 1, 2016)
  bool timeIsSet();
  // Debug current time and timezone info
  void debugTimeInfo();
  // Force timezone refresh
  void forceRefresh(const char* tz);
  // No-op placeholder for future resyncs
  void loop();
}
