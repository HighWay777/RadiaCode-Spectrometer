#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Arduino_GFX_Library.h>

/**
 * Weather Icons Header
 * Provides functions to draw weather icons based on weather condition codes
 * from Open-Meteo and OpenWeather API services
 */

// Main function to draw weather icon based on weather code
void drawWeatherIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, int weatherCode, bool isNight = false);

// Individual icon drawing functions
void drawSun(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);
void drawCloud(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);
void drawRain(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color, uint8_t drops = 3);
void drawSnow(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color, uint8_t flakes = 3);
void drawThunder(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);
void drawFog(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);
void drawPartlyCloudy(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t sunColor, uint16_t cloudColor);
void drawMist(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);
void drawWarning(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color);

#endif // WEATHER_ICONS_H