#include "page_home.h"
#include "ui_framework.h"
#include "weather_icons.h"
#include "style_overrides.h"
#include <time.h>
#include <Fonts/GFXFF/FreeSans9pt7b.h>
#include <Fonts/GFXFF/FreeSans12pt7b.h>
#include <Fonts/GFXFF/FreeSansBold12pt7b.h>
#include <Fonts/GFXFF/FreeSansBold18pt7b.h>
#include <Fonts/GFXFF/FreeSansBold24pt7b.h>
#include "../utils/time_manager.h"
#include "../network/network_manager.h"

/* ============================================================================
   CLEAN HOME PAGE IMPLEMENTATION FOR ESP32-S3 + NV3041A QSPI DISPLAY
   Screen: 480x272 pixels, patched driver for proper partial updates
   Design: Simple, clean, no overlap, proper spacing
   ============================================================================ */

// ========== HELPER FUNCTIONS ==========

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t alphaBlend(uint16_t fg, uint16_t bg, uint8_t alpha) {
  uint8_t r1 = (fg >> 11) & 0x1F, g1 = (fg >> 5) & 0x3F, b1 = fg & 0x1F;
  uint8_t r2 = (bg >> 11) & 0x1F, g2 = (bg >> 5) & 0x3F, b2 = bg & 0x1F;
  uint8_t r = ((r1 * alpha) + (r2 * (255 - alpha))) / 255;
  uint8_t g = ((g1 * alpha) + (g2 * (255 - alpha))) / 255;
  uint8_t b = ((b1 * alpha) + (b2 * (255 - alpha))) / 255;
  return (r << 11) | (g << 5) | b;
}

// Get font by size level
static const GFXfont* getFont(int size) {
  switch (size) {
    case 1: return &FreeSans9pt7b;
    case 2: return &FreeSans12pt7b;
    case 3: return &FreeSansBold12pt7b;
    case 4: return &FreeSansBold18pt7b;
    case 5: return &FreeSansBold24pt7b;
    default: return &FreeSans9pt7b;
  }
}

// Draw text at position (handles font baseline automatically)
static void printText(Arduino_GFX* gfx, int16_t x, int16_t y, const char* text, 
                     const GFXfont* font, uint16_t color) {
  if (!text || !font) return;
  gfx->setFont(font);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  
  // Get text bounds to calculate baseline
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t baseline = y - y1;
  
  gfx->setCursor(x, baseline);
  gfx->print(text);
}

// Get text width
static int16_t getTextWidth(Arduino_GFX* gfx, const char* text, const GFXfont* font) {
  if (!text || !font) return 0;
  gfx->setFont(font);
  gfx->setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return (int16_t)w;
}

// FIX #5: Convert WiFi RSSI (dBm) to signal strength percentage
static int getRssiPercent(int rssi) {
  // RSSI typical range: -30 dBm (excellent) to -120 dBm (poor)
  // Clamp and convert to percentage: 0-100
  if (rssi >= -30) return 100;
  if (rssi <= -120) return 0;
  
  // Linear interpolation: -30 to -120 dBm maps to 100 to 0%
  return (rssi + 120) * 100 / 90;
}

// FIX #5: Get WiFi signal strength color coding
static uint16_t getWifiStrengthColor(int percent) {
  if (percent >= 80) return rgb565(0, 255, 0);        // Green: Excellent
  if (percent >= 60) return rgb565(150, 255, 0);      // Yellow-green: Good
  if (percent >= 40) return rgb565(255, 255, 0);      // Yellow: Fair
  if (percent >= 20) return rgb565(255, 180, 0);      // Orange: Weak
  return rgb565(255, 0, 0);                            // Red: Poor
}

// NEW: Get boiler temperature gradient color (35°C to 52°C range)
// 35°C = Cool (Blue) → 52°C = Hot (Red)
static uint16_t getTemperatureColor(float tempC) {
  // Clamp temperature to range 35-52°C
  if (tempC <= 35.0f) return rgb565(0, 100, 255);     // Cold: Blue
  if (tempC >= 52.0f) return rgb565(255, 50, 0);      // Hot: Red-Orange
  
  // Linear gradient from cold (blue) to hot (red) across 35-52°C range
  // Calculate position in range: 0.0 (cold) to 1.0 (hot)
  float position = (tempC - 35.0f) / (52.0f - 35.0f); // 0.0 to 1.0
  
  // Gradient: Blue (0,100,255) → Cyan (0,200,255) → Green (0,255,0) → Yellow (255,255,0) → Orange (255,150,0) → Red (255,50,0)
  if (position < 0.2f) {
    // 35-39°C: Blue to Cyan
    float p = position / 0.2f;
    uint8_t r = 0;
    uint8_t g = (uint8_t)(100 + (200 - 100) * p);
    uint8_t b = 255;
    return rgb565(r, g, b);
  }
  else if (position < 0.4f) {
    // 39-43°C: Cyan to Green
    float p = (position - 0.2f) / 0.2f;
    uint8_t r = 0;
    uint8_t g = 200 + (55 * p);
    uint8_t b = (uint8_t)(255 - (255 * p));
    return rgb565(r, g, b);
  }
  else if (position < 0.6f) {
    // 43-47°C: Green to Yellow
    float p = (position - 0.4f) / 0.2f;
    uint8_t r = (uint8_t)(255 * p);
    uint8_t g = 255;
    uint8_t b = 0;
    return rgb565(r, g, b);
  }
  else if (position < 0.8f) {
    // 47-50°C: Yellow to Orange
    float p = (position - 0.6f) / 0.2f;
    uint8_t r = 255;
    uint8_t g = (uint8_t)(255 - (105 * p));
    uint8_t b = 0;
    return rgb565(r, g, b);
  }
  else {
    // 50-52°C: Orange to Red
    float p = (position - 0.8f) / 0.2f;
    uint8_t r = 255;
    uint8_t g = (uint8_t)(150 - (100 * p));
    uint8_t b = 0;
    return rgb565(r, g, b);
  }
}

