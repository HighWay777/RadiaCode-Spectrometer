#pragma once

namespace ScreenSpectrum {
  void draw();    // full redraw — call on screen entry
  void update();  // rebin spectrum + redraw plot area — call periodically
}
