# QUICK REFERENCE - Issues and Fixes Checklist

## 🔴 CRITICAL ISSUES - FIX IMMEDIATELY

### Issue #1: Clock Seconds Frozen ⏰
- **What's Wrong:** Clock shows seconds (HH:MM:SS) but never updates
- **Where:** Home page display
- **Cause:** No refresh loop for home page after initial draw
- **Quick Fix:** Add per-second refresh in `uiLoop()` 
- **Est. Time:** 1 hour
- **Files to Edit:** 
  - `src/ui/ui_framework.cpp` - Add clock refresh in uiLoop()

**Implementation Checklist:**
- [ ] Add `lastHomeRefreshMs` and `lastHomeSecond` static variables
- [ ] Check current second in uiLoop()
- [ ] Call `uiRequestRedraw()` only when second changes
- [ ] Test: Verify seconds update every second on home screen

---

### Issue #2: Overlapping Text in Panels 📝
- **What's Wrong:** Weather description, badges, and sensor box overlap
- **Where:** Weather panel, Status bar
- **Cause:** No text width pre-calculation; fixed positions don't account for content
- **Quick Fix:** Calculate text width before drawing; implement truncation with ellipsis
- **Est. Time:** 2-3 hours
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Replace drawWeatherPanel() and drawStatusBar()

**Implementation Checklist:**
- [ ] Add `getTextWidth()` calls to calculate space needed
- [ ] Implement text truncation with "..." for long strings
- [ ] Calculate available width before positioning elements
- [ ] Test with long weather descriptions
- [ ] Test with long status bar elements
- [ ] Verify no text extends beyond panel boundaries

---

### Issue #3: Data Changes Don't Auto-Update Home Page 📊
- **What's Wrong:** When boiler/weather data updates, home page doesn't refresh
- **Where:** Home page display after sensor/API updates
- **Cause:** Navigation lock blocks rapid redraws for 500ms after navigation
- **Quick Fix:** Create `uiRequestHomeUpdate()` that bypasses navigation lock
- **Est. Time:** 1.5 hours
- **Files to Edit:**
  - `src/ui/ui_framework.h` - Add new function declaration
  - `src/ui/ui_framework.cpp` - Implement uiRequestHomeUpdate()
  - `src/ui/page_home.cpp` - Use uiRequestHomeUpdate() in data update functions

**Implementation Checklist:**
- [ ] Add `uiRequestHomeUpdate()` function to bypass nav lock
- [ ] Update `updateWeatherOnly()` to call new function
- [ ] Update `updateBoilerData()` to call new function
- [ ] Test: Update weather API, verify page updates without navigation
- [ ] Test: Update boiler temp, verify page updates instantly

---

## 🟡 IMPORTANT ISSUES - Fix Next

### Issue #4: Status Bar Overflow 📊
- **What's Wrong:** Status bar elements can overflow screen width (480px)
- **Where:** Bottom of home page
- **Cause:** Multiple variable-length elements without bounds checking
- **Quick Fix:** Add space validation; hide elements if not enough space
- **Est. Time:** 1 hour
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Replace drawStatusBar() with bounds checking

**Implementation Checklist:**
- [ ] Calculate max width for each status bar section
- [ ] Add early exit if element doesn't fit
- [ ] Convert large memory values to MB
- [ ] Show WiFi as percentage, not raw dBm
- [ ] Show heater on/off state, not just mode
- [ ] Test: Verify all elements fit at different WiFi/memory states
- [ ] Test: Very long strings don't cause overflow

---

### Issue #5: WiFi Strength Display Incomplete 📡
- **What's Wrong:** Shows raw dBm value (-120 to 0) without context
- **Where:** Status bar
- **Cause:** No RSSI-to-percentage conversion; no visual indicator
- **Quick Fix:** Add RSSI calculation function; display as percentage
- **Est. Time:** 30 minutes
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Add getRssiPercent() helper function

**Implementation Checklist:**
- [ ] Add `getRssiPercent(int rssi)` function that converts dBm to 0-100%
- [ ] Use formula: (rssi + 120) * 100 / 90
- [ ] Change display from dBm to percentage
- [ ] Add color coding: Green (80-100), Yellow (60-80), Orange (40-60), Red (0-40)
- [ ] Test: Check WiFi display at various signal levels

