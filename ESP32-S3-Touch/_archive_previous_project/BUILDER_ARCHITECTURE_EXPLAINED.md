# Builder Architecture Explanation - Why You Can't Edit Built-in Widgets

## The Real Problem (Nov 2, 2025)

**The clock card and other built-in dashboard widgets are NOT editable through the builder because:**

1. **They are hardcoded in C++ firmware** (not stored as JSON widget data)
   - Location: `src/ui/page_home.cpp`
   - They are rendered directly by C++ drawing functions
   - NOT available as "base widgets" to the builder

2. **The `/api/pages/base` endpoint returns EMPTY** 
   - Code: `src/main.cpp` lines 415-418
   - If layout is empty, it returns `{"widgets":[]}`
   - This is why you see nothing to click on!

3. **The builder is designed for OVERLAY widgets only**
   - Overlays: Custom widgets you ADD on top of built-in pages
   - Built-in: Static elements rendered by C++ code

---

## Current Architecture

```
┌─────────────────────────────────────────┐
│          HOME PAGE (LCD DISPLAY)        │
├─────────────────────────────────────────┤
│ ┌─────────────────────────────────────┐ │
│ │  OVERLAY LAYER (Editable via UI)    │ │
│ │  ├─ Custom widgets added by user    │ │
│ │  └─ Saved to /overlay0.json         │ │
│ ├─────────────────────────────────────┤ │
│ │  BASE LAYER (Hardcoded in C++)      │ │
│ │  ├─ Clock card                      │ │
│ │  ├─ Weather card                    │ │
│ │  ├─ Boiler status                   │ │
│ │  └─ Rendered by page_home.cpp       │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

**What the builder CAN do:**
- Add widgets to OVERLAY layer
- Edit overlay widget properties
- Save overlay to device

**What the builder CANNOT do:**
- Edit built-in hardcoded widgets
- Change clock card color/size/position
- Modify weather card layout
- Remove default dashboard cards

---

## How to Customize Built-in Widgets

### Option 1: Add Custom Widgets on Top (✅ WORKS NOW)
1. Open Builder
2. Select "Home" page (or any built-in page)
3. **ADD NEW WIDGETS** using the palette (don't try to edit existing ones)
4. Customize your new widgets
5. Deploy to device

### Option 2: Modify C++ Code (⚙️ FOR DEVELOPERS)

To actually edit the clock card or other built-in widgets, you must:

1. **Edit source file**: `src/ui/page_home.cpp`
2. **Modify widget properties** in the C++ code:
   ```cpp
   // Example: Change clock card color
   // Around line 85-100, find the clock drawing code
   // Modify colors, fonts, positions as needed
   ```
3. **Rebuild firmware**: Run "Build+Upload firmware" task
4. **Upload to device**: Firmware updates automatically apply

---

## Testing the Builder (What DOES Work)

### ✅ Fully Working Features:

1. **Add overlay widgets**
   - Click widget in palette
   - Place on canvas
   - Edit properties (color, size, text, etc.)
   - Click Apply → Deploy

2. **Edit overlay properties**
   - Position (X, Y)
   - Size (Width, Height)
   - Colors
   - Text content
   - Font size
   - Bindings to device data (currentTemp, humidity, etc.)

3. **Custom pages**
   - Create new pages
   - Delete pages
   - Import/export pages
   - Edit custom page widgets

4. **Persistence**
   - All changes save to device
   - Deploy verifies deployment
   - Changes survive device restart

### ❌ NOT Working Features:

1. **Editing built-in widgets** (by design)
   - Clock card (hardcoded in C++)
   - Weather card (hardcoded in C++)
   - Status cards (hardcoded in C++)

2. **Deploying custom pages** to LCD
   - Custom pages work in builder
   - But deploy only works for built-in pages
   - TODO: Need backend support for custom page rendering

---

## Why This Design?

The current architecture:
- **Pros**: Built-in widgets are stable, consistent, and optimized
- **Cons**: Can't customize them without recompiling firmware
- **Reason**: Embedded device memory constraints - pre-compiled C++ is smaller than data-driven JSON approach

---

## What You SHOULD Do Now

### For Testing the Builder:
1. **Create a custom page** with your own widgets
2. **Add labels, values, icons** to HOME page overlay
3. **Change colors and positions** of your custom widgets
4. **Deploy** and verify they appear on LCD

### For Customizing Built-in Widgets:
1. Edit `src/ui/page_home.cpp` lines 80-200
2. Modify C++ drawing code for colors/sizes/positions
3. Run build/upload task
4. Restart device

---

## Example: How to Change Clock Card Color (C++ Way)

File: `src/ui/page_home.cpp`

```cpp
// BEFORE (around line 100):
void drawClockCard(TFT_eGFX* gfx) {
  gfx->setTextColor(TFT_WHITE);  // White text
  gfx->setTextSize(4);
  gfx->drawString("12:34", 40, 50);  // Position: x=40, y=50
}

// AFTER (modified colors/position):
void drawClockCard(TFT_eGFX* gfx) {
  gfx->setTextColor(TFT_RED);    // Changed to red
  gfx->setTextSize(5);           // Larger font
  gfx->drawString("12:34", 50, 60);  // New position
}
```

Then rebuild firmware.

---

## Summary

| Feature | Builder UI | C++ Code |
|---------|-----------|----------|
| Add custom widgets | ✅ Works | N/A |
| Edit custom widgets | ✅ Works | N/A |
| Edit clock card | ❌ Can't | ✅ Yes |
| Edit weather card | ❌ Can't | ✅ Yes |
| Create custom pages | ✅ Works | N/A |
| Export/Import pages | ✅ Works | N/A |

---

## Next Steps

1. **Test overlay widgets** - Add custom widgets to HOME page
2. **Verify deployment** - Changes should appear on LCD
3. **For clock editing** - Modify C++ code or contact developer
4. **Custom pages** - Create and deploy your own page designs

