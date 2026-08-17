# 🧪 QUICK MANUAL API TEST

Run these commands in your browser console while on the builder page to test the API directly.

## Test 1: Can you reach the device?

```javascript
fetch('/api/status')
  .then(r => r.json())
  .then(d => console.log('Status:', d))
  .catch(e => console.error('FAILED:', e.message))
```

**Expected**: Logs device info
**Bad**: Error about network or 404

---

## Test 2: Get all pages

```javascript
fetch('/api/pages/all')
  .then(r => r.json())
  .then(d => console.log('Pages:', JSON.stringify(d, null, 2)))
  .catch(e => console.error('FAILED:', e.message))
```

**Expected**: Shows all pages with builtins array
**Bad**: Empty response or error

---

## Test 3: Get base layout for HOME (page 0)

```javascript
fetch('/api/pages/base?base=0')
  .then(r => r.json())
  .then(d => {
    console.log('Base HOME widgets:', d.widgets.length, 'items');
    d.widgets.forEach(w => console.log(`  - ${w.id}: type=${w.type} color=${w.color} x=${w.x} y=${w.y}`));
  })
  .catch(e => console.error('FAILED:', e.message))
```

**Expected**: Shows list of widgets with colors like "#ffffff"
**Bad**: Empty array or missing colors

**MOST IMPORTANT**: Look at the output - does each widget show a COLOR value?
- YES colors shown → Backend working ✓
- NO colors shown → Backend broken ✗

---

## Test 4: Get overlay for HOME

```javascript
fetch('/api/pages/overlay?base=0')
  .then(r => r.json())
  .then(d => console.log('Overlay:', JSON.stringify(d, null, 2)))
  .catch(e => console.error('FAILED:', e.message))
```

**Expected**: `{"widgets":[]}`  (empty)
**Bad**: Error

---

## Test 5: Get presets

```javascript
fetch('/api/presets/list')
  .then(r => r.json())
  .then(d => console.log('Presets:', JSON.stringify(d, null, 2)))
  .catch(e => console.error('FAILED:', e.message))
```

**Expected**: `{"presets":[]}`
**Bad**: JSON parse error or empty response

---

## Test 6: Check builder state

```javascript
// Check if state is loaded
console.log('Builder state exists:', typeof state === 'object');
console.log('Base elements loaded:', state?.builder?.baseElements?.length ?? 0);
console.log('Overlay elements loaded:', state?.builder?.overlayElements?.length ?? 0);
console.log('Show base enabled:', state?.builder?.showBase ?? false);
console.log('Current page:', state?.builder?.currentPage ?? 'none');
```

**Expected**: Shows non-zero for baseElements, true for showBase
**Bad**: 0 for baseElements or false for showBase

---

## How to Use

1. Open builder in browser
2. Press F12 to open Developer Tools  
3. Go to Console tab
4. Copy ONE test above
5. Paste it in console (type the command, don't copy multiple lines)
6. Press Enter
7. **Read the output carefully**

---

## Critical Questions After Running Tests

### After Test 3 (Base layout):
- **Question**: Do all widgets show a COLOR value?
- **YES** → Backend is working correctly! Problem is in browser
- **NO** → Backend broken. Need to rebuild firmware

### After Test 6 (Builder state):
- **Question**: Is baseElements > 0?
- **YES** → Data loaded. Should be visible on canvas
- **NO** → Data didn't load. Reload page

---

## Report Format

When you run these tests, tell me:

```
Test 1 (status): SUCCESS / FAILED - [error message]
Test 2 (pages): SUCCESS / FAILED - [error or num pages]
Test 3 (base layout): SUCCESS / FAILED - [number of widgets] - [do colors show? YES/NO]
Test 4 (overlay): SUCCESS / FAILED - [error or "empty"]
Test 5 (presets): SUCCESS / FAILED - [error or "empty"]
Test 6 (state): baseElements=[number], showBase=[true/false]

MOST IMPORTANT: In Test 3, does each widget show "color=#..." ? YES / NO
```

This will tell me EXACTLY where the problem is! 🎯

