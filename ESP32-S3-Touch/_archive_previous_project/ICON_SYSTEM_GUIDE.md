# Icon System Documentation

## Overview
The ESP32-S3 LCD Dashboard now includes a comprehensive icon system that allows you to customize the appearance of dashboard widgets with various icons. This system provides a wide range of icon types including system indicators, controls, weather symbols, charts, and status icons.

## Icon Categories

### System Icons
- **Temperature** (1): Thermometer icon for temperature displays
- **Humidity** (2): Water droplet for humidity indicators
- **Pressure** (3): Barometer for pressure readings
- **WiFi** (4): Network connectivity status
- **CPU** (5): Processor usage and stats
- **Memory** (6): RAM and storage indicators
- **Battery** (7): Power level indicator
- **Light** (8): Light sensor or control
- **Settings** (9): Configuration and system settings
- **Performance** (10): System performance metrics
- **Home** (11): Home screen indicator
- **Dashboard** (12): Dashboard/overview screen

### Control Icons
- **Mode** (20): Mode selector for different operational modes
- **Target** (21): Target/setpoint indicator
- **Power** (22): Power on/off control
- **Fan** (23): Fan speed or cooling control
- **LED** (24): LED control indicator
- **Timer** (25): Timer functions
- **Toggle** (26): On/off toggle switch
- **Slider** (27): Slider control

### Weather Icons
- **Sun** (40): Clear sky
- **Partly Cloudy** (41): Partial cloud cover
- **Cloudy** (42): Overcast conditions
- **Fog** (43): Foggy conditions
- **Drizzle** (44): Light rain
- **Rain** (45): Rain conditions
- **Snow** (46): Snow conditions
- **Thunder** (47): Thunderstorm
- **Freezing Rain** (48): Mixed precipitation

### Chart Icons
- **Gauge** (60): Circular gauge display
- **Bar Chart** (61): Vertical bar chart
- **Line Chart** (62): Line graph 
- **Sparkline** (63): Compact trend line

### Status Icons
- **OK** (80): Success/completed indicator
- **Warning** (81): Warning/alert indicator
- **Error** (82): Error indicator
- **Loading** (83): Process in progress
- **Sync** (84): Synchronization indicator

## Using the Icon System

### In the Web UI
1. Navigate to the Dashboard tab in the Web UI
2. For each card, you'll find a dropdown labeled "Icon"
3. Select the desired icon from the categorized dropdown menu
4. Click "Apply" or "Apply & Redraw" to see the changes on the LCD

### Matching Icons to Data
For best results, choose icons that match the type of data being displayed:
- Use temperature icon (1) for temperature readings
- Use humidity icon (2) for humidity data
- Use weather icons (40-48) for weather conditions
- Use gauge icon (60) for gauge widgets
- Use chart icons (61-63) for numerical trend data

## Technical Implementation
The icon system consists of several components:
1. **Icon Definitions**: Enum values in `icon_system.h` define all available icons
2. **Drawing Functions**: Functions in `icon_system.cpp` handle drawing each icon type
3. **Configuration**: Icons are stored in the `cardIcon` array in the `AppConfig` structure
4. **Web UI**: Icon selection is provided through dropdown menus for each dashboard card

## Tips and Best Practices
- Use consistent icons across similar data types for a unified look
- Consider color contrast - icons are drawn in the text color by default
- Smaller widgets (size 1) work best with simple icons
- For weather data, the system can automatically display appropriate icons based on weather codes

## Known Issues and Limitations
- Very small widgets may have limited space for icons
- Complex icons may appear pixelated at small sizes
- Custom icon colors are not yet supported (icons use the text color)

## Future Enhancements
Planned improvements to the icon system include:
- Icon color customization
- Additional icon types
- Animation for certain icons (like loading or sync)
- Custom icon uploads