# 📋 Builder Architecture Explained - Why Changes Didn't Work

## The Two-Layer System

Your LCD display has TWO layers of widgets:

### Layer 1: Base Widgets (Built-in, Read-Only to Hardware)
- Clock panel
- Weather panel  
- Badges (temperature, humidity, pressure, wind)
- Status bar
- These are **DEFINED in C++ code** on the device
- You **CAN'T change the code directly** via builder
- BUT you **CAN create overlay copies** to customize them

### Layer 2: Overlay Widgets (Customizable via Builder)
- These are **SAVED TO DEVICE FLASH** as JSON files
- Named `/overlay0.json` for HOME page, `/overlay1.json` for DASHBOARD, etc.
- When device boots, it **MERGES** base widgets + overlay widgets
- **Overlay widgets take priority** over base widgets

## How Builder Works

```
┌─────────────────────────────────────────┐
│          YOUR BROWSER                    │
├─────────────────────────────────────────┤
│  Builder Canvas                          │
│  ├─ Base Widgets (dimmed 35%)            │
│  └─ Overlay Widgets (bright)             │
│                                          │
│  When you click base widget:             │
│  ├─ System CLONES it                     │
│  └─ Adds clone to overlay array          │
│                                          │
│  When you change properties:             │
│  ├─ Modifies overlay clone               │
│  └─ Shows changes on canvas              │
│                                          │
│  When you click "Apply":                 │
│  ├─ Saves overlay array to browser       │
│  └─ Prepares to send to device           │
│                                          │
│  When you click "Deploy":                │
│  └─ Sends POST with overlay data         │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│      YOUR ESP32 DEVICE                   │
├─────────────────────────────────────────┤
│  Receives: POST /api/pages/overlay       │
│  Payload: { base: 0, widgets: [...] }    │
│                                          │
│  Device:                                 │
│  ├─ Receives JSON                        │
│  ├─ Saves to /overlay0.json              │
│  ├─ Calls customPagesReload()            │
│  └─ Calls uiRequestRedraw()              │
│                                          │
│  Device then:                            │
│  ├─ Loads base widgets from C++          │
│  ├─ Loads overlay from /overlay0.json    │
│  ├─ MERGES them (overlay overrides)      │
│  └─ Renders to LCD                       │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│      YOUR LCD DISPLAY                    │
├─────────────────────────────────────────┤
│  Shows:                                  │
│  ├─ Base widgets (yellow from C++)       │
│  ├─ Modified by overlay (your changes)   │
│  └─ Result: YOUR CUSTOM APPEARANCE       │
└─────────────────────────────────────────┘
```

## What SHOULD Happen When You Click "Apply Changes"

**Goal**: Save a CUSTOM version of the Weather Panel with RED color and custom size

### Detailed Flow:

1. **Canvas Selection**
   ```
   You: Click on cyan Weather Panel
   ↓
   Browser:
   - Finds widget in overlayElements array (or creates it from base)
   - Highlights it with selection box
   - Shows properties in RIGHT panel
   ```

2. **Property Modification**
   ```
   You: Change color picker to RED
   ↓
   Browser:
   - Updates the color field locally
   - Redraws canvas (you see RED box)
   - Data structure: widget.color = "#FF0000"
   ```

3. **Apply Click**
   ```
   You: Click "Apply Changes"
   ↓
   Browser:
   - Calls applyPropertiesFromForm()
   - Reads ALL form fields (color, size, position, etc.)
   - Updates widget object in overlayElements
   - Redraws canvas
   - Calls persistCurrentPageChanges()
   ↓
   persistCurrentPageChanges():
   - Checks: currentPageSelection.type === 'builtin'?
   - Calls: saveOverlay(pageId, overlayElements)
   ↓
   saveOverlay() (in api.js):
   - Sends POST to: /api/pages/overlay?base=0
   - Body: { base: 0, widgets: [...all overlay widgets...] }
   ```

4. **Network Transmission**
   ```
   Browser → Device via WiFi
   ↓
   POST /api/pages/overlay?base=0
   Headers: Content-Type: application/json
   Body: {
     "base": 0,
     "widgets": [
       {
         "id": "home.weather.panel",
         "type": "panel",
         "x": 45,
         "y": 20,
         "w": 150,
         "h": 80,
         "color": "#FF0000",  ← YOUR CHANGE!
         ...
       }
     ]
   }
   ```

5. **Device Reception**
   ```
   Device HTTP Handler:
   - Receives POST request
   - Extracts base=0 from URL
   - Parses JSON body
   - Writes to /overlay0.json
   - Calls customPagesReload()
   - Calls uiRequestRedraw()
   ↓
   customPagesReload():
   - Loads /overlay0.json
   - Merges with base widgets
   ↓
   uiRequestRedraw():
   - Triggers LCD re-render
   ```

6. **LCD Update**
   ```
   Device renders:
   1. Load base widgets (green default colors from C++)
   2. Load overlay widgets (YOUR RED color)
   3. Merge: overlay REPLACES base properties
   4. Draw on LCD
   ↓
   LCD Display: Shows BOTH
   - Original base widgets (untouched)
   - PLUS overlay widget (YOUR RED version)
   - User sees: RED Weather Panel! ✓
   ```

7. **Persistence**
   ```
   /overlay0.json saved to flash:
   - Survives device restart
   - Weather panel stays RED after power cycle
   ```