// ========== GLOBAL WEATHER DATA ==========
static WeatherData globalWeather;
static bool weatherInitialized = false;

// ========== CHANGE TRACKING FOR SMART UPDATES ==========
// Cache of last displayed values to detect changes and avoid unnecessary redraws
static struct {
  float currentTemp;
  float targetTemp;
  float extTemp;
  int humidity;
  int pressure;
  float windKph;
  bool heaterOn;
  int heaterControlMode;
  float efficiency;
  float dailyKWh;
  int minute;
  int apiFailCount;
  String storageMode;
  String remoteServerTime;
  int remoteFreeMemory;
  bool initialized;
} lastDisplayedValues = {0, 0, 0, 0, 0, 0, false, 0, 0, 0, -1, 0, "", "", 0, false};

// Check if any value has changed that requires update
bool hasBoilerDataChanged() {
  return lastDisplayedValues.currentTemp != globalWeather.currentTemp ||
         lastDisplayedValues.targetTemp != globalWeather.targetTemp ||
         lastDisplayedValues.heaterOn != globalWeather.heaterOn;
}

bool hasStatusBarDataChanged() {
  return lastDisplayedValues.efficiency != globalWeather.efficiency ||
         lastDisplayedValues.dailyKWh != globalWeather.dailyKWh ||
         lastDisplayedValues.heaterControlMode != globalWeather.heaterControlMode ||
         lastDisplayedValues.storageMode != globalWeather.storageMode ||
         lastDisplayedValues.remoteServerTime != globalWeather.remoteServerTime ||
         lastDisplayedValues.remoteFreeMemory != globalWeather.remoteFreeMemory;
}

bool hasWeatherDataChanged() {
  return lastDisplayedValues.extTemp != globalWeather.extTemp ||
         lastDisplayedValues.humidity != globalWeather.humidity ||
         lastDisplayedValues.pressure != globalWeather.pressure ||
         lastDisplayedValues.windKph != globalWeather.windKph;
}

bool hasClockChanged() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return lastDisplayedValues.minute != timeinfo.tm_min;
}

// Update the tracking cache after display update
static void updateDisplayCache() {
  lastDisplayedValues.currentTemp = globalWeather.currentTemp;
  lastDisplayedValues.targetTemp = globalWeather.targetTemp;
  lastDisplayedValues.extTemp = globalWeather.extTemp;
  lastDisplayedValues.humidity = globalWeather.humidity;
  lastDisplayedValues.pressure = globalWeather.pressure;
  lastDisplayedValues.windKph = globalWeather.windKph;
  lastDisplayedValues.heaterOn = globalWeather.heaterOn;
  lastDisplayedValues.heaterControlMode = globalWeather.heaterControlMode;
  lastDisplayedValues.efficiency = globalWeather.efficiency;
  lastDisplayedValues.dailyKWh = globalWeather.dailyKWh;
  lastDisplayedValues.storageMode = globalWeather.storageMode;
  lastDisplayedValues.remoteServerTime = globalWeather.remoteServerTime;
  lastDisplayedValues.remoteFreeMemory = globalWeather.remoteFreeMemory;
  lastDisplayedValues.apiFailCount = NetworkManager::getApiFailureCount();
  
  // Update minute tracker
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  lastDisplayedValues.minute = timeinfo.tm_min;
  
  lastDisplayedValues.initialized = true;
}

static void initWeatherData() {
  if (weatherInitialized) return;
  
  // Set defaults
  globalWeather.currentTemp = 19.5f;
  globalWeather.targetTemp = 21.0f;
  globalWeather.extTemp = 15.2f;
  globalWeather.humidity = 65;
  globalWeather.pressure = 1013;
  globalWeather.windKph = 8.5f;
  globalWeather.solar = 800;
  globalWeather.weatherCode = 1; // Sunny
  globalWeather.weatherDesc = "Clear sky";
  globalWeather.heaterOn = false;
  globalWeather.heaterControlMode = 0; // OFF
  globalWeather.efficiency = 0.0f;
  globalWeather.kwhTotal = 0.0f;
  globalWeather.dailyKWh = 0.0f;
  globalWeather.wifiRSSI = -50;
  globalWeather.isNightTime = false;
  
  weatherInitialized = true;
}

void updateWeatherOnly(const WeatherData& data) {
  initWeatherData();
  globalWeather.extTemp = data.extTemp;
  globalWeather.humidity = data.humidity;
  globalWeather.pressure = data.pressure;
  globalWeather.windKph = data.windKph;
  globalWeather.solar = data.solar;
  globalWeather.weatherCode = data.weatherCode;
  globalWeather.weatherDesc = data.weatherDesc;
  globalWeather.wifiRSSI = data.wifiRSSI;
  globalWeather.isNightTime = data.isNightTime;
  // NOTE: Don't call uiRequestHomeUpdate() here - uiLoop handles periodic updates
  // to avoid duplicate refresh and flickering
}

