# Visual System Diagram - Data Refresh Architecture

## System Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   ESP32-S3 TOUCH DISPLAY SYSTEM                            │
│                   Complete Data Refresh Architecture                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ EXTERNAL DATA SOURCES                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ☁️  BOILER API                    ☁️  WEATHER API                         │
│  http://5.104.177.12:7000         OpenWeather or Open-Meteo               │
│  /api/status                                                               │
│                                                                             │
│  Data: Temperature, Pressure      Data: Temp, Conditions, Humidity        │
│        Status, Energy              Wind, Pressure                         │
│                                                                             │
│  Refresh: Every 1 minute          Refresh: Every 5 minutes               │
└─────────────────────────────────────────────────────────────────────────────┘
              │                                    │
              │                                    │
              ↓                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│ ESP32-S3 MICROCONTROLLER                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────┐           │
│  │ MAIN LOOP: uiLoop()                                         │           │
│  ├─────────────────────────────────────────────────────────────┤           │
│  │                                                             │           │
│  │  Every 60 seconds:                                          │           │
│  │  ┌──────────────────────────────────────────────────────┐  │           │
│  │  │ 1. Check if minute changed (via time())             │  │           │
│  │  │    if (currentMinute != lastHomeMinute) {            │  │           │
│  │  │                                                      │  │           │
│  │  │    2. FETCH BOILER API DATA                          │  │           │
│  │  │       ├─ HTTP GET to boiler endpoint                │  │           │
│  │  │       ├─ Parse JSON: temp, pressure, status         │  │           │
│  │  │       ├─ Track failures (apiFailCount++)            │  │           │
│  │  │       └─ Reset to 0 if success                      │  │           │
│  │  │                                                      │  │           │
│  │  │    3. CHECK 5-MINUTE BOUNDARY                        │  │           │
│  │  │       if (now - lastWeatherUpdateMs >= 300000) {     │  │           │
│  │  │          4. FETCH WEATHER API DATA                   │  │           │
│  │  │             ├─ Call WeatherService::forceUpdate()    │  │           │
│  │  │             ├─ OpenWeather or Open-Meteo fetch       │  │           │
│  │  │             ├─ Parse: temp, icon, conditions         │  │           │
│  │  │             └─ updateWeatherOnly(weatherData)        │  │           │
│  │  │       }                                              │  │           │
│  │  │                                                      │  │           │
│  │  │    5. REDRAW DISPLAY WITH FRESH DATA                 │  │           │
│  │  │       if (currentPageId == PAGE_HOME) {              │  │           │
│  │  │          ├─ drawClockPanelTime(fresh time)           │  │           │
│  │  │          ├─ drawWeatherPanel(fresh weather)          │  │           │
│  │  │          └─ drawStatusBar(fresh metrics)             │  │           │
│  │  │       }                                              │  │           │
│  │  │                                                      │  │           │
│  │  │    6. CHECK API STATUS                               │  │           │
│  │  │       if (apiFailCount >= 10) {                      │  │           │
│  │  │          ├─ Show "API offline" red badge             │  │           │
│  │  │       } else {                                       │  │           │
│  │  │          └─ Hide badge                              │  │           │
│  │  │       }                                              │  │           │
│  │  │    }                                                 │  │           │
│  │  └──────────────────────────────────────────────────────┘  │           │
│  │                                                             │           │
│  └─────────────────────────────────────────────────────────────┘           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
              │
              ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│ DISPLAY OUTPUT (QSPI 480x272)                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────┐       │
