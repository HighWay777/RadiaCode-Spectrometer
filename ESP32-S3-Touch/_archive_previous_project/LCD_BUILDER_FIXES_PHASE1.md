# LCD Builder - Phase 1 Implementation Guide (Critical Fixes)

**Objective**: Fix 7 critical bugs to make builder functional  
**Estimated Time**: 12-16 hours  
**Success Criteria**: All items on testing checklist pass  

---

## Bug #1: Fix Properties Panel Form Apply

**File**: `data/js/builder.js`  
**Lines**: 650-820 (buildPropertyForm & applyPropertiesFromForm)  
**Issue**: Form doesn't properly apply changes to widget

### Current Code Problem
```javascript
// Line 768-770 - Problem: If getValue returns null, uses 0 as default
// instead of keeping original value
const getValue = (id) => form.querySelector(`#${id}`);
widget.x = Number(getValue('prop-x')?.value) || 0;
widget.y = Number(getValue('prop-y')?.value) || 0;
```

### Solution

**Replace the `applyPropertiesFromForm()` function** with:

```javascript
function applyPropertiesFromForm(form, widget) {
  // Helper to safely get input value
  const getInputValue = (id) => {
    const input = form.querySelector(`#${id}`);
    return input instanceof HTMLInputElement ? input.value : null;
  };

  // Position - validate bounds
  const xVal = parseFloat(getInputValue('prop-x'));
  const yVal = parseFloat(getInputValue('prop-y'));
  if (!isNaN(xVal)) widget.x = Math.max(0, Math.min(xVal, LCD_WIDTH - 50));
  if (!isNaN(yVal)) widget.y = Math.max(0, Math.min(yVal, LCD_HEIGHT - 30));

  // Size - validate positive values
  const wVal = parseFloat(getInputValue('prop-w'));
  const hVal = parseFloat(getInputValue('prop-h'));
  if (!isNaN(wVal) && wVal > 0) {
    widget.w = wVal;
    if ('width' in widget) widget.width = wVal;
  }
  if (!isNaN(hVal) && hVal > 0) {
    widget.h = hVal;
    if ('height' in widget) widget.height = hVal;
  }

  // Text properties
  const textVal = getInputValue('prop-text');
  if (textVal !== null) widget.text = textVal;

  const fontVal = parseFloat(getInputValue('prop-font'));
  if (!isNaN(fontVal) && fontVal > 0) widget.fontSize = fontVal;

  // Colors
  const colorVal = getInputValue('prop-color');
  if (colorVal) widget.color = colorVal;

  const textColorVal = getInputValue('prop-text-color');
  if (textColorVal) widget.textColor = textColorVal;

  // Icon
  const iconVal = getInputValue('prop-icon');
  if (iconVal !== null) widget.icon = iconVal;

  // Radius (for buttons)
  const radiusVal = parseFloat(getInputValue('prop-radius'));
  if (!isNaN(radiusVal) && radiusVal >= 0) widget.radius = radiusVal;

  // Binding
  const bindingVal = getInputValue('prop-binding');
  widget.binding = bindingVal || null;

  // Update UI and notify user
  redrawCanvas();
  renderPreview();
  updatePropertiesPanel({ silent: true }); // Update form with actual values
  showToast('Widget properties updated', 'success');
}
```

### Testing
1. Select a widget
2. Change X position in properties
3. Click "Apply Changes"
4. Verify widget moved on canvas
5. Repeat for other properties (Y, Width, Height, Text, Color)

---

## Bug #2: Implement Custom Page Persistence

**File**: `data/js/builder.js`  
**Lines**: 312-328 (Create page from button)  
**Issue**: Custom pages not saved to backend

### Current Code Problem
```javascript
// Lines 314-322 - Page added to memory but never saved to device
document.getElementById('page-add')?.addEventListener('click', () => {
  const name = prompt('New custom page name?');
  if (!name) return;
  if (!pagesCache) pagesCache = { pages: [] };
  pagesCache.pages = pagesCache.pages || [];
  pagesCache.pages.push({ name, widgets: [] });
  loadPagesIntoUi();
  // MISSING: Save to backend!
  showToast(`Created custom page "${name}"`, 'success');
});
```

### Solution

**Add this function** (add after the page loading functions, around line 190):

```javascript
// Save custom pages to backend
async function saveCustomPages() {
  try {
    if (!pagesCache) return;
    
    // Show saving indicator
    const status = document.getElementById('save-status');
    if (status) status.textContent = 'Saving...';

    // POST to backend
    const response = await fetch('/api/pages', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(pagesCache)
    });

    if (!response.ok) {
      throw new Error(`Save failed: ${response.statusText}`);
    }

    // Update UI to show unsaved changes
    updateSaveStatus();
    showToast('Custom pages saved to device', 'success');
  } catch (error) {
    console.error('Save failed', error);
    showToast(`Failed to save pages: ${error.message}`, 'error');
  }
}

