#pragma once

#include <stdint.h>

namespace ScreenData {
  void draw();
  void update();
  bool handleTap(int16_t x, int16_t y);

  // True once after the LIBRARY button was tapped, so main.cpp can navigate
  // to the background-library sub-screen. Self-clearing.
  bool wantsLibrary();
}
