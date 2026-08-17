# ESP32-S3 Weather Icons Update Guide

## Update Summary
This update adds colorful, dynamic weather icons to the ESP32-S3 display Home screen. The icons change based on weather conditions fetched from your configured weather API, providing a visual representation alongside the text-based weather information.

## New Features
- Dynamic weather icons on the Home screen
- Web UI toggle to enable/disable icons
- Visual representation of different weather conditions

## Update Steps

### 1. Update Files
All files have already been updated and uploaded to your device:
- `src/ui/weather_icons.h` (new file with icon drawing functions)
- `src/config/config_store.h` (updated with showWeatherIcons flag)
- `src/config/config_store.cpp` (updated to handle the new configuration)
- `src/ui/webui_server.cpp` (updated to expose the setting in Web UI)
- `src/main.cpp` (updated to draw weather icons on the Home screen)
- `data/index.html` (updated with toggle checkbox in settings)

### 2. Verify Operation
1. The ESP32-S3 has been rebooted and should now be operational.
2. Navigate to the Home screen (page 0) to see weather icons if weather data is available.
3. Open the Web UI by connecting to the ESP32's IP address from a browser.
4. Go to Settings and find the "Show Weather Icons" checkbox.
5. Toggle the setting to enable/disable icons as desired.

### 3. Troubleshooting
If you don't see weather icons:
1. Ensure weather data is being fetched (check serial monitor output)
2. Verify the "Show Weather Icons" setting is enabled in the Web UI
3. Check that your weather API key is valid and working
4. Try refreshing the Home screen or restarting the ESP32-S3

### 4. Weather Code Mapping
The implementation maps weather codes to different icon types:

| Weather Code Range | Weather Condition | Icon Description |
|-------------------|-------------------|-----------------|
| 0                 | Clear sky         | Sun with rays |
| 1-3               | Partly cloudy     | Sun with cloud |
| 4-9               | Cloudy/Foggy      | Cloud shapes |
| 10-19             | Light rain        | Small raindrops |
| 20-29             | Heavy rain        | Large raindrops |
| 30-39             | Snow              | Snowflakes |
| 40-49             | Thunderstorm      | Lightning bolt |
| 50+               | Extreme weather   | Warning symbol |

## Additional Notes
- The weather icons are designed to be visible and clear on the ESP32-S3 display
- Icons are drawn using primitive shapes rather than bitmap images for efficiency
- The feature has minimal impact on performance and memory usage
- Weather data refresh interval remains unchanged from your existing configuration