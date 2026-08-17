#include "weather_icons.h"
#include <cmath>

// Define PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI

/**
 * Weather Icons Implementation
 * Provides functions to draw weather icons based on weather condition codes
 * from Open-Meteo and OpenWeather API services
 */

void drawWeatherIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, int weatherCode, bool isNight) {
  // Colors for icons
  uint16_t sunColor = 0xFDA0;  // Orange/yellow
  uint16_t cloudColor = 0xC618; // Light gray
  uint16_t rainColor = 0x5D9B;  // Blue
  uint16_t snowColor = 0xFFFF;  // White
  uint16_t fogColor = 0xAD55;   // Gray
  uint16_t warningColor = 0xF800; // Red
  
  // If night time, make the sun color more subdued (moon-like)
  if (isNight) {
    sunColor = 0xCE59; // Pale yellow
  }
  
  // Map weather code to appropriate icon
  // Based on Open-Meteo API weather codes
  // https://open-meteo.com/en/docs
  
  if (weatherCode == 0) {
    // Clear sky
    drawSun(gfx, x, y, size, sunColor);
  }
  else if (weatherCode == 1) {
    // Mainly clear
    drawPartlyCloudy(gfx, x, y, size, sunColor, cloudColor);
  }
  else if (weatherCode == 2) {
    // Partly cloudy
    drawPartlyCloudy(gfx, x, y, size, sunColor, cloudColor);
  }
  else if (weatherCode == 3) {
    // Overcast
    drawCloud(gfx, x, y, size, cloudColor);
  }
  else if (weatherCode == 45 || weatherCode == 48) {
    // Fog or depositing rime fog
    drawFog(gfx, x, y, size, fogColor);
  }
  else if (weatherCode == 51 || weatherCode == 53 || weatherCode == 55) {
    // Drizzle: Light, moderate, dense intensity
    drawRain(gfx, x, y, size, rainColor, 3);
  }
  else if (weatherCode == 56 || weatherCode == 57) {
    // Freezing Drizzle: Light and dense intensity
    drawRain(gfx, x, y, size, rainColor, 2);
    drawSnow(gfx, x, y, size, snowColor, 1);
  }
  else if (weatherCode == 61) {
    // Rain: Slight intensity
    drawRain(gfx, x, y, size, rainColor, 2);
  }
  else if (weatherCode == 63) {
    // Rain: Moderate intensity
    drawRain(gfx, x, y, size, rainColor, 3);
  }
  else if (weatherCode == 65) {
    // Rain: Heavy intensity
    drawRain(gfx, x, y, size, rainColor, 5);
  }
  else if (weatherCode == 66 || weatherCode == 67) {
    // Freezing Rain: Light and heavy intensity
    drawRain(gfx, x, y, size, rainColor, 2);
    drawSnow(gfx, x, y, size, snowColor, 2);
  }
  else if (weatherCode == 71 || weatherCode == 73 || weatherCode == 75) {
    // Snow fall: Slight, moderate, and heavy intensity
    drawSnow(gfx, x, y, size, snowColor, 4);
  }
  else if (weatherCode == 77) {
    // Snow grains
    drawSnow(gfx, x, y, size, snowColor, 3);
  }
  else if (weatherCode >= 80 && weatherCode <= 82) {
    // Rain showers: Slight, moderate, and violent
    drawRain(gfx, x, y, size, rainColor, weatherCode - 78);
  }
  else if (weatherCode >= 85 && weatherCode <= 86) {
    // Snow showers: Slight and heavy
    drawSnow(gfx, x, y, size, snowColor, weatherCode - 82);
  }
  else if (weatherCode == 95) {
    // Thunderstorm: Slight or moderate
    drawThunder(gfx, x, y, size, 0xFC00);
  }
  else if (weatherCode == 96 || weatherCode == 99) {
    // Thunderstorm with slight and heavy hail
    drawThunder(gfx, x, y, size, 0xFC00);
    drawSnow(gfx, x, y, size, snowColor, 2);
  }
  else {
    // Unknown weather code: draw a neutral cloudy icon (full size for consistency)
    drawCloud(gfx, x, y, size, cloudColor);
  }
}

void drawSun(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Draw sun circle
  gfx->fillCircle(x, y, size * 2/3, color);
  
  // Draw sun rays with proper floating-point rounding
  float rayRadius = size * 2.0f / 3.0f;  // Inner circle radius
  float rayLength = (float)size;  // Outer ray length
  
  for (int i = 0; i < 8; i++) {
    float angle = i * PI / 4.0f;
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    // Inner point on circle circumference (with proper rounding)
    int x1 = x + (int16_t)(cosA * rayRadius + (cosA >= 0 ? 0.5f : -0.5f));
    int y1 = y + (int16_t)(sinA * rayRadius + (sinA >= 0 ? 0.5f : -0.5f));
    
    // Outer point at ray end (with proper rounding)
    int x2 = x + (int16_t)(cosA * rayLength + (cosA >= 0 ? 0.5f : -0.5f));
    int y2 = y + (int16_t)(sinA * rayLength + (sinA >= 0 ? 0.5f : -0.5f));
    
    gfx->drawLine(x1, y1, x2, y2, color);
  }
}

