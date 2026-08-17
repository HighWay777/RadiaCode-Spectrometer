# Debug Guide - Understanding LCD Builder Data Structure

## How to Get Debug Information

### Method 1: Browser Console (Recommended)

1. Open the builder page in your browser
2. Open Developer Tools: **F12** or **Ctrl+Shift+I**
3. Go to **Console** tab
4. Run these commands one by one:

#### Check what pages are available:
```javascript
console.log('Overlay Elements:', state.builder.overlayElements);
console.log('Base Elements:', state.builder.baseElements);
```

#### Get details about Status Bar specifically:
```javascript
const statusBar = state.builder.overlayElements.find(el => el.text === 'Status Bar');
console.log('Status Bar element:', JSON.stringify(statusBar, null, 2));
```

#### Check the structure:
```javascript
state.builder.overlayElements.forEach((el, idx) => {
  console.log(`[${idx}] Type: ${el.type}, Text: ${el.text ?? 'N/A'}, Has children: ${!!el.children}`);
  if (el.children) {
    console.log(`  Children:`, el.children);
  }
});
```

#### Get full export of current page:
```javascript
console.log(JSON.stringify(state.builder.overlayElements, null, 2));
```

#### Check element at point (test selection):
```javascript
console.log('Last Status Data:', state.lastStatus);
console.log('Available bindings:', Object.keys(state.lastStatus ?? {}));
```

### Method 2: Check Backend Directly

The overlay data is stored in SPIFFS. You can check what's actually saved:

#### Using curl (from command line):
```bash
curl http://192.168.40.16/api/pages/overlay?base=0
```

This returns the JSON structure that gets loaded.

---

## What to Look For

When you run these commands, look for:

### 1. Does Status Bar have children?
- ✅ YES: `"children": [...]` array exists in Status Bar object
- ❌ NO: No children array, all elements are flat

### 2. What type is Status Bar?
- Look at the `"type"` field
- Common types: "label", "value", "button", "group", "panel", "container"

### 3. Are child elements separate?
- Count total elements in overlay array
- Does the sum make sense with Status Bar + children?
- Are child elements physically positioned relative to parent?

### 4. Child element structure:
- If children exist, what properties do they have?
- Do they have `x`, `y` coordinates?
- Are x/y absolute or relative to parent?

---

## Share This Information

Once you run these debug commands, please copy/paste the output and share:

1. **Full structure** of Status Bar element (from `JSON.stringify()`)
2. **Count of total elements** vs what you see in UI
3. **Any errors** in the browser console
4. **How child elements are positioned** (absolute vs relative to Status Bar)

---

## Expected Output Examples

###If Status Bar has children (hierarchical):
```javascript
{
  "type": "group",
  "text": "Status Bar",
  "x": 10,
  "y": 238,
  "w": 460,
  "h": 24,
  "children": [
    {"type": "label", "text": "Status", "x": 10, "y": 238, ...},
    {"type": "value", "text": "Online", "x": 100, "y": 238, ...},
    ...
  ]
}
```

### If Status Bar is flat (all siblings):
```javascript
{
  "type": "label",
  "text": "Status Bar",
  "x": 10,
  "y": 238,
  "w": 460,
  "h": 24
}
// Followed by separate elements at same y position
```

---

## Once I Have This Info

I can properly fix:
1. ✅ Canvas rendering (to show all elements)
2. ✅ Element selection (to click specific children)
3. ✅ Properties editing (to edit child properties)
4. ✅ Nested structure UI (if needed)

---

## Quick Check Right Now

Just do this quickly:

1. Open builder in browser
2. Press **F12**
3. Go to **Console** tab
4. Copy/paste this one line:
```javascript
console.log(JSON.stringify(state.builder.overlayElements.filter(el => el.text === 'Status Bar' || el.x === 10), null, 2));
```

5. Copy the output and send it to me

This will show me the Status Bar and nearby elements!

