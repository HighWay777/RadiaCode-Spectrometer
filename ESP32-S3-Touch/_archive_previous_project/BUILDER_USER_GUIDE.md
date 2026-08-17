# LCD Builder - Keyboard Shortcuts & Features Guide

## Keyboard Shortcuts

### Undo & Redo
| Shortcut | Action | Notes |
|----------|--------|-------|
| **Ctrl+Z** (Windows/Linux) or **Cmd+Z** (Mac) | Undo | Reverses last action (20 state history) |
| **Ctrl+Shift+Z** (Windows/Linux) or **Cmd+Shift+Z** (Mac) | Redo | Reapplies undone action |
| **Ctrl+Y** (Windows/Linux) or **Cmd+Y** (Mac) | Redo | Alternative redo shortcut |
| **Delete** | Delete Selected Widget | Removes currently selected widget |

### Toolbar Buttons
| Button | Icon | Action | Keyboard |
|--------|------|--------|----------|
| Add Label | + | Opens widget palette | N/A |
| Delete | 🗑️ | Deletes selected widget | Delete key |
| Copy | 📋 | Copies selected widget to clipboard | N/A |
| Paste | 📌 | Pastes widget with 16px offset | N/A |
| Undo | ↶ | Undoes last change | Ctrl+Z |
| Redo | ↷ | Redoes last undone change | Ctrl+Y |
| Save | 💾 | Downloads overlay as JSON | N/A |
| Deploy | 📤 | Sends overlay to LCD with verification | N/A |
| **Presets** | 📑 | **NEW** Opens preset management | N/A |
| **Reset** | 🔄 | **NEW** Reset to default layouts | N/A |

## New Features (Phase 1)

### 1. Undo/Redo History
**What**: Full undo/redo for all builder operations  
**How**: Use Ctrl+Z to undo, Ctrl+Y to redo  
**Limit**: 20 actions stored (prevents memory bloat)  
**Tracked Actions**:
- ✅ Add widget
- ✅ Delete widget
- ✅ Edit properties
- ✅ Paste widget
- ✅ Drag widget to new position
- ✅ Copy from base layer

**Example Workflow**:
1. Add 5 widgets
2. Delete 2 accidentally
3. Press Ctrl+Z twice to restore them
4. Press Ctrl+Y once to delete them again

---

### 2. Preset System (Save/Load Layouts)
**What**: Save your current layout as a named preset, then restore it later  
**How**: Click 📑 "Presets" button → Save current layout → Give it a name  
**Storage**: Saved in SPIFFS (persistent across reboots)  
**Presets Included**: Timestamps show when each was saved

**Example Workflow**:
1. Create custom layout (add 10 widgets, arrange nicely)
2. Click Presets button
3. Enter name: "My Dark Theme"
4. Click "Save Preset"
5. Later: Click Presets → Click "Load" on "My Dark Theme"
6. Your layout is restored instantly

**Features**:
- ✅ Save unlimited presets
- ✅ Load any preset (overwrites current layout)
- ✅ Delete presets you don't need
- ✅ Timestamps show creation time
- ✅ Persists across device reboot

---

### 3. Reset to Defaults (Safety Feature)
**What**: One-click restore of original layouts  
**Why**: Safety net if editing goes wrong  
**How**: Click 🔄 "Reset" button → Confirm in dialog → All reverted  

**What Gets Reset**:
- ✅ All custom overlays cleared
- ✅ Custom pages removed
- ✅ Original builtin pages reloaded
- ✅ Undo/redo history cleared

**Important**: This is permanent and can't be undone (outside of undo stack before reset)

**Example Workflow**:
1. Accidentally delete all widgets
2. Click Reset button
3. Confirm the red "RESET" button
4. Original layouts restored

---

### 4. Deploy Verification
**What**: Deploy now confirms layout actually reached the LCD  
**How**: Automatic - happens after deploy  
**Shows**:
- ✅ "Deploying..." while sending
- ✅ "Verifying..." while checking
- ✅ "✓ Deployment verified: 12 widgets deployed" on success
- ✅ "⚠ Partial: sent 12, received 8" if something went wrong

**Benefits**:
- No more guessing if deploy worked
- Catch network glitches immediately
- Clear feedback on what happened

---

## Operation Summary

### Common Tasks

