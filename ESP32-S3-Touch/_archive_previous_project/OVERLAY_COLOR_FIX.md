# OVERLAY COLOR FIX - Builder Changes Not Appearing

## Problem
User changed the color of the Clock Panel in the LCD builder, clicked "Apply Changes", but the changes did NOT appear on the physical display even though the overlay was saving successfully.

## Root Cause Analysis

### What Was Happening:
1. ✅ Frontend correctly sent both `color` (background) and `textColor` fields
2. ✅ Overlay saved to `/overlay0.json` successfully  
3. ✅ Overlay loaded from SPIFFS successfully
4. ❌ **ESP32 CustomWidget struct only had ONE color field**
5. ❌ **textColor field was ignored during JSON parsing**
6. ❌ **Widgets rendered with theme default text color instead of user's choice**

### The Missing Link:
The `CustomWidget` struct definition was incomplete:

**BEFORE:**
```cpp
struct CustomWidget {
  String type;
  String binding;
  String text;
  uint16_t color;    // Only ONE color field!
  uint8_t fontSize;
  int16_t x, y, w, h;
};
```

**AFTER:**
```cpp
struct CustomWidget {
  String type;
  String binding;
  String text;
  uint16_t color;      // Background/fill color
  uint16_t textColor;  // Text color (NEW!)
  uint8_t fontSize;
  int16_t x, y, w, h;
};
```

## Fixes Applied

### 1. Added textColor Field to CustomWidget Struct
**File:** `src/ui/custom_pages.h`
- Added `uint16_t textColor` field to store text color separately from background color

### 2. Updated JSON Parsing to Read textColor
**File:** `src/ui/custom_pages.cpp` - Lines ~128 and ~186

**Custom Pages Parsing:**
```cpp
cw.textColor = w.containsKey("textColor") ? toColor(w["textColor"]) : (currentTheme ? currentTheme->text : 0xFFFF);
```

**Overlay Parsing:**
```cpp
cw.textColor = w.containsKey("textColor") ? toColor(w["textColor"]) : (currentTheme ? currentTheme->text : 0xFFFF);
```

- Reads `textColor` from JSON if present
- Falls back to theme default if not specified
- Uses the same `toColor()` function to convert hex colors to RGB565

### 3. Updated Widget Rendering to Use textColor
**File:** `src/ui/custom_pages.cpp` - `drawWidget()` function

**Label widgets:**
```cpp
drawTextTopLeft(d, font, w.textColor ? w.textColor : currentTheme->text, w.x, w.y + 6, w.text);
```

**Value widgets:**
```cpp
drawTextTopLeft(d, font, w.textColor ? w.textColor : currentTheme->text, w.x, w.y + 6, v.length() ? v : w.text);
```

**Badge widgets:**
```cpp
drawTextCentered(d, font, w.textColor ? w.textColor : currentTheme->text, w.x + 6, w.y + 4, w.w - 12, w.h - 8, v);
```

**Panel widgets:**
- Still use `w.color` for background fill (correct behavior)

## How It Works Now

### Color Assignment Flow:
1. User sets colors in builder:
   - **Color picker #1** → `color` field (background/fill)
   - **Text Color picker** → `textColor` field (text)

2. Frontend sends JSON:
   ```json
   {
     "id": "home.clock.panel",
     "type": "value",
     "color": "#ff0080",
     "textColor": "#0000ff",
     ...
   }
   ```

3. ESP32 receives and saves to `/overlay0.json`

4. On page draw:
   - Load overlay from SPIFFS
   - Parse JSON → `CustomWidget` struct
   - `toColor("#ff0080")` → `color` field (RGB565)
   - `toColor("#0000ff")` → `textColor` field (RGB565)

5. Render widget:
   - Panel background: Use `color`
   - Text rendering: Use `textColor`

## Testing Checklist

Upload firmware and test these scenarios:

### Test 1: Change Panel Background Color
1. Select Clock Panel widget
2. Change "Color" to bright pink `#ff0080`
3. Click "Apply Changes"
4. ✅ **Expected:** Panel background turns pink
5. ✅ **Expected:** Text remains its original color

### Test 2: Change Text Color
1. Select Clock Panel widget  
2. Change "Text Color" to blue `#0000ff`
3. Click "Apply Changes"
4. ✅ **Expected:** Text turns blue
5. ✅ **Expected:** Panel background stays same

### Test 3: Change Both Colors
1. Select any widget
2. Change both color pickers
3. Click "Apply Changes"
4. ✅ **Expected:** BOTH colors update immediately

### Test 4: Label Widget Colors
1. Select a label widget
2. Change text color
3. Click "Apply Changes"
4. ✅ **Expected:** Label text changes color

### Test 5: Persistence
1. Change widget colors
2. Apply changes
3. Reboot ESP32
4. ✅ **Expected:** Colors persist after reboot

## Build Status
- ✅ Code compiles without errors
- ✅ All widget types support textColor
- ✅ Backward compatible (missing textColor uses theme default)
- ⚠️ **Ready to upload and test**

## Upload Commands
```bash
# Upload firmware
pio run -e esp32-s3-devkitc-1 -t upload --upload-port COM24

# No need to upload SPIFFS (only firmware changed)
```

## Additional Benefits

This fix also enables:
- **Better contrast control** - Set light text on dark backgrounds
- **Accessibility** - Users can choose high-contrast color combinations
- **Design flexibility** - Independent control of all visual elements
- **Theme consistency** - Can override theme colors per widget

## Files Modified
1. `src/ui/custom_pages.h` - Added textColor field to CustomWidget
2. `src/ui/custom_pages.cpp` - Updated parsing and rendering logic
3. `platformio.ini` - Increased async_tcp stack (previous fix)
4. `src/main.cpp` - Fixed stack overflow handlers (previous fix)

## What Was NOT Changed
- Frontend code (`builder.js`) - Already sends textColor correctly ✓
- Overlay save endpoint - Already working correctly ✓
- File format - JSON structure unchanged ✓

## Summary
The builder was working perfectly on the frontend side. The ESP32 firmware was simply ignoring the `textColor` field because it wasn't defined in the struct. Now both colors are properly parsed and applied during rendering.

**Status: FIXED - Ready for device testing**
