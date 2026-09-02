#pragma once

#include <stdint.h>
#include "spectrometer_data.h"

// What slice of the accumulation the analyser should look at.
//
// The RC-110 integrates internally and never forgets, which is exactly what
// makes a long run possible - and exactly what makes a source presented
// halfway through invisible. A new source adds counts linearly while the
// accumulated background it must be seen against grows as sqrt(t), so the
// evidence needed to notice it scales with the square root of however long
// the device happened to have been running. On a two-hour accumulation, real
// uranium ore accounts for 2.6% of the spectrum and reads as BACKGROUND.
//
// The device's answer to this was for the user to press RESET, which throws
// away the run. This module keeps the run and changes only what the analyser
// is shown: a baseline snapshot is held, and analysis operates on the counts
// accumulated since it.
//
// The full accumulation is untouched and stays on screen; nothing here
// discards data.

namespace SpectrumWindow {

  void begin();

  // Called on every spectrum poll, before analysis. Detects a step change in
  // count rate and rolls the baseline forward when one occurs, so presenting
  // a sample starts a fresh evidence window by itself.
  void update(const SpectrometerData& full);

  // Fills `out` with the windowed spectrum and its duration, copying the
  // remaining live fields from `full`. Analysis runs on this; results are
  // copied back to the shared struct by the caller.
  void view(const SpectrometerData& full, SpectrometerData& out);

  // Discard the window and start again from the current accumulation. Called
  // on RESET, and whenever the accumulation itself goes backwards.
  void rebase(const SpectrometerData& full);

  // Analyse the whole accumulation instead. Right for a genuinely weak sample
  // that needs hours of integration, wrong for walking around with the
  // detector, so it is a user setting rather than a fixed policy.
  void setCumulative(bool on);
  bool cumulative();

  uint32_t windowSeconds();     // acquisition time the analyser is seeing
  bool     windowed();          // false when the window is the whole run
}
