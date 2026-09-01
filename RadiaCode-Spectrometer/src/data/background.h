#pragma once

#include <stdint.h>
#include "spectrometer_data.h"

// Persistent background reference spectrum.
//
// Why this matters: ambient background is not noise, it is real signal that
// simply is not your sample — room air contains radon daughters (Bi-214,
// Pb-214, genuine U-238 chain members) and building materials contain K-40.
// Near background level, no amount of extra measuring time separates a faint
// sample from those, because they grow at the same rate. Subtracting a stored
// reference is the only way to do trace work.
//
// Stored in NVS as counts/second per channel (uint16 milli-counts/s, 2 KB),
// so it can be scaled to any later acquisition time.
namespace Background {
  void begin();               // load reference + enabled flag from NVS
  bool available();           // a reference spectrum is stored
  bool enabled();             // user wants subtraction (independent of availability)
  bool active();              // available() && enabled() — what analysis checks
  void setEnabled(bool on);   // persisted

  // Snapshots the currently accumulated spectrum as the new reference.
  // Rejects acquisitions shorter than MIN_RECORD_SECONDS, since a noisy
  // reference would inject its own noise into every later measurement.
  static const uint32_t MIN_RECORD_SECONDS = 30;
  bool record(const SpectrometerData& d);

  void erase();

  // Installs a reference from raw per-channel rates (counts/second) and
  // persists it, exactly as record() would. Used when loading a saved
  // reference back off the SD card.
  bool setFromRates(const float* cps, uint32_t durationSec, uint32_t totalCounts);

  float rateAt(int channel);  // counts/second, 0 if none stored
  uint32_t durationSec();     // how long the stored reference was measured for
  uint32_t totalCounts();     // total counts in the stored reference

  // Where the active reference came from ("HOME", "recorded here", ...).
  // Without this the UI can say subtraction is on but not *which* background
  // is being subtracted, which was the single most confusing thing about the
  // first version of this feature.
  const char* label();
  void setLabel(const char* text);   // persisted alongside the data
}
