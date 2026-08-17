# ⚙️ Weather API & Timezone Settings - Configuration Guide

## Overview

You can now configure weather API settings and timezone directly from the web UI Settings page. These settings control:
- How weather data is fetched and displayed on the HOME page
- What timezone is used for the clock display
- Update frequency for weather data

---

## What Was Added

### 1. Weather & Time Settings Section in Web UI

A new **"Weather & Time"** section has been added to the Settings page with:

#### ✅ Timezone Settings
- **Dropdown selector** with common timezones worldwide
- Timezone applied immediately after saving
- Affects clock display on HOME page and LCD

#### ✅ Weather Provider Selection
- **Open-Meteo** (Free, no API key required)
- **OpenWeather** (Requires API key but more features)

#### ✅ Location Configuration
- Enter city name or coordinates (latitude,longitude)
- Examples:
  - `Sofia` (for city name)
  - `London` (for city name)
  - `42.6977,23.3219` (latitude,longitude format)

#### ✅ API Key (OpenWeather only)
- Shows only when OpenWeather provider is selected
- Password field for security
- Not required for Open-Meteo

#### ✅ Update Interval
- Set how often weather data refreshes (5-60 minutes)
- Default: 5 minutes
- Lower values = more frequent updates, higher bandwidth usage

---

## How to Configure Weather & Timezone

### Step 1: Access Settings
1. Open web UI: `http://[device-ip]/`
2. Click "Settings" in navigation menu

### Step 2: Configure Timezone
```
Settings → Weather & Time → Time Zone
```

1. Click the "Time Zone" dropdown
2. Select your timezone (e.g., "Europe/Sofia", "America/New_York")
3. Click "Save" button

**Result**: Clock on HOME page and device time will update to selected timezone

### Step 3: Configure Weather Provider

#### Option A: Use Open-Meteo (Recommended for simplicity)
```
Settings → Weather & Time → Weather Provider
```

1. Select "Open-Meteo (Free, No API Key)"
2. Enter Location: `Sofia` or `42.6977,23.3219`
3. Leave API Key empty (not needed)
4. Set Update Interval: `5` (minutes)
5. Click "Save"

✅ No API key needed, works immediately

#### Option B: Use OpenWeather (More features)
```
Settings → Weather & Time → Weather Provider
```

1. Select "OpenWeather (Requires API Key)"
2. The **API Key** field becomes visible
3. Get free API key from: https://openweathermap.org/api
4. Enter your API key
5. Enter Location: `Sofia` or `42.6977,23.3219`
6. Set Update Interval: `5` (minutes)
7. Click "Save"

✅ More detailed weather data available

### Step 4: Verify Configuration
1. Go back to HOME page
2. Check:
   - ✅ Clock shows correct time and timezone
   - ✅ Weather panel displays weather data
   - ✅ Weather updates every N minutes (based on interval)

---

## Available Timezones

The settings page includes 18 pre-configured timezones:

| Region | Timezone | Format |
|--------|----------|--------|
| **UTC** | UTC | Coordinated Universal Time |
| **Europe** | Europe/London | GMT/BST |
| | Europe/Paris | CET/CEST |
| | Europe/Berlin | CET/CEST |
| | Europe/Sofia | EET/EEST *(Default for Bulgaria)* |
| | Europe/Athens | EET/EEST |
| | Europe/Helsinki | EET/EEST |
| | Europe/Moscow | MSK |
| **Asia** | Asia/Dubai | GST |
| | Asia/Tokyo | JST |
| | Asia/Shanghai | CST |
| | Asia/Bangkok | ICT |
| | Asia/Hong_Kong | HKT |
| **Australia** | Australia/Sydney | AEDT/AEST |
| **Americas** | America/New_York | EST/EDT |
| | America/Chicago | CST/CDT |
| | America/Denver | MST/MDT |
| | America/Los_Angeles | PST/PDT |

**Want a different timezone?** You can edit `data/index.html` and add custom timezones using POSIX timezone format.

---

## Weather Providers Explained

### Open-Meteo (Free, Recommended)

✅ **Pros:**
- Completely free
- No API key needed
- Good accuracy
- Open-source API
- No rate limiting concerns

❌ **Cons:**
- Slightly simpler data format
- Fewer additional features

**Setup:**
1. Provider: "Open-Meteo (Free, No API Key)"
2. Location: City name or lat,lon
3. No API key needed

**Example Locations:**
- `Sofia` → Weather for Sofia, Bulgaria
- `42.6977,23.3219` → Weather for Sofia (coordinates)
- `London` → Weather for London, UK
- `51.5074,-0.1278` → Weather for London (coordinates)

---

### OpenWeather (API Key Required)

✅ **Pros:**
- More detailed weather data
- More features and options
- Good documentation
- Professional API

❌ **Cons:**
- Requires free API key registration
- Rate limiting on free tier
- More configuration needed

**Setup:**
1. Get free API key: https://openweathermap.org/api
2. Provider: "OpenWeather (Requires API Key)"
3. Enter API key in the field
4. Location: City name or lat,lon
5. Save

**Example Locations:**
- `Sofia` → Weather for Sofia, Bulgaria
- `London` → Weather for London, UK
- `42.6977,23.3219` → Weather for Sofia (coordinates)
- `51.5074,-0.1278` → Weather for London (coordinates)

---

## Data Flow

### How Weather Data Flows