void updateBoilerData(const WeatherData& data) {
  initWeatherData();
  
  // Temperature data
  globalWeather.currentTemp = data.currentTemp;
  globalWeather.targetTemp = data.targetTemp;
  globalWeather.dayTemp = data.dayTemp;
  globalWeather.nightTemp = data.nightTemp;
  globalWeather.boostTemp = data.boostTemp;
  
  // State data
  globalWeather.heaterOn = data.heaterOn;
  globalWeather.heaterControlMode = data.heaterControlMode;
  globalWeather.aiEnabled = data.aiEnabled;
  globalWeather.manualOverride = data.manualOverride;
  globalWeather.isNightTime = data.isNightTime;
  globalWeather.isBoostTime = data.isBoostTime;
  globalWeather.targetSource = data.targetSource;
  
  // Energy metrics
  globalWeather.efficiency = data.efficiency;
  globalWeather.heatLossRate = data.heatLossRate;
  globalWeather.kwhTotal = data.kwhTotal;
  globalWeather.dailyKWh = data.dailyKWh;
  globalWeather.costTotal = data.costTotal;
  globalWeather.costToday = data.costToday;
  globalWeather.dailyCost = data.dailyCost;
  
  // Storage mode (MQTT or NVRAM)
  globalWeather.storageMode = data.storageMode;
  
  // Remote device status
  globalWeather.remoteFreeMemory = data.remoteFreeMemory;
  globalWeather.remoteRSSI = data.remoteRSSI;
  globalWeather.remoteUptime = data.remoteUptime;
  globalWeather.remoteServerTime = data.remoteServerTime;
  
  // WiFi RSSI from remote API (if provided)
  if (data.wifiRSSI != 0 && data.wifiRSSI != -100) {
    globalWeather.wifiRSSI = data.wifiRSSI;
  }
  // NOTE: Don't call uiRequestHomeUpdate() here - uiLoop handles periodic updates
  // to avoid duplicate refresh and flickering
}

WeatherData* getCurrentWeatherData() {
  initWeatherData();
  return &globalWeather;
}

// ========== LAYOUT CALCULATION ==========

HomeLayoutGeometry getHomeLayoutGeometry() {
  HomeLayoutGeometry layout;
  layout.margin = 10;
  layout.headerHeight = 32;
  layout.headerY = layout.margin;
  
  // Weather panel
  layout.weatherY = layout.headerY + layout.headerHeight + 8;
  layout.weatherHeight = 120;
  
  // Clock panel
  layout.clockY = layout.weatherY + layout.weatherHeight + 8;
  layout.clockHeight = 60;
  
  // Status bar at bottom
  layout.statusHeight = 24;
  layout.statusY = 272 - layout.statusHeight - layout.margin;
  
  return layout;
}

HomeWeatherLayout computeHomeWeatherLayout(bool showSensor) {
  HomeLayoutGeometry layout = getHomeLayoutGeometry();
  HomeWeatherLayout out;
  
  out.panelX = layout.margin;
  out.panelY = layout.weatherY;
  out.panelW = 480 - (layout.margin * 2);
  out.panelH = layout.weatherHeight;
  out.showSensor = showSensor;
  
  // Weather icon position (left side of panel)
  int16_t contentY = out.panelY + 30; // Below "Weather" label
  out.iconX = out.panelX + 15;
  out.iconY = contentY;
  out.iconW = 64;
  out.iconH = 64;
  
  // Temperature summary (to the right of icon)
  out.summaryX = out.iconX + out.iconW + 12;
  out.summaryY = contentY;
  out.summaryW = out.panelW - (out.iconW + 12 + 30); // Leave margin on right
  out.summaryH = 48;
  
  // Badge grid (below summary)
  out.badgesY = out.summaryY + out.summaryH + 8;
  out.badgeHeight = 20;
  out.badgeGap = 6;
  
  // Boiler sensor badge (optional, bottom-right of panel)
  if (showSensor) {
    out.sensorX = out.panelX + out.panelW - 100;
    out.sensorY = out.panelY + out.panelH - 30;
    out.sensorW = 90;
    out.sensorH = 24;
  } else {
    out.sensorX = 0;
    out.sensorY = 0;
    out.sensorW = 0;
    out.sensorH = 0;
  }
  
  return out;
}

// ========== DRAWING FUNCTIONS ==========

void drawPageHome(Arduino_GFX* gfx, AppConfig* config) {
  initWeatherData();
  
  // Clear screen
  gfx->fillScreen(currentTheme->background);
  gfx->setTextWrap(false);
  
  HomeLayoutGeometry layout = getHomeLayoutGeometry();
  int16_t width = 480 - (layout.margin * 2);
  
  // Draw all panels
  drawHomeHeader(gfx, layout.margin, layout.headerY, width, layout.headerHeight);
  drawWeatherPanel(gfx, layout.margin, layout.weatherY, width, layout.weatherHeight);
  drawClockPanel(gfx, layout.margin, layout.clockY, width, layout.clockHeight);
  drawStatusBar(gfx, layout.margin, layout.statusY, width, layout.statusHeight);
}

// ========== HEADER ==========

