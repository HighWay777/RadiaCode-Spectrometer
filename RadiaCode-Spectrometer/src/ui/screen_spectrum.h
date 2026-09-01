#pragma once

#include <stdint.h>

namespace ScreenSpectrum {
  void draw();    // full redraw — call on screen entry
  void update();  // rebin spectrum + redraw plot area — call periodically

  // Tapping the plot toggles log/linear vertical scale. Returns true if
  // the tap was consumed.
  bool handleTap(int16_t x, int16_t y);
}
