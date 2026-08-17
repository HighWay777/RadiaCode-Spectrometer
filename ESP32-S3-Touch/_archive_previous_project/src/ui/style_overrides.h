#pragma once

#include <Arduino.h>
#include <map>
#include <SPIFFS.h>
#include <ArduinoJson.h>

/**
 * Style Override System
 * 
 * This system allows customization of built-in page elements (colors, visibility)
 * without redrawing the entire rendering code.
 * 
 * Element IDs follow the pattern: "page.element" e.g., "home.clock", "home.weather"
 */

struct StyleOverride {
  bool hasColor = false;
  bool hasTextColor = false;
  bool hasFontSize = false;
  bool hidden = false;
  uint16_t color = 0;       // Background/fill color (RGB565)
  uint16_t textColor = 0;   // Text color (RGB565)
  uint8_t fontSize = 2;     // Font size (1-8)
};

class StyleOverrides {
public:
  // Initialize and load from SPIFFS
  static void init();
  
  // Save current overrides to SPIFFS
  static bool save();
  
  // Get override for an element (returns nullptr if no override)
  static StyleOverride* get(const char* elementId);
  
  // Set override for an element
  static void set(const char* elementId, const StyleOverride& style);
  
  // Remove override for an element
  static void remove(const char* elementId);
  
  // Clear all overrides
  static void clear();
  
  // Get color with fallback to default
  static uint16_t getColor(const char* elementId, uint16_t defaultColor);
  
  // Get text color with fallback to default
  static uint16_t getTextColor(const char* elementId, uint16_t defaultColor);
  
  // Check if element is hidden
  static bool isHidden(const char* elementId);
  
  // Convert hex string "#RRGGBB" to RGB565
  static uint16_t hexToRgb565(const char* hex);
  
  // Convert RGB565 to hex string
  static String rgb565ToHex(uint16_t color);
  
  // Get font size with fallback to default
  static uint8_t getFontSize(const char* elementId, uint8_t defaultSize);

private:
  static std::map<String, StyleOverride> overrides;
  static bool initialized;
  static const char* STYLES_FILE;
};