void drawHomeHeader(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Skip drawing if hidden via style override
  if (StyleOverrides::isHidden("home.header")) return;
  
  // Use style override for background color if available
  uint16_t defaultBg = alphaBlend(currentTheme->header, currentTheme->background, 200);
  uint16_t bgColor = StyleOverrides::getColor("home.header", defaultBg);
  gfx->fillRoundRect(x, y, w, h, 8, bgColor);
  gfx->drawRoundRect(x, y, w, h, 8, alphaBlend(bgColor, currentTheme->text, 100));
  
  // "Home" title - use style override for text color and font size
  uint16_t textColor = StyleOverrides::getTextColor("home.header", currentTheme->text);
  uint8_t fontSize = StyleOverrides::getFontSize("home.header", 3);
  printText(gfx, x + 12, y + 8, "Home", getFont(fontSize), textColor);
  
  // Decorative dots
  for (int i = 0; i < 5; i++) {
    gfx->fillCircle(x + w - 15 - (i * 10), y + h / 2, 2, 
                   alphaBlend(currentTheme->text, currentTheme->background, 150));
  }
  
  // API Status Badge - Show if API is offline (> 10 failures)
  if (NetworkManager::getApiFailureCount() >= 10) {
    // Red badge for API offline
    uint16_t badgeX = x + w - 95;
    uint16_t badgeY = y + 6;
    uint16_t badgeW = 85;
    uint16_t badgeH = 20;
    uint16_t badgeColor = rgb565(200, 60, 60);  // Red
    
    // Draw rounded badge background
    gfx->fillRoundRect(badgeX, badgeY, badgeW, badgeH, 4, badgeColor);
    gfx->drawRoundRect(badgeX, badgeY, badgeW, badgeH, 4, rgb565(255, 255, 255));
    
    // Draw text "API offline"
    gfx->setFont(&FreeSans9pt7b);
    gfx->setTextColor(rgb565(255, 255, 255));
    int16_t textX = badgeX + 8;
    int16_t textY = badgeY + 16;
    gfx->setCursor(textX, textY);
    gfx->println("API offline");
  }
}

// ========== WEATHER PANEL ==========

