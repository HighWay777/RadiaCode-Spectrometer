# ESP32-S3 Touch LCD Builder - Current Status & Fixes Applied

**Last Updated:** October 2, 2025  
**Project:** ESP32-S3 Touch Display with Professional LCD Builder  
**Device:** ESP32-S3 DevKitC-1, JC4827W543 480×272 QSPI Display, COM24

---

## ✅ WORKING FEATURES

### Main UI
- ✅ **Dashboard** - Fully functional with real-time data
- ✅ **Control** - Mode selector, temperature slider working
- ✅ **Settings** - Display, WiFi, system settings accessible
- ✅ **Navigation** - All tabs switching correctly
- ✅ **LCD Clock/Date** - Time and date displaying on physical LCD
- ✅ **Data Polling** - Status updates every 5 seconds
- ✅ **Connection Status** - Green "Connected" indicator

### Builder Page
- ✅ **Builder Tab Accessible** - Can navigate to builder page
- ✅ **Professional UI Layout** - Page manager, mode tabs, toolbar, widget palette visible
- ✅ **Canvas** - 480×272 canvas rendering
- ✅ **Basic Tools** - New/Save/Load buttons present

---

## ❌ NOT WORKING / ISSUES

### Critical Issues
1. **❌ Dropdown Menu Empty** - Page selector shows "Select page..." but no options
2. **❌ Properties Panel Inactive** - Widget properties not updating
3. **❌ Bindings Not Working** - Data binding browser not populated
4. **❌ API Returns Incomplete Data** - `/api/pages/all` only returns `pages`, missing `builtins` array

### Known Bugs
- **SPIFFS Backup Files** - VS Code/PlatformIO keeps recreating `.backup` files causing upload errors
- **JavaScript File Size** - 25KB file was too large for ESP32 buffer (caused NS_ERROR_NET_PARTIAL_TRANSFER)
- **Firmware Not Rebuilding Properly** - C++ changes to API not taking effect

---

## 🔧 FIXES APPLIED

### Fix #1: Page Loading Functions Added (app.js)
**Location:** `data/js/app.js` lines 197-292  
**What was added:**
```javascript
async function loadBuiltinPages() {
  // Fetches from /api/pages/all
  // Hardcoded fallback for 5 built-in pages if API fails
  const builtins = data.builtins || [
    {id: 0, name: 'Home'},
    {id: 1, name: 'Boiler'},
    {id: 2, name: 'Stats'},
    {id: 3, name: 'Controls'},
    {id: 4, name: 'Settings'}
  ];
  // Populates dropdown with built-in and custom pages
}

async function loadBuiltinPageWidgets(pageId) {
  // Fetches widgets from /api/pages/base?base={id}
  // Displays on canvas
}

async function loadCustomPage(index, pages) {
  // Loads custom page widgets
}
```

**Status:** Code added but **dropdown still not populating** (may be VS Code file handling issue)

### Fix #2: setupBuilder Modified
**Location:** `data/js/app.js` line 297  
**Change:** Added call to `loadBuiltinPages()` on builder initialization  
**Status:** ✅ Applied

### Fix #3: Comments Removed (File Size Reduction)
**What:** Removed comment blocks to reduce JavaScript file size  
**Before:** 25,520 bytes  
**After:** 25,218 bytes (minimal reduction)  
**Status:** ⚠️ File still too large for reliable transfer

### Fix #4: Backup File Cleanup
**Files Removed:**
- `data/js/app.js.backup`
- `data/js/app.js.backup_before_professional`
- `data/js/app.js.old`
- `data/css/style.css.backup`
- `data/index.html.backup`

**Result:** SPIFFS size reduced from 67KB to 24KB  
**Status:** ✅ But files keep being recreated by VS Code/PlatformIO

### Fix #5: Server Configuration Update (main.cpp)
**Location:** `src/main.cpp` lines 561-562  
**Change:**
```cpp
// Old:
server.serveStatic("/css/", SPIFFS, "/css/").setCacheControl("no-store");
server.serveStatic("/js/", SPIFFS, "/js/").setCacheControl("no-store");

// New:
server.serveStatic("/css/", SPIFFS, "/css/").setCacheControl("no-store").setDefaultFile("style.css");
server.serveStatic("/js/", SPIFFS, "/js/").setCacheControl("no-store").setDefaultFile("app.js");
```
**Status:** ✅ Applied and firmware uploaded

---

## 📊 API ENDPOINTS DISCOVERED

