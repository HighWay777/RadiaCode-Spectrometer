# Comprehensive Weather Icons Bug Report & Code Investigation
**Date**: May 8, 2026  
**Project**: ESP32-S3 Touch UI with Weather Display  
**Investigation Status**: COMPLETE (No code modifications applied)  
**Report Type**: Deep code analysis and potential issue identification

---

## Executive Summary

Investigation of the weather icons system reveals **the code is functional but contains several quality and potential runtime issues**. While the system appears to compile and execute (weather icons display), there are **structural problems** that could cause:

1. **Quality Issues**: Poor coding practices that violate best practices
2. **Potential Runtime Issues**: Type casting problems that could cause rendering artifacts
3. **Edge Case Handling**: Some weather conditions may render differently than expected
4. **Maintainability Issues**: Missing explicit includes and unclear intent

**Current Status**: Weather icons ARE working but may exhibit occasional rendering issues depending on:
- Specific weather codes returned by API
- Floating-point precision in trigonometric calculations
- Size/coordinate edge cases
- Transitive include dependencies (Arduino.h → cmath)

---

## System Architecture Overview

### Weather Data Flow
```
Weather API (OpenWeatherMap or Open-Meteo)
    ↓
WeatherService::loop() / fetchOpenWeather/Meteo()
    ↓
updateWeatherOnly() → globalWeather.weatherCode
    ↓
drawPageHome() → drawWeatherPanel()
    ↓
drawWeatherIcon(weatherCode) → specific icon drawing function
    ↓
drawLine() / fillCircle() / fillTriangle() primitives
    ↓
Display updated
```

### Configuration
- **Default showWeatherIcons**: `true` (enabled)
- **Icon Size**: 50 pixels
- **Weather Provider**: Open-Meteo (weatherProvider = 1)
- **Update Interval**: 900 seconds (15 minutes)
- **Location**: Sofia (42.6977, 23.3219)

### Weather Code Mapping
- **Clear**: 0
- **Mainly clear / Partly cloudy**: 1, 2  
- **Overcast**: 3
- **Fog / Mist**: 45, 48
- **Drizzle**: 51, 53, 55, 56, 57
- **Rain**: 61, 63, 65, 66, 67, 80-82
- **Snow**: 71, 73, 75, 77, 85, 86
- **Thunderstorm**: 95, 96, 99
- **Unknown**: Falls back to cloud icon (size * 2/3)

---

## ISSUE #1: Missing Explicit Math Includes (Quality Issue)

### Severity: ⚠️ MEDIUM (Code Quality)
### File: `src/ui/weather_icons.cpp`
### Line: 111-115

### Problem
```cpp
float angle = i * PI / 4;              // Line 111 - PI used
int x1 = x + cos(angle) * size * 2/3;  // cos() function
int y1 = y + sin(angle) * size * 2/3;  // sin() function
```

**Issue**: 
- `PI` constant is **not explicitly declared** in weather_icons.cpp
- `#include <cmath>` is **missing**
- `cos()` and `sin()` functions are **not explicitly included**

### Why It Works
- Arduino_GFX_Library.h **transitively includes `<cmath>`** through Arduino framework
- Compiles successfully due to transitive includes
- However, **not best practice** and creates dependency on implicit behavior

### Why It's a Problem
1. **Code clarity**: Unclear where PI and trig functions come from
2. **Portability**: May not work if libraries change or code is ported
3. **Maintenance**: Future developers may not understand the dependency chain
4. **Compiler warnings**: Some strict build settings might flag this

### Recommended Fix
Add explicit includes at the top of weather_icons.cpp:
```cpp
#include <cmath>  // For cos(), sin(), and M_PI
#define PI M_PI   // Use standard M_PI constant
```

Or safer approach:
```cpp
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI
```

---

## ISSUE #2: Floating-Point to Integer Type Casting (Precision Loss)

### Severity: ⚠️ MEDIUM (Potential Rendering Issues)
### File: `src/ui/weather_icons.cpp`
### Multiple Locations

### Problem 2A: drawSun() - Trigonometric Calculations
```cpp
// Line 111-115
int x1 = x + cos(angle) * size * 2/3;  // IMPLICIT CAST: double → int16_t
int y1 = y + sin(angle) * size * 2/3;  // IMPLICIT CAST: double → int16_t
int x2 = x + cos(angle) * rayLength;   // IMPLICIT CAST: double → int16_t
int y2 = y + sin(angle) * rayLength;   // IMPLICIT CAST: double → int16_t
```

