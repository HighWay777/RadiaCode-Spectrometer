#include "weather_service.h"
#include "network_manager.h"
#include "../config/config_store.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../ui/page_home.h"
#include <WiFiClientSecure.h>

unsigned long WeatherService::lastUpdate = 0;

void WeatherService::init() {
  lastUpdate = 0;
}

void WeatherService::forceUpdate() {
  lastUpdate = 0;
}

bool WeatherService::parseLatLon(const char* location, float &lat, float &lon) {
  // Expect "lat,lon"
  const char* comma = strchr(location, ',');
  if (!comma) return false;
  String latStr = String(location).substring(0, comma - location);
  String lonStr = String(comma + 1);
  lat = latStr.toFloat();
  lon = lonStr.toFloat();
  return true;
}

void WeatherService::loop() {
  AppConfig* cfg = ConfigStore::getConfig();
  if (WiFi.status() != WL_CONNECTED) return;

  // Update interval in minutes -> millis
  unsigned long intervalMs = (unsigned long)cfg->weatherUpdateInterval * 60UL * 1000UL;
  unsigned long now = millis();
  if (intervalMs == 0) intervalMs = 5UL * 60UL * 1000UL; // default 5 minutes
  // Fetch immediately on first run (lastUpdate==0) or when interval has elapsed
  if (lastUpdate != 0 && (now - lastUpdate < intervalMs)) return;

  bool ok = false;
  if (cfg->weatherProvider == (uint8_t)WeatherProvider::OpenWeather) {
    ok = fetchOpenWeather(cfg->apiKey, cfg->location);
  } else {
    ok = fetchOpenMeteo(cfg->location);
  }
  if (ok) lastUpdate = now;
}

bool WeatherService::fetchOpenWeather(const char* apiKey, const char* location) {
  if (!apiKey || strlen(apiKey) == 0 || !location || strlen(location) == 0) return false;
  String url = String("http://api.openweathermap.org/data/2.5/weather?q=") + location + "&units=metric&appid=" + apiKey;
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  auto err = deserializeJson(doc, body);
  if (err) return false;

  auto mapOWCode = [](int ow) -> int {
    // Map OpenWeather codes to Open-Meteo-like codes we draw
    if (ow == 800) return 0;                    // clear
    if (ow == 801) return 1;                    // few clouds
    if (ow == 802) return 2;                    // scattered
    if (ow == 803 || ow == 804) return 3;       // broken/overcast
    if (ow >= 200 && ow <= 232) return 95;      // thunderstorm
    if (ow >= 300 && ow <= 321) return 51;      // drizzle
    if (ow >= 500 && ow <= 504) return 63;      // rain
    if (ow == 511) return 66;                   // freezing rain
    if (ow >= 520 && ow <= 531) return 63;      // shower rain
    if (ow >= 600 && ow <= 602) return 71;      // snow
    if (ow >= 611 && ow <= 616) return 77;      // sleet / snow grains
    if (ow >= 620 && ow <= 622) return 73;      // snow showers
    if (ow >= 701 && ow <= 781) return 45;      // mist/fog etc.
    return 3; // default to overcast (neutral) instead of clear (false positive)
  };

  WeatherData wd = {};
  wd.extTemp = doc["main"]["temp"].as<float>();
  wd.weatherDesc = doc["weather"][0]["description"].as<String>();
  wd.weatherCode = mapOWCode(doc["weather"][0]["id"].as<int>());
  wd.humidity = doc["main"]["humidity"].as<int>();
  wd.pressure = doc["main"]["pressure"].as<int>();
  wd.windKph = doc["wind"]["speed"].as<float>() * 3.6f; // m/s -> km/h (metric)
  wd.solar = 0; // not available directly
  wd.isNightTime = false; // quick heuristic can be added using "sys" sunrise/sunset
  wd.wifiRSSI = NetworkManager::getWiFiRSSI();
  updateWeatherOnly(wd);
  return true;
}

bool WeatherService::fetchOpenMeteo(const char* location) {
  if (!location || strlen(location) == 0) return false;
  float lat=0, lon=0;
  if (!parseLatLon(location, lat, lon)) return false;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(lat, 4) + "&longitude=" + String(lon, 4) + "&current=temperature_2m,relative_humidity_2m,pressure_msl,wind_speed_10m,weather_code";
  WiFiClientSecure client;
  client.setInsecure(); // accept all certs to avoid TLS issues on embedded
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  auto err = deserializeJson(doc, body);
  if (err) return false;

  WeatherData wd = {};
  wd.extTemp = doc["current"]["temperature_2m"].as<float>();
  wd.humidity = doc["current"]["relative_humidity_2m"].as<int>();
  wd.pressure = doc["current"]["pressure_msl"].as<int>();
  wd.windKph = doc["current"]["wind_speed_10m"].as<float>(); // already km/h
  wd.weatherCode = doc["current"]["weather_code"].as<int>();
  wd.weatherDesc = String();
  wd.isNightTime = false;
  wd.wifiRSSI = NetworkManager::getWiFiRSSI();
  updateWeatherOnly(wd);
  return true;
}
