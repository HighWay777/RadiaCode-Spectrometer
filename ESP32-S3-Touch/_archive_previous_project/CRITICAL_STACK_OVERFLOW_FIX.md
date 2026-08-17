# CRITICAL Stack Overflow Fix

## Problem Identified
Device was crashing with stack overflow in `async_tcp` task when saving LCD builder changes.

### Root Cause
Large `StaticJsonDocument` allocations (12KB - 16KB) on the stack within AsyncWebServer callback handlers exceeded the async_tcp task stack size (default ~4KB).

### Crash Symptoms
```
***ERROR*** A stack overflow in task async_tcp has been detected.
Backtrace: 0x403780de:0x3fca99d0 0x4037d1ad:0x3fca99f0 ...
ESP32 reboots immediately
Browser shows: NS_ERROR_NET_RESET
```

## Fixes Applied

### 1. Increased Async TCP Stack Size
**File:** `platformio.ini`

Added build flag:
```ini
-D CONFIG_ASYNC_TCP_STACK_SIZE=8192
```

This doubles the default stack from 4KB to 8KB.

### 2. Eliminated Large Stack Allocations in POST Handlers

#### `/api/pages/overlay` POST Handler
**Before:** Used `StaticJsonDocument<12288>` on stack
**After:** Direct file write with minimal stack usage using static `String*` accumulator

```cpp
// Now uses request->_tempObject as String* buffer
// Writes directly to SPIFFS without JSON parsing
// Stack usage: ~200 bytes
```

#### `/api/pages` POST Handler  
**Before:** Used `StaticJsonDocument<16384>` on stack
**After:** Direct file write without intermediate JSON parsing

```cpp
// Writes body directly to /pages.json
// No JSON parsing in handler
// Stack usage: ~200 bytes
```

#### `/api/pages/delete` POST Handler
**Before:** Used `StaticJsonDocument<16384>` on stack  
**After:** Uses `DynamicJsonDocument` (heap allocated)

```cpp
DynamicJsonDocument* doc = new DynamicJsonDocument(16384);
// ... process ...
delete doc;
// Stack usage: ~100 bytes
```

### 3. Safe Allocations Retained

These handlers use smaller stack allocations (< 4KB) and are safe:

- `/api/status` - 512 bytes
- `/api/config` GET - 1024 bytes  
- `/api/config` POST - 1024 bytes
- `/api/pages/base` - 4096 bytes (borderline, but GET only)
- `/api/presets/save` - Uses DynamicJsonDocument (heap) ✓

## Testing Checklist

Before claiming "production ready", test these scenarios:

1. **Save Overlay Changes**
   - Open LCD Builder
   - Add/modify widgets on overlay
   - Click "Apply Changes"
   - Verify: Device does NOT reboot
   - Check: Serial monitor for any errors

2. **Create Custom Page**
   - Create new page with 5+ widgets
   - Click "Deploy"
   - Verify: Page saves successfully
   - Check: Device stability

3. **Delete Custom Page**
   - Delete a custom page
   - Verify: No crash
   - Check: pages.json updated correctly

4. **Save Preset**
   - Create page with multiple widgets
   - Save as preset
   - Verify: Preset appears in list

5. **Load Preset**
   - Load a saved preset
   - Verify: Widgets render correctly

6. **Serial Monitor Check**
   - Monitor for ANY stack warnings
   - Look for memory allocation failures
   - Verify no task watchdog triggers

## Memory Guidelines for Future Development

### NEVER Do This in Async Handlers:
```cpp
server.on("/endpoint", HTTP_POST, [](AsyncWebServerRequest* req) {
  StaticJsonDocument<8192> doc;  // ❌ TOO LARGE FOR STACK!
  // ...
});
```

### DO This Instead:
```cpp
// Option 1: Use DynamicJsonDocument (heap)
DynamicJsonDocument* doc = new DynamicJsonDocument(8192);
// ... process ...
delete doc;

// Option 2: Direct file write (no parsing)
String* buf = static_cast<String*>(req->_tempObject);
File f = SPIFFS.open("/file.json", FILE_WRITE);
f.print(*buf);
f.close();

// Option 3: Static allocation outside handler (persistent)
static DynamicJsonDocument sharedDoc(8192);
// Use mutex if multi-threaded access
```

### Stack Budget Rules:
- **< 1KB**: Safe for any handler
- **1-4KB**: Okay for simple GET handlers  
- **> 4KB**: Use heap allocation (DynamicJsonDocument)
- **> 8KB**: MUST use heap, even with increased stack

## Build & Upload Commands

```bash
# Build firmware
pio run -e esp32-s3-devkitc-1

# Upload firmware
pio run -e esp32-s3-devkitc-1 -t upload --upload-port COM24

# Upload SPIFFS (if web files changed)
pio run -e esp32-s3-devkitc-1 -t uploadfs --upload-port COM24
```

## Status
- ✅ Stack size increased to 8KB
- ✅ All large stack allocations eliminated
- ✅ Code compiles without errors
- ⚠️ **NEEDS DEVICE TESTING** - Upload and verify no crashes
- ⚠️ **NEEDS FULL BUILDER TEST** - Test all save/load operations

## Next Steps
1. Upload firmware to device
2. Test overlay save (the original crash scenario)
3. Test custom page creation
4. Monitor serial output for any warnings
5. Only claim "production ready" after ALL tests pass
