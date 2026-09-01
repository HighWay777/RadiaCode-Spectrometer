#pragma once

#include "../data/spectrometer_data.h"

// Turns the template fit into the answers the UI shows: ranked candidates,
// per-chain evidence, the plain-language verdict and the counting-quality
// figures.
//
// This replaces the score-and-gate cascade in peak_search.cpp. The difference
// that matters is where a decision comes from: every number here is derived
// from a fitted amplitude and its uncertainty, so "detected" means the same
// thing for every nuclide and does not need a per-nuclide rule to hold it up.

namespace Identify {

  // Fills d.candidates / d.uranium / d.thorium / d.sourceClass / d.quality.
  // Returns false if the fit could not run, leaving d untouched.
  bool run(SpectrometerData& d);

  // Clears the verdict hysteresis. Call when the accumulation is discarded.
  void reset();
}

// Is there something in front of the detector, as opposed to ordinary
// ambient? Single definition, because the Home screen and the verdict logic
// disagreeing about this produced a card reading "MEASURING..." while the
// analysis had already decided the sample was elevated.
//
// Answered from the fit where one is available - a detected chain or man-made
// nuclide means the analyser has found something - and otherwise from the
// live rates, which is all there is before the first analysis completes.
bool sampleIsElevated(const SpectrometerData& d);
