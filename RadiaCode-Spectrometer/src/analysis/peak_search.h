#pragma once

#include "../data/spectrometer_data.h"

#define MAX_DETECTED_PEAKS 12

struct DetectedPeak {
  int channel;
  float energyKeV;
  uint32_t netCounts; // peak height above local continuum
};

// Classical peak-search + energy-library matching (see CLAUDE.md, option 1).
// Trivial CPU/memory cost — this is meant to run every update tick on-device.
// Writes results into d.candidates/d.candidateCount, sorted by confidence desc.
void analyzeSpectrum(SpectrometerData& d);

// Exposed separately so the spectrum chart screen can mark peak positions.
int findPeaks(const SpectrometerData& d, DetectedPeak* out, int maxPeaks);
