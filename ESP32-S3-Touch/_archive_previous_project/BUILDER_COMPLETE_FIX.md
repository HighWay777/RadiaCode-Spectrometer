# Complete Builder Interactive Features Implementation

## Current Status
- ✅ Widgets load and display on canvas
- ✅ Drag and move widgets
- ❌ Properties panel not visible/updating
- ❌ No visual selection indicator
- ❌ Tab switching not implemented

## Required Components

### 1. Tab Switching System
**File**: `data/js/app.js` in `setupBuilder()` function

```javascript
// Add after canvas setup, before widget loading
const tabBtns = document.querySelectorAll('.builder-sidebar .tab-btn');
const tabContents = document.querySelectorAll('.builder-sidebar .tab-content');

tabBtns.forEach(btn => {
  btn.addEventListener('click', () => {
    const targetTab = btn.getAttribute('data-tab');
    
    // Remove active from all tabs and contents
    tabBtns.forEach(b => b.classList.remove('active'));
    tabContents.forEach(c => c.classList.remove('active'));
    
    // Activate clicked tab
    btn.classList.add('active');
    document.getElementById(`tab-${targetTab}`).classList.add('active');
  });
});
```

### 2. Canvas Click Handler with Selection
**File**: `data/js/app.js` - Add to canvas event listeners

```javascript
builderCanvas.addEventListener('mousedown', (e) => {
  const rect = builderCanvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;
  
  // Find clicked element (reverse order for top elements first)
  for (let i = builderElements.length - 1; i >= 0; i--) {
    if (isPointInElement(x, y, builderElements[i])) {
      selectedElement = i;
      isDragging = true;
      dragOffset.x = x - builderElements[i].x;
      dragOffset.y = y - builderElements[i].y;
      redrawCanvas();
      updatePropertiesPanel();
      
      // Auto-switch to Properties tab when selecting
      const propsTab = document.querySelector('[data-tab="properties"]');
      if (propsTab) propsTab.click();
      
      return;
    }
  }
  
  // Clicked outside - deselect
  selectedElement = null;
  redrawCanvas();
  updatePropertiesPanel();
});
```

### 3. Hit Detection Function
**File**: `data/js/app.js`

```javascript
function isPointInElement(x, y, element) {
  const ex = element.x || 0;
  const ey = element.y || 0;
  const ew = element.w || element.width || 100;
  const eh = element.h || element.height || 30;
  
  return x >= ex && x <= ex + ew && y >= ey && y <= ey + eh;
}
```

### 4. Properties Panel Update
**File**: `data/js/app.js`

```javascript
function updatePropertiesPanel() {
  const panel = document.getElementById('properties-content');
  if (!panel) return;
  
  if (selectedElement === null) {
    panel.innerHTML = '<p class="no-selection">Select a widget to edit properties</p>';
    return;
  }
  
  const element = builderElements[selectedElement];
  const type = element.type || 'label';
  
  let html = `<div class="property-form">`;
  html += `<div class="form-row"><label>Type:</label><input type="text" value="${type}" disabled></div>`;
  html += `<div class="form-row"><label>X:</label><input type="number" id="prop-x" value="${element.x || 0}"></div>`;
  html += `<div class="form-row"><label>Y:</label><input type="number" id="prop-y" value="${element.y || 0}"></div>`;
  html += `<div class="form-row"><label>Width:</label><input type="number" id="prop-w" value="${element.w || 100}"></div>`;
  html += `<div class="form-row"><label>Height:</label><input type="number" id="prop-h" value="${element.h || 30}"></div>`;
  
  if (element.text !== undefined) {
    html += `<div class="form-row"><label>Text:</label><input type="text" id="prop-text" value="${element.text}"></div>`;
  }
  
  if (element.fontSize !== undefined) {
    html += `<div class="form-row"><label>Font Size:</label><input type="number" id="prop-fontSize" value="${element.fontSize}" min="1" max="5"></div>`;
  }
  
  if (element.color !== undefined) {
    html += `<div class="form-row"><label>Color:</label><input type="color" id="prop-color" value="${element.color}"></div>`;
  }
  
  html += `<button class="btn btn-primary" id="apply-properties" style="margin-top:10px;">Apply Changes</button>`;
  html += `</div>`;
  
  panel.innerHTML = html;
  
  // Apply changes handler
  document.getElementById('apply-properties').addEventListener('click', () => {
    const el = builderElements[selectedElement];
    el.x = parseInt(document.getElementById('prop-x').value) || 0;
    el.y = parseInt(document.getElementById('prop-y').value) || 0;
    el.w = parseInt(document.getElementById('prop-w').value) || 100;
    el.h = parseInt(document.getElementById('prop-h').value) || 30;
    
    const textInput = document.getElementById('prop-text');
    if (textInput) el.text = textInput.value;
    
    const fontSizeInput = document.getElementById('prop-fontSize');
    if (fontSizeInput) el.fontSize = parseInt(fontSizeInput.value);
    
    const colorInput = document.getElementById('prop-color');
    if (colorInput) el.color = colorInput.value;
    
    redrawCanvas();
    showToast('Properties updated', 'success');
  });
}
```

### 5. Visual Selection Indicator
**File**: `data/js/app.js` in `redrawCanvas()` function, after drawing each element:

```javascript
// After drawing the widget, add this:
if (index === selectedElement) {
  builderCtx.strokeStyle = '#00ff00';
  builderCtx.lineWidth = 2;
  builderCtx.setLineDash([5, 3]);
  builderCtx.strokeRect(x - 2, y - 2, w + 4, h + 4);
  builderCtx.setLineDash([]);
  
  // Corner handles
  builderCtx.fillStyle = '#00ff00';
  [[x-3, y-3], [x+w-3, y-3], [x-3, y+h-3], [x+w-3, y+h-3]].forEach(([cx, cy]) => {
    builderCtx.fillRect(cx, cy, 6, 6);
  });
}
```

## Implementation Order

1. **First**: Add tab switching (non-breaking)
2. **Second**: Add `isPointInElement()` helper function
3. **Third**: Add `updatePropertiesPanel()` function  
4. **Fourth**: Add canvas click handler
5. **Fifth**: Add selection visual indicator in redrawCanvas()
6. **Test**: After each step, verify no breaks

## Testing Checklist

- [ ] Dashboard page loads
- [ ] Builder page loads
- [ ] Widgets display on canvas
- [ ] Can drag widgets
- [ ] Clicking Properties tab switches view
- [ ] Clicking widget shows green border
- [ ] Properties panel populates with widget data
- [ ] Changing properties and clicking Apply updates widget
- [ ] Other pages (Boiler, Stats) still load

## Rollback Plan

If any issues:
```powershell
Copy-Item "data/js/app.js.SAFE_BACKUP" "data/js/app.js" -Force
pio run -e esp32-s3-devkitc-1 -t uploadfs
```