**Issue**: 
- `cos(angle)` returns `double` (e.g., 0.7071 for 45°)
- `rayLength` is `int` (50 pixels)
- `cos(angle) * rayLength` → `double` (e.g., 35.355)
- Implicitly cast to `int16_t` → **truncates to 35** (loss of 0.355)
- Result: Slight misalignment in ray positioning

**Impact on Sun Icon**:
- 8 rays drawn at angles 0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°
- Each ray position loses fractional pixel information
- Sun rays may appear **slightly asymmetrical or misaligned**
- Especially noticeable at certain zoom levels or on high-DPI displays

### Problem 2B: drawSnow() - Snowflake Scaling
```cpp
// Line 167-171
gfx->drawLine(flakeX - flakeSize * 0.7, flakeY - flakeSize * 0.7,
             flakeX + flakeSize * 0.7, flakeY + flakeSize * 0.7, color);
```

**Issue**:
- `flakeSize` is `int` (e.g., 6)
- `flakeSize * 0.7` → `double` (4.2)
- Implicitly cast to `int16_t` → **truncates to 4** (loss of 0.2)
- All four diagonal lines use this truncated value

**Impact**:
- Snowflake appearance **inconsistent** depending on flakeSize
- When flakeSize=6: all 4 diagonals position at 4 pixels offset
- When flakeSize=7: diagonals position at 4 pixels (5*0.7=3.5→3)
- **Inconsistent visual appearance** for snowflake icons

### Problem 2C: drawFog() - Line Width Calculation
```cpp
// Line 206
int lineWidth = size * 1.5;  // IMPLICIT CAST: double → int16_t
```

**Issue**:
- `size` is `int` (50)
- `size * 1.5` → `double` (75.0)
- Implicitly cast to `int16_t` → **75** (happens to work here)
- But for odd sizes, this causes rounding: size=51 → 76.5 → 76

**Impact**:
- Minor inconsistency in fog line widths
- Not visible to user but poor code practice

### Problem 2D: drawMist() - Similar Issue
```cpp
// Line 214
int lineMaxWidth = size * 1.2;  // IMPLICIT CAST: double → int16_t
```

### Recommended Fix
Use explicit casting with rounding:
```cpp
int x1 = x + (int16_t)(cos(angle) * size * 2.0 / 3.0 + 0.5);  // +0.5 for proper rounding
int y1 = y + (int16_t)(sin(angle) * size * 2.0 / 3.0 + 0.5);
```

Or separate calculation:
```cpp
float radius = size * 2.0 / 3.0;
int x1 = x + (int16_t)(cos(angle) * radius);
int y1 = y + (int16_t)(sin(angle) * radius);
```

---

## ISSUE #3: Non-Deterministic Rendering in drawMist()

### Severity: 🔴 HIGH (Functional Issue)
### File: `src/ui/weather_icons.cpp`
### Lines: 216-219

### Problem
```cpp
void drawMist(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  for (int i = 0; i < lineCount; i++) {
    int lineY = y - size/2 + i * lineSpacing;
    int lineWidth = lineMaxWidth - random(size/4);    // ← RANDOM!
    int offset = random(size/3);                       // ← RANDOM!
    gfx->fillRoundRect(x - lineWidth/2 + offset, lineY, lineWidth - offset, lineHeight, lineHeight/2, color);
  }
}
```

### Issue
- **`random()` function is called** every time drawMist() is called
- **Each redraw produces different output** (different random fog pattern)
- Violates principle of **deterministic rendering**

### Impact
1. **Visual Flickering**: If home screen redraws frequently, fog icon changes appearance
2. **Difficult to Debug**: Can't reproduce exact rendering
3. **Performance**: Unnecessary randomness each frame
4. **Battery Drain**: More computation for non-essential randomness

### Why This Happens
- Fog is meant to look "misty" - hence randomness
- But this is called during regular screen updates
- Should use **static seed or timestamp** if randomness is needed