│  │ HOME PAGE (Updated Per-Minute + Per-5-Minute Weather)           │       │
│  ├─────────────────────────────────────────────────────────────────┤       │
│  │                                                                 │       │
│  │ Home                                           [API offline] ◀─┼─ Badge │
│  │                                                                 │       │
│  │ 10:35                                                           │       │
│  │ Tuesday, October 21                                             │       │
│  │                                                                 │       │
│  │ ☀️  Clear, 22°C                                                │       │
│  │ Humidity: 65% | Wind: 5 km/h                                   │       │
│  │                                                                 │       │
│  │ Boiler: 42°C 🟠 (Gradient Color)                               │       │
│  │ Pressure: 2.1 bar | Status: Running                            │       │
│  │                                                                 │       │
│  │ WiFi: Home (-45dBm) | API: Online                              │       │
│  │                                                                 │       │
│  └─────────────────────────────────────────────────────────────────┘       │
│                                                                             │
│  Fresh Data Timeline:                                                       │
│  ├─ Temperature: 42°C  (1 min old) ← Boiler API                           │
│  ├─ Weather: Clear 22°C (5 min old) ← Weather API                         │
│  ├─ Time: 10:35 (< 1 min old) ← System Time                               │
│  ├─ Pressure: 2.1 bar (1 min old) ← Boiler API                            │
│  └─ API Status: Live (Real-time) ← apiFailCount                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Timing Diagram

```
TIME PROGRESSION - 12 MINUTES

Time    │ 0:00  │ 1:00  │ 2:00  │ 3:00  │ 4:00  │ 5:00  │ ...  │ 10:00
────────┼───────┼───────┼───────┼───────┼───────┼───────┼──────┼───────
Minute  │   0   │   1   │   2   │   3   │   4   │   5   │  ...  │  10
        │       │       │       │       │       │       │       │
Boiler  │  ✓✓  │  ✓✓  │  ✓✓  │  ✓✓  │  ✓✓  │  ✓✓  │  ...  │  ✓✓
API     │ FETCH │ FETCH │ FETCH │ FETCH │ FETCH │ FETCH │  ...  │ FETCH
        │       │       │       │       │       │       │       │
Weather │  ✓✓  │       │       │       │       │  ✓✓  │  ...  │  ✓✓
API     │ FETCH │   -   │   -   │   -   │   -   │ FETCH │  ...  │ FETCH
        │       │       │       │       │       │       │       │
Display │ ✨✨  │ ✨✨  │ ✨✨  │ ✨✨  │ ✨✨  │ ✨✨  │ ✨✨  │ ✨✨
Update  │ FULL  │ FULL  │ FULL  │ FULL  │ FULL  │ FULL  │ FULL  │ FULL
        │       │       │       │       │       │       │       │

Legend:
✓✓ FETCH = Fresh API call made
-     = No API call
✨✨ FULL = Full page redraw with fresh data
```

---

## Data Flow Cascade

```
┌───────────────────────────────────────────────────────────────────┐
│ MINUTE BOUNDARY DETECTED                                          │
│ (e.g., transition from 10:34 to 10:35)                           │
└───────────────────┬───────────────────────────────────────────────┘
                    │ T=0ms
                    ↓
        ┌───────────────────────┐
        │ lastHomeMinute != cm?  │
        │ currentMinute = 35     │
        └────────┬──────────────┘
                 │ YES
                 ↓ T=1ms
    ┌────────────────────────────┐
    │ NetworkManager::            │
    │ fetchApiData()              │
    │                             │
    │ HTTP GET boiler API         │
    │ ├─ /api/status              │
    │ ├─ Parse JSON               │
    │ │  {temp:42.5,             │
    │ │   pressure:2.1,          │
    │ │   status:"running"}       │
    │ ├─ Update data structures   │
    │ └─ Reset apiFailCount=0     │
    └────────┬───────────────────┘
             │ T=150ms
             ↓
    ┌────────────────────────────┐
    │ Check 5-minute boundary?   │
    │ now - lastWeatherMs        │
    │ >= 300000?                 │
    └────────┬────────┬──────────┘
             │ NO     │ YES
             │        ↓ T=151ms
             │    ┌────────────────┐
             │    │ WeatherService::│
             │    │ forceUpdate()   │
             │    │                │
             │    │ HTTP GET       │
             │    │ weather API    │
             │    │ ├─ Parse JSON  │
             │    │ ├─ extTemp:22  │
             │    │ ├─ icon:"sun"  │
             │    │ └─ conditions  │
             │    └────────┬───────┘
             │             │ T=300ms
             │             ↓
             └─────────┬────┘
                       │
                       ↓ T=301ms
        ┌──────────────────────────────────┐
        │ Page-specific redraw             │
        │ if (PAGE_HOME) {                 │
        │   drawClockPanelTime()           │
        │   drawWeatherPanel()             │
        │   drawStatusBar()                │
        │   if (apiFailCount >= 10)        │
        │     draw API offline badge       │
        │ }                                │
        └──────────┬───────────────────────┘
                   │ T=450ms
                   ↓
        ┌──────────────────────────┐
        │ DISPLAY UPDATED          │
        │ WITH ALL FRESH DATA      │
        │ ✨✨✨                     │
        │ All values current:      │
        │ ├─ Time: 10:35          │
        │ ├─ Temp: 42.5°C         │
        │ ├─ Weather: Clear 22°C   │
        │ ├─ Pressure: 2.1 bar     │
        │ └─ Status: Running       │
        └──────────────────────────┘
```

