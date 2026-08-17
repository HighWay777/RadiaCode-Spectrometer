# 🔧 Debugging: Apply Changes Not Working

## The Problem
You change color/size of a widget → Click "Apply Changes" → No changes appear on LCD

## Expected Flow (Step-by-Step)

### Step 1: Click on a Widget
- You click on a cyan box (e.g., Weather Panel)
- Builder should:
  - ✓ Highlight the widget with selection box
  - ✓ Show properties on the RIGHT panel
  - ✓ Display current color, size, position

### Step 2: Change a Property
- You change color to RED in the color picker
- Browser shows RED in the preview
- ✓ This is LOCAL ONLY - not saved yet

### Step 3: Click "Apply Changes"
- THIS SHOULD:
  1. Record the new color in overlayElements array
  2. Redraw canvas (you see RED box)
  3. Call `persistCurrentPageChanges()`
  4. Send POST request to: `POST /api/pages/overlay?base=0`
  5. Body contains: `{ base: 0, widgets: [...] }`
  6. Device receives and saves to NVS flash
  7. Device UI redraws with new colors

### Step 4: LCD Updates
- LCD should show RED weather panel
- Change persists after restart

---

## Where It Could Break

### ❌ PROBLEM AREA 1: Missing Click on Widget
**Symptom**: Properties panel stays empty, can't edit anything

**Check**:
1. Open Builder
2. Select "Home" page
3. Look for cyan boxes on canvas
4. **Try clicking on them** - click in the middle of each cyan box
5. Properties should appear on RIGHT

**Fix if broken**: Reload browser (Ctrl+F5)

---

### ❌ PROBLEM AREA 2: Properties Not Showing
**Symptom**: Widget selected but no properties appear

**Check**:
1. Open browser console (F12)
2. Click on a widget
3. Look for errors in console
4. Check if properties HTML appears

**Expected console output**: Nothing (no errors)

---

### ❌ PROBLEM AREA 3: "Apply Changes" Doesn't Work
**Symptom**: Widget selected, properties visible, but clicking Apply does nothing

**Debugging Steps**:

#### Step A: Check Browser Console
1. Open F12 Developer Tools → Console tab
2. Click on a widget
3. Change color to RED
4. **Click "Apply Changes"**
5. **Look for messages** (should say "Saving..." then "✓ Changes saved to device")
6. **Look for errors** (red text in console)

**What you should see**:
```
✓ Changes saved to device  (green toast)
```

**If you see ERROR**:
```
Failed to save: [error message]
```
Then there's a network issue.

---

#### Step B: Check Network Activity
1. Open F12 DevTools → Network tab
2. Click on a widget, change color, click Apply
3. **Look for POST request** to `/api/pages/overlay`
4. Click on that request
5. Check:
   - **Request tab**: Shows `base: 0, widgets: [...]`
   - **Response tab**: Should be empty or `{"status":"ok"}`
   - **Status**: Should be `200 OK`

**If response is ERROR** (404, 500, etc):
- Device API endpoint is broken
- Need to rebuild firmware

**If NO request appears**:
- JavaScript never called the save function
- Check console for errors

---

#### Step C: Check Device Actually Received It
1. Open device **Serial Monitor** (PlatformIO)
2. Make a change and click Apply
3. Look for log output

**Should see**: 
```
POST /api/pages/overlay?base=0
Saving overlay for page 0...
Overlay saved successfully
```

**If you see ERROR**:
```
Failed to parse JSON
Invalid widget array
```
Then there's a data format issue.

**If you see NOTHING**:
- Device didn't receive the request
- Check WiFi/USB connection
- Try accessing `http://device-ip/api/status` to verify device is reachable

---

### ❌ PROBLEM AREA 4: LCD Doesn't Update
**Symptom**: Save succeeds (console shows ✓) but LCD doesn't change

**Check**:
1. Device received the save (check serial logs)
2. But LCD still shows old widget
3. This means: **LCD didn't redraw with new overlay data**

**Possible causes**:
- Overlay data wasn't loaded by LCD UI code
- LCD UI code has bug in rendering
- Device needs restart

**Try this**:
1. Power cycle device (unplug for 10 seconds)
2. Open builder again
3. See if changes persisted (they should)

---

## Complete Testing Checklist

Use this **exact** sequence to test:

### Test 1: Can I Select a Widget?
```
1. Open Builder
2. Load HOME page
3. Do you see cyan boxes? YES/NO
4. Click on Weather Panel (large cyan box at top)
5. Does properties panel appear on RIGHT? YES/NO
6. Does it show position, size, color fields? YES/NO
```

**If NO at any step**: Something wrong with widget rendering
→ Run: `Ctrl+F5` refresh and try again

---

