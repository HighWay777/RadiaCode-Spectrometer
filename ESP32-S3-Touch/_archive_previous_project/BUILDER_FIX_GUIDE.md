# 🔧 Builder Canvas Fix - Diagnostic Guide

## Problem Identified

The Builder canvas was not initializing because:
1. Canvas was being drawn at page load when Builder page was hidden (display:none)
2. No redraw trigger when switching to Builder page
3. Initial canvas setup function was called too early

## Fixes Applied

### 1. **Navigation Redraw Trigger**
Added canvas redraw when switching to Builder page:
```javascript
// In setupNavigation()
if (targetPage === 'builder') {
  setTimeout(() => redrawCanvas(), 100);
}
```

### 2. **Removed Duplicate Initialization**
Removed standalone `initBuilderCanvas()` that was called at page load:
- **Before:** Canvas drawn at page load (when hidden)
- **After:** Canvas drawn by `redrawCanvas()` in `setupBuilder()` and when switching pages

### 3. **File Size**
- **app.js**: 21.7 KB, 581 lines (optimized, removed redundant code)

---

## Testing Instructions

### Step 1: Clear Browser Cache
**CRITICAL:** You must clear cache to load new JavaScript!

**Method A - Hard Refresh:**
```
Ctrl + F5
```

**Method B - Clear Cache:**
1. Press `F12` to open DevTools
2. Right-click refresh button
3. Select "Empty Cache and Hard Reload"

**Method C - Incognito Mode:**
```
Ctrl + Shift + N (Chrome)
Ctrl + Shift + P (Firefox)
```

### Step 2: Navigate to Builder
1. Open: `http://192.168.40.48/`
2. Click "Builder" link in navigation
3. **Expected:** Grid should appear on canvas immediately

### Step 3: Test Builder Functions

**A. Add Elements:**
- Click 📝 (Text) button → Should add "Text" at (100, 100)
- Click 🎯 (Icon) button → Should add 🌡️ icon at (150, 100)
- Click 🔘 (Button) button → Should add blue button at (200, 100)
- Toast notification should appear for each

**B. Select Elements:**
- Click any element on canvas
- **Expected:** Blue dashed border appears around it
- **Expected:** Properties panel shows element details

**C. Drag Elements:**
- Click and drag selected element
- **Expected:** Element follows mouse
- Enable "Snap" checkbox → Element snaps to 20px grid

**D. Edit Properties:**
1. Select element
2. Change values in Properties panel (position, text, size, color)
3. Click "Apply"
4. **Expected:** Element updates immediately
5. **Expected:** Toast "Properties updated" appears

**E. Delete:**
- Select element
- Click 🗑️ Delete
- **Expected:** Element removed
- **Expected:** Toast "Element deleted" appears

**F. Save/Load:**
- Add several elements
- Click 💾 Save
- **Expected:** Downloads `lcd-page.json`
- Click 📂 Load → Select the JSON file
- **Expected:** Elements restored
- **Expected:** Toast "Page loaded" appears

**G. Clear Canvas:**
- Click 📄 New
- Confirm dialog
- **Expected:** All elements removed
- **Expected:** Canvas shows empty grid

---

## Debugging: If Canvas Still Empty

### Check 1: Browser Console
Press `F12` and look for errors:

**Expected messages:**
```
ESP32 Web UI initialized
ESP32 Web UI ready
```

**Look for errors:**
- "Cannot read properties of null" → Element not found
- "builderCanvas is null" → Canvas not available
- "redrawCanvas is not defined" → JavaScript not loaded

### Check 2: Verify JavaScript Loaded
In browser console, type:
```javascript
typeof redrawCanvas
```
**Expected:** `"function"`
**If:** `"undefined"` → JavaScript not loaded, clear cache!

### Check 3: Check Canvas Element
In browser console:
```javascript
document.getElementById('builder-canvas')
```
**Expected:** `<canvas id="builder-canvas" width="480" height="272">`
**If:** `null` → HTML not loaded properly

### Check 4: Manual Redraw
Try forcing a redraw in console:
```javascript
redrawCanvas()
```
**Expected:** Grid appears on canvas
**If Error:** Check console error message

### Check 5: Check Builder Setup
```javascript
typeof setupBuilder
```
**Expected:** `"function"`

### Check 6: Verify Context
```javascript
builderCtx
```
**Expected:** Canvas rendering context object
**If:** `null` → Canvas not initialized

---

## Common Issues & Solutions

