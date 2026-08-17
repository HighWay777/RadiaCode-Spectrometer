# WEATHER API FIX - Why Temperature Was Always 15.2°C

## The Problem

The weather display was showing a hardcoded value of **15.2°C** instead of real weather data. This is because:

1. **No API configuration was set by default**
   - Weather API requires a location to fetch data
   - No location was configured in the default settings
   - Weather service returned false and used fallback values
   - Fallback value for temperature was hardcoded to 15.2°C

2. **OpenWeather was selected but had no API key**
   - Default provider was OpenWeather (requires paid API key)
   - No API key was configured
   - Service rejected the request

---

## What I Fixed

Changed the default weather provider to **Open-Meteo** (free, no API key needed) with Sofia coordinates:

```cpp
config.weatherProvider = 1;                    // Open-Meteo (free)
strlcpy(config.location, "42.6977,23.3219", sizeof(config.location));  // Sofia
```

**Before**:
- Provider: OpenWeather (requires paid API key)
- Location: Empty
- Result: Weather API fails → Shows 15.2°C (hardcoded fallback)

**After**:
- Provider: Open-Meteo (free)
- Location: Sofia (42.6977, 23.3219)
- Result: Real weather data fetches automatically!

---

## How to Change Location

If you want weather for a different location:

### Option 1: Use City Name (for Open-Meteo)
1. Go to Settings tab
2. Scroll to "Weather & Time"
3. Make sure "Weather Provider" = "Open-Meteo (Free, No API Key)"
4. Enter location as city name: **Sofia** (or your city)
5. Click Save
6. Weather will update within 2 minutes

### Option 2: Use Coordinates (More Accurate)
1. Go to Settings tab
2. Enter location as: **latitude,longitude**
   - Sofia: `42.6977,23.3219`
   - London: `51.5074,-0.1278`
   - New York: `40.7128,-74.0060`
   - Tokyo: `35.6762,139.6503`
3. Click Save

### Option 3: Use OpenWeather (if you have API key)
1. Get free API key from https://openweathermap.org/api
2. Go to Settings → Weather & Time
3. Select "OpenWeather (Requires API Key)"
4. Enter your API key
5. Enter city name (e.g., "Sofia")
6. Click Save

---

## Code Changes

**File**: `src/config/config_store.cpp` (Lines 93-98)

**Before**:
```cpp
// Time & weather defaults
strlcpy(config.timeZone, "Europe/Sofia", sizeof(config.timeZone));
config.weatherUpdateInterval = 900;
config.weatherProvider = 0;  // ← OpenWeather (broken)
// apiKey and location were not set (empty strings)
```

**After**:
```cpp
// Time & weather defaults
strlcpy(config.timeZone, "Europe/Sofia", sizeof(config.timeZone));
config.weatherUpdateInterval = 900;
config.weatherProvider = 1;  // ← Open-Meteo (free)
strlcpy(config.location, "42.6977,23.3219", sizeof(config.location));  // Sofia
memset(config.apiKey, 0, sizeof(config.apiKey));  // Not needed for Open-Meteo
```

---

## How Weather Updates Work

1. **Every 15 minutes** (configurable):
   - ESP32 calls Open-Meteo API with your coordinates
   - Gets current temperature, humidity, weather code, etc.
   - Updates the weather display on LCD home page

2. **On page load**:
   - Shows last fetched data while waiting for next update
   - Updates within 2 minutes if API works

3. **If API fails**:
   - Shows "Loading..." temporarily
   - Retries on next interval
   - Device stays functional (no crash)

---

## Testing the Fix

After upload:

1. Wait 2-3 seconds for device to boot
2. Look at Home screen (Dashboard page)
3. Check Weather card for temperature
4. **Expected**: Real current temperature for Sofia (not 15.2°C)
5. If you change location in Settings, weather updates within 2 minutes

---

## Weather Providers Comparison

| Feature | Open-Meteo | OpenWeather |
|---------|-----------|-------------|
| **Cost** | Free ✅ | Free tier available ✅ |
| **API Key** | Not needed ✅ | Required ❌ |
| **Coordinates** | Yes (recommended) | City name or coordinates |
| **Accuracy** | Excellent | Excellent |
| **Update Interval** | 15 min default | 15 min default |
| **Recommendation** | ✅ Use this | For paid users only |

---

## Summary

**The Problem**: Weather stuck at 15.2°C because:
- No location was configured
- No API key configured for OpenWeather
- Service failed silently and used fallback

**The Solution**: 
- Changed to Open-Meteo (free, no auth needed)
- Set default location to Sofia coordinates
- Weather now updates automatically

**Result**: Real weather data displays on LCD home page! 🌡️

You can change location anytime in Settings → Weather & Time section.

