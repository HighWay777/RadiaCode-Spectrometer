#pragma once

#include <Arduino_GFX_Library.h>
#include "../config/config_store.h"

// Forward declarations for page-specific drawing functions
void drawPageHome(Arduino_GFX* gfx, AppConfig* config);
void handleHomeTouchEvents(int16_t x, int16_t y, bool pressed);

// Weather data structure for displaying on the home screen
struct WeatherData {
  // Boiler indoor/flow temperature (from Boiler API)
  float currentTemp;
  float targetTemp;
  float dayTemp;
  float nightTemp;
  float boostTemp;
  // Outdoor weather temperature (from Weather provider)
  float extTemp;
  int humidity;
  int pressure;
  float windKph;
  int solar;
  bool isNightTime;
  bool isBoostTime;
  String targetSource;
  bool heaterOn;
  bool manualOverride;
  int weatherCode;      // For icon display
  String weatherDesc;   // Weather description
  int heaterControlMode;
  bool aiEnabled;
  int wifiRSSI;
  String storageMode;  // "MQTT" or "NVRAM" - from activeStorageSource API field
  // Remote device status (from boiler API)
  int remoteFreeMemory;     // Remote device free heap memory
  int remoteRSSI;           // Remote device WiFi RSSI
  String remoteUptime;      // Remote device uptime string
  String remoteServerTime;  // Remote device current time
  // Boiler/Energy stats
  float efficiency;
  float heatLossRate;
  float kwhTotal;
  float costTotal;
  float dailyKWh;
  float costToday;
  float dailyCost;
};

// External functions to update data
// - updateWeatherOnly: updates only weather-related fields (extTemp, humidity, pressure, wind, weatherCode, weatherDesc, isNightTime, solar, wifiRSSI)
// - updateBoilerData: updates only boiler/energy-related fields (currentTemp, targetTemp, schedule temps, heater flags, energy metrics)
extern void updateWeatherOnly(const WeatherData& data);
extern void updateBoilerData(const WeatherData& data);
extern WeatherData* getCurrentWeatherData();

// Layout helpers for home screen
void drawHomeHeader(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);  // Header with API status badge
void drawWeatherPanel(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);
void drawBoilerSensorOnly(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);  // Anti-flicker: update boiler values only
void drawClockPanel(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);
void drawClockPanelTime(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);  // Internal: updates only time each second
void drawClockPanelContent(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);  // Internal: initial full draw
void drawStatusBar(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);

// Anti-flicker change detection functions
bool hasBoilerDataChanged();
bool hasStatusBarDataChanged();
bool hasClockChanged();
bool hasWeatherDataChanged();

struct HomeLayoutGeometry {
  int16_t margin;
  int16_t headerY;
  int16_t headerHeight;
  int16_t weatherY;
  int16_t weatherHeight;
  int16_t clockY;
  int16_t clockHeight;
  int16_t statusY;
  int16_t statusHeight;
};

HomeLayoutGeometry getHomeLayoutGeometry();

struct HomeWeatherLayout {
  int16_t panelX;
  int16_t panelY;
  int16_t panelW;
  int16_t panelH;
  int16_t iconX;
  int16_t iconY;
  int16_t iconW;
  int16_t iconH;
  int16_t summaryX;
  int16_t summaryY;
  int16_t summaryW;
  int16_t summaryH;
  int16_t badgesY;
  int16_t badgeHeight;
  int16_t badgeGap;
  bool showSensor;
  int16_t sensorX;
  int16_t sensorY;
  int16_t sensorW;
  int16_t sensorH;
};

HomeWeatherLayout computeHomeWeatherLayout(bool showSensor);