### Issue 1: "Canvas is blank/empty"
**Cause:** Old JavaScript cached
**Solution:** Hard refresh (`Ctrl + F5`) or use Incognito mode

### Issue 2: "Buttons don't work"
**Cause:** JavaScript errors or old code
**Solution:** 
1. Open console (`F12`)
2. Check for errors
3. Clear cache and reload

### Issue 3: "Elements don't appear when clicking buttons"
**Cause:** `redrawCanvas()` not being called
**Solution:** Check console for errors, verify JavaScript loaded

### Issue 4: "Can't select elements"
**Cause:** Mouse event handlers not attached
**Solution:** Verify `setupBuilder()` was called (check console)

### Issue 5: "Properties panel doesn't update"
**Cause:** `updatePropertiesPanel()` error
**Solution:** Check console for errors

### Issue 6: "Drag doesn't work"
**Cause:** Mouse event listeners not attached
**Solution:** 
1. Check console for errors
2. Verify canvas exists: `document.getElementById('builder-canvas')`

---

## Current File Status

### Uploaded Files:
```
data/
├── index.html      13.7 KB   362 lines  ✅ Contains Builder HTML
├── css/
│   └── style.css   13.5 KB   719 lines  ✅ Builder styles present
└── js/
    └── app.js      21.7 KB   581 lines  ✅ Fixed Builder JS
```

### Upload Status:
```
Firmware:    ✅ Uploaded (Exit Code 0)
Filesystem:  ✅ Uploaded (Exit Code 0)
SPIFFS:      ✅ Hash verified
```

---

## Expected Behavior After Fix

### On Page Load:
1. Dashboard shows by default
2. LCD Preview canvas shows dark background with "LCD Screen Preview"
3. Console shows: "ESP32 Web UI initialized" and "ESP32 Web UI ready"

### When Clicking Builder Link:
1. Builder page becomes visible
2. **Canvas grid draws immediately** (20px grid, 480×272)
3. Properties panel shows "Select an element"
4. All toolbar buttons are clickable

### When Adding Elements:
1. Click button
2. Element appears on canvas
3. Toast notification appears
4. Element is immediately visible

### When Interacting:
1. Click element → Blue border
2. Drag element → Follows mouse
3. Edit properties → Updates live
4. All changes instant feedback

---

## Quick Test Checklist

- [ ] Hard refresh browser (`Ctrl + F5`)
- [ ] Navigate to: `http://192.168.40.48/#builder`
- [ ] See 20px grid on canvas
- [ ] Click 📝 button → "Text" appears
- [ ] Click text → Blue border appears
- [ ] Drag text → Moves with mouse
- [ ] Properties show in right panel
- [ ] Change "Text" to "Hello"
- [ ] Click Apply → Text changes
- [ ] Click 🗑️ → Element deleted
- [ ] Toast notifications appear

---

## If Still Not Working

### 1. Check URL
Make sure you're accessing:
```
http://192.168.40.48/#builder
```
NOT:
```
http://192.168.40.48/#builder-card  ❌ (wrong)
```

### 2. Verify Upload
In browser, access JavaScript directly:
```
http://192.168.40.48/js/app.js
```
Check file size in browser:
- Should be around 21-22 KB
- Should contain "setupBuilder" function
- Should contain "redrawCanvas" function

### 3. Force Reload Everything
```javascript
// In browser console:
location.reload(true)
```

### 4. Check Serial Monitor
See if ESP32 reports any errors during page load

### 5. Try Different Browser
Test in Chrome, Firefox, or Edge to rule out browser caching issues

---

## Success Criteria

✅ **Working properly if:**
1. Grid visible immediately when opening Builder
2. Click buttons → Elements appear
3. Click elements → Blue selection border
4. Drag elements → They move
5. Edit properties → Changes apply
6. Toast notifications show
7. Save/load works
8. Delete works

❌ **Still broken if:**
1. Canvas is blank/white/empty
2. Buttons don't add elements
3. Can't select or drag
4. No toast notifications
5. Console shows JavaScript errors

---

## Final Notes

- JavaScript file size: **21.7 KB** (down from 22.2 KB)
- Code lines: **581 lines** (optimized)
- Key fix: Canvas redraws when switching to Builder page
- Upload status: ✅ **All files uploaded successfully**

**Next Action:** 
1. Clear browser cache (`Ctrl + F5`)
2. Navigate to Builder
3. Test adding elements
4. Report results

If canvas still empty after clearing cache, check browser console (F12) for errors and share the error messages!