void drawWeatherPanel(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Skip drawing if hidden via style override
  if (StyleOverrides::isHidden("home.weather.panel")) return;
  
  AppConfig* config = ConfigStore::getConfig();
  
  // Use style override for panel background color if available
  uint16_t defaultBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  uint16_t panelBg = StyleOverrides::getColor("home.weather.panel", defaultBg);
  uint16_t panelText = StyleOverrides::getTextColor("home.weather.panel", currentTheme->text);
  gfx->fillRoundRect(x, y, w, h, 12, panelBg);
  gfx->drawRoundRect(x, y, w, h, 12, alphaBlend(panelBg, currentTheme->text, 80));
  
  // "Weather" label chip
  gfx->fillRoundRect(x + 10, y + 6, 70, 16, 8, currentTheme->accent);
  printText(gfx, x + 15, y + 6, "Weather", getFont(1), WHITE);
  
  // FIX #2: Calculate available space properly to avoid overlaps
  int16_t contentY = y + 30;
  int16_t iconX = x + 15;
  int16_t iconSize = 50;
  
  // Check if boiler sensor should be shown
  bool showBoilerSensor = config->showBoilerTempOnHome && !isnan(globalWeather.currentTemp);
  // Check if boiler is hidden via style override
  if (StyleOverrides::isHidden("home.badge.boiler")) {
    showBoilerSensor = false;
  }
  int16_t boilerSensorW = showBoilerSensor ? 100 : 0;
  int16_t boilerSensorGap = showBoilerSensor ? 10 : 0;
  
  // Calculate available width for temperature and details
  int16_t tempSectionW = w - (iconSize + 25 + 30 + boilerSensorW + boilerSensorGap);
  int16_t tempX = iconX + iconSize + 15;
  
  // Draw weather icon (check if hidden)
  if (!StyleOverrides::isHidden("home.weather.icon")) {
    // Validate weatherCode is in valid range (0-99 for Open-Meteo)
    if (config->showWeatherIcons && globalWeather.weatherCode >= 0 && globalWeather.weatherCode < 100) {
      drawWeatherIcon(gfx, iconX + iconSize/2, contentY + iconSize/2, iconSize,
                     globalWeather.weatherCode, globalWeather.isNightTime);
    } else {
      drawSun(gfx, iconX + iconSize/2, contentY + iconSize/2, iconSize, currentTheme->accent);
    }
  }
  
  // Temperature summary (check if hidden)
  if (!StyleOverrides::isHidden("home.weather.summary")) {
    // Outdoor temperature - large (use panel text color if overridden)
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f°C", globalWeather.extTemp);
    uint16_t summaryColor = StyleOverrides::getTextColor("home.weather.summary", panelText);
    uint8_t summaryFontSize = StyleOverrides::getFontSize("home.weather.summary", 4);
    printText(gfx, tempX, contentY, tempStr, getFont(summaryFontSize), summaryColor);
    
    // Weather description - FIX #2: Truncate if too long
    const char* desc = globalWeather.weatherDesc.c_str();
    if (!desc || strlen(desc) == 0) desc = "Clear";
    
    char truncDesc[32];
    strncpy(truncDesc, desc, sizeof(truncDesc) - 1);
    truncDesc[sizeof(truncDesc) - 1] = '\0';
    
    int16_t descW = getTextWidth(gfx, truncDesc, getFont(2));
    if (descW > tempSectionW - 10) {
      // Truncate with ellipsis
      for (int i = strlen(truncDesc) - 1; i >= 0; i--) {
        truncDesc[i] = '\0';
        descW = getTextWidth(gfx, truncDesc, getFont(2)) + getTextWidth(gfx, "...", getFont(2));
        if (descW <= tempSectionW - 10) {
          strcat(truncDesc, "...");
          break;
        }
      }
    }
    
    printText(gfx, tempX, contentY + 32, truncDesc, getFont(2), 
             alphaBlend(currentTheme->textSecondary, summaryColor, 180));
  }
  
  // Weather details grid (2x2) - matches base_layouts.cpp layout
  int16_t detailY = contentY + 55;
  int16_t detailsPerRow = 2;
  int16_t detailColW = (tempSectionW - 10) / detailsPerRow;
  int16_t detailX1 = tempX;
  int16_t detailX2 = tempX + detailColW;
  
  char str[32];
  
  // Row 1: Outside Temp (extTemp) + Humidity - matches base_layouts order
  if (!StyleOverrides::isHidden("home.badge.extTemp")) {
    snprintf(str, sizeof(str), "Out: %.1fC", globalWeather.extTemp);
    uint16_t extTempColor = StyleOverrides::getTextColor("home.badge.extTemp", panelText);
    printText(gfx, detailX1, detailY, str, getFont(1), extTempColor);
  }
  
  if (!StyleOverrides::isHidden("home.badge.humidity")) {
    snprintf(str, sizeof(str), "Hum: %d%%", globalWeather.humidity);
    uint16_t humidColor = StyleOverrides::getTextColor("home.badge.humidity", panelText);
    printText(gfx, detailX2, detailY, str, getFont(1), humidColor);
  }
  
  // Row 2: Pressure + Wind - matches base_layouts order
  detailY += 18;
  
  if (!StyleOverrides::isHidden("home.badge.pressure")) {
    snprintf(str, sizeof(str), "P: %dhPa", globalWeather.pressure);
    uint16_t pressColor = StyleOverrides::getTextColor("home.badge.pressure", panelText);
    printText(gfx, detailX1, detailY, str, getFont(1), pressColor);
  }
  
  if (!StyleOverrides::isHidden("home.badge.wind")) {
    snprintf(str, sizeof(str), "Wind: %.0fkm/h", globalWeather.windKph);
    uint16_t windColor = StyleOverrides::getTextColor("home.badge.wind", panelText);
    printText(gfx, detailX2, detailY, str, getFont(1), windColor);
  }
  
  // Boiler temperature sensor - FIX #2: Proper spacing to avoid overlap
  if (showBoilerSensor) {
    int16_t boilerX = x + w - boilerSensorW - 10;
    int16_t boilerY = contentY;
    int16_t boilerH = h - 38;
    
    // Sensor box
    uint16_t sensorBg = StyleOverrides::getColor("home.badge.boiler", alphaBlend(currentTheme->accent, panelBg, 50));
    gfx->fillRoundRect(boilerX, boilerY, boilerSensorW, boilerH, 8, sensorBg);
    gfx->drawRoundRect(boilerX, boilerY, boilerSensorW, boilerH, 8, 
                      alphaBlend(currentTheme->accent, currentTheme->text, 120));
    
    // "Boiler" label
    uint16_t boilerTextColor = StyleOverrides::getTextColor("home.badge.boiler", currentTheme->text);
    printText(gfx, boilerX + 8, boilerY + 8, "Boiler", getFont(1), boilerTextColor);
    
    // Heater status icon (flame) - positioned to the right of "Boiler" label
    int16_t flameX = boilerX + boilerSensorW - 22;  // Right side of sensor box
    int16_t flameY = boilerY + 6;
    int16_t flameSize = 14;
    
    if (globalWeather.heaterOn) {
      // Draw flame icon when heater is ON (orange/red)
      uint16_t flameColor = rgb565(255, 100, 0);  // Orange flame
      uint16_t flameTip = rgb565(255, 200, 0);    // Yellow tip
      
      // Flame shape: simple triangular flame with rounded base
      // Main flame body
      gfx->fillTriangle(flameX, flameY + flameSize,           // Bottom left
                        flameX + flameSize, flameY + flameSize, // Bottom right  
                        flameX + flameSize/2, flameY,           // Top center
                        flameColor);
      // Inner flame (smaller, brighter)
      gfx->fillTriangle(flameX + 3, flameY + flameSize - 2,
                        flameX + flameSize - 3, flameY + flameSize - 2,
                        flameX + flameSize/2, flameY + 4,
                        flameTip);
    } else {
      // Draw inactive/off indicator (gray flame outline or circle)
      uint16_t offColor = rgb565(128, 128, 128);  // Gray
      
      // Empty flame outline when OFF
      gfx->drawTriangle(flameX, flameY + flameSize,
                        flameX + flameSize, flameY + flameSize,
                        flameX + flameSize/2, flameY,
                        offColor);
    }
    
    // Current temperature - large with gradient color based on temperature
    snprintf(str, sizeof(str), "%.1f°", globalWeather.currentTemp);
    int16_t centerX = boilerX + boilerSensorW / 2;
    int16_t textW = getTextWidth(gfx, str, getFont(4));
    uint16_t tempColor = getTemperatureColor(globalWeather.currentTemp);  // Get gradient color
    printText(gfx, centerX - textW/2, boilerY + 28, str, getFont(4), tempColor);
    
    // Target temperature
    snprintf(str, sizeof(str), "→ %.0f°", globalWeather.targetTemp);
    textW = getTextWidth(gfx, str, getFont(1));
    printText(gfx, centerX - textW/2, boilerY + 56, str, getFont(1), currentTheme->cool);
  }
  
  // Update the display cache after drawing
  updateDisplayCache();
}