---

### Issue #6: Heater Mode Display Unclear 🔥
- **What's Wrong:** Shows only mode (OFF/MAN/AUTO/AI) but not on/off state
- **Where:** Status bar mode chip
- **Cause:** Missing state indicator; could be manually OFF but chip says "MAN"
- **Quick Fix:** Add on/off state and icon to mode display
- **Est. Time:** 30 minutes
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Enhance drawStatusBar() mode display

**Implementation Checklist:**
- [ ] Show heater on/off status even in OFF mode
- [ ] Add icons: 🔥 for ON, ❌ for OFF, 🎛️ for MAN, ⚙️ for AUTO, 🤖 for AI
- [ ] Example: "🔥 MAN" or "❌ OFF" or "⚙️ AUTO"
- [ ] Test: Verify state changes with heater on/off

---

### Issue #7: Temperature Sensor Status Too Minimal 🌡️
- **What's Wrong:** Status bar shows only "✓" or "✗" for sensor
- **Where:** Status bar
- **Cause:** Doesn't show actual temperature value for quick reference
- **Quick Fix:** Show abbreviated temperature value instead of checkmark
- **Est. Time:** 15 minutes
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Update status bar sensor display

**Implementation Checklist:**
- [ ] Change from "TEMP:✓" to "TEMP:45°" format
- [ ] Keep checkmark color coding for status
- [ ] Green for valid, Red for invalid
- [ ] Test: Verify temperature displays correctly

---

## 🟢 RECOMMENDED LONG-TERM IMPROVEMENTS

### Issue #8: Partial Screen Updates Not Implemented 📐
- **What's Wrong:** Every update redraws entire screen causing flicker
- **Where:** Home page redraw mechanism
- **Cause:** `drawPageHome()` does full screen clear and redraw
- **Improvement:** Implement dirty rect system to update only changed areas
- **Est. Time:** 2-3 hours
- **Files to Edit:**
  - `src/ui/page_home.cpp` - Extract panel redraws; add change detection

**Implementation Checklist:**
- [ ] Create `redrawClockPanelIfChanged()` function
- [ ] Create `redrawWeatherPanelIfChanged()` function
- [ ] Create `redrawStatusBarIfChanged()` function
- [ ] Track last-drawn values (time, temp, humidity, etc.)
- [ ] Update `uiRequestHomeUpdate()` to call partial redraws
- [ ] Test: Verify no flicker on updates
- [ ] Measure: Compare before/after update speed

---

### Issue #9: Web UI Status Polling Too Slow 🌐
- **What's Wrong:** Web UI updates status every 3 seconds (feels sluggish)
- **Where:** `data/js/app.js` - `startStatusPolling()`
- **Cause:** `setInterval(fetchStatus, 3000)` is hardcoded
- **Improvement:** Reduce to 1-2 seconds; add adaptive polling
- **Est. Time:** 30 minutes
- **Files to Edit:**
  - `data/js/app.js` - Update polling interval

**Implementation Checklist:**
- [ ] Change interval from 3000ms to 1500ms
- [ ] Add fetch timeout (5 seconds)
- [ ] Add error retry with exponential backoff
- [ ] Test: Verify faster updates don't cause lag

---

### Issue #10: No Timezone Auto-Detection 🌍
- **What's Wrong:** Timezone is hardcoded; must be manually configured
- **Where:** `src/main.cpp` - hardcoded during setup
- **Cause:** No IP-based geolocation integration
- **Improvement:** Auto-detect timezone on first boot
- **Est. Time:** 45 minutes
- **Files to Edit:**
  - `src/network/network_manager.h` - Add geolocation function
  - `src/main.cpp` - Call geolocation on first boot

**Implementation Checklist:**
- [ ] Add IP-based timezone lookup function
- [ ] Call on first boot (when time not set)
- [ ] Fall back to UTC if lookup fails
- [ ] Allow manual override in settings
- [ ] Test: Verify correct timezone is set

---

## 📋 FILE MODIFICATION SUMMARY

### Core Files to Modify

