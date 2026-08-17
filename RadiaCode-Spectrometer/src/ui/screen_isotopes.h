#pragma once

#include <stdint.h>

namespace ScreenIsotopes {
  void draw();    // full redraw — call on screen entry
  void update();  // refresh candidate cards — call periodically

  // Returns the tapped candidate's index (0..candidateCount-1) if the tap
  // landed on a populated slot, else -1 (empty slot or outside the grid).
  int handleTap(int16_t x, int16_t y);
}
