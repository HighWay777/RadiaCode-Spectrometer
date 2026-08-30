#pragma once

#include <stdint.h>

namespace ScreenIsotopeDetail {
  // Snapshots g_data.candidates[index] at call time (call before navigating
  // here) so the detail view stays stable even if the live candidate list
  // reorders while it's on screen.
  void setCandidateIndex(int index);

  void draw();
  void update();

  // Returns true if the Back button was tapped.
  bool handleTap(int16_t x, int16_t y);
}