#### Add a Widget
1. Click widget type in palette (Label, Value, Icon, etc.)
2. Drag onto canvas
3. Position by dragging
4. Edit properties in sidebar

#### Edit Widget Properties
1. Click widget on canvas to select
2. Properties panel shows all editable fields
3. Change values (position, size, text, color, etc.)
4. Changes apply immediately
5. Use Ctrl+Z if you make a mistake

#### Delete a Widget
1. Click widget to select
2. Press Delete key OR click Delete button
3. Widget removed from canvas
4. Use Ctrl+Z to restore if accidental

#### Copy & Paste Widget
1. Click widget to select
2. Click Copy button (or Ctrl+C manually)
3. Click Paste button
4. New widget appears offset by 16px
5. Move to desired position

#### Undo Recent Changes
1. Press Ctrl+Z repeatedly to step back through history
2. Each press goes back one action
3. Use Ctrl+Y to step forward (redo)
4. Up to 20 recent actions available

#### Save Your Layout
1. Arrange all widgets perfectly
2. Click Presets button (📑)
3. Type a name: "Dashboard Home", "Dark Mode", etc.
4. Click "Save Preset"
5. Layout saved permanently

#### Load a Saved Layout
1. Click Presets button (📑)
2. Find your preset in the list
3. Click "Load" button
4. Your saved layout appears
5. Continue editing or deploy it

#### Reset Everything
1. If something went very wrong...
2. Click Reset button (🔄)
3. Read confirmation carefully
4. Click red "RESET" button
5. Original layouts restored completely

#### Deploy to Device
1. Select which page to deploy to
2. Click Deploy button (📤)
3. Watch for status: "⏳ Deploying..." → "🔍 Verifying..."
4. Get confirmation: "✓ Deployment verified: X widgets"
5. Changes appear on LCD instantly

---

## Tips & Tricks

### Pro Tips
1. **Name Your Presets Clearly**: Use descriptive names like "Dark Dashboard" not "Layout1"
2. **Save Before Big Changes**: If about to try something risky, save a preset first
3. **Use Keyboard Shortcuts**: Much faster than clicking buttons
4. **Drag to Move**: Click and drag widgets on canvas - faster than editing X/Y
5. **Undo Often**: No penalty, so experiment freely with Ctrl+Z nearby
6. **Check Deployment**: Always verify deploy succeeded (check the message)

### Troubleshooting

**"Deploy failed"**
- Check you selected a page
- Check network connection
- Try again

**"Undo doesn't work"**
- Only 20 actions stored - very old changes can't be undone
- Reset clears the undo stack
- Take a preset before major changes

**"Preset won't save"**
- Name can't be empty
- Name can't be longer than 32 characters
- Device might be out of storage

**"Reset is grayed out"**
- Some pages are locked (can't reset individual pages)
- Reset resets entire system

---

## Keyboard Shortcut Cheat Sheet (Print This!)

```
UNDO/REDO
  Ctrl+Z     Undo (Windows/Linux) or Cmd+Z (Mac)
  Ctrl+Y     Redo (Windows/Linux) or Cmd+Y (Mac)
  Ctrl+Shift+Z  Redo alternative (Windows/Linux)
  Delete     Delete selected widget

QUICK REFERENCE
  Presets    📑 Button - Save/load/delete layouts
  Reset      🔄 Button - Restore original layouts
  Deploy     📤 Button - Send to LCD with verify
  Copy/Paste 📋→📌 Buttons - Duplicate widgets
```

---

## Widget Types Available

| Type | Use For | Properties |
|------|---------|-----------|
| **Label** | Static text | Text, position, size, color, font size |
| **Value** | Dynamic numbers | Text format, position, size, color |
| **Icon** | Emoji/symbols | Icon char, position, size, color |
| **Shape** | Boxes/decorations | Shape type, position, size, color, radius |
| **Button** | Interactive areas | Text, position, size, color, radius, binding |

---

## Presets Storage Info

**Where Stored**: SPIFFS filesystem on device  
**Persistence**: Survives power loss and reboots  
**Limit**: ~12 presets typical (depends on widget count)  
**Format**: Each preset stores widget array with name and timestamp  
**Reset Effect**: Reset operation doesn't delete presets

---

## Questions?

For more help:
- Check the builder tooltips (hover over buttons)
- Read the API documentation
- Review the implementation guides in docs/

**Enjoy building! 🎨**