---

## API Failure State Machine

```
                         ┌─────────────────────────┐
                         │                         │
                         │  SUCCESS FLOW           │
                         ↓                         │
            ┌────────────────────────┐             │
            │ API ONLINE             │             │
            │ apiFailCount = 0       │◄────────────┘
            │ Badge: HIDDEN          │
            │ Status: Normal fetch   │
            │ Interval: 1 minute     │
            └────────┬───────────────┘
                     │
              ┌──────│ Fetch fails
              │      ↓
              │  ┌──────────────────┐
              │  │ RETRY BACKOFF    │
              │  │ apiFailCount++   │
              │  │ (1 to 9 retries) │
              │  │ Badge: HIDDEN    │
              │  │ Backoff: 60-300s │
              │  └────┬────────────┬┘
              │       │            │
              │       │ (10+ times)│
              │       ↓            │
              │  ┌──────────────────────┐
              │  │ API OFFLINE          │
              │  │ apiFailCount >= 10   │
              │  │ Badge: "API offline" │
              │  │ Status: Exponential  │
              │  │ Interval: 60-600s    │
              │  │ Last known values    │
              │  │ displayed            │
              │  └────┬─────────────────┘
              │       │
              │       │ SUCCESS
              │       ↓
              └──►┌────────────────────────┐
                 │ RECOVERY               │
                 │ apiFailCount = 0       │
                 │ Badge: DISAPPEARS      │
                 │ Status: Back to normal │
                 │ Interval: 1 minute     │
                 └────────────────────────┘
```

---

## Memory Layout

```
ESP32-S3 Memory Usage
════════════════════════════════════════════════════════════════

FLASH MEMORY (8 MB Total)
┌────────────────────────────────────────────────────────────────┐
│ Used: 1,141,749 bytes (87.1%)                                 │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│ ┌─ Bootloader + OTA ───────────────────────────┐              │
│ │ Arduino Framework & Libraries                │              │
│ │ WiFi, HTTP, JSON parsing                     │              │
│ │ Display Graphics & Fonts                     │              │
│ │ UI Framework & Pages                         │  87.1% USED  │
│ │ ├─ page_home.cpp (with API badge)            │              │
│ │ ├─ page_boiler.cpp                           │              │
│ │ └─ page_stats.cpp                            │              │
│ │ Network Managers                             │              │
│ │ ├─ NetworkManager (boiler + badge)           │              │
│ │ └─ WeatherService (5-min fetch)              │              │
│ └─ User Configuration Storage ───────────────────┘              │
│                                                                │
│ Available: 169,019 bytes (12.9%) ◄─ Room for future features │
│                                                                │
└────────────────────────────────────────────────────────────────┘

RAM (320 KB Total)
┌────────────────────────────────────────────────────────────────┐
│ Used: 47,916 bytes (14.6%)                                    │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│ Stack: ~10 KB                                                 │
│ Heap: ~37 KB                                                  │
│ ├─ WiFi buffers                                              │
│ ├─ HTTP response buffer (~2 KB boiler + ~1 KB weather)       │
│ ├─ JSON parse buffer                                         │
│ ├─ Display buffer segments                                   │
│ └─ Data structures (configs, weather, API state)             │
│                                                                │
│ Available: 279,884 bytes (85.4%) ◄─ Excellent headroom      │
│                                                                │
└────────────────────────────────────────────────────────────────┘

Runtime Allocation:
- Network buffers: ~3 KB
- Weather data structure: ~512 bytes
- API tracking variables: ~200 bytes (lastWeatherUpdateMs, apiFailCount)
- Display buffers: ~20 KB
```