### Test 2: Can I Change Properties?
```
1. With Weather Panel selected
2. Change Width field from current to 300
3. Does canvas widget get BIGGER? YES/NO
4. Change Color to RED (click color picker)
5. Does canvas widget turn RED? YES/NO
```

**If NO**: Local canvas updates not working
→ Check console for errors

---

### Test 3: Can I Apply Changes?
```
1. With Weather Panel showing changes (bigger, red)
2. Click "Apply Changes" button
3. Does toast say "Saving..."? YES/NO
4. Does toast then say "✓ Changes saved..."? YES/NO
5. Check console (F12) for errors? YES ERROR / NO OK
```

**If ERROR**: Network/API issue
→ Check network tab for failed requests

---

### Test 4: Did Device Receive It?
```
1. Open Serial Monitor
2. Repeat Test 3 steps
3. Do you see "POST /api/pages/overlay"? YES/NO
4. Do you see "Overlay saved successfully"? YES/NO
```

**If NO**: Device didn't get the request
→ Check WiFi connection

---

### Test 5: Did LCD Update?
```
1. Repeat all previous tests
2. Look at LCD display
3. Does Weather Panel appear BIGGER? YES/NO
4. Does Weather Panel appear RED? YES/NO
```

**If NO**: LCD rendering issue
→ Power cycle device

---

## Step-by-Step Fix Process

### If Test 1 FAILS (Can't select widgets):
1. Browser refresh: `Ctrl+F5`
2. Reload builder
3. If still fails: **Report this issue**

### If Test 2 FAILS (Properties don't update locally):
1. Check console: `F12` → Console tab
2. Look for errors
3. Report error message

### If Test 3 FAILS (Apply doesn't send data):
1. Check console for error message
2. Check Network tab → look for POST request
3. Report what you see

### If Test 4 FAILS (Device didn't receive):
1. Check WiFi connection: Can you access other pages?
2. Try `/api/status` endpoint - should work
3. Restart device
4. Try again

### If Test 5 FAILS (LCD didn't update):
1. Power cycle device (10 seconds unplugged)
2. Try again
3. If still fails: **Report this issue**

---

## Quick Troubleshooting Commands

### Open Device Serial Monitor
In VSCode: 
- Terminal → New Terminal
- Run: `pio device monitor --port COM24`

### Check Device is Online
```
curl http://192.168.x.x/api/status
```
(Replace IP with your device IP)

### Force Device Restart
Unplug for 10 seconds, plug back in

### Check Builder Cache
```
Browser F12 → Application → Local Storage
Look for builder state
```

---

## What Should Happen (Complete Sequence)

```
YOU: Click Weather Panel widget
  ↓
BUILDER: Highlights cyan box, shows properties
  ↓
YOU: Change color to RED in color picker
  ↓
BUILDER: Shows RED in preview
  ↓
YOU: Click "Apply Changes"
  ↓
BUILDER: Shows "Saving..." toast
  ↓
BUILDER: Sends POST to /api/pages/overlay with RED color
  ↓
DEVICE: Receives POST, saves overlay to NVS flash
  ↓
DEVICE: Logs "Overlay saved successfully"
  ↓
BUILDER: Shows "✓ Changes saved" toast
  ↓
DEVICE LCD: Redraws with RED weather panel
  ↓
YOU: See RED weather panel on LCD! ✓
```

---

## Report Template

If something doesn't work, tell me:

```
TEST RESULTS:
- Test 1 (Select widget): PASS / FAIL
- Test 2 (Change properties): PASS / FAIL  
- Test 3 (Apply changes): PASS / FAIL
- Test 4 (Device received): PASS / FAIL
- Test 5 (LCD updated): PASS / FAIL

CONSOLE ERRORS (F12):
[paste any red error text]

NETWORK ACTIVITY:
[describe what you see in Network tab]

SERIAL LOG OUTPUT:
[paste what device prints]

EXPECTED vs ACTUAL:
Expected: [what should happen]
Actual: [what did happen]
```

---

## Most Common Issues

### Issue 1: "No changes appear on LCD"
**Usually caused by**: LCD not redrawn after save
**Fix**: Power cycle device

### Issue 2: "Apply shows error"
**Usually caused by**: Network request failed  
**Fix**: Check device IP, verify WiFi connected, check browser console

### Issue 3: "Can't see cyan boxes"
**Usually caused by**: showBase not enabled
**Fix**: Check "Show Base" checkbox in builder UI

### Issue 4: "Toast doesn't show"
**Usually caused by**: Browser cache issue
**Fix**: Refresh: `Ctrl+F5`

---

## Next Steps

1. **Run all 5 tests** above
2. **Tell me which test FAILS**
3. **Report console errors/network activity**
4. I'll provide specific fix

**You're almost there! We just need to pinpoint where the break is.** 🔍

