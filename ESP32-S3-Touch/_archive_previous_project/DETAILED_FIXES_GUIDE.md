# DETAILED FIXES GUIDE - ESP32-S3 Touch UI

## Fix #1: Clock Seconds Not Updating ⏰

### Problem
Clock shows seconds but never updates - time appears frozen.

### Root Cause
- `drawPageHome()` is called once on navigation
- No continuous refresh loop for home page
- Seconds remain static after initial draw

### Solution

**File: `src/ui/ui_framework.cpp`**

Add a per-second refresh mechanism in `uiLoop()`:

```cpp
// At module level, add these static variables
static unsigned long lastHomeRefreshMs = 0;
static int lastHomeSecond = -1;

void uiLoop() {
  // ... existing auto-rotation code ...

  // NEW: Refresh home page every second for live clock
  if (currentPageId == PAGE_HOME && display) {
    unsigned long now = millis();
    
    // Check if 1 second has passed
    if (now - lastHomeRefreshMs >= 1000) {
      // Get current second
      time_t t = time(nullptr);
      struct tm* timeinfo = localtime(&t);
      int currentSecond = timeinfo->tm_sec;
      
      // Only redraw if second changed (avoid excessive redraws)
      if (currentSecond != lastHomeSecond) {
        lastHomeSecond = currentSecond;
        lastHomeRefreshMs = now;
        
        // Trigger smart redraw (see Fix #10 for partial updates)
        // For now, just request full redraw
        uiRequestRedraw();
      }
    }
  }
}
```

**Benefits:**
- Clock updates every second
- Time appears live and responsive
- No manual navigation needed to update