### Working Endpoints
- ✅ `GET /api/status` - Real-time device status
- ✅ `POST /api/mode` - Set control mode
- ✅ `POST /api/target` - Set target temperature
- ⚠️ `GET /api/pages/all` - **Returns incomplete data** (only `pages`, missing `builtins`)
- ✅ `GET /api/pages/base?base={id}` - Get built-in page widgets
- ✅ `GET /api/pages/overlay?base={id}` - Get overlay widgets
- ✅ `POST /api/pages/overlay?base={id}` - Save overlay widgets
- ✅ `DELETE /api/pages/overlay?base={id}` - Remove overlay

### Expected API Response Format

**`/api/pages/all` SHOULD return:**
```json
{
  "builtins": [
    {"id": 0, "name": "Home"},
    {"id": 1, "name": "Boiler"},
    {"id": 2, "name": "Stats"},
    {"id": 3, "name": "Controls"},
    {"id": 4, "name": "Settings"}
  ],
  "pages": [
    {
      "name": "My Custom",
      "base": 0,
      "widgets": [...]
    }
  ]
}
```

**ACTUALLY returns:**
```json
{
  "pages": [
    {"name": "My Custom", "base": 0, "widgets": [...]}
  ]
}
```

**Root Cause:** Firmware not rebuilding properly or C++ compilation issue with `PAGE_COUNT`

---

## 🗂️ FILE STRUCTURE

### HTML (363 lines)
**Location:** `data/index.html`

**Builder Section (lines 263-353):**
```html
<section id="builder" class="page">
  <h2>🎨 LCD Builder</h2>
  
  <!-- Page Manager -->
  <div class="page-manager">
    <label>🗂️ Editing:</label>
    <select id="page-selector">
      <option value="">Select page...</option>
    </select>
  </div>
  
  <!-- Mode Tabs -->
  <div class="mode-tabs">
    <button class="tab-button" data-mode="new">New Page</button>
    <button class="tab-button" data-mode="builtin">Edit Built-in</button>
    <button class="tab-button active" data-mode="overlay">Overlay</button>
  </div>
  
  <!-- Advanced Toolbar -->
  <div class="builder-toolbar-advanced">
    <!-- 4 toolbar sections with 20+ buttons -->
  </div>
  
  <!-- Widget Palette (Sidebar) -->
  <div class="widget-palette">
    <div class="widget-category">TEXT</div>
    <button class="widget-item" data-widget="text">📝 Label</button>
    <button class="widget-item" data-widget="value">🔢 Value</button>
    <!-- More widgets... -->
  </div>
  
  <!-- Canvas -->
  <div class="canvas-workspace">
    <canvas id="builder-canvas" width="480" height="272"></canvas>
    <div class="zoom-controls">
      <button id="btn-zoom-in">+</button>
      <button id="btn-zoom-out">-</button>
      <button id="btn-zoom-fit">Fit</button>
      <span id="zoom-level">100%</span>
    </div>
  </div>
  
  <!-- Properties Panel -->
  <div class="properties-panel">
    <div class="properties-tabs">
      <button class="tab-button active" data-tab="widget">Widget</button>
      <button class="tab-button" data-tab="page">Page</button>
      <button class="tab-button" data-tab="data">Data</button>
      <button class="tab-button" data-tab="deploy">Deploy</button>
    </div>
    <!-- Property forms for each tab -->
  </div>
</section>
```

**Status:** ✅ HTML structure complete

### CSS (981 lines)
**Location:** `data/css/style.css`

**Builder Styles (lines 719-981):**
- Professional two-column layout
- Collapsible widget palette
- Responsive design (breakpoints at 1024px, 768px)
- Active tab states
- Resize handles styling
- Grid visualization

**Status:** ✅ CSS complete

### JavaScript (710 lines, 25KB)
**Location:** `data/js/app.js`

**Key Functions:**
- `init()` - Main initialization
- `setupNavigation()` - Tab switching
- `setupDashboard()` - Dashboard data updates
- `setupControl()` - Control page interactions
- `setupSettings()` - Settings page
- **`loadBuiltinPages()`** - ✅ Added (lines 197-266)
- **`loadBuiltinPageWidgets(pageId)`** - ✅ Added (lines 268-282)
- **`loadCustomPage(index, pages)`** - ✅ Added (lines 284-291)
- `setupBuilder()` - Builder initialization (line 294)
- `redrawCanvas()` - Canvas rendering
- `updatePropertiesPanel()` - Properties form updates

**Status:** ⚠️ Code added but dropdown still not working

---

## 🎯 WHAT NEEDS TO BE DONE NEXT

### Priority 1: Fix Dropdown Population
**Issue:** Dropdown shows "Select page..." but no options appear

