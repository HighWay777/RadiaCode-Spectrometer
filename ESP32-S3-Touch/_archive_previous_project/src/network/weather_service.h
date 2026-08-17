#pragma once

#include <Arduino.h>

enum class WeatherProvider : uint8_t {
  OpenWeather = 0,
  OpenMeteo = 1
};

class WeatherService {
public:
  static void init();
  static void loop();
  static void forceUpdate();
private:
  static unsigned long lastUpdate;
  static bool fetchOpenWeather(const char* apiKey, const char* location);
  static bool fetchOpenMeteo(const char* location);
  static bool parseLatLon(const char* location, float &lat, float &lon);
};