---

## Page Update Schedule

```
HOME PAGE UPDATE SCHEDULE (480x272 QSPI Display)
════════════════════════════════════════════════════════════════

Per-Minute Full Update:
┌─────────────────────────────────────────────────────┐
│ Header (70 px)      → drawHomeHeader()              │
│ ├─ "Home" title                                     │
│ ├─ Decorative dots                                 │
│ └─ "API offline" badge (if apiFailCount >= 10)    │
│                                                     │
│ Clock Panel (80 px) → drawClockPanelTime()         │
│ ├─ Large time display (HH:MM)                       │
│ ├─ Date display (Day, Month, Date)                 │
│ └─ Updated every minute                            │
│                                                     │
│ Weather Panel (80 px) → drawWeatherPanel()         │
│ ├─ Weather icon (refreshed per-5-min)              │
│ ├─ Temperature & conditions                        │
│ ├─ Humidity & wind speed                           │
│ └─ Barometric pressure                             │
│                                                     │
│ Boiler Card (50 px)                                │
│ ├─ Temperature with gradient (per-1-min)           │
│ ├─ Pressure reading (per-1-min)                    │
│ └─ System status (per-1-min)                       │
│                                                     │
│ Status Bar (12 px) → drawStatusBar()               │
│ ├─ WiFi indicator (per-1-min)                      │
│ ├─ API status (per-1-min)                          │
│ └─ Current time (HH:MM:SS)                         │
│                                                     │
└─────────────────────────────────────────────────────┘

Per-5-Minute Updates:
- Weather data refresh (icon, temp, conditions, humidity, wind)
- Weather panel redraw only
- Other elements retain previous draw
```

---

## Network Traffic Pattern

```
NETWORK ACTIVITY PATTERN (24-Hour Period)
════════════════════════════════════════════════════════════════

Boiler API Calls: 1 per minute = 1,440 per 24h
┌─ Every minute on the minute boundary
├─ Request: ~200 bytes
├─ Response: ~500 bytes
└─ Total: 100 KB per 24h

Weather API Calls: 1 per 5 minutes = 288 per 24h
┌─ Every 5 minutes (on multiples)
├─ Request: ~300 bytes
├─ Response: ~1,000 bytes
└─ Total: 370 KB per 24h

WiFi Check: Every 10 seconds (background ping)
├─ Status only, no data transfer
└─ ~8.6 K checks per 24h

Total Network Data:
├─ Boiler API: 100 KB
├─ Weather API: 370 KB
├─ HTTP Headers: 200 KB
└─ Total: ~670 KB per 24h (very minimal)

Bandwidth Impact:
├─ 670 KB / 24 hours = 0.28 kB/s average
├─ Peak usage: During API calls (< 1 sec)
└─ Impact: Negligible (typical WiFi is 1-5 Mbps)
```

---

This visual architecture shows how your ESP32-S3 system elegantly manages:
✅ Multiple data sources
✅ Synchronized refresh cycles
✅ Efficient memory usage
✅ Minimal network traffic
✅ Professional display updates
