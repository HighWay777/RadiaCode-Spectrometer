#pragma once

#include "../data/spectrometer_data.h"

// Peaks kept per analysis pass. Raised from the original 12 after a real
// hardware failure: the old code scanned channels upward and stopped at the
// cap, so on a uranium-ore spectrum (dense low-energy forest) every slot was
// consumed below ~650 keV and the diagnostic high-energy lines
// (Bi-214 1120/1764 keV, Pa-234m 1001 keV) were never examined at all.
// Peaks are now ranked by statistical significance, not by scan order.
#define MAX_DETECTED_PEAKS 24

struct DetectedPeak {
  int channel;          // channel of the maximum
  float energyKeV;      // sub-channel interpolated centroid energy
  float netCounts;      // continuum-subtracted area over ~1 FWHM
  float significance;   // netArea / sqrt(backgroundArea), i.e. "sigma"
};

// Classical peak search + energy-library matching (CLAUDE.md, option 1).
// Writes d.candidates/d.candidateCount (sorted by confidence), the decay
// chain evidence, the plain-language d.sourceClass, and d.quality.
void analyzeSpectrum(SpectrometerData& d);

// Runs the full detection pass. Callers that just want to draw the peaks
// found by the last analyzeSpectrum() should use lastPeaks() instead —
// it avoids recomputing and keeps the chart consistent with the ID list.
int findPeaks(const SpectrometerData& d, DetectedPeak* out, int maxPeaks);

const DetectedPeak* lastPeaks();
int lastPeakCount();

// Views of the last analysis pass, so the spectrum chart can show what the
// identification actually worked on. Without these the chart plotted the raw
// spectrum unconditionally and background subtraction was invisible — the
// feature appeared to do nothing.
//   correctedSpectrum() : sample minus stored background (may be negative)
//   backgroundCounts()  : background counts expected over this acquisition
// Both are valid only after analyzeSpectrum() has run at least once.
const float* correctedSpectrum();
const float* backgroundCounts();
bool backgroundApplied();

// Detector resolution model, anchored to the measured RC-110 figure of
// 8.4% FWHM at 662 keV. Scintillator resolution is photon-statistics
// dominated, so FWHM scales as sqrt(E): FWHM(E) = 0.084 * sqrt(662 * E).
float fwhmKeVAt(float energyKeV);