// ========== BOILER SENSOR ONLY UPDATE (Anti-Flicker) ==========
// Updates only the boiler sensor values without redrawing the entire panel
// This is much faster and eliminates visible flicker
void drawBoilerSensorOnly(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  AppConfig* config = ConfigStore::getConfig();
  
  // Check if boiler sensor should be shown
  bool showBoilerSensor = config->showBoilerTempOnHome && !isnan(globalWeather.currentTemp);
  if (StyleOverrides::isHidden("home.badge.boiler")) {
    showBoilerSensor = false;
  }
  if (!showBoilerSensor) return;
  
  // Static cache for last drawn values - only redraw if values changed
  static float lastDrawnTemp = -999;
  static float lastDrawnTarget = -999;
  static bool lastDrawnHeaterOn = false;
  static bool firstDraw = true;
  
  // Check if anything actually changed
  bool tempChanged = (lastDrawnTemp != globalWeather.currentTemp);
  bool targetChanged = (lastDrawnTarget != globalWeather.targetTemp);
  bool heaterChanged = (lastDrawnHeaterOn != globalWeather.heaterOn);
  
  // Skip redraw if nothing changed (unless first draw)
  if (!firstDraw && !tempChanged && !targetChanged && !heaterChanged) {
    return;
  }
  
  int16_t boilerSensorW = 100;
  int16_t contentY = y + 30;
  int16_t boilerX = x + w - boilerSensorW - 10;
  int16_t boilerY = contentY;
  int16_t boilerH = h - 38;
  
  // Get the background color to clear value areas
  uint16_t defaultBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  uint16_t panelBg = StyleOverrides::getColor("home.weather.panel", defaultBg);
  uint16_t sensorBg = StyleOverrides::getColor("home.badge.boiler", alphaBlend(currentTheme->accent, panelBg, 50));
  
  char str[16];
  int16_t centerX = boilerX + boilerSensorW / 2;
  int16_t textW;
  
  // Only update temperature if it changed
  if (tempChanged || firstDraw) {
    int16_t tempValueY = boilerY + 24;
    int16_t tempValueH = 28;
    gfx->fillRect(boilerX + 2, tempValueY, boilerSensorW - 4, tempValueH, sensorBg);
    
    snprintf(str, sizeof(str), "%.1f°", globalWeather.currentTemp);
    textW = getTextWidth(gfx, str, getFont(4));
    uint16_t tempColor = getTemperatureColor(globalWeather.currentTemp);
    printText(gfx, centerX - textW/2, boilerY + 28, str, getFont(4), tempColor);
    lastDrawnTemp = globalWeather.currentTemp;
  }
  
  // Only update target if it changed
  if (targetChanged || firstDraw) {
    int16_t targetValueY = boilerY + 52;
    int16_t targetValueH = 18;
    gfx->fillRect(boilerX + 2, targetValueY, boilerSensorW - 4, targetValueH, sensorBg);
    
    snprintf(str, sizeof(str), "→ %.0f°", globalWeather.targetTemp);
    textW = getTextWidth(gfx, str, getFont(1));
    printText(gfx, centerX - textW/2, boilerY + 56, str, getFont(1), currentTheme->cool);
    lastDrawnTarget = globalWeather.targetTemp;
  }
  
  // Only update flame icon if heater state changed
  if (heaterChanged || firstDraw) {
    int16_t flameX = boilerX + boilerSensorW - 22;
    int16_t flameY = boilerY + 6;
    int16_t flameSize = 14;
    
    // Clear flame area
    gfx->fillRect(flameX - 1, flameY - 1, flameSize + 2, flameSize + 2, sensorBg);
    
    if (globalWeather.heaterOn) {
      uint16_t flameColor = rgb565(255, 100, 0);
      uint16_t flameTip = rgb565(255, 200, 0);
      gfx->fillTriangle(flameX, flameY + flameSize,
                        flameX + flameSize, flameY + flameSize,
                        flameX + flameSize/2, flameY,
                        flameColor);
      gfx->fillTriangle(flameX + 3, flameY + flameSize - 2,
                        flameX + flameSize - 3, flameY + flameSize - 2,
                        flameX + flameSize/2, flameY + 4,
                        flameTip);
    } else {
      uint16_t offColor = rgb565(128, 128, 128);
      gfx->drawTriangle(flameX, flameY + flameSize,
                        flameX + flameSize, flameY + flameSize,
                        flameX + flameSize/2, flameY,
                        offColor);
    }
    lastDrawnHeaterOn = globalWeather.heaterOn;
  }
  
  firstDraw = false;
}

// ========== CLOCK PANEL ==========