**Possible Causes:**
1. VS Code file corruption during save
2. JavaScript file too large (25KB > ESP32 buffer)
3. `loadBuiltinPages()` not being called
4. Selector element ID mismatch
5. Console errors preventing execution

**Debug Steps:**
```javascript
// Add to browser console:
console.log(document.getElementById('page-selector'));
// Should show <select> element, not null

// Check if function is called:
// Add console.log at start of loadBuiltinPages()
```

**Fix Options:**
- **Option A:** Split JavaScript into multiple smaller files
- **Option B:** Minify JavaScript to reduce file size
- **Option C:** Check VS Code settings for file encoding/line endings
- **Option D:** Manually verify uploaded file matches source

### Priority 2: Fix API `/api/pages/all`
**Issue:** API not returning `builtins` array

**C++ Code Location:** `src/main.cpp` lines 392-426

**Expected Code:**
```cpp
server.on("/api/pages/all", HTTP_GET, [](AsyncWebServerRequest* request){
  StaticJsonDocument<2048> outDoc;
  JsonArray builtinsArr = outDoc.createNestedArray("builtins");
  const char* names[] = {"Home","Boiler","Stats","Controls","Settings"};
  for (uint8_t i=0; i<PAGE_COUNT; ++i) {
    JsonObject o = builtinsArr.createNestedObject();
    o["id"] = i;
    o["name"] = names[i];
  }
  // ... rest of code
});
```

**Debug Steps:**
1. Verify firmware was actually rebuilt (check compile output)
2. Verify firmware was uploaded (check upload output)
3. Check if `PAGE_COUNT` is defined (`src/ui/ui_framework.h` line 40)
4. Add Serial.println debug output to confirm code is executing

### Priority 3: Properties Panel
**What to Add:**
```javascript
function updatePropertiesPanel() {
  if (builder.selection.ids.length === 0) {
    document.getElementById('prop-info').textContent = 'No widget selected';
    return;
  }
  
  if (builder.selection.ids.length === 1) {
    const widget = builder.widgets.find(w => w.id === builder.selection.ids[0]);
    if (!widget) return;
    
    document.getElementById('prop-x').value = widget.x;
    document.getElementById('prop-y').value = widget.y;
    document.getElementById('prop-w').value = widget.w;
    document.getElementById('prop-h').value = widget.h;
    document.getElementById('prop-text').value = widget.text || '';
    document.getElementById('prop-fontsize').value = widget.fontSize || 16;
    document.getElementById('prop-binding').value = widget.binding || '';
    
    // Color conversion RGB565 to hex
    const rgb = rgb565ToRgb(widget.color || 0xFFFF);
    const hex = `#${rgb.r.toString(16).padStart(2,'0')}${rgb.g.toString(16).padStart(2,'0')}${rgb.b.toString(16).padStart(2,'0')}`;
    document.getElementById('prop-color').value = hex;
  }
}
```

### Priority 4: Bindings Browser
**What to Add:**
```javascript
function setupBindingBrowser() {
  const bindingList = document.getElementById('binding-list');
  if (!bindingList) return;
  
  const bindings = [
    {name: 'currentTemp', desc: 'Current Temperature'},
    {name: 'targetTemp', desc: 'Target Temperature'},
    {name: 'humidity', desc: 'Humidity %'},
    {name: 'pressure', desc: 'Air Pressure'},
    {name: 'heaterState', desc: 'Heater Status'},
    {name: 'mode', desc: 'Control Mode'},
    {name: 'wifiRSSI', desc: 'WiFi Signal'},
    {name: 'uptime', desc: 'Device Uptime'}
  ];
  
  bindings.forEach(b => {
    const item = document.createElement('div');
    item.className = 'binding-item';
    item.textContent = `${b.name} - ${b.desc}`;
    item.onclick = () => {
      // Assign to selected widget
      if (builder.selection.ids.length > 0) {
        builder.widgets.find(w => w.id === builder.selection.ids[0]).binding = b.name;
        updatePropertiesPanel();
        redrawCanvas();
      }
    };
    bindingList.appendChild(item);
  });
}
```

---

## 🔍 TROUBLESHOOTING GUIDE

### Symptom: Dropdown Shows "Loading pages..." Forever
**Causes:**
- JavaScript file not loading (NS_ERROR_NET_PARTIAL_TRANSFER)
- `loadBuiltinPages()` throwing error
- API call failing

**Fix:**
1. Open browser console (F12)
2. Check for JavaScript errors
3. Check Network tab for `/api/pages/all` response
4. Verify file size: `app.js` should be fully downloaded

### Symptom: NS_ERROR_NET_PARTIAL_TRANSFER
**Cause:** JavaScript file too large (>25KB)

**Fix:**
- **Immediate:** Split into multiple files
- **Better:** Minify JavaScript
- **Best:** Increase ESP32 AsyncWebServer buffer size

### Symptom: Changes Not Taking Effect
**Causes:**
- Browser cache
- VS Code not saving properly
- PlatformIO not uploading correctly

**Fix:**
1. Hard refresh browser (Ctrl+F5)
2. Check file modification times
3. Manually delete `.pio/build` folder and rebuild
4. Use `pio run -t clean` before upload

### Symptom: Firmware Not Rebuilding
**Check:**
```bash
pio run -e esp32-s3-devkitc-1 -t upload --upload-port COM24
```

**Look for:**
- "Building in release mode"
- "Compiling .pio/..."
- "Linking .pio/.../firmware.elf"

If skipped, C++ changes didn't trigger rebuild.

**Force rebuild:**
```bash
pio run -t clean
pio run -e esp32-s3-devkitc-1 -t upload --upload-port COM24
```

---

## 📝 LESSONS LEARNED

### What Worked
1. ✅ **Incremental fixes** - One feature at a time, test immediately
2. ✅ **Hardcoded fallbacks** - When API fails, use client-side defaults
3. ✅ **File cleanup** - Removing backups drastically reduced SPIFFS size
4. ✅ **Console logging** - Adding `console.log()` helped debug issues

### What Didn't Work
1. ❌ **Large file creation** - `create_file` tool corrupts 1000+ line JavaScript files
2. ❌ **Assuming firmware rebuilds** - Must verify compilation actually ran
3. ❌ **Trusting VS Code autosave** - File corruption issues
4. ❌ **"Perfect" claims before testing** - Set realistic expectations

### Best Practices Going Forward
1. ✅ **Always backup before major changes** - `app.js.SAFE_BACKUP` exists
2. ✅ **Test in browser first** - Verify JavaScript syntax before upload
3. ✅ **Check file sizes** - ESP32 has buffer limits
4. ✅ **Use replace_string_in_file for edits** - Avoid full file recreation
5. ✅ **Verify uploads** - Check SPIFFS listing or download file from device

---

## 🚀 QUICK START (After VS Code Reinstall)

### Step 1: Verify Current State
```bash
# Check device IP
ping 192.168.40.48