### Note
- `drawFog()` does **NOT** use random (deterministic) - only `drawMist()` does
- `drawMist()` is **not currently called** in drawWeatherIcon() mapping
- **No weather codes map to drawMist()** in current implementation
- This is **latent code** that would cause issues if mist weather code is added

---

## ISSUE #4: Inconsistent Icon Fallback Size

### Severity: ⚠️ LOW (Visual Inconsistency)
### File: `src/ui/weather_icons.cpp`
### Lines: 101-102

### Problem
```cpp
else {
  // Unknown weather code: draw a neutral cloudy icon instead of a warning
  drawCloud(gfx, x, y, size * 2/3, cloudColor);  // ← Uses 2/3 of size!
}
```

### Issue
- All other icons use **full `size` parameter**
- Fallback cloud uses **`size * 2/3`** (smaller)
- Inconsistent with other icon sizes

### Impact
1. **Unknown weather codes render smaller cloud**
2. If API returns invalid code, cloud appears smaller
3. Visual inconsistency in icon display

### Current Implementation Note
- Fallback happens if weatherCode doesn't match any known condition
- Both API providers should return valid codes (0-99 for Open-Meteo, mapped for OpenWeather)
- Fallback is rarely triggered but inconsistent when it is

### Recommended Fix
```cpp
else {
  // Unknown weather code: draw a neutral cloudy icon
  drawCloud(gfx, x, y, size, cloudColor);  // Use full size for consistency
}
```

---

## ISSUE #5: Potential API Integration Issue

### Severity: ⚠️ MEDIUM (Conditional)
### File: `src/network/weather_service.cpp`
### Lines: 46-93

### Problem
```cpp
// OpenWeather mapping function
auto mapOWCode = [](int ow) -> int {
  if (ow == 800) return 0;  // clear
  if (ow == 801) return 1;  // few clouds
  // ... more mappings ...
  return 0;  // default to clear instead of warning
};
```

### Issue
- If an **unmapped OpenWeather code is received**, it defaults to **0 (clear sky)**
- This could mask API errors or unusual weather conditions
- User sees false "clear sky" when code is unknown

### Current Coverage
- Most OpenWeather codes are mapped
- But some edge cases might return unmapped codes
- Results in incorrect icon (clear sky when actually different)

### Recommended Improvement
```cpp
auto mapOWCode = [](int ow) -> int {
  // ... existing mappings ...
  // Default to 3 (overcast) instead of 0 (clear)
  // Or return -1 for "unknown" and handle in drawWeatherIcon()
  return 3;  // neutral choice rather than false clear
};
```

---

## ISSUE #6: No Explicit Validation of weatherCode Range

### Severity: ⚠️ LOW (Edge Case)
### File: `src/ui/page_home.cpp`
### Line: 491

### Problem
```cpp
if (config->showWeatherIcons && globalWeather.weatherCode >= 0) {
  drawWeatherIcon(gfx, iconX + iconSize/2, contentY + iconSize/2, iconSize,
                 globalWeather.weatherCode, globalWeather.isNightTime);
}
```

### Issue
- Check ensures weatherCode >= 0
- But no upper bound check (e.g., weatherCode < 100)
- If API returns invalid code (e.g., 999, 32767), it passes to drawWeatherIcon()
- Results in else clause (fallback cloud)

### Recommended Check
```cpp
if (config->showWeatherIcons && globalWeather.weatherCode >= 0 && globalWeather.weatherCode < 100) {
  // Safe range for Open-Meteo codes
}
```

---

## ISSUE #7: Weather Icons Function Not Declared if showWeatherIcons is False

### Severity: ✅ LOW (Not an issue)
### File: `src/ui/page_home.cpp`
### Lines: 491-496

### Detail
The code has a guard:
```cpp
if (!StyleOverrides::isHidden("home.weather.icon")) {
  if (config->showWeatherIcons && globalWeather.weatherCode >= 0) {
    drawWeatherIcon(...);
  }
}
```

This means:
- If `showWeatherIcons = false` in config, weather icon is NOT drawn
- If style override hides it, weather icon is NOT drawn
- Only fallback sun is drawn from another code path

**Status**: ✅ This is intentional and correct behavior

---

## ISSUE #8: Missing Documentation for Weather Code Mapping

### Severity: ℹ️ LOW (Documentation)
### Files: `weather_icons.cpp`, `weather_service.cpp`

