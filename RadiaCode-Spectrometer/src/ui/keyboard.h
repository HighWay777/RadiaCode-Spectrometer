#pragma once

#include <stdint.h>
#include <stddef.h>

// Full-screen on-screen keyboard, so WiFi credentials can be typed on the
// device itself and kept in NVS. That is what allows the firmware and its
// source to be published without embedding anyone's network details.
namespace Keyboard {
  // Opens the editor. `title` labels the field, `initial` pre-fills it.
  // `mask` hides the text (passwords). Draws immediately.
  void open(const char* title, const char* initial, bool mask, size_t maxLen);

  void draw();

  // Feed taps while open. Returns true if consumed.
  bool handleTap(int16_t x, int16_t y);

  bool isOpen();
  bool wasAccepted();   // true once OK was pressed
  bool wasCancelled();
  void consumeResult(); // clears accepted/cancelled latches
  const char* text();
}
