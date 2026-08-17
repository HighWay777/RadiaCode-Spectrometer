#pragma once

#include <Arduino.h>

// Minimal theme struct — split out of the old app's ui_framework.h so that
// touch_feedback/page_transitions (generic, reusable) don't pull in the
// whole discarded app UI framework. Extend/populate for the spectrometer UI.
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
};

// Set by the application; touch_feedback/page_transitions fall back to
// sensible defaults when this is null.
extern UITheme* currentTheme;
