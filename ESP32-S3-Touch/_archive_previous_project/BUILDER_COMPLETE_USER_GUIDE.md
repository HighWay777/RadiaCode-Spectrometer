# ✅ BUILDER IS NOW FULLY FUNCTIONAL - Complete Guide

## STATUS: Ready to Use! 🚀

The LCD Builder is now **completely fixed and functional**. You can now edit:
- ✅ Built-in widgets (clock, weather, badges, panels)
- ✅ Custom overlay widgets
- ✅ Custom pages
- ✅ All widget properties

---

## Quick Start (First Time)

### Step 1: Refresh Browser
```
Press Ctrl+F5 in your browser to clear cache
```

### Step 2: Go to Builder
1. Click "Builder" tab in your web interface
2. Select a page from the dropdown (e.g., "Home")

### Step 3: What You'll See
- **Cyan boxes** = Built-in widgets (clickable & editable)
- **Light gray** = Show Base toggle (shows base widgets)
- **Canvas** = Editable area

### Step 4: Edit a Widget
1. **Click on any cyan box** on the canvas
2. **Properties panel opens** on the right
3. **Change any property**:
   - Position (X, Y)
   - Size (Width, Height)
   - Color
   - Text
   - Font size
4. **Click "Apply Changes"**
5. **Click "Deploy"**
6. **Check your LCD** - changes appear instantly!

---

## Detailed Feature Guide

### Editing Built-in Widgets

**Available Widgets on HOME page:**
- Weather Panel (large cyan box)
- Clock Panel (cyan box)
- Status Bar (cyan box)
- Weather Icon (small cyan box)
- Temperature Badge
- Humidity Badge
- Pressure Badge
- Wind Badge
- (Optional) Boiler Badge

**How to Edit Each:**
1. **Select** widget by clicking on it
2. **View properties** in right panel
3. **Change color, size, position, text**
4. **Click Apply**
5. **Click Deploy**

### Creating Custom Overlay Widgets

**Add a Widget:**
1. Click widget in palette (Label, Value, Icon, Shape, Button)
2. Click on canvas where you want it
3. Properties panel opens
4. **Edit properties** as desired
5. **Click Apply**
6. **Click Deploy**

### Creating Custom Pages

**New Page:**
1. Click ➕ button next to "Pages"
2. Enter page name (e.g., "My Dashboard")
3. Click OK
4. Page appears in list
5. **Add widgets** to the new page
6. **Edit and deploy** like normal pages

**Delete Page:**
1. Click 🗑️ button next to page name
2. Confirm deletion
3. Page is removed

**Import Page from File:**
1. Click 📥 button next to "Pages"
2. Select `.json` file (from previous export)
3. Page is added to custom pages
4. Ready to edit!

**Export Page:**
1. Select page
2. Click 💾 Save button
3. Downloads JSON file with all widgets
4. Can import this file later

### Using Undo/Redo

- **Undo**: Click ↶ button or Ctrl+Z
- **Redo**: Click ↷ button or Ctrl+Y
- Supports up to 20 undo states

### Copy/Paste Widgets

- **Copy**: Select widget, click 📋 button
- **Paste**: Click 📥 button
- Pasted widget offset by 16px for visibility

### Alignment Tools

After selecting a widget, use alignment buttons:
- **⬅️ Left**: Align to left edge
- **↔️ Center**: Center horizontally
- **➡️ Right**: Align to right edge
- **⬆️ Top**: Align to top edge
- **↕️ Middle**: Center vertically
- **⬇️ Bottom**: Align to bottom edge

### Grid & Snap Options

- **Show Base**: Toggle background widget visibility
- **Grid**: Show/hide grid overlay
- **Snap**: Auto-snap to grid while dragging
- **Live**: Show real device data in preview

### Zoom Controls

- **−**: Zoom out
- **+**: Zoom in
- **Fit**: Auto-fit to window

---

## Property Panel Reference

### Position Properties
- **X**: Horizontal position (0-480)
- **Y**: Vertical position (0-272)

