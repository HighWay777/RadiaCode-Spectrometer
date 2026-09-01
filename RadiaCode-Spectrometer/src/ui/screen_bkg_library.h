#pragma once

#include <stdint.h>

// Named background-reference slots. Reached by tapping LIBRARY on the
// Data screen; leaves via BACK. Not part of the swipe cycle.
namespace ScreenBkgLibrary {
  void draw();
  void update();
  // backPressed / importPressed report which navigation the tap requested,
  // so the caller can move to the Data screen or the import list.
  bool handleTap(int16_t x, int16_t y, bool* backPressed, bool* importPressed);
}
