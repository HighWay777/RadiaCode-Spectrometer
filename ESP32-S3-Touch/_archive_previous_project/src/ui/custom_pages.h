#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <vector>
#include "ui_framework.h"
#include "../config/config_store.h"

// Custom page/widget model
struct CustomWidget {
  String type;       // "label", "value", "icon"
  String binding;    // e.g., "currentTemp", "humidity" for value widgets
  String text;       // for label widgets
  uint16_t color;    // 16-bit RGB565 color (background/fill color)
  uint16_t textColor; // 16-bit RGB565 text color
  uint8_t fontSize;  // 1..3
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct CustomPage {
  uint8_t id;        // PAGE_COUNT + index
  String name;       // display name
  uint8_t base;      // optional built-in base page (0..PAGE_COUNT-1) or 255 for none
  std::vector<CustomWidget> widgets;
};

// Lifecycle
void customPagesInit();
void customPagesReload();

// Accessors
uint8_t customPagesCount();

// Draw
void drawCustomPageById(uint8_t pageId);
bool isCustomPageId(uint8_t pageId);

// Overlays for built-in pages
void drawBuiltinOverlay(uint8_t base);
