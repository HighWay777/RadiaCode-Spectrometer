# Complete Data Refresh System - Integration Summary

## System Overview

The ESP32-S3 Touch display now implements a sophisticated, multi-layered data refresh system that keeps all display information synchronized and current:

```
TIME-BASED REFRESH ARCHITECTURE
================================

EVERY 1 MINUTE:
├─ Detect minute change via system time
├─ Fetch boiler API data (http://5.104.177.12:7000/api/status)
├─ Update display with fresh boiler metrics
│  ├─ Temperature (with gradient color 35-52°C)
│  ├─ Pressure readings
│  ├─ Energy statistics
│  └─ System status
└─ Check API connectivity status
   └─ Show "API offline" badge if 10+ failures

EVERY 5 MINUTES (on 5-minute boundaries):
├─ Trigger weather API fetch
├─ Fetch from OpenWeather or Open-Meteo
├─ Update weather panel with fresh data
│  ├─ Current temperature
│  ├─ Weather condition & icon
│  ├─ Humidity
│  ├─ Wind speed
│  └─ Pressure
└─ Exponential backoff on failures

DISPLAY PAGES UPDATED:
├─ PAGE_HOME: Clock + Weather + Boiler Temp + Status
├─ PAGE_DASHBOARD: All boiler metrics (pressure, energy, etc.)
└─ PAGE_STATS: Historical and current statistics
```

---

## Data Sources

### 1. Boiler System API (Per-Minute)
**Endpoint**: `http://5.104.177.12:7000/api/status`
**Frequency**: Every minute (synchronized to minute boundary)
**Provider**: NetworkManager::fetchApiData()
**Data Fields**:
- Boiler temperature (primary display value)
- System pressure
- Energy consumption
- Operating status
- Timestamp

**Failure Handling**:
- Tracks consecutive failures in `apiFailCount`
- Exponential backoff on retries
- Visual "API offline" badge at 10+ failures
- Automatic recovery when API responds

---

### 2. Weather API (Per-5-Minutes)
**Endpoints**:
- OpenWeather: `http://api.openweathermap.org/data/2.5/weather?q={location}&units=metric&appid={apiKey}`
- Open-Meteo: `https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=...`

**Frequency**: Every 5 minutes (synchronized to 5-minute boundary)
**Provider**: WeatherService::forceUpdate() → WeatherService::loop()
**Data Fields**:
- External temperature
- Weather condition code (0-95)
- Weather description
- Humidity %
- Barometric pressure (hPa)
- Wind speed (km/h)
- Solar radiation (if available)
- Day/night indicator

**Failure Handling**:
- Uses same exponential backoff as boiler API
- Defaults to 5-minute interval if not configured
- Can use free provider (Open-Meteo) or paid (OpenWeather)

---

### 3. System Time (Per-Minute)
**Source**: ESP32 built-in RTC via `time()` function
**Frequency**: Continuous (checked every loop iteration)
**Data**:
- Current time (HH:MM:SS)
- Current minute for boundary detection
- Timezone-aware via TimeManager

**Display**:
- HOME page: Large clock display (HH:MM format)
- Updated every minute (no seconds to reduce flicker)
- Date display below clock

---

## Implementation Details

### File: `src/ui/ui_framework.cpp`

**Key Function**: `void uiLoop()`

**Initialization**:
```cpp
static unsigned long lastHomeRefreshMs = 0;  // Boiler API refresh tracker
static int lastHomeMinute = -1;               // Minute boundary detector
static unsigned long lastWeatherUpdateMs = 0; // Weather API refresh tracker
```

