#pragma once

#include <Arduino.h>

// Theme struct kept separate from the drawing code (cards.h) so
// touch_feedback/page_transitions (generic, carried over from the board base)
// don't need to depend on the app-specific card layout code.
struct UITheme {
  uint16_t background;
  uint16_t header;
  uint16_t card;
  uint16_t cardBorder;
  uint16_t text;
  uint16_t textSecondary;
  uint16_t accent;
  uint16_t warm;
  uint16_t cool;
  uint16_t warning;
  uint16_t danger;
  uint16_t highlight; // top isotope match / active-state border
};

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

extern UITheme APP_THEME;
extern UITheme* currentTheme; // set to &APP_THEME in main.cpp setup()