### Detail
- Code references Open-Meteo API documentation URL
- But doesn't document the **actual codes used**
- Makes it hard to understand what each code represents

### Example Missing Documentation
```cpp
// Weather code mapping for Open-Meteo API
// Code 0:   Clear sky (no precipitation, no clouds)
// Code 1-2: Partly cloudy (clear or partly cloudy)
// Code 3:   Overcast (cloudiness > 75%)
// ... etc
```

---

## ISSUE #9: No Error Logging for Failed Weather Updates

### Severity: ⚠️ LOW (Debugging)
### File: `src/ui/page_home.cpp`

### Detail
- If weather API fails, `globalWeather` retains old values
- No visible indication that data is stale
- Users can't tell if weather is current or outdated

**Current Implementation**:
- Default weatherCode is 1 (Mainly clear)
- If API fails, screen continues showing last successful data
- Or shows defaults (15.2°C, 65% humidity) indefinitely

---

## INVESTIGATION RESULTS: Testing Recommendations

### What to Check on Device
1. **Verify sun icon renders** - check if rays are visible and symmetrical
2. **Test all weather conditions** - manually verify different weather codes render
3. **Check for flickering** - monitor home screen during weather updates
4. **Test with invalid weather codes** - if API returns 999 or negative value
5. **Monitor performance** - check if frequent redraws cause lag
6. **Test night mode** - verify moon color appears different from sun

### Recommended Testing Scenarios
| Weather Code | Condition | Expected Icon |
|---|---|---|
| 0 | Clear | Sun with rays |
| 1 | Mainly clear | Sun + partial cloud |
| 3 | Overcast | Cloud |
| 45 | Fog | Horizontal lines |
| 61 | Rain | Cloud + 2 drops |
| 71 | Snow | Cloud + 3 flakes |
| 95 | Thunderstorm | Lightning bolt |
| 999 | Invalid | Cloud (fallback) |

---

## Summary of Findings

### Quality Issues (Non-Critical)
✅ **Missing includes**: Weather icons code works but violates best practices  
✅ **Type casting**: Implicit conversions cause minor precision loss  
✅ **Code clarity**: Intent unclear for developers  
✅ **Non-deterministic code**: `drawMist()` uses randomness (not currently used)  

### Potential Runtime Issues
⚠️ **Sun rays**: May appear slightly misaligned due to floating-point truncation  
⚠️ **Snowflake rendering**: Inconsistent depending on size values  
⚠️ **Fog lines**: Minor width inconsistency  
⚠️ **API fallback**: Unmapped codes default to clear sky (false positive)  

### Current Functionality
✅ **Code compiles successfully**  
✅ **Weather icons display**  
✅ **All major weather codes handled**  
✅ **Safe fallback for unknown codes**  
✅ **Proper initialization with defaults**  

---

## Conclusion

The weather icons system is **functionally working** and icons should be displaying on the home screen. However, there are several **code quality issues and potential rendering edge cases** that could cause:

1. **Minor visual artifacts** in specific weather conditions
2. **Floating-point precision issues** in trigonometric calculations
3. **Inconsistent sizing** for fallback cases
4. **Maintenance challenges** due to implicit includes

**The code is safe to run** and unlikely to cause crashes, but **should be refactored** to follow best practices and eliminate floating-point precision issues.

---

## Recommendations for Next Steps

### Priority 1 (Best Practice Fixes)
- [ ] Add explicit `#include <cmath>` to weather_icons.cpp
- [ ] Add `#define PI M_PI` or use M_PI directly
- [ ] Use explicit type casting: `(int16_t)(cos(...) + 0.5)` for rounding

### Priority 2 (Correctness Fixes)  
- [ ] Fix fallback cloud to use full size (not 2/3)
- [ ] Remove or properly implement randomness in drawMist()
- [ ] Add validation: `weatherCode < 100`

### Priority 3 (Enhancement)
- [ ] Add error logging for failed weather updates
- [ ] Add timestamp to show when weather data was last updated
- [ ] Add unit tests for weather icon rendering

### Status Before Next Action
**AWAITING USER DECISION**: Do you want me to:
1. Fix all issues identified (recommended)
2. Fix only the critical issues
3. Leave as-is (code is working)
4. Continue investigation for additional issues