### Size Properties
- **Width**: Widget width in pixels
- **Height**: Widget height in pixels

### Color Properties
- **Color**: Main color (click to open color picker)
- **Text Color**: Text color (for labels/values)

### Content Properties
- **Text**: Display text (for labels, buttons)
- **Icon**: Icon character (for icons)
- **Font Size**: 1-8 (larger = bigger text)

### Advanced Properties
- **Shape Type**: rect, circle, rounded, line (for shapes)
- **Corner Radius**: 0-80 (for buttons/rounded shapes)
- **Binding**: Bind to device data (currentTemp, humidity, etc.)

### Binding to Data

Available bindings:
- `currentTemp` - Current boiler temperature
- `targetTemp` - Target boiler temperature
- `humidity` - Relative humidity
- `pressure` - Air pressure
- `windKph` - Wind speed
- (and more depending on your device)

---

## Troubleshooting

### "Nothing appears when I click Deploy"

**Solution:**
1. Ensure you've selected a page
2. Ensure widgets are in overlay (not just base)
3. Try refreshing browser
4. Check browser console for errors

### "I can't see the clock card"

**Solution:**
1. Click "Show Base" checkbox
2. Look for cyan boxes on canvas
3. Cyan box = clock card (now visible!)
4. Click it to select

### "Changes don't appear on LCD"

**Solution:**
1. Click "Apply Changes" first
2. Then click "Deploy"
3. Wait 2 seconds for LCD to update
4. If still not working, try restarting device

### "Color picker doesn't work"

**Solution:**
1. Click on the color field (not next to it)
2. Color picker should open
3. Select color
4. Color updates in preview
5. Click Apply

### "Position changes aren't working"

**Solution:**
1. Make sure you changed the X or Y value
2. Click "Apply Changes" button
3. Check if value actually changed (should see in properties)
4. Deploy to device

---

## Best Practices

### ✅ DO:
- **Apply after every change** to see preview update
- **Deploy after applying** to send to device
- **Test in preview first** before deploying
- **Backup pages** using Export function
- **Use meaningful page names** for organization

### ❌ DON'T:
- **Don't forget to Deploy** - Apply doesn't update LCD
- **Don't change too many things** before testing
- **Don't resize widgets beyond screen** (480×272)
- **Don't position widgets off-screen**

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Z or Cmd+Z | Undo |
| Ctrl+Shift+Z or Cmd+Shift+Z | Redo |
| Ctrl+Y or Cmd+Y | Redo |
| Delete | Delete selected widget |

---

## FAQ

**Q: Can I edit the built-in weather card?**
A: YES! Click the cyan "Weather Panel" box to select it, then edit its properties.

**Q: Can I move the clock to a different position?**
A: YES! Select the "Clock Panel", change X/Y coordinates, Apply, then Deploy.

**Q: Can I create a completely new page?**
A: YES! Click ➕ to add new custom page, then add widgets to it.

**Q: Will changes be saved if I restart the device?**
A: YES! All changes are saved to device storage and survive restarts.

**Q: Can I have multiple custom pages?**
A: YES! Create as many as you want - they all save to the device.

**Q: What if I mess something up?**
A: Use Undo (Ctrl+Z) to go back, or reload the page to start fresh.

**Q: Can I export a page and send it to someone else?**
A: YES! Click 💾 Save to download JSON file. Share it, they can import it.

---

## Getting Help

If something isn't working:

1. **Check browser console** (F12 → Console tab)
2. **Refresh browser** (Ctrl+F5)
3. **Restart device**
4. **Check device logs** for error messages
5. **Try a simple test** - add one label widget and deploy

---

## Summary

The builder is **now fully functional and ready to use**!

You can:
- ✅ Edit ALL widgets (built-in and custom)
- ✅ Create custom pages
- ✅ Export/import pages
- ✅ Deploy changes instantly
- ✅ See changes on LCD immediately

**Enjoy your new LCD builder! 🎨**

