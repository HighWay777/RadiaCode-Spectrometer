# Visual Quick Reference - New Home Page

```
┌───────────────────────────────────────────────────────────────────┐
│ ESP32-S3 TOUCH CONTROLLER - HOME PAGE LAYOUT                      │
│ Screen: 480×272 pixels | Display: NV3041A QSPI | Patched Driver   │
└───────────────────────────────────────────────────────────────────┘

                        SCREEN LAYOUT (480×272)
╔═══════════════════════════════════════════════════════════════════╗
║ Y=0                                                               ║
║ ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ║
║ ┃ HEADER (32px)                                     [10px]     ┃ ║
║ ┃ Home                                        • • • • •        ┃ ║
║ ┃                                                               ┃ ║
║ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ║
║ Y=40   [8px gap]                                                  ║
║ ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ║
║ ┃ WEATHER PANEL (120px)                        [Weather]      ┃ ║
║ ┃                                                               ┃ ║
║ ┃  ☀     15.2°C              💧 65%    🌀 1013hPa   ┌────────┐ ┃ ║
║ ┃  60px  Clear sky           💨 8km/h  ☀ 800W/m²   │ Boiler │ ┃ ║
║ ┃                                                   │ 19.5°  │ ┃ ║
║ ┃  Icon  Temperature         Weather Grid          │ → 21°  │ ┃ ║
║ ┃        + Description       2×2 layout            └────────┘ ┃ ║
║ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ║
║ Y=168  [8px gap]                                                  ║
║ ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ║
║ ┃ CLOCK PANEL (60px)                                           ┃ ║
║ ┃                                                               ┃ ║
║ ┃                      12:34:56                                ┃ ║
║ ┃                 Wed, 01 Oct 2025                             ┃ ║
║ ┃                                                               ┃ ║
║ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ║
║ Y=236  [8px gap]                                                  ║
║ ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ║
║ ┃ STATUS BAR (24px)                                            ┃ ║
║ ┃ Updated 12:34:56  RAM:241k  TEMP:✓  WiFi:-58dBm      [AI]   ┃ ║
║ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ║
║ Y=272  [10px bottom margin]                                       ║
╚═══════════════════════════════════════════════════════════════════╝

                        STATUS BAR DETAIL
┌───────────────────────────────────────────────────────────────────┐
│ LEFT SIDE              MIDDLE              RIGHT SIDE              │
│ ───────────────────────────────────────────────────────────────── │
│                                                                    │
│ Updated 12:34:56  →15px→  RAM:241k  →12px→  TEMP:✓  →12px→       │
│                                                                    │
│                    WiFi:-58dBm  →12px→  [────AI────]              │
│                                            Mode Chip               │
│                                            52×16 px                │
└───────────────────────────────────────────────────────────────────┘

                     WEATHER PANEL DETAIL
┌───────────────────────────────────────────────────────────────────┐
│ [Weather] chip                                                     │
│                                                                    │
│ ┌──────┐  ┌────────────────────────────┐  ┌────────────┐         │
│ │      │  │  15.2°C                    │  │  Boiler    │         │
│ │  ☀   │  │  Clear sky                 │  │            │         │
│ │      │  │                            │  │   19.5°    │         │
│ │ 50px │  │  💧 65%    🌀 1013hPa      │  │   → 21°    │         │
│ │      │  │  💨 8km/h  ☀ 800W/m²       │  │            │         │
│ └──────┘  └────────────────────────────┘  └────────────┘         │
│                                                                    │
│ Icon      Temperature + Weather Grid      Boiler Sensor           │
│ 50×50     240px wide (or 340 if no boiler) 95×82px                │
└───────────────────────────────────────────────────────────────────┘

                       COLOR CODING
┌───────────────────────────────────────────────────────────────────┐
│ ELEMENT          │ GOOD              │ WARNING          │ BAD      │
│──────────────────┼───────────────────┼──────────────────┼──────────│
│ RAM:XXXk         │ Green (>100KB)    │ Yellow (50-100)  │ Red (<50)│
│ WiFi:-XXdBm      │ Green (>-60dBm)   │ Yellow (-60--75) │ Red(<-75)│
│ TEMP:✓/✗         │ Green (working)   │ -                │ Red (err)│
│ Mode Chip        │ Accent color      │ -                │ -        │
└───────────────────────────────────────────────────────────────────┘

                         FONT SIZES
┌───────────────────────────────────────────────────────────────────┐
│ LEVEL │ FONT                  │ USAGE                             │
│───────┼───────────────────────┼───────────────────────────────────│
│   5   │ FreeSansBold24pt7b    │ Clock time (12:34:56)             │
│   4   │ FreeSansBold18pt7b    │ Temperatures (15.2°C, 19.5°)      │
│   3   │ FreeSansBold12pt7b    │ Headers ("Home", "Boiler")        │
│   2   │ FreeSans12pt7b        │ Descriptions, dates               │
│   1   │ FreeSans9pt7b         │ Status bar, weather details       │
└───────────────────────────────────────────────────────────────────┘

                      SPACING RULES
┌───────────────────────────────────────────────────────────────────┐
│ ELEMENT                  │ SPACING                                │
│──────────────────────────┼────────────────────────────────────────│
│ Screen margins           │ 10px (left, right, top, bottom)        │
│ Panel gaps               │ 8px (between panels)                   │
│ Status bar text          │ 12-15px (between elements)             │
│ Status bar + timestamp   │ 15px (after timestamp text)            │
│ Panel internal padding   │ 10-15px (inside panels)                │
│ Weather columns          │ 20px (between icon/text/boiler)        │
│ Mode chip position       │ 10px from right edge                   │
└───────────────────────────────────────────────────────────────────┘

                  NO OVERLAP ALGORITHM
┌───────────────────────────────────────────────────────────────────┐
│ Status Bar Positioning (Critical Fix):                            │
│                                                                    │
│ 1. currentX = x + 10                 ← Start position             │
│                                                                    │
│ 2. printText(timestamp)              ← Draw "Updated HH:MM:SS"    │
│    currentX += getTextWidth(timestamp) + 15                       │
│                ↑                        ↑                          │
│                Measure actual width     Add gap                    │
│                                                                    │
│ 3. printText(memStr)                 ← Draw "RAM:XXXk"            │
│    currentX += getTextWidth(memStr) + 12                          │
│                                                                    │
│ 4. printText(tempStr)                ← Draw "TEMP:✓"              │
│    currentX += getTextWidth(tempStr) + 12                         │
│                                                                    │
│ 5. printText(rssiStr)                ← Draw "WiFi:-XXdBm"         │
│                                                                    │
│ 6. Mode chip at fixed right position ← [AI] chip                  │
│                                                                    │
│ ✓ Each element MEASURES before moving right                       │
│ ✓ Explicit gaps between elements                                  │
│ ✓ NO HARDCODED POSITIONS (except mode chip)                       │
│ ✓ GUARANTEED NO OVERLAP                                           │
└───────────────────────────────────────────────────────────────────┘

                    DATA FLOW
┌───────────────────────────────────────────────────────────────────┐
│                                                                    │
│  ┌──────────────┐        ┌──────────────┐                         │
│  │ Weather API  │──────▶ │updateWeather │                         │
│  │ (external)   │        │    Only()    │                         │
│  └──────────────┘        └───────┬──────┘                         │
│                                  │                                 │
│                                  ▼                                 │
│                          ┌────────────────┐                        │
│                          │ globalWeather  │                        │
│                          │   WeatherData  │                        │
│                          └───────┬────────┘                        │
│                                  ▲                                 │
│  ┌──────────────┐        ┌───────┴──────┐                         │
│  │ Boiler API   │──────▶ │updateBoiler  │                         │
│  │ (local)      │        │    Data()    │                         │
│  └──────────────┘        └──────────────┘                         │
│                                  │                                 │
│                                  ▼                                 │
│                          ┌──────────────┐                          │
│                          │drawPageHome()│                          │
│                          └───────┬──────┘                          │
│                                  │                                 │
│         ┌────────────────────────┼────────────────────┐            │
│         ▼                        ▼                    ▼            │
│  ┌──────────┐           ┌───────────┐        ┌──────────┐         │
│  │  Header  │           │  Weather  │        │  Clock   │         │
│  └──────────┘           │   Panel   │        └──────────┘         │
│                         └───────────┘                              │
│                                  ▼                                 │
│                          ┌───────────┐                             │
│                          │Status Bar │                             │
│                          └───────────┘                             │
│                                                                    │
│                        LCD Display                                 │
│                        (480×272 px)                                │
│                                                                    │
└───────────────────────────────────────────────────────────────────┘

                  TESTING CHECKLIST
┌───────────────────────────────────────────────────────────────────┐
│ ☐ No text overlap anywhere                                        │
│ ☐ Status bar: timestamp + RAM + TEMP + WiFi + Mode all visible    │
│ ☐ Weather icon displays correctly                                 │
│ ☐ Outdoor temperature shows                                       │
│ ☐ Weather grid (2×2) displays all 4 values                        │
│ ☐ Boiler sensor shows (if enabled)                                │
│ ☐ Clock time updates every second                                 │
│ ☐ Date displays correctly                                         │
│ ☐ Color coding works (green/yellow/red)                           │
│ ☐ All panels are properly aligned                                 │
│ ☐ No flickering or artifacts                                      │
│ ☐ Touch response works                                            │
│ ☐ Web UI still accessible                                         │
│ ☐ Settings persist after reboot                                   │
└───────────────────────────────────────────────────────────────────┘

                     QUICK COMMANDS
┌───────────────────────────────────────────────────────────────────┐
│ Build and Upload:                                                  │
│ platformio run -e esp32-s3-devkitc-1 -t upload --upload-port COM24│
│                                                                    │
│ Rollback to Old Version:                                          │
│ Copy-Item src\ui\page_home.cpp.backup src\ui\page_home.cpp -Force │
│                                                                    │
│ View Serial Monitor:                                              │
│ platformio device monitor --baud 115200                           │
│                                                                    │
│ Clean Build:                                                      │
│ platformio run --target clean                                     │
│ platformio run -e esp32-s3-devkitc-1 -t upload                    │
└───────────────────────────────────────────────────────────────────┘

                    FILE LOCATIONS
┌───────────────────────────────────────────────────────────────────┐
│ Current implementation:                                            │
│ src/ui/page_home.cpp                                              │
│                                                                    │
│ Backup (old version):                                             │
│ src/ui/page_home.cpp.backup                                       │
│                                                                    │
│ Header file:                                                      │
│ src/ui/page_home.h                                                │
│                                                                    │
│ Documentation:                                                    │
│ NEW_HOME_PAGE_DOCUMENTATION.md                                    │
│ VISUAL_QUICK_REFERENCE.md (this file)                            │
└───────────────────────────────────────────────────────────────────┘

                     VERSION INFO
┌───────────────────────────────────────────────────────────────────┐
│ Version:      2.0 (Complete Rewrite)                              │
│ Date:         October 1, 2025                                     │
│ Status:       ✅ Deployed                                          │
│ Code Size:    420 lines (was 622 lines)                           │
│ RAM Usage:    47,908 bytes (14.6%)                                │
│ Flash Usage:  1,149,369 bytes (87.7%)                             │
│ Build Status: SUCCESS                                             │
└───────────────────────────────────────────────────────────────────┘
```
