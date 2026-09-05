#pragma once

#include <stdint.h>
#include "../data/spectrometer_data.h"
#include "templates.h"

// Poisson maximum-likelihood fit of the measured spectrum against the
// template set.
//
// The measured spectrum y is modelled as a non-negative mixture of the
// template columns:   y_c  ~  Poisson( sum_j A_cj x_j )
//
// Solving for x answers "how much of each nuclide is present", which is the
// question the device is actually asking. Three consequences follow, and they
// are the reason for replacing the peak-picker:
//
//  * Nuclides compete automatically. Two cannot both be credited with the
//    same counts, because those counts are apportioned by the likelihood.
//    All the hand-written tie-breakers (competitive assignment, share
//    margins, "better explained elsewhere") become unnecessary.
//  * Every nuclide gets a real error bar, so "detected" means a genuine
//    statistical statement (x / sigma) rather than a fraction of matched
//    lines - which was unavoidably 100% for any single-line nuclide.
//  * Counts nothing explains land in the continuum columns instead of being
//    awarded to whichever nuclide happens to have a line nearby.

struct FitResult {
  float amplitude;     // fitted counts attributable to this column
  float sigma;         // 1-sigma uncertainty on that amplitude
  float significance;  // amplitude / sigma
};

namespace Fit {

  // Runs the fit for the spectrum currently in d. Templates::build() must
  // have succeeded first. Returns false if there is nothing to fit.
  bool run(const SpectrometerData& d);

  int count();                       // one per template column
  const FitResult& at(int j);

  // Counts the model failed to account for, as a fraction of the total. A
  // large value means the library or the response model is missing
  // something, and any identification should be treated with suspicion.
  float residualFraction();

  // Reduced chi-square of the final model, as a goodness-of-fit check.
  float reducedChiSquare();

  // Model prediction per channel, for the spectrum chart.
  const float* model();

  // Lowest energy included in the likelihood. Anything below this was not
  // used, so the UI must not name such a line as a nuclide's evidence.
  float minEnergyKeV();
}
