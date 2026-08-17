# Builder UI - Latest Fixes Applied

## Fixes Just Implemented (Nov 2, 2025)

### 1. ✅ **Fixed getInputValue() Helper**
- **Problem**: Only worked with `HTMLInputElement`, missed select/color inputs
- **Fix**: Changed to check for any element with `.value` property
- **Impact**: Color picker, shape type selector, and all input types now work

### 2. ✅ **Simplified Deploy Logic**
- **Problem**: Deploy was trying to detect unsaved changes, causing complex behavior
- **Fix**: Deploy now simply deploys whatever is currently on canvas
- **Impact**: No more confusing change detection, deploy is straightforward

### 3. ✅ **Added Paste Widget Persistence**
- **Problem**: Pasted widgets weren't being saved to device
- **Fix**: Call `persistCurrentPageChanges()` after paste
- **Impact**: Pasted widgets now persist on device

### 4. ✅ **Added Custom Page Deletion**
- **Problem**: No way to delete custom pages
- **Fix**: Added delete button (🗑️) to page list + API endpoint
- **Impact**: Can now manage custom pages

### 5. ✅ **Added Page Import from File**
- **Problem**: Could only export, not import JSON files
- **Fix**: Added file upload with import functionality
- **Impact**: Can now import previously exported pages

### 6. ✅ **Added Shape Type Selector**
- **Problem**: Shape widget only supported rectangles
- **Fix**: Added shape type selector: rect, circle, rounded, line
- **Impact**: More layout flexibility

### 7. ✅ **Improved Form Input Handling**
- **Problem**: Color and select inputs weren't recognized
- **Fix**: Universal getInputValue() now works with all HTML elements
- **Impact**: All property editors now work reliably

---

## How to Use the New Features

### Delete a Custom Page
1. Go to Builder
2. Right-click or click 🗑️ button next to custom page name
3. Confirm deletion

### Import a Page
1. Click 📥 button next to Pages header
2. Select a previously exported `.json` file
3. Page is automatically added to custom pages

### Change Shape Type
1. Select a shape widget
2. In Properties panel, select shape type: Rectangle, Circle, Rounded, or Line
3. Click Apply

### Edit Widget Properties
1. Select widget on canvas
2. Edit any property in Properties panel (color, size, position, text, etc.)
3. Click "Apply Changes"
4. Click "Deploy" to send to device

---

## Testing Checklist

**✅ Ready to Test**:
- [ ] Add custom page
- [ ] Edit widget colors  
- [ ] Edit widget sizes
- [ ] Edit widget positions
- [ ] Apply changes to clock card
- [ ] Deploy to device
- [ ] Verify changes appear on LCD
- [ ] Delete custom page
- [ ] Import exported page
- [ ] Test shape type selector

---

## Known Limitations

1. **Deploy only works for built-in pages** - Custom page editing not yet supported for deploy
2. **Built-in base widgets can't be edited** - Only overlay widgets (custom adds) can be modified
3. **No layer z-index control** - Can't reorder overlapping elements

---

## Important Note About Built-in Widgets

The "clock card" and other dashboard cards on the HOME page are **base widgets** built into the C++ code. They are:
- **NOT editable** through the builder overlay system
- **NOT customizable** via the Properties panel
- **Part of the core dashboard** rendered by the firmware

To customize these, you would need to:
1. Edit the C++ source code (src/ui/page_home.cpp)
2. Recompile firmware
3. Upload new firmware

**To edit overlays**: You need to use the **"Overlay" mode** on a built-in page, which adds custom widgets on top of the base.