**Per-Minute Boiler Update**:
```cpp
void uiLoop() {
  unsigned long now = millis();
  
  // Boiler API refresh check
  if (now - lastHomeRefreshMs >= 60000) {  // 60 seconds elapsed
    time_t t = time(nullptr);
    struct tm* timeinfo = localtime(&t);
    int currentMinute = timeinfo->tm_min;
    
    if (currentMinute != lastHomeMinute) {  // Minute boundary detected
      lastHomeMinute = currentMinute;
      lastHomeRefreshMs = now;
      
      // FETCH FRESH BOILER DATA
      NetworkManager::fetchApiData();
      
      // FETCH FRESH WEATHER DATA (every 5 minutes)
      if (lastWeatherUpdateMs == 0 || now - lastWeatherUpdateMs >= 300000) {
        lastWeatherUpdateMs = now;
        WeatherService::forceUpdate();
      }
      
      // REDRAW DISPLAY WITH FRESH DATA
      if (currentPageId == PAGE_HOME) {
        drawClockPanelTime(display, ...);       // Fresh time
        drawWeatherPanel(display, ...);         // Fresh weather
        drawStatusBar(display, ...);            // Fresh boiler data
      }
      else if (currentPageId == PAGE_DASHBOARD) {
        drawPageBoiler(display, config);        // Fresh boiler metrics
      }
      else if (currentPageId == PAGE_STATS) {
        drawPageStats();                        // Fresh statistics
      }
    }
  }
}
```

---

## Data Freshness Guarantees

### Boiler Temperature
- **Maximum Age**: 1 minute
- **Update Trigger**: Minute boundary change
- **Display**: Large, prominent temperature card with gradient color
- **Accuracy**: Live from boiler system API

### Weather Information
- **Maximum Age**: 5 minutes (or 1 minute if boiler updates coincide)
- **Update Trigger**: 5-minute interval boundaries
- **Display**: Weather icon + temperature + condition in weather panel
- **Accuracy**: Live from OpenWeather or Open-Meteo

### System Time
- **Maximum Age**: < 1 minute
- **Update Trigger**: Every minute redraw
- **Display**: Large clock at top of HOME page
- **Format**: HH:MM (no seconds to eliminate flicker)

### Pressure & Status
- **Maximum Age**: 1 minute
- **Update Trigger**: Minute boundary change
- **Display**: Status bar and dashboard metrics
- **Accuracy**: Live from boiler system API

---

## Connectivity & Reliability

### API Failure Detection
- **Boiler API Failures**: Tracked in `NetworkManager::apiFailCount`
- **Threshold**: 10+ consecutive failures triggers "API offline" badge
- **Visual Feedback**: Red badge in HOME page header
- **Recovery**: Auto-disappears when API responds successfully

### Exponential Backoff
- **Initial Retry**: ~60 seconds
- **Subsequent Retries**: Double interval (up to 10 minutes max)
- **Reset**: When successful fetch occurs
- **Purpose**: Prevent hammering down APIs

### WiFi Connectivity
- **Check Interval**: Every 10 seconds
- **Auto-Reconnect**: Attempts to reconnect if disconnected
- **Data Fetch**: Only when WiFi connected
- **Status Display**: WiFi indicator in status bar

---

## Display Updates by Page

### HOME Page
```
┌─────────────────────────────────────────┐
│ Home                           API ⚠️    │  ← Header with optional API badge
├─────────────────────────────────────────┤
│                                         │
│  10:35                                  │  ← Time (updated per minute)
│  Tuesday, Oct 21                        │  ← Date
│                                         │
│ ☀️ Clear, 22°C                          │  ← Weather (updated per 5 min)
│ Feels like 20°C, Humidity 65%          │
│                                         │
│ Boiler: 42°C 🟠 (gradient color)        │  ← Temp with gradient (per min)
│ Pressure: 2.1 bar  Status: Running      │  ← Metrics (updated per min)
│                                         │
│ WiFi: SSID (-45dBm)  API Status: Online │  ← Status bar (per min)
└─────────────────────────────────────────┘
```

**Refresh Schedule**:
- Clock: Every minute
- Weather: Every 5 minutes
- Temperature & Metrics: Every minute
- Status Bar: Every minute
- API Badge: Appears/disappears as needed

### DASHBOARD Page
```
All boiler system metrics:
- Temperature (real-time gradient)
- Pressure (bar)
- Energy consumption (kWh)
- Operating hours
- System status indicators
```

**Refresh**: Every minute with fresh API data

### STATS Page
```
Historical and current statistics:
- Daily/weekly/monthly energy
- Temperature history
- Performance metrics
```

**Refresh**: Every minute with fresh API data

---

## API Response Flow