**`src/ui/ui_framework.cpp`** (HIGH PRIORITY)
- [ ] Add clock refresh variables
- [ ] Implement per-second refresh in uiLoop()
- [ ] Add uiRequestHomeUpdate() function
- [ ] Update to use partial panel redraws

**`src/ui/page_home.cpp`** (CRITICAL)
- [ ] Fix drawWeatherPanel() with text overflow handling
- [ ] Fix drawStatusBar() with bounds checking
- [ ] Add getRssiPercent() helper
- [ ] Extract partial redraw functions
- [ ] Update data update callbacks

**`src/ui/ui_framework.h`** (MEDIUM)
- [ ] Add function declarations
- [ ] Add partial redraw function declarations

**`data/js/app.js`** (LOW)
- [ ] Update polling interval
- [ ] Add error handling

---

## ✅ Testing Checklist

### Functional Testing
- [ ] Clock seconds update every second on home screen
- [ ] No text overlaps in any panels
- [ ] Weather panel displays correctly with long descriptions
- [ ] Status bar fits within 480px width
- [ ] Boiler sensor box doesn't overlap with details
- [ ] WiFi strength shows as percentage
- [ ] Heater mode shows with on/off state and icon
- [ ] Temperature sensor shows value, not just checkmark
- [ ] Data updates appear on home screen without navigation
- [ ] No page flicker on data updates

### Performance Testing
- [ ] Measure redraw time: should be < 100ms for partial, < 500ms for full
- [ ] Verify no memory leaks over 24 hours
- [ ] Check CPU usage before/after partial update implementation
- [ ] Profile: partial updates should be 10-100x faster than full

### Visual Testing
- [ ] Test with extreme values (huge WiFi values, very long strings)
- [ ] Test with missing data (undefined values, NaN)
- [ ] Test at different font sizes (0.85x, 1.0x, 1.15x, 1.30x)
- [ ] Test in both Dark and Light themes
- [ ] Verify alignment on 480x272 display
- [ ] Check text readability at all sizes

---

## 🚀 Implementation Order (Recommended)

1. **First Day:**
   - Fix #1: Clock seconds update (1 hour) ✓
   - Fix #4: Status bar overflow (1 hour) ✓
   - Fix #5: WiFi strength (30 min) ✓
   - **Total: 2.5 hours** - Visible improvements

2. **Second Day:**
   - Fix #2: Text overlaps (2-3 hours) ✓
   - Fix #6: Heater mode clarity (30 min) ✓
   - Fix #7: Temperature display (15 min) ✓
   - **Total: 3-4 hours** - Polish UI

3. **Third Day:**
   - Fix #3: Real-time data updates (1.5 hours) ✓
   - Fix #8: Partial screen updates (2-3 hours) ✓
   - **Total: 3.5-4.5 hours** - Performance & responsiveness

4. **Future:**
   - Fix #9: Web UI polling (30 min)
   - Fix #10: Timezone auto-detect (45 min)
   - Additional improvements and optimizations

---

## 🔗 Relevant Code Locations

### Time Management
- Time display: `src/ui/page_home.cpp` - `drawClockPanel()`
- Time update: `src/ui/ui_framework.cpp` - `uiLoop()`
- Time library: `src/utils/time_manager.h`

### Text Rendering
- Text utility: `src/ui/page_home.cpp` - `printText()`, `getTextWidth()`
- Font selection: `src/ui/page_home.cpp` - `getFont()`
- Text color: Throughout page_home.cpp

### Weather Display
- Weather panel: `src/ui/page_home.cpp` - `drawWeatherPanel()`
- Weather data: `src/ui/page_home.h` - `WeatherData` struct
- Weather update: `src/ui/page_home.cpp` - `updateWeatherOnly()`

### Status Display
- Status bar: `src/ui/page_home.cpp` - `drawStatusBar()`
- Heater state: `globalWeather.heaterControlMode`, `globalWeather.heaterOn`
- WiFi status: `NetworkManager::getWiFiRSSI()`
- Memory status: `ESP.getFreeHeap()`

### UI Framework
- Page navigation: `src/ui/ui_framework.cpp` - `navigateToPage()`
- UI loop: `src/ui/ui_framework.cpp` - `uiLoop()`
- Theme system: `src/ui/ui_framework.cpp` - `THEME_DARK`, `THEME_LIGHT`

---