// Update save status display
function updateSaveStatus() {
  const status = document.getElementById('save-status');
  if (!status) return;
  
  const count = state.builder.overlayElements?.length ?? 0;
  if (count === 0) {
    status.textContent = 'Empty';
  } else {
    status.textContent = `${count} widgets`;
  }
}
```

**Replace the page-add button handler** (lines 314-322) with:

```javascript
document.getElementById('page-add')?.addEventListener('click', async () => {
  const name = prompt('New custom page name?');
  if (!name) return;

  // Validate name
  if (name.length > 50) {
    showToast('Page name too long (max 50 chars)', 'error');
    return;
  }

  if (!pagesCache) pagesCache = { pages: [] };
  pagesCache.pages = pagesCache.pages || [];

  // Check for duplicate names
  if (pagesCache.pages.some(p => p.name === name)) {
    showToast('A page with that name already exists', 'error');
    return;
  }

  // Add page to cache
  pagesCache.pages.push({ name, widgets: [] });
  
  // Update UI immediately
  loadPagesIntoUi();
  showToast(`Created page "${name}"`, 'info');
  
  // Save to backend
  await saveCustomPages();
});
```

### Testing
1. Click "Add Page"
2. Enter page name "TestPage"
3. Page appears in list
4. Refresh page in browser
5. Verify "TestPage" still exists
6. Reboot ESP32
7. Verify page persists on device

---

## Bug #3: Implement Palette Drag-Drop to Canvas

**File**: `data/js/builder.js`  
**Lines**: 197-230 (bindCanvasEvents function)  
**Issue**: No drop handler on canvas for palette drag-drop

### Current Code Problem
```javascript
// Lines 308-312 - Drag handlers set pendingWidgetType but nothing uses it
item.addEventListener('dragstart', () => {
  state.builder.drag.pendingWidgetType = type;
});
item.addEventListener('dragend', () => {
  state.builder.drag.pendingWidgetType = null;
});
// Canvas never has dragover/drop handlers!
```

### Solution

**Find the bindCanvasEvents() function** and add these handlers after the mousemove handler (around line 230):

```javascript
function bindCanvasEvents() {
  const builder = state.builder;
  const canvas = builder.canvas;
  if (!canvas) return;

  canvas.addEventListener('mousedown', (event) => {
    const point = canvasPoint(event);
    const idx = findElementAtPoint(point.x, point.y, state.builder.overlayElements);
    if (idx !== null) {
      builder.selectedIndex = idx;
      builder.drag.active = true;
      builder.drag.offsetX = point.x - (state.builder.overlayElements[idx].x ?? 0);
      builder.drag.offsetY = point.y - (state.builder.overlayElements[idx].y ?? 0);
      updatePropertiesPanel();
      redrawCanvas();
      return;
    }
    builder.selectedIndex = null;
    updatePropertiesPanel();
    redrawCanvas();
  });

  canvas.addEventListener('mousemove', (event) => {
    if (!builder.drag.active) return;
    const widget = selectedWidget();
    if (!widget) return;
    const point = canvasPoint(event);
    widget.x = Math.max(0, Math.min(point.x - builder.drag.offsetX, LCD_WIDTH - 50));
    widget.y = Math.max(0, Math.min(point.y - builder.drag.offsetY, LCD_HEIGHT - 30));
    updatePropertiesPanel({ silent: true });
    redrawCanvas();
    renderPreview();
  });

  canvas.addEventListener('mouseup', () => {
    builder.drag.active = false;
    renderPreview();
  });

  canvas.addEventListener('mouseleave', () => {
    builder.drag.active = false;
  });

  // ADD THESE NEW HANDLERS FOR DRAG-DROP FROM PALETTE:
  
  canvas.addEventListener('dragover', (event) => {
    if (!state.builder.drag.pendingWidgetType) return;
    event.preventDefault();
    event.stopPropagation();
    canvas.style.backgroundColor = '#f0fdf4'; // Light green feedback
  });

  canvas.addEventListener('dragleave', (event) => {
    if (event.target !== canvas) return;
    canvas.style.backgroundColor = ''; // Reset background
  });

  canvas.addEventListener('drop', (event) => {
    if (!state.builder.drag.pendingWidgetType) return;
    event.preventDefault();
    event.stopPropagation();
    
    canvas.style.backgroundColor = ''; // Reset background

    // Get drop position
    const rect = canvas.getBoundingClientRect();
    const x = (event.clientX - rect.left) / builder.zoom;
    const y = (event.clientY - rect.top) / builder.zoom;

    // Add widget at drop position
    addWidgetToOverlay(state.builder.drag.pendingWidgetType, x, y);
    
    showToast(`Widget added to canvas`, 'success');
  });

  // ... rest of existing bindCanvasEvents code ...
}
```

### Testing
1. Open builder
2. Load a builtin page
3. Drag "Label" widget from palette to canvas
4. Verify widget appears at drop location
5. Repeat with other widget types (Icon, Button, Shape, Value)
6. Try dragging to different positions

---

## Bug #4: Implement Delete/Copy/Paste

**File**: `data/js/builder.js`  
**Lines**: 240-280 (Toolbar button bindings)  
**Issue**: Button handlers are empty

### Current Code Problem
```javascript
// Lines 258-268 - Event listeners but empty callbacks
document.getElementById('btn-delete')?.addEventListener('click', () => {
  // EMPTY!
});
document.getElementById('btn-copy')?.addEventListener('click', () => {
  // EMPTY!
});
document.getElementById('btn-paste')?.addEventListener('click', () => {
  // EMPTY!
});
```

### Solution

**Replace the toolbar bindings** (lines 240-280) with:

```javascript
function bindToolbarActions() {
  // Delete button
  document.getElementById('btn-delete')?.addEventListener('click', () => {
    const widget = selectedWidget();
    if (!widget) {
      showToast('Select a widget to delete', 'error');
      return;
    }

    const confirmed = confirm('Delete selected widget?');
    if (!confirmed) return;

    state.builder.overlayElements.splice(state.builder.selectedIndex, 1);
    state.builder.selectedIndex = null;
    
    updatePropertiesPanel();
    redrawCanvas();
    renderPreview();
    showToast('Widget deleted', 'success');
  });

  // Copy button
  document.getElementById('btn-copy')?.addEventListener('click', () => {
    const widget = selectedWidget();
    if (!widget) {
      showToast('Select a widget to copy', 'error');
      return;
    }

    // Deep clone the widget
    state.builder.clipboard = cloneWidget(widget);
    showToast('Widget copied to clipboard', 'info');
  });

  // Paste button
  document.getElementById('btn-paste')?.addEventListener('click', () => {
    if (!state.builder.clipboard) {
      showToast('Nothing in clipboard to paste', 'error');
      return;
    }

    // Clone from clipboard and offset position
    const clone = cloneWidget(state.builder.clipboard);
    clone.x = (clone.x ?? 0) + 10; // Offset by 10px so not exactly on top
    clone.y = (clone.y ?? 0) + 10;

    // Keep within bounds
    const w = clone.w ?? clone.width ?? 100;
    const h = clone.h ?? clone.height ?? 30;
    if (clone.x + w > LCD_WIDTH) clone.x = LCD_WIDTH - w;
    if (clone.y + h > LCD_HEIGHT) clone.y = LCD_HEIGHT - h;

    state.builder.overlayElements.push(clone);
    state.builder.selectedIndex = state.builder.overlayElements.length - 1;

    updatePropertiesPanel();
    redrawCanvas();
    renderPreview();
    showToast('Widget pasted', 'success');
  });

  // Undo button
  document.getElementById('btn-undo')?.addEventListener('click', () => {
    if (!state.builder.undoStack || state.builder.undoStack.length === 0) {
      showToast('Nothing to undo', 'info');
      return;
    }

    // Pop from undo stack
    const previous = state.builder.undoStack.pop();
    
    // Push current to redo stack
    if (!state.builder.redoStack) state.builder.redoStack = [];
    state.builder.redoStack.push(cloneWidgets(state.builder.overlayElements));

    // Restore previous state
    state.builder.overlayElements = cloneWidgets(previous);
    state.builder.selectedIndex = null;

    redrawCanvas();
    renderPreview();
    updatePropertiesPanel();
    showToast('Undo', 'info');
  });

  // Redo button
  document.getElementById('btn-redo')?.addEventListener('click', () => {
    if (!state.builder.redoStack || state.builder.redoStack.length === 0) {
      showToast('Nothing to redo', 'info');
      return;
    }

    // Pop from redo stack
    const next = state.builder.redoStack.pop();
    
    // Push current to undo stack
    if (!state.builder.undoStack) state.builder.undoStack = [];
    state.builder.undoStack.push(cloneWidgets(state.builder.overlayElements));

    // Restore next state
    state.builder.overlayElements = cloneWidgets(next);
    state.builder.selectedIndex = null;

    redrawCanvas();
    renderPreview();
    updatePropertiesPanel();
    showToast('Redo', 'info');
  });

  // Existing toolbar actions continue...
  // (Align buttons, Zoom controls, etc.)
  // ...
}
```

### Testing
1. Add a widget to canvas
2. Select it
3. Click Copy
4. Verify "Widget copied" message
5. Click Paste
6. Verify cloned widget appears offset
7. Click Delete
8. Verify confirmation dialog
9. Confirm delete
10. Verify widget removed

---

## Bug #5: Implement Undo/Redo with History Tracking

**File**: `data/js/builder.js`  
**Lines**: 25-40 (initBuilder) and throughout  
**Issue**: No state snapshot recording

### Current Code Problem
Undo/Redo stacks exist but are never populated with state snapshots.

### Solution

**Modify initBuilder()** to initialize undo/redo:

```javascript
// Add this in the state initialization section (around line 25)
if (!state.builder.undoStack) state.builder.undoStack = [];
if (!state.builder.redoStack) state.builder.redoStack = [];
if (!state.builder.clipboard) state.builder.clipboard = null;
```

**Add this helper function** (around line 45, before bindCanvasEvents):

```javascript
// Record current state for undo/redo
function recordUndoState() {
  if (!state.builder.undoStack) state.builder.undoStack = [];
  if (!state.builder.redoStack) state.builder.redoStack = [];

  // Add current state to undo stack (max 20 states)
  state.builder.undoStack.push(cloneWidgets(state.builder.overlayElements));
  if (state.builder.undoStack.length > 20) {
    state.builder.undoStack.shift(); // Remove oldest if over limit
  }

  // Clear redo stack when new action happens
  state.builder.redoStack = [];
}
```

**Call `recordUndoState()` before modifications:**
- In `addWidgetToOverlay()` - add line after line 378: `recordUndoState();`
- In `applyPropertiesFromForm()` - add line before modifications
- In delete handler - add line before splice
- In paste handler - add line before push

### Testing
1. Add widget 1
2. Add widget 2
3. Move widget 1
4. Click Undo
5. Verify move reversed
6. Click Undo again
7. Verify widget 2 removed
8. Click Redo
9. Verify widget 2 reappears

---

## Bug #6: Fix Canvas Drag-Drop Position Tracking

**File**: `data/js/builder.js`  
**Lines**: 197-280 (bindCanvasEvents - mousemove handler)  
**Issue**: Widget doesn't follow cursor during drag

### Solution

See Bug #3 above - the mousemove handler was added as part of the comprehensive bindCanvasEvents fix. It includes:

```javascript
canvas.addEventListener('mousemove', (event) => {
  if (!builder.drag.active) return;
  const widget = selectedWidget();
  if (!widget) return;
  const point = canvasPoint(event);
  widget.x = Math.max(0, Math.min(point.x - builder.drag.offsetX, LCD_WIDTH - 50));
  widget.y = Math.max(0, Math.min(point.y - builder.drag.offsetY, LCD_HEIGHT - 30));
  updatePropertiesPanel({ silent: true });
  redrawCanvas();
  renderPreview();
});
```

This handles:
- ✓ Tracking mouse movement during drag
- ✓ Updating widget position in real-time
- ✓ Keeping widget within canvas bounds
- ✓ Live updating properties panel
- ✓ Redrawing canvas during drag
- ✓ Updating preview

---

## Bug #7: Implement Deploy Verification

**File**: `data/js/builder.js`  
**Lines**: 414-430 (deployOverlay function)  
**Issue**: No verification that deploy succeeded

### Current Code Problem
```javascript
// Lines 414-430 - Assumes success after API call
async function deployOverlay() {
  // ...
  try {
    const base = Number(idRaw);
    await saveOverlay(base, state.builder.overlayElements);
    showToast('Overlay deployed to LCD', 'success');
    await triggerRedraw();
    // MISSING: Verify data actually made it to device!
  } catch (error) {
    // ...
  }
}
```

### Solution

**Replace deployOverlay() function** with:

```javascript
async function deployOverlay() {
  const selector = document.getElementById('current-page-selector');
  if (!(selector instanceof HTMLSelectElement) || !selector.value) {
    showToast('Select a page before deploying', 'error');
    return;
  }

  const [type, idRaw] = selector.value.split(':');
  if (type !== 'builtin') {
    showToast('Deploy only supports built-in pages', 'error');
    return;
  }

  try {
    const base = Number(idRaw);
    
    // Show progress
    showToast('Sending overlay to device...', 'info');

    // Step 1: Send overlay widgets
    await saveOverlay(base, state.builder.overlayElements);

    // Step 2: Verify by reading back
    showToast('Verifying deployment...', 'info');
    const verification = await fetchOverlay(base);

    if (!verification?.widgets) {
      throw new Error('Verification failed: no data returned from device');
    }

    // Step 3: Basic consistency check
    if (verification.widgets.length !== state.builder.overlayElements.length) {
      console.warn('Widget count mismatch - device may have older data');
    }

    // Step 4: Trigger redraw on device
    await triggerRedraw();

    // Success!
    showToast('✓ Overlay deployed and verified on LCD', 'success');
  } catch (error) {
    console.error('Deploy failed', error);
    showToast(`Deploy failed: ${error.message}`, 'error');
  }
}
```

### Testing
1. Open builder
2. Load a builtin page
3. Add some widgets
4. Click "Deploy to LCD"
5. Verify "Sending..." toast appears
6. Verify "Verifying..." toast appears
7. Verify success message appears
8. Look at actual LCD display
9. Verify widgets appear on LCD

---

## Implementation Checklist

Use this checklist to track progress:

- [ ] **Bug #1**: Properties form apply
  - [ ] Replace applyPropertiesFromForm() with validation version
  - [ ] Test editing each property type
  - [ ] Verify changes persist in canvas

- [ ] **Bug #2**: Custom page persistence
  - [ ] Add saveCustomPages() function
  - [ ] Modify page-add handler to call save
  - [ ] Test page persists after refresh
  - [ ] Test page persists after reboot

- [ ] **Bug #3**: Palette drag-drop
  - [ ] Add dragover/drop/dragleave handlers to canvas
  - [ ] Test dragging each widget type to canvas
  - [ ] Test drop at various positions

- [ ] **Bug #4**: Delete/Copy/Paste
  - [ ] Implement delete handler with confirmation
  - [ ] Implement copy to clipboard
  - [ ] Implement paste with offset
  - [ ] Test each operation

- [ ] **Bug #5**: Undo/Redo
  - [ ] Initialize undo/redo stacks
  - [ ] Add recordUndoState() calls
  - [ ] Implement undo handler (already done in #4)
  - [ ] Implement redo handler (already done in #4)
  - [ ] Test undo/redo chain

- [ ] **Bug #6**: Canvas drag tracking
  - [ ] Verify mousemove handler from #3 is in place
  - [ ] Test dragging widget on canvas
  - [ ] Verify position updates live
  - [ ] Verify stays within bounds

- [ ] **Bug #7**: Deploy verification
  - [ ] Replace deployOverlay() with verification version
  - [ ] Test deploy to LCD
  - [ ] Verify success message
  - [ ] Check widgets appear on LCD

---

## Testing After Each Bug Fix

After implementing each bug fix, test:

1. **No JavaScript errors** in browser console
2. **Feature works as intended** for that bug
3. **No regression** - previously working features still work
4. **UI feedback** appears (toasts, messages)
5. **Canvas updates** correctly

When all 7 bugs are fixed, run the full testing checklist in LCD_BUILDER_BUG_SUMMARY.md

---

## Next Steps

1. Implement bugs in order (1-7)
2. Test each fix
3. After all 7 bugs fixed, run full testing checklist
4. Move to Phase 2 (Major bugs & robustness)
5. Then Phase 3 (Polish & features)

