# LCD Builder - UI Improvements & Issues Found

## Critical Issues Found

### 1. ❌ **MISSING: Custom Page Deletion**
**Status**: NOT IMPLEMENTED
- Pages list shows "➕" button to add, but NO delete button
- Once a custom page is created, users can't delete it
- No API endpoint exists for deleting custom pages

**Fix**: 
- Add delete button (🗑️) to custom page items in the list
- Implement `/api/pages/delete` endpoint in backend
- Add `deleteCustomPage(index)` function in frontend

---

### 2. ❌ **MISSING: Page Import/File Upload**
**Status**: NOT IMPLEMENTED
- Can export overlay with "Save" button (downloads JSON)
- BUT can't import/load a previously exported JSON file
- Users must manually recreate pages

**Fix**:
- Add file input below page list
- Implement "Import Page" functionality
- Parse JSON and add as new custom page

---

### 3. ❌ **INCOMPLETE: Custom vs Overlay Mode Toggle**
**Status**: UI EXISTS BUT NON-FUNCTIONAL
- Radio buttons visible: "Custom" and "Overlay" modes
- No actual functionality connected
- Clicking toggle does nothing

**Fix**:
- Connect toggle to switch between edit modes
- Overlay mode should edit built-in page overlays only
- Custom mode should edit custom pages

---

### 4. ❌ **INCOMPLETE: Page Rename**
**Status**: NOT IMPLEMENTED  
- Can create pages but can't rename them
- Must delete and recreate to rename

**Fix**:
- Add right-click context menu on page items
- OR add pencil icon to rename
- Implement rename API endpoint

---

### 5. ⚠️ **ISSUE: Paste Button May Not Persist**
**Status**: PARTIALLY WORKING
- Paste button copies widget to canvas
- Canvas shows new widget
- BUT may not be persisting to device after paste

**Fix**:
- Add `persistCurrentPageChanges()` after paste
- Same fix as drag/drop operations

---

### 6. ⚠️ **INCOMPLETE: Shape Widget Type Selector**
**Status**: EXISTS BUT LIMITED
- Shape widget added to canvas
- Only supports one shape type (rect)
- No selector for circle, line, polygon, etc.

**Fix**:
- Add shape type selector in properties panel
- Support: rect, circle, line, poly
- Update drawing logic for each type

---

### 7. ⚠️ **INCOMPLETE: Widget Rotation & Scale**
**Status**: NOT IMPLEMENTED
- No rotation angle property
- No scale/stretch factor
- Limits layout flexibility

**Fix**:
- Add "Rotation (°)" number input
- Add "Scale (%)" number input
- Update drawing to apply transforms

---

### 8. ⚠️ **ISSUE: Live Preview**
**Status**: CHECKBOX EXISTS BUT MAY BE BROKEN
- Checkbox visible: "Live" preview option
- Should show real device data in preview
- May not be connected to actual live data

**Fix**:
- Verify live data binding works
- Ensure bindings pull from `state.lastStatus`
- Test with multiple widgets

---

### 9. ❌ **MISSING: Layer Management**
**Status**: NOT IMPLEMENTED
- No z-index controls
- No "Bring Forward" / "Send Backward"
- Overlapping elements can't be reordered
- Selection always picks top element

**Fix**:
- Add "⬆️ Front" and "⬇️ Back" buttons in toolbar
- Implement layer ordering in array
- Update draw order logic

---

### 10. ⚠️ **ISSUE: Limited Widget Types**
**Status**: ONLY 5 WIDGET TYPES
Currently available:
- Label
- Value
- Icon
- Shape
- Button

**Missing potentially useful**:
- Text Input (for user interaction)
- Slider
- Progress Bar
- Gauge
- Image/Bitmap

**Fix**:
- Add more widget presets
- Extend properties panel for each type

---

## Quick Fix Priority

### 🔴 **HIGH PRIORITY** (Users need these)
1. Custom page deletion - currently no way to clean up
2. Page import - currently can only export, not import
3. Paste persistence - paste may not save to device
4. Custom/Overlay mode - confusing UI suggests non-existent feature

### 🟡 **MEDIUM PRIORITY** (Nice to have)
5. Page rename - workaround: delete + recreate
6. Layer management - needed for complex layouts
7. Shape type selector - currently only rectangles

### 🟢 **LOW PRIORITY** (Polish/Enhancement)
8. Widget rotation/scale - nice but not essential
9. Live preview fix - verify it works
10. More widget types - good for future expansion

---

## Testing Checklist

After fixes:
- [ ] Create custom page
- [ ] Add widgets
- [ ] Save widgets persisted to device
- [ ] Rename page
- [ ] Delete page
- [ ] Export page
- [ ] Import page back
- [ ] Switch between Custom/Overlay modes
- [ ] Test all alignment tools
- [ ] Test undo/redo after paste
- [ ] Verify live preview shows real data
- [ ] Test layer management