**Optimization:** After implementing partial updates (Fix #10), only redraw the clock panel instead of full screen.

---

## Fix #2: Overlapping Text in Weather Panel 📝

### Problem
Weather description, detail badges, and boiler sensor box can overlap. Text extends beyond intended boundaries.

### Root Cause
- No text width pre-calculation
- Fixed positions without accounting for content size
- No overflow handling

### Solution

**File: `src/ui/page_home.cpp`**

Replace the `drawWeatherPanel()` function with proper layout:

```cpp
void drawWeatherPanel(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  AppConfig* config = ConfigStore::getConfig();
  
  // Panel background
  uint16_t panelBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  gfx->fillRoundRect(x, y, w, h, 12, panelBg);
  gfx->drawRoundRect(x, y, w, h, 12, alphaBlend(panelBg, currentTheme->text, 80));
  
  // "Weather" label chip
  gfx->fillRoundRect(x + 10, y + 6, 70, 16, 8, currentTheme->accent);
  printText(gfx, x + 15, y + 6, "Weather", getFont(1), WHITE);
  
  // === LAYOUT CALCULATION ===
  int16_t contentY = y + 30;
  int16_t iconX = x + 15;
  int16_t iconSize = 50;
  
  // Check if boiler sensor should be shown
  bool showBoilerSensor = config->showBoilerTempOnHome && !isnan(globalWeather.currentTemp);
  int16_t boilerSensorW = showBoilerSensor ? 100 : 0;
  int16_t boilerSensorGap = showBoilerSensor ? 10 : 0;
  
  // Calculate available width for temperature and details
  int16_t tempSectionW = w - (iconSize + 25 + 30 + boilerSensorW + boilerSensorGap);
  int16_t tempX = iconX + iconSize + 15;
  
  // === DRAW WEATHER ICON ===
  if (config->showWeatherIcons && globalWeather.weatherCode >= 0) {
    drawWeatherIcon(gfx, iconX + iconSize/2, contentY + iconSize/2, iconSize,
                   globalWeather.weatherCode, globalWeather.isNightTime);
  } else {
    drawSun(gfx, iconX + iconSize/2, contentY + iconSize/2, iconSize, currentTheme->accent);
  }
  
  // === DRAW TEMPERATURE (with overflow handling) ===
  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%.1f°C", globalWeather.extTemp);
  printText(gfx, tempX, contentY, tempStr, getFont(4), currentTheme->text);
  
  // === DRAW WEATHER DESCRIPTION (with truncation) ===
  const char* desc = globalWeather.weatherDesc.c_str();
  if (!desc || strlen(desc) == 0) desc = "Clear";
  
  // Truncate description if too long
  char truncDesc[32];
  strncpy(truncDesc, desc, sizeof(truncDesc) - 1);
  truncDesc[sizeof(truncDesc) - 1] = '\0';
  
  int16_t descW = getTextWidth(gfx, truncDesc, getFont(2));
  if (descW > tempSectionW - 10) {
    // Truncate with ellipsis
    for (int i = strlen(truncDesc) - 1; i >= 0; i--) {
      truncDesc[i] = '\0';
      descW = getTextWidth(gfx, truncDesc, getFont(2)) + 
              getTextWidth(gfx, "...", getFont(2));
      if (descW <= tempSectionW - 10) {
        strcat(truncDesc, "...");
        break;
      }
    }
  }
  
  printText(gfx, tempX, contentY + 32, truncDesc, getFont(2), 
           alphaBlend(currentTheme->textSecondary, currentTheme->text, 180));
  
  // === WEATHER DETAILS GRID (with bounds checking) ===
  int16_t detailY = contentY + 55;
  int16_t detailsPerRow = 2;
  int16_t detailColW = (tempSectionW - 10) / detailsPerRow;
  int16_t detailX1 = tempX;
  int16_t detailX2 = tempX + detailColW;
  
  char str[32];
  
  // Row 1: Humidity and Pressure
  snprintf(str, sizeof(str), "💧 %d%%", globalWeather.humidity);
  char humid[24];
  strncpy(humid, str, sizeof(humid) - 1);
  humid[sizeof(humid) - 1] = '\0';
  
  snprintf(str, sizeof(str), "🌀 %dhPa", globalWeather.pressure);
  char press[24];
  strncpy(press, str, sizeof(press) - 1);
  press[sizeof(press) - 1] = '\0';
  
  printText(gfx, detailX1, detailY, humid, getFont(1), currentTheme->text);
  printText(gfx, detailX2, detailY, press, getFont(1), currentTheme->text);
  
  // Row 2: Wind and Energy/Solar
  detailY += 18;
  snprintf(str, sizeof(str), "💨 %.0fkm/h", globalWeather.windKph);
  char wind[24];
  strncpy(wind, str, sizeof(wind) - 1);
  wind[sizeof(wind) - 1] = '\0';
  
  if (globalWeather.dailyKWh > 0) {
    snprintf(str, sizeof(str), "⚡ %.1fkWh", globalWeather.dailyKWh);
  } else {
    snprintf(str, sizeof(str), "☀ %dW/m²", globalWeather.solar);
  }
  char solar[24];
  strncpy(solar, str, sizeof(solar) - 1);
  solar[sizeof(solar) - 1] = '\0';
  
  printText(gfx, detailX1, detailY, wind, getFont(1), currentTheme->text);
  printText(gfx, detailX2, detailY, solar, getFont(1), currentTheme->text);
  
  // === BOILER SENSOR BOX (right side, proper spacing) ===
  if (showBoilerSensor) {
    int16_t boilerX = x + w - boilerSensorW - 10;
    int16_t boilerY = contentY;
    int16_t boilerH = h - 38;
    
    // Sensor box with border
    uint16_t sensorBg = alphaBlend(currentTheme->accent, panelBg, 50);
    gfx->fillRoundRect(boilerX, boilerY, boilerSensorW, boilerH, 8, sensorBg);
    gfx->drawRoundRect(boilerX, boilerY, boilerSensorW, boilerH, 8, 
                      alphaBlend(currentTheme->accent, currentTheme->text, 120));
    
    // "Boiler" label
    printText(gfx, boilerX + 8, boilerY + 8, "Boiler", getFont(1), currentTheme->text);
    
    // Current temperature - large, centered
    snprintf(str, sizeof(str), "%.1f°", globalWeather.currentTemp);
    int16_t centerX = boilerX + boilerSensorW / 2;
    int16_t textW = getTextWidth(gfx, str, getFont(4));
    printText(gfx, centerX - textW/2, boilerY + 28, str, getFont(4), currentTheme->warm);
    
    // Target temperature
    snprintf(str, sizeof(str), "→ %.0f°", globalWeather.targetTemp);
    textW = getTextWidth(gfx, str, getFont(1));
    printText(gfx, centerX - textW/2, boilerY + 56, str, getFont(1), currentTheme->cool);
  }
}
```

**Key Improvements:**
1. Pre-calculates available space before drawing
2. Truncates text with ellipsis if needed
3. Properly spaces boiler sensor to avoid overlap
4. Uses columns for detail grid with proper alignment
5. Centers text within calculated boundaries

---

## Fix #3: Status Bar Text Overflow 📊

### Problem
Status bar elements (timestamp, memory, sensor, WiFi, mode) can overflow screen width.

### Root Cause
- No width validation before positioning
- Multiple variable-length strings without bounds checking

### Solution

**File: `src/ui/page_home.cpp`**

Replace `drawStatusBar()` with overflow-safe version:

```cpp
void drawStatusBar(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Background
  uint16_t barBg = alphaBlend(currentTheme->header, currentTheme->card, 150);
  gfx->fillRoundRect(x, y, w, h, 8, barBg);
  gfx->drawRoundRect(x, y, w, h, 8, alphaBlend(barBg, currentTheme->text, 100));
  
  const GFXfont* font = getFont(1);
  int16_t textY = y + 6;
  int16_t currentX = x + 8;
  int16_t maxX = x + w - 68; // Reserve space for mode chip
  int16_t gap = 10;
  
  // Helper function to safely add text with overflow check
  auto addStatusElement = [&](const char* text, uint16_t color) -> bool {
    if (!text || currentX >= maxX) return false;
    
    int16_t textW = getTextWidth(gfx, text, font);
    if (currentX + textW + gap >= maxX) {
      // Not enough space, truncate or skip
      return false;
    }
    
    printText(gfx, currentX, textY, text, font, color);
    currentX += textW + gap;
    return true;
  };
  
  // === TIMESTAMP (left side) ===
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timestamp[32];
  if (TimeManager::timeIsSet()) {
    snprintf(timestamp, sizeof(timestamp), "🕐 %02d:%02d", 
            timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    strncpy(timestamp, "🕐 --:--", sizeof(timestamp));
  }
  addStatusElement(timestamp, currentTheme->text);
  
  // === MEMORY ===
  char memStr[16];
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap > 1024 * 1024) {
    snprintf(memStr, sizeof(memStr), "RAM:%.1fM", freeHeap / (1024.0 * 1024.0));
  } else {
    snprintf(memStr, sizeof(memStr), "RAM:%dk", freeHeap / 1024);
  }
  
  uint16_t memColor = (freeHeap > 100000) ? rgb565(0, 255, 0) : 
                     (freeHeap > 50000) ? rgb565(255, 255, 0) : rgb565(255, 0, 0);
  memColor = alphaBlend(memColor, currentTheme->text, 180);
  
  addStatusElement(memStr, memColor);
  
  // === TEMPERATURE SENSOR (show value, not just checkmark) ===
  char sensorStr[16];
  if (!isnan(globalWeather.currentTemp)) {
    snprintf(sensorStr, sizeof(sensorStr), "🌡 %.0f°", globalWeather.currentTemp);
    addStatusElement(sensorStr, alphaBlend(rgb565(0, 200, 0), currentTheme->text, 180));
  } else {
    snprintf(sensorStr, sizeof(sensorStr), "🌡 --°");
    addStatusElement(sensorStr, alphaBlend(rgb565(200, 0, 0), currentTheme->text, 180));
  }
  
  // === WiFi RSSI (with percentage indicator) ===
  char rssiStr[16];
  int rssi = NetworkManager::getWiFiRSSI();
  int rssiPercent = (rssi + 120) * 100 / 70; // Convert dBm to percentage
  rssiPercent = max(0, min(100, rssiPercent)); // Clamp to 0-100
  
  snprintf(rssiStr, sizeof(rssiStr), "📡 %d%%", rssiPercent);
  
  uint16_t rssiColor = (rssiPercent > 70) ? alphaBlend(rgb565(0, 255, 0), currentTheme->text, 180) :
                      (rssiPercent > 40) ? alphaBlend(rgb565(255, 255, 0), currentTheme->text, 180) :
                      alphaBlend(rgb565(255, 0, 0), currentTheme->text, 180);
  
  addStatusElement(rssiStr, rssiColor);
  
  // === HEATER MODE CHIP (right side) ===
  const char* mode = "";
  const char* modeIcon = "";
  switch (globalWeather.heaterControlMode) {
    case 0: 
      mode = globalWeather.heaterOn ? "ON" : "OFF"; 
      modeIcon = globalWeather.heaterOn ? "🔥" : "❌";
      break;
    case 1: 
      mode = "MAN"; 
      modeIcon = "🎛️";
      break;
    case 2: 
      mode = "AUTO"; 
      modeIcon = "⚙️";
      break;
    case 3: 
      mode = "AI"; 
      modeIcon = "🤖";
      break;
    default: 
      mode = "---"; 
      modeIcon = "?";
      break;
  }
  
  char modeDisplay[16];
  snprintf(modeDisplay, sizeof(modeDisplay), "%s %s", modeIcon, mode);
  
  int16_t chipW = 58;
  int16_t chipH = h - 8;
  int16_t chipX = x + w - chipW - 8;
  int16_t chipY = y + 4;
  
  uint16_t chipBg = alphaBlend(currentTheme->accent, barBg, 200);
  gfx->fillRoundRect(chipX, chipY, chipW, chipH, 6, chipBg);
  gfx->drawRoundRect(chipX, chipY, chipW, chipH, 6, currentTheme->accent);
  
  int16_t modeW = getTextWidth(gfx, modeDisplay, getFont(1));
  printText(gfx, chipX + (chipW - modeW)/2, chipY + 2, modeDisplay, getFont(1), currentTheme->text);
}
```

**Key Improvements:**
1. Checks available space before adding each element
2. Skips elements if not enough space (graceful degradation)
3. Converts memory to MB if large
4. Shows temperature value instead of just checkmark
5. Converts WiFi RSSI to percentage
6. Shows heater on/off state with icon
7. All elements have proper gap calculations

---

## Fix #4: No Continuous Sensor Updates 📊

### Problem
When boiler or weather data is updated, the home page doesn't automatically refresh.

### Root Cause
- `uiRequestRedraw()` calls `navigateToPage()` which respects navigation lock
- Navigation lock blocks updates for 500ms after navigation
- Users must navigate away and back to see updated data

### Solution

**File: `src/ui/ui_framework.h`**

Add a new function declaration:

```cpp
// Request update without blocking on navigation lock
void uiRequestHomeUpdate();
```

**File: `src/ui/ui_framework.cpp`**

Add this function before `uiLoop()`:

```cpp
void uiRequestHomeUpdate() {
  if (!display) return;
  
  // Force home page redraw even if navigation is locked
  if (currentPageId == PAGE_HOME) {
    display->fillScreen(currentTheme->background);
    drawPageHome();
    drawBuiltinOverlay(static_cast<uint8_t>(PAGE_HOME));
  }
}
```

**File: `src/ui/page_home.h`**

Update the external function declarations:

```cpp
extern void updateWeatherOnly(const WeatherData& data);
extern void updateBoilerData(const WeatherData& data);
```

**File: `src/ui/page_home.cpp`**

Replace update functions:

```cpp
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
  
  // Use new direct update function instead of navigation
  uiRequestHomeUpdate();
}

void updateBoilerData(const WeatherData& data) {
  initWeatherData();
  globalWeather.currentTemp = data.currentTemp;
  globalWeather.targetTemp = data.targetTemp;
  globalWeather.heaterOn = data.heaterOn;
  globalWeather.heaterControlMode = data.heaterControlMode;
  globalWeather.efficiency = data.efficiency;
  globalWeather.kwhTotal = data.kwhTotal;
  globalWeather.dailyKWh = data.dailyKWh;
  globalWeather.costTotal = data.costTotal;
  globalWeather.costToday = data.costToday;
  
  // Use new direct update function instead of navigation
  uiRequestHomeUpdate();
}
```

**Benefits:**
- Real-time updates to home page
- Bypasses navigation lock
- Data feels responsive
- No need for user to manually navigate

---

## Fix #5: WiFi Strength Indicator Calculation ⚡

### Problem
WiFi RSSI displayed as raw dBm without context or percentage.

### Solution

**File: `src/ui/page_home.cpp`**

Add helper function before `drawPageHome()`:

```cpp
// Convert WiFi RSSI (dBm) to signal strength percentage
static int getRssiPercent(int rssi) {
  // RSSI typical range: -30 dBm (excellent) to -120 dBm (poor)
  // Clamp and convert to percentage: 0-100
  if (rssi >= -30) return 100;
  if (rssi <= -120) return 0;
  
  // Linear interpolation: -30 to -120 dBm maps to 100 to 0%
  return (rssi + 120) * 100 / 90;
}

// Get WiFi signal strength indicator bar
static const char* getWifiStrengthBar(int rssi) {
  int percent = getRssiPercent(rssi);
  if (percent >= 80) return "▓▓▓▓ Excellent";
  if (percent >= 60) return "▓▓▓░ Good";
  if (percent >= 40) return "▓▓░░ Fair";
  if (percent >= 20) return "▓░░░ Weak";
  return "░░░░ Poor";
}
```

Then use in `drawStatusBar()`:

```cpp
// === WiFi RSSI (with visual indicator) ===
char rssiStr[20];
int rssi = NetworkManager::getWiFiRSSI();
int rssiPercent = getRssiPercent(rssi);

snprintf(rssiStr, sizeof(rssiStr), "📡 %d%%", rssiPercent);

uint16_t rssiColor = (rssiPercent >= 80) ? rgb565(0, 255, 0) :
                    (rssiPercent >= 60) ? rgb565(100, 255, 0) :
                    (rssiPercent >= 40) ? rgb565(255, 255, 0) :
                    (rssiPercent >= 20) ? rgb565(255, 150, 0) :
                    rgb565(255, 0, 0);
rssiColor = alphaBlend(rssiColor, currentTheme->text, 180);

addStatusElement(rssiStr, rssiColor);
```

---

## Fix #6: Heater Mode Display with State 🔥

### Problem
Heater mode display doesn't show whether heater is actually on/off.

### Solution

Already implemented in Fix #3 above. The key improvement:

```cpp
switch (globalWeather.heaterControlMode) {
  case 0: 
    mode = globalWeather.heaterOn ? "ON" : "OFF";  // Show state for OFF mode
    modeIcon = globalWeather.heaterOn ? "🔥" : "❌";
    break;
  case 1: 
    mode = "MAN"; 
    modeIcon = "🎛️";
    break;
  case 2: 
    mode = "AUTO"; 
    modeIcon = "⚙️";
    break;
  // ... etc
}
```

---

## Fix #7: Partial Screen Update System 📐

### Problem
Every data change redraws entire screen, causing flicker.

### Solution - Implement Dirty Rect System

**File: `src/ui/page_home.h`**

Add panel redraw function declarations:

```cpp
// Partial redraw functions (avoid full screen redraw)
void redrawClockPanel(Arduino_GFX* gfx, AppConfig* config);
void redrawWeatherPanel(Arduino_GFX* gfx, AppConfig* config);
void redrawStatusBar(Arduino_GFX* gfx, AppConfig* config);
```

**File: `src/ui/page_home.cpp`**

Extract individual panel drawing into separate functions and track changes:

```cpp
// Track last drawn state
static struct {
  int lastSecond;
  float lastExtTemp;
  int lastHumidity;
  float lastCurrentTemp;
  int lastHeaterMode;
  int lastWifiRssi;
} lastDrawnState = {-1, -999, -1, -999, -1, -200};

// Partial redraw: Only update changed elements
static void redrawClockPanelIfChanged(Arduino_GFX* gfx, AppConfig* config) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  if (timeinfo.tm_sec == lastDrawnState.lastSecond) {
    return; // No change
  }
  
  lastDrawnState.lastSecond = timeinfo.tm_sec;
  
  // Only redraw clock panel, not entire screen
  HomeLayoutGeometry layout = getHomeLayoutGeometry();
  int16_t width = 480 - (layout.margin * 2);
  
  // Clear just the clock area
  gfx->fillRoundRect(layout.margin, layout.clockY, width, layout.clockHeight, 12, 
                     alphaBlend(currentTheme->card, currentTheme->background, 220));
  
  // Redraw clock panel
  drawClockPanel(gfx, layout.margin, layout.clockY, width, layout.clockHeight);
}

static void redrawWeatherPanelIfChanged(Arduino_GFX* gfx, AppConfig* config) {
  if (globalWeather.extTemp == lastDrawnState.lastExtTemp &&
      globalWeather.humidity == lastDrawnState.lastHumidity) {
    return;
  }
  
  lastDrawnState.lastExtTemp = globalWeather.extTemp;
  lastDrawnState.lastHumidity = globalWeather.humidity;
  
  HomeLayoutGeometry layout = getHomeLayoutGeometry();
  int16_t width = 480 - (layout.margin * 2);
  
  // Clear and redraw weather panel
  gfx->fillRoundRect(layout.margin, layout.weatherY, width, layout.weatherHeight, 12,
                     alphaBlend(currentTheme->card, currentTheme->background, 220));
  
  drawWeatherPanel(gfx, layout.margin, layout.weatherY, width, layout.weatherHeight);
}

static void redrawStatusBarIfChanged(Arduino_GFX* gfx, AppConfig* config) {
  int rssi = NetworkManager::getWiFiRSSI();
  if (globalWeather.currentTemp == lastDrawnState.lastCurrentTemp &&
      globalWeather.heaterControlMode == lastDrawnState.lastHeaterMode &&
      rssi == lastDrawnState.lastWifiRssi) {
    return;
  }
  
  lastDrawnState.lastCurrentTemp = globalWeather.currentTemp;
  lastDrawnState.lastHeaterMode = globalWeather.heaterControlMode;
  lastDrawnState.lastWifiRssi = rssi;
  
  HomeLayoutGeometry layout = getHomeLayoutGeometry();
  int16_t width = 480 - (layout.margin * 2);
  
  // Clear and redraw status bar
  gfx->fillRoundRect(layout.margin, layout.statusY, width, layout.statusHeight, 8,
                     alphaBlend(currentTheme->header, currentTheme->card, 150));
  
  drawStatusBar(gfx, layout.margin, layout.statusY, width, layout.statusHeight);
}
```

**File: `src/ui/ui_framework.cpp`**

Update `uiRequestHomeUpdate()` to use partial redraws:

```cpp
void uiRequestHomeUpdate() {
  if (!display) return;
  
  if (currentPageId == PAGE_HOME) {
    AppConfig* config = ConfigStore::getConfig();
    redrawClockPanelIfChanged(display, config);
    redrawWeatherPanelIfChanged(display, config);
    redrawStatusBarIfChanged(display, config);
  }
}
```

**Benefits:**
- No more full-screen flicker
- 10-100x faster updates (only changed regions)
- Smoother, more responsive UI
- Reduced display load

---

## Summary of Quick Fixes

| Fix | Time | Complexity | Impact |
|-----|------|-----------|--------|
| #1: Clock update | 1 hour | Low | High |
| #2: Text overlap | 2 hours | Medium | High |
| #3: Status bar overflow | 1 hour | Medium | High |
| #4: Sensor updates | 1.5 hours | Medium | High |
| #5: WiFi strength | 30 min | Low | Medium |
| #6: Heater mode | 30 min | Low | Medium |
| #7: Partial updates | 2-3 hours | High | Very High |

**Recommendation:** Implement fixes in this order for maximum impact:
1. Start with Fix #1 (clock) - quick win
2. Then Fix #2 (text overlap) - visible improvement
3. Then Fix #4 (sensor updates) - real-time feel
4. Then Fix #3 (status bar) - polish
5. Then Fix #7 (partial updates) - performance optimization

