# Comprehensive Code Audit - ESP32-S3 Touch Project

## Executive Summary

This project is an ESP32-S3 touchscreen controller that:
1. Fetches data from a remote boiler/heating API at `http://5.104.177.12:7000/api/status`
2. Fetches weather data from OpenWeather or OpenMeteo
3. Displays information on a 480x272 LCD screen with touch input
4. Provides a Web UI for configuration and a "page builder" to customize LCD layouts

After thorough analysis, I've identified and fixed several issues.

---

## ✅ FIXES APPLIED

### 1. Added Missing Control API Endpoints with Boiler Forwarding
**File: [src/main.cpp](src/main.cpp)**

Added the following endpoints that were being called by the Web UI but didn't exist:
- `/api/control/mode` - For changing heating mode (Off/Manual/Auto/AI) - **Now forwards to remote boiler**
- `/api/control/temp` - For setting target temperature - **Now forwards to remote boiler**

The endpoints now properly forward commands to the remote boiler API when configured:
- Mode changes are sent as form data to `/api/mode`
- Temperature changes are sent as JSON to `/api/temperature`

### 2. Fixed Dashboard Field Name Mismatches
**File: [data/js/dashboard.js](data/js/dashboard.js)**

Fixed field names to match the actual API response:
- `wifiRssi` → `wifiRSSI` (case sensitivity)
- `energyToday` → `dailyKWh`
- `energyTotal` → `kwhTotal`
- `heatLoss` → `heatLossRate`

Added proper handling for:
- Memory usage calculation from `freeHeap` (ESP32 has ~320KB total heap)
- Uptime formatting from seconds to "Xd Yh Zm" format
- Sensor status derived from `apiStatus` field
- Proper placeholders for unavailable fields (CPU temp, sunrise/sunset times)

### 3. Fixed Builder Bug - Missing Function
**File: [data/js/builder.js](data/js/builder.js)**

Fixed reference to undefined function `onPageSelect` - now correctly calls `loadSelectedPage`.
Also fixed the element ID from `page-select` to `current-page-selector`.

---

## 🟡 REMAINING ISSUES (Lower Priority)

### 1. Duplicate/Dead Code: app.js

**File: [data/js/app.js](data/js/app.js)**

This file contains a complete **standalone builder implementation** that:
- Is NOT imported by main.js (the active entry point)
- Conflicts with the modular builder.js implementation
- Takes up ~27KB of SPIFFS space unnecessarily

**Recommendation:** Delete `app.js` (keep backup if needed) or repurpose it.

---

## 🟡 MEDIUM ISSUES

### 4. C++ API Status Missing Fields from Remote API

**File: [src/main.cpp](src/main.cpp) - `statusJson()` function**

The local `/api/status` endpoint doesn't include several useful fields from the remote boiler API:

Missing fields that could be useful:
- `firmwareVersion` - remote boiler firmware version
- `uptime` - formatted uptime string
- `serverTime` - server's local time
- `mode` - current operation mode string

### 5. Memory/Health Fields Not Available

The Dashboard UI shows memory, CPU temperature, and sensor status fields that are never populated by the API. Either:
- Add these fields to the statusJson() response, OR
- Remove them from the Dashboard UI

### 6. Weather Data Not Included in Status API

Weather data (sunrise, sunset, next update time) is fetched separately but not exposed in the `/api/status` response for the Web UI.

---

## 🟢 LOW PRIORITY ISSUES

### 7. Duplicate Helper Functions

Several functions are duplicated across files:
- `getModeText()` - in app.js, dashboard.js, control.js
- `getWeatherDescription()` - in app.js, dashboard.js
- Toast functions

### 8. Unused Files That Can Be Removed

- `data/js/app.js.SAFE_BACKUP` - Old backup
- `data/js/app_new_complete.js` - Unused alternative

### 9. Missing Uptime Formatting in statusJson

The statusJson() returns `uptime` as raw seconds, but the dashboard expects a formatted string.

---

## API Field Mapping Reference

### Remote Boiler API Returns (Confirmed Working):
```json
{
  "firmwareVersion": "2026-01-03-V45-SAFE-THRESHOLDS",
  "currentTemp": 50.4,
  "targetTemp": 50.0,
  "target": 50.0,
  "aiTargetTemp": 50.0,
  "targetSource": "ai",
  "isNightTime": false,
  "isBoostTime": false,
  "dayTemp": 50.0,
  "nightTemp": 45.0,
  "boostTemp": 52.0,
  "heater": "off",
  "heaterOn": false,
  "override": false,
  "heaterControlMode": 3,
  "manualOverride": false,
  "aiEnabled": true,
  "efficiency": 9.4,
  "heatLossRate": 0.00,
  "kwhTotal": 149.755,
  "costTotal": 16.84,
  "dailyKWh": 2.941,
  "costToday": 0.29,
  "dailyCost": 0.29,
  "wifiRSSI": -60,
  "uptime": "0d 1h 29m",
  "serverTime": "16:37:28",
  "timezone": "Europe/Sofia"
}
```

### Local ESP32 Status Returns:
```json
{
  "wifiConnected": true,
  "wifiRSSI": -60,
  "wifiSsid": "...",
  "ipAddress": "...",
  "uptime": 12345,  // Raw seconds
  "freeHeap": 80000,
  "apiStatus": 1,
  "apiFailCount": 0,
  "timeZone": "Europe/Sofia",
  "epoch": 1234567890,
  "localTime": "2026-01-06T17:00:00",
  // Plus all boiler data fields...
}
```

---

## Recommended Fix Order

1. **Add missing `/api/control/mode` and `/api/control/temp` endpoints** - Control panel broken
2. **Fix dashboard field names** - Dashboard shows wrong data
3. **Add memory percentage calculation** to statusJson
4. **Format uptime properly** in statusJson
5. **Clean up dead code** (app.js, backup files)

---

## Architecture Notes

The Web UI uses a **modular JavaScript architecture**:
- `main.js` - Entry point, initializes all modules
- `dashboard.js` - Dashboard view updates
- `control.js` - Temperature and mode control
- `settings.js` - Settings forms
- `builder.js` - LCD page builder (2302 lines)
- `api.js` - API communication
- `state.js` - Global state management
- `toast.js` - Toast notifications
- `format.js` - Value formatting
- `navigation.js` - Tab navigation

The **app.js** file is a legacy standalone implementation that should be removed.