# Check API
curl http://192.168.40.48/api/pages/all

# Should return JSON with pages array
```

### Step 2: Test Builder Page
1. Open `http://192.168.40.48/#builder`
2. Open browser console (F12)
3. Check for errors
4. Look for "Loaded pages:" log message

### Step 3: If Dropdown Still Broken
```javascript
// Run in browser console:
const selector = document.getElementById('page-selector');
console.log('Selector:', selector);
console.log('Options:', selector.options.length);

// If null: HTML ID mismatch
// If 0 options: loadBuiltinPages() didn't run or failed
```

### Step 4: Manual Test
```javascript
// Run in browser console to manually populate:
const selector = document.getElementById('page-selector');
selector.innerHTML = '<option value="">Select...</option>';
const opt = document.createElement('option');
opt.value = 'builtin-0';
opt.textContent = 'Home';
selector.appendChild(opt);

// If this works, problem is in loadBuiltinPages() function
```

---

## 📚 REFERENCE FILES

### Key Files Modified
1. `data/index.html` (lines 263-353) - Builder HTML structure
2. `data/css/style.css` (lines 719-981) - Builder CSS styles
3. `data/js/app.js` (lines 197-292) - Page loading functions
4. `src/main.cpp` (lines 561-562) - Server configuration

### Backup Files
- `data/js/app.js.SAFE_BACKUP` - Last known working version (710 lines, 25KB)

### Important Constants
- **Device IP:** 192.168.40.48
- **Upload Port:** COM24
- **Canvas Size:** 480×272 pixels
- **Built-in Pages:** 5 (Home, Boiler, Stats, Controls, Settings)
- **SPIFFS Size:** 1.44MB total, currently using ~24KB

---

## ✉️ CONTACT POINTS

**When resuming work, focus on:**
1. Fix dropdown population (Priority 1)
2. Verify API returns `builtins` array
3. Test widget selection and properties panel
4. Implement bindings browser

**Success Criteria:**
- ✅ Dropdown shows 5 built-in pages
- ✅ Selecting a page loads its widgets
- ✅ Widgets display on canvas
- ✅ Properties panel updates when selecting widgets
- ✅ Can save overlay back to device

---

**End of Status Document**  
*Good luck with VS Code reinstall! This document captures everything we've done and where to continue.*
