#pragma once

#include <stdint.h>

namespace ScreenSettings {
  void draw();
  void update();
  bool handleTap(int16_t x, int16_t y, bool* backPressed);
}