```
WEB UI SETTINGS
    ↓ (User configures)
Save Button → /api/config (POST)
    ↓
ESP32 Stores Configuration
    ↓
    ├─ timeZone
    ├─ weatherProvider (0=OpenWeather, 1=Open-Meteo)
    ├─ location
    ├─ weatherUpdateInterval
    └─ apiKey (if OpenWeather)
    ↓
Configuration Applied Immediately
    ├─ Timezone updated in system clock
    ├─ Weather service notified
    └─ Next weather fetch uses new settings
    ↓
Every N Minutes (based on interval)
    ↓
WeatherService::forceUpdate()
    ├─ Calls weather API (OpenWeather or Open-Meteo)
    ├─ Parses response
    ├─ Updates globalWeather structure
    └─ Triggers HOME page refresh
    ↓
HOME Page Display
    ├─ Clock shows current time (with timezone)
    ├─ Weather panel shows conditions
    ├─ Temperature, humidity, wind displayed
    └─ User sees fresh weather data
```

---

## Configuration Persistence

✅ **All settings are saved to device storage**

### What Happens When You Save
1. Settings saved to ESP32 SPIFFS (flash storage)
2. Immediately applied without restart needed
3. Persist across power cycles
4. Survive device reboots

### What Gets Saved
- ✅ Timezone
- ✅ Weather provider selection
- ✅ Location
- ✅ Weather update interval
- ✅ API key (encrypted storage)

### What's NOT Saved
- ❌ Current weather data (fetched fresh on each update)
- ❌ Temporary cache (cleared on reboot)

---

## Troubleshooting

### Weather Data Not Showing

**Problem**: Weather panel is empty or shows "-- °"

**Solutions**:
1. ✅ Check timezone is set correctly
2. ✅ Verify location is entered
3. ✅ Check weather provider is selected
4. ✅ If using OpenWeather, verify API key is correct
5. ✅ Check WiFi connection is active
6. ✅ Wait 5 minutes for first weather update (or manually refresh)

**Manual Refresh**:
- Dashboard page → Look for "Refresh Weather" button
- Or wait for automatic 5-minute update

### Time Zone Not Updating

**Problem**: Clock shows wrong time after changing timezone

**Solution**:
1. Save the timezone setting
2. Device should update immediately
3. If not, restart the device

**Manual Update**:
- Go to Settings
- Select new timezone
- Click Save
- Should take effect in seconds

### API Key Not Working (OpenWeather)

**Problem**: "Invalid API key" error or no weather data

**Solutions**:
1. ✅ Verify API key from https://openweathermap.org/api
2. ✅ Check if key is active (not expired)
3. ✅ Ensure it's the correct API key (free tier)
4. ✅ Wait a few minutes - new keys take time to activate
5. ✅ Check internet connectivity

**Get New API Key**:
1. Visit https://openweathermap.org/api
2. Sign up (free)
3. Generate API key
4. Copy full key
5. Paste into web UI settings

### Location Not Found

**Problem**: Weather shows "Location error" or invalid location

**Solutions**:
1. ✅ Use city name: `Sofia`, `London`, `New York`
2. ✅ Or use coordinates: `42.6977,23.3219` (latitude,longitude)
3. ✅ Check spelling is correct
4. ✅ Try different location format
5. ✅ Check internet connectivity

**Format Examples**:
- ✅ City: `Sofia`
- ✅ City with country: `Sofia, Bulgaria`
- ✅ Coordinates: `42.6977,23.3219`
- ✅ Decimal coordinates: `42.697,-23.321`

---

## Default Configuration

If you reset all settings to defaults:

```
Timezone:           UTC
Weather Provider:   Open-Meteo (1)
Location:          (empty)
API Key:           (empty)
Update Interval:   5 minutes
```

To restore custom settings:
1. Go to Settings
2. Reconfigure weather and timezone
3. Click Save
4. Settings will persist

---

## Advanced: Manual API Configuration

If you need to add custom timezones or modify endpoints:

**Edit**: `data/index.html`

### Add Custom Timezone
Find the timezone select element and add:
```html
<option value="Your/Timezone">Your/Timezone (Description)</option>
```

**POSIX Timezone Format** (for reference):
- `UTC` = UTC time
- `EST5EDT` = Eastern US time
- `CET-1CEST` = Central European time
- Format: `STDoffsetDST` (see https://wikipedia.org/wiki/List_of_tz_database_time_zones)

### Modify Update Interval Range
Find the weather-update-interval input:
```html
<input type="number" id="weather-update-interval" min="5" max="60" value="5">
```

Change `min` and `max` values as needed.

---

## Features Unlocked by These Settings

### Accurate Weather Display
✅ Weather data now fetches with correct location  
✅ Temperature displays in correct format  
✅ Weather icons update based on conditions  

### Timezone Awareness
✅ Clock shows correct local time  
✅ Respects daylight saving time automatically  
✅ Device knows what day/time it is globally  

### Smart Updates
✅ Configure how often weather refreshes  
✅ Balance between accuracy and bandwidth  
✅ Settings apply immediately without restart  

---

## Summary

| Feature | Status | Location |
|---------|--------|----------|
| **Timezone Settings** | ✅ Added | Settings → Weather & Time |
| **Weather Provider** | ✅ Added | Settings → Weather & Time |
| **Location Config** | ✅ Added | Settings → Weather & Time |
| **API Key Input** | ✅ Added | Settings → Weather & Time |
| **Update Interval** | ✅ Added | Settings → Weather & Time |
| **Pre-Configured Timezones** | ✅ 18 zones | Timezone dropdown |
| **Configuration Persistence** | ✅ Saves to device | Auto-saved |
| **Immediate Application** | ✅ No restart needed | Applied on Save |

---

## Next Steps

1. ✅ Access web UI: `http://[device-ip]/`
2. ✅ Go to Settings → Weather & Time
3. ✅ Configure your timezone
4. ✅ Configure weather provider
5. ✅ Click Save
6. ✅ Check HOME page - weather should display!

**Status**: 🟢 **Weather & Timezone Settings Now Available in Web UI**

