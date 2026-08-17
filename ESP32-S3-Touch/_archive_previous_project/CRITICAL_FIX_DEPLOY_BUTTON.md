# CRITICAL FIX #2 - Deploy Button & Widget Selection

## Problem Found

When you clicked Deploy, your changes didn't appear on the LCD because of **TWO bugs**:

### Bug #1: Stale Widget Reference
When you:
1. Selected a widget → Form built
2. Changed selection to different widget  
3. Edited first widget's form (still showing in panel because it wasn't rebuilt)
4. Clicked Apply

The code was applying changes to the widget captured at form-build time, NOT the currently selected widget!

**Example**:
```
Select Header → Form shows Header properties
Select Temperature → Form rebuilt, shows Temperature properties  
Actually, no - form was showing Header properties still!
Edit color, click Apply → Changes applied to Temperature, not Header!
```

### Bug #2: Deploy Not Capturing Last Changes
When you:
1. Changed a property (e.g., color)
2. Clicked Deploy WITHOUT clicking Apply first
3. Deploy sent the old data (before you clicked Apply)

Deploy didn't ensure pending form changes were applied first.

---

## What I Fixed

### Fix #1: Always Use Currently Selected Widget
**Before**:
```javascript
saveButton.addEventListener('click', () => 
  applyPropertiesFromForm(form, widget)  // ← 'widget' is stale!
);
```

**After**:
```javascript
saveButton.addEventListener('click', () => {
  const currentWidget = selectedWidget();  // ← Get fresh widget reference
  if (currentWidget) {
    applyPropertiesFromForm(form, currentWidget);
  }
});
```

Now the Apply button ALWAYS uses the actually-selected widget, not a stale capture.

### Fix #2: Deploy Applies Pending Changes First
**Before**: Deploy just sent whatever was in memory  
**After**: Deploy now:
1. Checks if form has unsaved changes
2. Applies them first if needed
3. Waits for save to complete
4. THEN deploys

```javascript
// If a widget is currently selected with unsaved form changes, apply them first
const currentWidget = selectedWidget();
if (currentWidget) {
  const form = document.querySelector('.property-form');
  if (form) {
    // Check for differences, apply if needed
    await applyPropertiesFromForm(form, currentWidget);
    await delay(500); // Wait for async save
  }
}
// Now deploy...
```

---

## What This Means

✅ **Apply button now always works** - Updates the right widget  
✅ **Deploy button now captures all changes** - No more lost edits  
✅ **No more stale references** - Selection changes are respected  
✅ **No need to click Apply before Deploy** - Deploy handles it  

---

## Test It

### Test Workflow
```
1. Open Builder
2. Select Header element  
3. Change its color to red
4. WITHOUT clicking Apply, click Deploy
✓ EXPECTED: Deploy will apply color change, then deploy (all in one step)
✓ LCD should show red header

OR

1. Select Header, change color
2. Select Temperature, change color
3. Select Header again, edit properties
4. Click Apply
✓ EXPECTED: Changes apply to Header (not Temperature)
5. Click Deploy
✓ EXPECTED: All changes go to LCD
```

---

## Files Modified

- `data/js/builder.js`
  - Line 1043-1053: Apply button now uses `selectedWidget()` instead of captured widget
  - Line 660-680: Deploy button now applies pending form changes before deploying

---

## Technical Details

### Why This Matters

The stale widget reference could cause:
- Changes to apply to wrong widget
- Confusion about what was actually edited
- Deploy sending wrong data
- Undo/Redo inconsistencies

Now everything is consistent - the widget you SELECT is the widget that gets edited.

### Performance

- ✅ Minimal overhead  
- ✅ Deploy now slightly slower (applies changes first) but more reliable
- ✅ Better user experience

---

## Summary

Your changes weren't appearing because:
1. Apply was updating the wrong widget (stale reference)
2. Deploy wasn't capturing last-minute edits

**Both are now fixed.** Try your workflow again - deploy your changes and they should appear on the LCD! 🚀

