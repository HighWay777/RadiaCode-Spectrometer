#include "time_manager.h"
#include <time.h>
#include <WiFi.h>
#include <cstring>
#include <Arduino.h>

namespace TimeManager {
  static bool initialized = false;
  static char currentTz[64] = "UTC";

  // Olson → POSIX lookup table for commonly used timezones.
  // Extend as needed. If the Olson name is not found here it is passed
  // verbatim to configTzTime() which will silently use UTC.
  static const char* olsonToPosix(const char* tz) {
    struct TzEntry { const char* olson; const char* posix; };
    static const TzEntry table[] = {
      { "UTC",                    "UTC0" },
      { "Europe/Sofia",           "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Athens",          "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Bucharest",       "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Helsinki",        "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Kyiv",            "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Riga",            "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Tallinn",         "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Vilnius",         "EET-2EEST,M3.5.0/3,M10.5.0/4" },
      { "Europe/Istanbul",        "TRT-3" },
      { "Europe/Moscow",          "MSK-3" },
      { "Europe/Berlin",          "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Paris",           "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Rome",            "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Madrid",          "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Warsaw",          "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Amsterdam",       "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Brussels",        "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Prague",          "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Budapest",        "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/Vienna",          "CET-1CEST,M3.5.0,M10.5.0/3" },
      { "Europe/London",          "GMT0BST,M3.5.0/1,M10.5.0" },
      { "Europe/Lisbon",          "WET0WEST,M3.5.0/1,M10.5.0" },
      { "America/New_York",       "EST5EDT,M3.2.0,M11.1.0" },
      { "America/Chicago",        "CST6CDT,M3.2.0,M11.1.0" },
      { "America/Denver",         "MST7MDT,M3.2.0,M11.1.0" },
      { "America/Los_Angeles",    "PST8PDT,M3.2.0,M11.1.0" },
      { "America/Sao_Paulo",      "<-03>3<-02>,M10.3.0/-2,M2.3.0/-1" },
      { "Asia/Dubai",             "<+04>-4" },
      { "Asia/Kolkata",           "IST-5:30" },
      { "Asia/Bangkok",           "<+07>-7" },
      { "Asia/Shanghai",          "CST-8" },
      { "Asia/Tokyo",             "JST-9" },
      { "Asia/Seoul",             "KST-9" },
      { "Australia/Sydney",       "AEST-10AEDT,M10.1.0,M4.1.0/3" },
      { nullptr, nullptr }
    };
    for (int i = 0; table[i].olson; i++) {
      if (strcmp(tz, table[i].olson) == 0) return table[i].posix;
    }
    Serial.printf("[TimeManager] WARNING: Unknown Olson timezone '%s', using verbatim (may default to UTC)\n", tz);
    return tz; // pass through — ESP IDF may handle some POSIX strings natively
  }

  void init(const char* tz) {
    if (!tz || strlen(tz) == 0) {
      tz = "UTC";
    }
    
    const char* actualTz = olsonToPosix(tz);
    Serial.printf("[TimeManager] Initializing timezone: %s -> %s\n", tz, actualTz);
    
    bool needsInit = !initialized || strncmp(currentTz, actualTz, sizeof(currentTz)) != 0;
    
    if (needsInit) {
      configTzTime(actualTz, "pool.ntp.org", "time.nist.gov");
      strlcpy(currentTz, actualTz, sizeof(currentTz));
      initialized = true;
      delay(100);
    }
  }

  bool timeIsSet() {
    time_t now = time(nullptr);
    // Consider time valid after 2016-01-01
    return now > 1451606400;
  }

  void debugTimeInfo() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    Serial.printf("[TimeManager] Debug info:\n");
    Serial.printf("  Current timezone: %s\n", currentTz);
    Serial.printf("  Raw time_t: %ld\n", now);
    Serial.printf("  Local time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Also show GMT for comparison
    struct tm gmtInfo;
    gmtime_r(&now, &gmtInfo);
    Serial.printf("  GMT time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  gmtInfo.tm_year + 1900, gmtInfo.tm_mon + 1, gmtInfo.tm_mday,
                  gmtInfo.tm_hour, gmtInfo.tm_min, gmtInfo.tm_sec);
  }

  void forceRefresh(const char* tz) {
    initialized = false;
    init(tz ? tz : "UTC");
    debugTimeInfo();
  }

  void loop() {
    // In future, we can trigger re-sync if needed
    (void)0;
  }
}