void drawCloud(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Draw cloud shape using circles
  int r1 = size / 2;
  int r2 = size * 2/5;
  
  // Main cloud body
  gfx->fillCircle(x, y, r1, color);
  gfx->fillCircle(x - r1, y, r2, color);
  gfx->fillCircle(x + r1, y, r2, color);
  gfx->fillCircle(x - r1/2, y - r2, r2, color);
  gfx->fillCircle(x + r1/2, y - r2, r2, color);
}

void drawRain(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color, uint8_t drops) {
  // Draw cloud first
  drawCloud(gfx, x, y - size/2, size * 2/3, 0xC618);
  
  // Draw raindrops
  int dropSize = size / 10;
  int spacing = size / (drops + 1);
  int startX = x - (drops - 1) * spacing / 2;
  
  for (int i = 0; i < drops; i++) {
    int dropX = startX + i * spacing;
    int dropY = y + size/3;
    
    // Draw teardrop shape
    gfx->fillCircle(dropX, dropY, dropSize, color);
    gfx->fillTriangle(
      dropX - dropSize, dropY,
      dropX + dropSize, dropY,
      dropX, dropY + dropSize * 2,
      color
    );
  }
}

void drawSnow(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color, uint8_t flakes) {
  // Draw cloud if it's primary snow (not mixed with rain)
  if (flakes > 2) {
    drawCloud(gfx, x, y - size/2, size * 2/3, 0xC618);
  }
  
  // Draw snowflakes with proper integer arithmetic
  int flakeSize = size / 8;
  int spacing = size / (flakes + 1);
  int startX = x - (flakes - 1) * spacing / 2;
  int diagonalOffset = (int16_t)(flakeSize * 0.7f + 0.5f);  // Proper rounding for diagonal offset
  
  for (int i = 0; i < flakes; i++) {
    int flakeX = startX + i * spacing;
    int flakeY = y + size/3;
    
    // Draw asterisk-like snowflake
    gfx->drawLine(flakeX - flakeSize, flakeY, flakeX + flakeSize, flakeY, color);
    gfx->drawLine(flakeX, flakeY - flakeSize, flakeX, flakeY + flakeSize, color);
    gfx->drawLine(flakeX - diagonalOffset, flakeY - diagonalOffset, 
                 flakeX + diagonalOffset, flakeY + diagonalOffset, color);
    gfx->drawLine(flakeX - diagonalOffset, flakeY + diagonalOffset, 
                 flakeX + diagonalOffset, flakeY - diagonalOffset, color);
  }
}

void drawThunder(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Lightning bolt
  int boltWidth = size / 3;
  int boltHeight = size;
  
  // Draw lightning bolt shape
  gfx->fillTriangle(
    x, y,
    x + boltWidth, y + boltHeight/3,
    x - boltWidth/2, y + boltHeight/3,
    color
  );
  
  gfx->fillTriangle(
    x - boltWidth/2, y + boltHeight/3,
    x + boltWidth/2, y + boltHeight/3,
    x - boltWidth, y + boltHeight,
    color
  );
}

void drawFog(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Draw fog as horizontal lines with proper rounding
  int lineCount = 5;
  int lineWidth = (int16_t)(size * 1.5f + 0.5f);  // Proper rounding
  int lineHeight = size / 10;
  int lineSpacing = size / 5;
  
  for (int i = 0; i < lineCount; i++) {
    int lineY = y - size/2 + i * lineSpacing;
    int offset = (i % 2) * size/4; // Stagger lines
    gfx->fillRoundRect(x - lineWidth/2 + offset, lineY, lineWidth - offset*2, lineHeight, lineHeight/2, color);
  }
}

void drawPartlyCloudy(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t sunColor, uint16_t cloudColor) {
  // Draw sun slightly to the left and back
  drawSun(gfx, x - size/3, y, size * 2/3, sunColor);
  
  // Draw cloud overlapping sun partially
  drawCloud(gfx, x + size/3, y, size * 2/3, cloudColor);
}

void drawMist(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Similar to fog but with shorter, more dispersed lines
  // Uses deterministic pattern based on line index instead of randomness
  int lineCount = 7;
  int lineMaxWidth = (int16_t)(size * 1.2f + 0.5f);  // Proper rounding
  int lineHeight = size / 12;
  int lineSpacing = size / 6;
  
  for (int i = 0; i < lineCount; i++) {
    int lineY = y - size/2 + i * lineSpacing;
    // Deterministic pattern based on line index (no randomness)
    int lineWidth = lineMaxWidth - (i % 2) * (size/8);  // Alternate widths
    int offset = ((i / 2) % 2) * (size/6);  // Alternate offsets
    gfx->fillRoundRect(x - lineWidth/2 + offset, lineY, lineWidth - offset, lineHeight, lineHeight/2, color);
  }
}

void drawWarning(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Draw warning triangle
  int triangleSize = size;
  
  gfx->fillTriangle(
    x, y - triangleSize,
    x + triangleSize, y + triangleSize/2,
    x - triangleSize, y + triangleSize/2,
    color
  );
  
  // Draw exclamation mark
  int exclamH = triangleSize;
  int exclamW = triangleSize/5;
  gfx->fillRect(x - exclamW/2, y - triangleSize/2, exclamW, exclamH/2, 0);
  gfx->fillRect(x - exclamW/2, y + exclamH/4 - exclamW, exclamW, exclamW, 0);
}