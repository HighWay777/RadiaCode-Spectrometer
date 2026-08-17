#pragma once

#include <Arduino_GFX_Library.h>

// Small card-based drawing toolkit shared by all screens. Hand-rolled on top
// of Arduino_GFX (not LVGL) — see CLAUDE.md "LVGL decision point" for why,
// and reconsider if the UI outgrows this.
namespace Cards {
  void init(Arduino_GFX* display);

  int16_t textWidth(const char* text, uint8_t textSize);
  void leftText(int16_t x, int16_t y, const char* text, uint8_t textSize, uint16_t color);
  void centeredText(int16_t cx, int16_t y, const char* text, uint8_t textSize, uint16_t color);

  // Card outline + optional small title in the top-left corner. Call once per
  // full screen draw; content is drawn separately so it can be partially
  // updated without redrawing the border every tick.
  void frame(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, bool highlighted = false);

  // Clears just the given sub-rect and draws a big value + small unit label.
  // Safe to call every tick for a live-updating number inside a card.
  void statValueArea(int16_t x, int16_t y, int16_t w, int16_t h, const char* value, const char* unit, uint16_t valueColor);

  // Solid button (e.g. Connect/Disconnect).
  void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t fillColor);

  // Horizontal 0..1 fraction bar (confidence, battery, etc.)
  void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction, uint16_t color);
}
