#pragma once

#include <stdint.h>

namespace ScreenBackground {
  void draw();
  void update();
  // libraryPressed reports that the caller should navigate to the slot
  // library / import screen.
  bool handleTap(int16_t x, int16_t y, bool* libraryPressed);
}
