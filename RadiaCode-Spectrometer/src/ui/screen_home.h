#pragma once

#include <stdint.h>

namespace ScreenHome {
  void draw();                              // full redraw — call on screen entry
  void update();                            // partial redraw of live values — call periodically
  // settingsPressed reports that the caller should open the Settings screen.
  bool handleTap(int16_t x, int16_t y, bool* settingsPressed);
}