// FIX: Function to redraw ONLY the time (for per-minute updates without flicker)
// This is used for per-minute updates to avoid flickering the display
void drawClockPanelTime(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  AppConfig* config = ConfigStore::getConfig();
  
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[32];
  
  if (TimeManager::timeIsSet()) {
    if (config->showClock) {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // FIX: No seconds, only HH:MM
    } else {
      strncpy(timeStr, "--:--", sizeof(timeStr));
    }
  } else {
    strncpy(timeStr, "--:--", sizeof(timeStr));
  }
  
  // Clear the time area with proper spacing (increased height to prevent artifacts)
  uint16_t panelBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  gfx->fillRect(x + 1, y + 6, w - 2, 26, panelBg);  // Larger clear area to prevent anti-aliasing artifacts
  
  // Draw time - centered, medium font
  int16_t centerX = x + w / 2;
  int16_t timeW = getTextWidth(gfx, timeStr, getFont(4));
  printText(gfx, centerX - timeW/2, y + 8, timeStr, getFont(4), currentTheme->text);
}

// FIX: Function to redraw the entire clock panel content (initial draw)
// This draws both time and date - only called on initial page draw
void drawClockPanelContent(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  AppConfig* config = ConfigStore::getConfig();
  
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[32], dateStr[32];
  
  if (TimeManager::timeIsSet()) {
    // Time - no seconds, only HH:MM
    if (config->showClock) {
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);  // FIX: No seconds
    } else {
      strncpy(timeStr, "--:--", sizeof(timeStr));
    }
    
    // Date
    if (config->showDate) {
      strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y", &timeinfo);
    } else {
      strncpy(dateStr, "---", sizeof(dateStr));
    }
  } else {
    strncpy(timeStr, "--:--", sizeof(timeStr));
    strncpy(dateStr, "Time not set", sizeof(dateStr));
  }
  
  // Clear entire clock area content (keep background intact)
  uint16_t panelBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  gfx->fillRect(x + 2, y + 8, w - 4, h - 10, panelBg);
  
  // Get style overrides for the clock
  uint16_t clockTextColor = StyleOverrides::getTextColor("home.clock.panel", currentTheme->text);
  uint8_t clockFontSize = StyleOverrides::getFontSize("home.clock.panel", 4);
  
  // Draw time - centered at top
  int16_t centerX = x + w / 2;
  int16_t timeW = getTextWidth(gfx, timeStr, getFont(clockFontSize));
  printText(gfx, centerX - timeW/2, y + 8, timeStr, getFont(clockFontSize), clockTextColor);
  
  // Draw date - centered, small font at bottom (with proper spacing to avoid overlap)
  int16_t dateW = getTextWidth(gfx, dateStr, getFont(1));
  printText(gfx, centerX - dateW/2, y + 35, dateStr, getFont(1),
           alphaBlend(currentTheme->textSecondary, clockTextColor, 180));
}

void drawClockPanel(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Skip drawing if hidden via style override
  if (StyleOverrides::isHidden("home.clock.panel")) return;
  
  // Use style override for panel background color if available
  uint16_t defaultBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  uint16_t panelBg = StyleOverrides::getColor("home.clock.panel", defaultBg);
  
  // Panel background with border
  gfx->fillRoundRect(x, y, w, h, 12, panelBg);
  gfx->drawRoundRect(x, y, w, h, 12, alphaBlend(panelBg, currentTheme->text, 80));
  
  // Draw clock content (initial full draw with both time and date)
  drawClockPanelContent(gfx, x, y, w, h);
}

// ========== STATUS BAR (CLEAN, NO OVERLAP) ==========

// Static cache for status bar positions (calculated once, reused)
static struct {
  int16_t timeX, timeW;
  int16_t memX, memW;
  int16_t energyX, energyW;
  int16_t effX, effW;
  bool layoutCalculated;
} statusBarLayout = {0, 0, 0, 0, 0, 0, 0, 0, false};