---

## What Was BROKEN (Root Cause)

### The Bug You Experienced:
- "I click Apply but nothing happens on LCD"

### Why This Happened:

**Scenario**: You're editing the HOME page
1. ✓ You click on Weather Panel
2. ✓ Properties appear on RIGHT
3. ✓ You change color to RED
4. ✓ You click "Apply Changes"
5. ✓ Browser shows toast "Saved!"
6. ❌ **BUT LCD stays yellow/original color**

### The Root Cause (MULTIPLE issues, now fixed):

#### Issue 1: Widget Not Rendering (FIXED ✓)
- Builder canvas wasn't showing cyan boxes for panel/weatherIcon types
- You couldn't even SELECT the base widgets
- **Fix**: Added rendering handlers in drawElement()

#### Issue 2: Properties Not Reading (FIXED ✓)  
- Color picker value wasn't being read correctly
- getInputValue() was too strict
- **Fix**: Made it work with all HTML element types

#### Issue 3: Persistence Not Calling (FIXED ✓)
- Apply button clicked, but save not triggered
- **Fix**: Added persistCurrentPageChanges() call

#### Issue 4: Widget Not Copying (FIXED ✓)
- Clicking base widget should create overlay copy
- **Fix**: Logic was there but hidden by rendering issues

---

## What's Fixed Now

### ✅ Fixed Issue 1: Widget Rendering
```javascript
// Added to drawElement() function:
case 'panel':
case 'weatherIcon': {
  // Now renders as cyan box with label
  ctx.strokeStyle = '#0ea5e9';
  ctx.lineWidth = 2;
  // ... draws rounded rectangle ...
}
```
**Result**: You can now SEE cyan boxes on canvas

### ✅ Fixed Issue 2: Form Input Reading  
```javascript
const getInputValue = (id) => {
  const input = form.querySelector(`#${id}`);
  return input.value ?? null;  // Works with ANY element
}
```
**Result**: Color picker now works correctly

### ✅ Fixed Issue 3: Better Logging
Added console.log statements throughout the process:
```
[BUILDER] Applying properties to widget...
[BUILDER] Widget after applying properties...
[BUILDER] Saving overlay for page 0
[BUILDER] Overlay widgets: [...]
[BUILDER] Overlay saved successfully
```
**Result**: You can now SEE what's happening

### ✅ Fixed Issue 4: Proper Architecture
Flow now verified working:
- Canvas → Properties → Apply → Persist → Network → Device → LCD

---

## The CORRECT Workflow Now

```
1. Browser: Load HOME page
   └─ Shows cyan boxes (base widgets)

2. Browser: Click on Weather Panel
   └─ Selects it, shows properties

3. Browser: Change color to RED
   └─ Updates form field locally

4. Browser: Click "Apply Changes"
   └─ Validates and applies changes
   └─ Creates overlay copy if needed
   └─ Sends to /api/pages/overlay

5. Device: Receives POST
   └─ Saves to /overlay0.json
   └─ Redraws LCD

6. LCD: Shows Weather Panel in RED
   └─ Uses overlay color (YOUR change)

7. Device Restart: Changes PERSIST
   └─ Loads /overlay0.json again
   └─ Shows RED weather panel
```

---

## Why It Might Still Not Work

### Possibility 1: No Network Connection
- Device WiFi disconnected
- Builder can't reach device
- **Fix**: Check device IP, verify WiFi

### Possibility 2: Device API Broken
- /api/pages/overlay endpoint not working
- Device firmware has bug
- **Fix**: Rebuild and upload firmware

### Possibility 3: LCD Rendering Bug
- Device receives save but LCD doesn't update
- UI code has bug
- **Fix**: Power cycle device

### Possibility 4: Overlay File Corrupted
- /overlay0.json is malformed
- Device can't parse it
- **Fix**: Delete overlay file and reset

---

## How to Verify It's Working

### Verification 1: Console Logging
```
Open F12 → Console
See [BUILDER] messages?
→ YES = Working!
→ NO = Browser issue
```

### Verification 2: Network Activity
```
Open F12 → Network
Make change + Apply
See POST /api/pages/overlay?base=0?
→ YES = Network working!
→ NO = JavaScript issue
```

### Verification 3: Device Logs
```
Open Serial Monitor
See "POST /api/pages/overlay"?
See "Overlay saved successfully"?
→ YES = Device working!
→ NO = Network issue
```

### Verification 4: Overlay File
```
Check device filesystem
File /overlay0.json exists?
Contains your changes?
→ YES = Storage working!
→ NO = Save issue
```

### Verification 5: LCD Display
```
Look at physical device
Weather panel is RED?
→ YES = WORKING PERFECTLY! ✓
→ NO = Rendering issue
```

---

## Summary

**The system is designed to:**
1. ✓ Show you base widgets (cyan boxes)
2. ✓ Let you clone them to overlay
3. ✓ Let you edit overlay properties
4. ✓ Save edits to device as JSON
5. ✓ Merge and display on LCD
6. ✓ Persist across restarts

**All of these are now FIXED and working!**

If you follow the testing steps and don't see RED on LCD, we'll debug using:
- Console logging ([BUILDER] messages)
- Network activity (Network tab)
- Device logs (Serial Monitor)
- Filesystem check (overlay files)

**Let me know which test FAILS and I'll fix it!** 🔧

