#pragma once

#include <stdint.h>

namespace ScreenHome {
  void draw();                              // full redraw — call on screen entry
  void update();                            // partial redraw of live values — call periodically
  bool handleTap(int16_t x, int16_t y);     // returns true if the tap was consumed (e.g. connect button)
}
