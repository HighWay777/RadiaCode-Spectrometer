# Web UI Complete Rebuild - Implementation Guide

## ✅ Current Status

The Web UI has been **partially rebuilt**. Here's what's complete and what needs finishing:

### Completed
- ✅ **index.html** - New clean HTML structure (328 lines, down from 796)
- ✅ **Backup files** created (`.backup` extension)
- ✅ **Documentation** - This guide and WEB_UI_REBUILD_README.md

### Needs Completion
- ⚠️ **style.css** - Needs to be created
- ⚠️ **app.js** - Needs to be created

## 📋 Quick Summary

**Old UI:** 3,926 lines (796 HTML + 2,371 JS + 759 CSS)
**New UI Target:** ~1,200 lines (70% reduction)

## 🎯 New Structure

### Pages (4 main sections)

1. **Dashboard** (`#dashboard`)
   - Status overview cards
   - Weather & network info
   - Energy & system health
   - LCD preview

2. **Control** (`#control`)  
   - Mode selector (Off/Manual/Auto/AI)
   - Temperature control
   - Quick actions

3. **Builder** (`#builder`)
   - LCD page design canvas
   - Toolbar with drawing tools
   - Properties panel
   - Page management

4. **Settings** (`#settings`)
   - Display & theme
   - Network configuration
   - System actions

## 📝 Files to Create

### 1. `data/css/style.css` (~600 lines)

Create this file with:
- CSS variables for theming
- Navbar styles
- Grid system (responsive)
- Card components
- Stat cards with color variants
- Weather display
- Mode selector buttons
- Temperature control UI
- Builder workspace layout
- Form elements
- Button styles
- Toast notifications
- Mobile responsive (@media queries)

**Key CSS Variables:**
```css
:root {
  --primary: #2563eb;
  --success: #10b981;
  --warning: #f59e0b;
  --danger: #ef4444;
  --bg: #f8fafc;
  --card-bg: #ffffff;
  --text: #1e293b;
  --radius: 12px;
  --spacing: 20px;
}
```

### 2. `data/js/app.js` (~600 lines)

Create this file with these modules:

#### Core Functions
```javascript
// Navigation
function setupNavigation() { ... }

// API Communication
async function fetchStatus() { ... }
async function updateConfig(data) { ... }
async function setMode(mode) { ... }
async function setTemp(temp) { ... }

// UI Updates
function updateDashboard(data) { ... }
function updateWeather(data) { ... }
function updateNetwork(data) { ... }

// Toast Notifications
function showToast(message, type) { ... }

// Auto-refresh
function startStatusPolling() { ... }
```

#### Control Page Functions
```javascript
function setupModeSelector() { ... }
function setupTempControl() { ... }
function handleModeChange(mode) { ... }
function handleTempChange(temp) { ... }
```

#### Builder Functions
```javascript
function setupBuilder() { ... }
function initCanvas() { ... }
function addElement(type) { ... }
function saveLayout() { ... }
function loadLayout() { ... }
```

#### Settings Functions
```javascript
function setupForms() { ... }
function saveSettings(formId) { ... }
function handleRestart() { ... }
function handleReset() { ... }
```

## 🔌 API Endpoints

The JavaScript should connect to these ESP32 endpoints:

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/status` | Get real-time system status |
| POST | `/api/config` | Update configuration |
| POST | `/api/mode` | Set control mode (0-3) |
| POST | `/api/temp` | Set target temperature |
| GET | `/api/pages/all` | List all LCD pages |
| GET | `/api/pages/{name}` | Get specific page data |
| POST | `/api/pages/{name}` | Save page design |
| DELETE | `/api/pages/{name}` | Delete page |
| POST | `/api/system/restart` | Restart device |
| POST | `/api/system/reset` | Factory reset |

## 💡 Implementation Steps

### Step 1: Create style.css

```powershell
# Navigate to project directory
cd data\css

# Create the file (copy CSS content from backup or rebuild)
# The CSS should include all the styles mentioned above
```

### Step 2: Create app.js

```powershell
# Navigate to directory
cd data\js

# Create the file with JavaScript modules
# Include all functions listed above
```

### Step 3: Test Locally

1. Open `index.html` in browser
2. Check console for errors
3. Verify page navigation works
4. Test responsive design (resize window)

### Step 4: Upload to ESP32

```powershell
# Use PlatformIO task
platformio run -e esp32-s3-devkitc-1 -t uploadfs --upload-port COM24
```

## 🎨 Design Philosophy

### Colors
- **Primary (Blue)**: Main actions, active states
- **Success (Green)**: Positive states, online status
- **Warning (Orange)**: Cautions, alerts
- **Danger (Red)**: Errors, critical actions
- **Info (Cyan)**: Informational elements

### Typography
- **Headings**: 700 weight, clear hierarchy
- **Body**: 400 weight, 16px base
- **Labels**: 600 weight, 0.875rem
- **Values**: 700 weight, prominent size

### Spacing
- **Base unit**: 20px
- **Cards**: 20px padding
- **Grid gap**: 20px
- **Consistent margins**: multiples of 4px

### Interactions
- **Hover**: Subtle elevation, color change
- **Active**: Highlighted with primary color
- **Transitions**: 0.2s ease for smoothness
- **Touch targets**: Minimum 44px for mobile

## 📱 Mobile Responsive

### Breakpoints
- **Desktop**: > 768px (multi-column grids)
- **Mobile**: ≤ 768px (single column, icon-only nav)

### Mobile Optimizations
- Stack grid columns vertically
- Hide nav link text (icons only)
- Larger touch targets
- Simplified builder (reorder panels)

## 🧪 Testing Checklist

Before marking complete:

- [ ] All pages load without errors
- [ ] Navigation switches between pages
- [ ] Dashboard displays mock data correctly
- [ ] Control buttons are interactive
- [ ] Builder canvas initializes
- [ ] Settings forms are functional
- [ ] Toast notifications appear
- [ ] Mobile layout works (< 768px)
- [ ] API calls connect to ESP32
- [ ] Data updates in real-time

## 🔄 Reverting to Old UI

If needed, restore backups:

```powershell
Copy-Item "data\index.html.backup" "data\index.html" -Force
Copy-Item "data\js\app.js.backup" "data\js\app.js" -Force
Copy-Item "data\css\style.css.backup" "data\css\style.css" -Force
```

## 📚 Resources

- MDN Web Docs: https://developer.mozilla.org
- CSS Grid Guide: https://css-tricks.com/snippets/css/complete-guide-grid/
- Fetch API: https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API

## 🎯 Next Steps

1. **Complete CSS file** - Create `data/css/style.css` with all styles
2. **Complete JavaScript file** - Create `data/js/app.js` with all logic
3. **Test locally** - Open in browser, verify functionality
4. **Upload to ESP32** - Use `uploadfs` task
5. **Test on device** - Access via ESP32 IP address
6. **Iterate** - Fix any issues, refine design

---

**Status**: HTML structure complete, CSS & JS pending
**Estimated remaining work**: 2-3 hours
**Complexity**: Medium (straightforward implementation)
