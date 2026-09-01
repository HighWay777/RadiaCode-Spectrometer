#pragma once

#include <stdint.h>

// Lists RadiaCode XML exports found on the card and imports the chosen one
// into a background slot. Reached from the Background Library screen.
namespace ScreenImport {
  void draw();
  void update();
  bool handleTap(int16_t x, int16_t y, bool* backPressed);
}