void drawStatusBar(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Skip drawing if hidden via style override
  if (StyleOverrides::isHidden("home.status")) return;
  
  // Use style override for background color if available
  uint16_t defaultBg = alphaBlend(currentTheme->header, currentTheme->card, 150);
  uint16_t barBg = StyleOverrides::getColor("home.status", defaultBg);
  uint16_t barText = StyleOverrides::getTextColor("home.status", currentTheme->text);
  
  // Only draw background on first draw or when forced
  static bool backgroundDrawn = false;
  if (!backgroundDrawn || !statusBarLayout.layoutCalculated) {
    gfx->fillRoundRect(x, y, w, h, 8, barBg);
    gfx->drawRoundRect(x, y, w, h, 8, alphaBlend(barBg, currentTheme->text, 100));
    backgroundDrawn = true;
  }
  
  const GFXfont* font = getFont(1);
  int16_t textY = y + 6;
  int16_t currentX = x + 8;
  int16_t maxX = x + w - 130;
  int16_t gap = 10;
  int16_t itemH = 16;  // Height of text items for clearing
  
  // Helper to update a value (clear area + redraw)
  auto updateValue = [&](int16_t& cachedX, int16_t& cachedW, const char* text, uint16_t color) {
    int16_t newW = getTextWidth(gfx, text, font);
    
    // Clear previous area if position known
    if (statusBarLayout.layoutCalculated && cachedW > 0) {
      gfx->fillRect(cachedX, y + 4, cachedW + 4, itemH, barBg);
    }
    
    // Draw new text
    printText(gfx, currentX, textY, text, font, color);
    
    // Cache position
    cachedX = currentX;
    cachedW = newW;
    currentX += newW + gap;
  };
  
  // === LEFT SIDE: Remote Server Time ===
  char timestamp[32];
  if (globalWeather.remoteServerTime.length() > 0) {
    snprintf(timestamp, sizeof(timestamp), "T:%s", globalWeather.remoteServerTime.c_str());
  } else {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (TimeManager::timeIsSet()) {
      snprintf(timestamp, sizeof(timestamp), "T:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
      strncpy(timestamp, "T:--:--", sizeof(timestamp));
    }
  }
  updateValue(statusBarLayout.timeX, statusBarLayout.timeW, timestamp, currentTheme->text);
  
  // === REMOTE MEMORY ===
  char memStr[16];
  int freeHeap = globalWeather.remoteFreeMemory;
  if (freeHeap > 0) {
    if (freeHeap > 1024 * 1024) {
      snprintf(memStr, sizeof(memStr), "M:%.1fM", freeHeap / (1024.0 * 1024.0));
    } else {
      snprintf(memStr, sizeof(memStr), "M:%dk", freeHeap / 1024);
    }
  } else {
    snprintf(memStr, sizeof(memStr), "M:--k");
  }
  
  uint16_t memColor = (freeHeap > 100000) ? rgb565(0, 255, 0) : 
                     (freeHeap > 50000) ? rgb565(255, 255, 0) : rgb565(255, 0, 0);
  memColor = alphaBlend(memColor, currentTheme->text, 180);
  updateValue(statusBarLayout.memX, statusBarLayout.memW, memStr, memColor);
  
  // === DAILY ENERGY ===
  char energyStr[16];
  if (globalWeather.dailyKWh > 0) {
    snprintf(energyStr, sizeof(energyStr), "E:%.1fkWh", globalWeather.dailyKWh);
  } else {
    snprintf(energyStr, sizeof(energyStr), "E:--kWh");
  }
  uint16_t energyColor = alphaBlend(rgb565(100, 200, 255), currentTheme->text, 180);
  updateValue(statusBarLayout.energyX, statusBarLayout.energyW, energyStr, energyColor);
  
  // === EFFICIENCY ===
  char effStr[16];
  float efficiency = globalWeather.efficiency;
  if (efficiency > 0) {
    snprintf(effStr, sizeof(effStr), "Eff:%.0f%%", efficiency);
  } else {
    snprintf(effStr, sizeof(effStr), "Eff:--%");
  }
  uint16_t effColor = (efficiency > 70) ? rgb565(0, 255, 0) : 
                     (efficiency > 50) ? rgb565(255, 255, 0) : rgb565(255, 128, 0);
  effColor = alphaBlend(effColor, currentTheme->text, 180);
  updateValue(statusBarLayout.effX, statusBarLayout.effW, effStr, effColor);
  
  // Mark layout as calculated
  statusBarLayout.layoutCalculated = true;
  
  // === RIGHT SIDE: Storage Mode Chip + Heater Mode Chip ===
  int16_t chipH = h - 8;
  int16_t chipY = y + 4;
  
  // Storage mode chip (MQTT/NVRAM) - leftmost of the two chips
  int16_t storageChipW = 48;
  int16_t storageChipX = x + w - 58 - 8 - storageChipW - 4;  // Position before mode chip
  
  // Determine storage text and color (no emojis - they don't render)
  bool isMqtt = globalWeather.storageMode.equalsIgnoreCase("MQTT");
  const char* storageText = isMqtt ? "MQTT" : "NVR";
  uint16_t storageBg = isMqtt ? alphaBlend(rgb565(0, 180, 0), barBg, 200)   // Green for MQTT
                              : alphaBlend(rgb565(200, 150, 0), barBg, 200); // Orange for NVRAM
  
  gfx->fillRoundRect(storageChipX, chipY, storageChipW, chipH, 6, storageBg);
  gfx->drawRoundRect(storageChipX, chipY, storageChipW, chipH, 6, 
                    isMqtt ? rgb565(0, 200, 0) : rgb565(220, 170, 0));
  
  int16_t storageTextW = getTextWidth(gfx, storageText, getFont(1));
  printText(gfx, storageChipX + (storageChipW - storageTextW)/2, chipY + 2, storageText, getFont(1), currentTheme->text);
  
  // Heater mode chip - just show mode text (no emojis)
  const char* mode = "";
  switch (globalWeather.heaterControlMode) {
    case 0: 
      mode = globalWeather.heaterOn ? "ON" : "OFF";
      break;
    case 1: 
      mode = "MAN";
      break;
    case 2: 
      mode = "AUTO";
      break;
    case 3: 
      mode = "AI";
      break;
    default: 
      mode = "---";
      break;
  }
  
  int16_t chipW = 58;
  int16_t chipX = x + w - chipW - 8;
  
  uint16_t chipBg = alphaBlend(currentTheme->accent, barBg, 200);
  gfx->fillRoundRect(chipX, chipY, chipW, chipH, 6, chipBg);
  gfx->drawRoundRect(chipX, chipY, chipW, chipH, 6, currentTheme->accent);
  
  // Center mode text in chip (no emoji prefix)
  int16_t modeW = getTextWidth(gfx, mode, getFont(1));
  printText(gfx, chipX + (chipW - modeW)/2, chipY + 2, mode, getFont(1), currentTheme->text);
}

// ========== TOUCH HANDLER ==========

void handleHomeTouchEvents(int16_t x, int16_t y, bool pressed) {
  // Touch handling if needed
  (void)x; (void)y; (void)pressed;
}