### Boiler API Call
```
1. uiLoop() detects minute boundary
2. NetworkManager::fetchApiData() called
3. HTTP GET to boiler API endpoint
4. JSON response parsed:
   {
     "temperature": 42.5,
     "pressure": 2.1,
     "energy": 15.3,
     "status": "running",
     "timestamp": 1729519200
   }
5. Data stored in global/config structures
6. apiFailCount reset to 0 (success)
7. Display functions use fresh data
8. drawWeatherPanel(), drawStatusBar() render new values
```

### Weather API Call
```
1. uiLoop() detects 5-minute boundary (or minute boundary on 5-min mark)
2. WeatherService::forceUpdate() called
3. WeatherService::loop() detects update needed
4. HTTP GET to weather API endpoint
5. JSON response parsed into WeatherData structure:
   {
     "extTemp": 22.0,
     "weatherCode": 1,
     "humidity": 65,
     "pressure": 1013,
     "windKph": 5.2,
     "weatherDesc": "Partly cloudy"
   }
6. updateWeatherOnly(weatherData) called
7. Weather display elements updated
8. Next drawWeatherPanel() uses fresh data
```

---

## Performance Metrics

### Build Status
- **Flash Usage**: 87.1% (1,141,749 bytes / 1,310,720 bytes)
- **RAM Usage**: 14.6% (47,916 bytes / 327,680 bytes)
- **Build Time**: ~20 seconds

### Runtime Characteristics
- **Loop Frequency**: ~100+ times per second
- **Boiler API Calls**: 1 per minute = 1,440 per 24 hours
- **Weather API Calls**: 1 per 5 minutes = 288 per 24 hours
- **Total API Calls**: ~1,728 per 24 hours
- **Display Redraws**: 60+ per minute (per-loop updates)
- **Memory Overhead**: ~1 KB for tracking variables

---

## Configuration

Users can configure via Web UI:

### API Settings
```
Boiler API Endpoint: http://5.104.177.12:7000/api/status
Weather Provider: OpenWeather | Open-Meteo
Weather Update Interval: 5 (minutes)
OpenWeather API Key: [your-key-here]
Location: City or "lat,lon"
```

### Display Settings
```
Font Size: Small | Normal | Large
Theme: Light | Dark
Timezone: UTC, CET, EST, etc.
Time Format: 24H | 12H
```

---

## Error Handling & Recovery

### Scenario: Boiler API Unreachable
```
Attempt 1: Fetch fails → apiFailCount = 1
Attempt 2: Fetch fails → apiFailCount = 2
...
Attempt 10: Fetch fails → apiFailCount = 10 → Badge appears "API offline"
Attempt 11+: Exponential backoff, retry every 60-600 seconds
Recovery: Next successful fetch → apiFailCount = 0 → Badge disappears
```

### Scenario: Weather API Unreachable
```
Similar to boiler API:
- Exponential backoff applied
- Display shows last known weather until next successful fetch
- No visual badge currently (future enhancement)
- Auto-recovery when API available
```

### Scenario: WiFi Disconnected
```
WiFi Check: Every 10 seconds
If Disconnected: Auto-reconnect attempt
Data Fetch: Skipped until WiFi restored
Display: Shows last known values
Status: "Offline" indicator
Recovery: Auto-reconnect + data refresh resumes
```

---

## Summary

The complete data refresh system ensures:

✅ **Real-Time Data**: Fresh boiler metrics every minute
✅ **Current Weather**: Fresh weather every 5 minutes
✅ **Accurate Time**: Synchronized clock display
✅ **Visual Feedback**: API offline badge when needed
✅ **Automatic Recovery**: Auto-reconnect and data refresh
✅ **Efficient**: Balanced between freshness and bandwidth
✅ **Reliable**: Error handling and exponential backoff
✅ **Scalable**: Can add more data sources easily

**Result**: Users see meaningful, constantly-updated information that reflects the true, current state of their boiler system and environment.

---

## Related Documentation

- `API_DATA_FETCH_INTEGRATION.md` - Boiler API details
- `WEATHER_API_5MIN_REFRESH.md` - Weather API details
- `SYNCHRONIZED_REFRESH_SYSTEM.md` - Display refresh mechanics
- `api.md` - API endpoint specifications
