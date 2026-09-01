#pragma once

#include <stdint.h>
#include "../data/spectrometer_data.h"
#include "../data/isotope_db.h"   // DecayChain

// Synthetic detector response per nuclide, for full-spectrum fitting.
//
// Why templates rather than peak picking: at 8.4% FWHM most of the counts a
// nuclide produces are NOT in its photopeaks. They are in the Compton
// continuum underneath, and neighbouring lines blend into single bumps. A
// peak finder throws that information away and then needs hand-written
// tie-breakers to decide which nuclide owns each surviving bump. Fitting the
// whole spectrum uses all of it, and makes the nuclides compete through the
// likelihood instead of through thresholds.
//
// Each column is the expected counts per channel from one unit of that
// nuclide's activity, normalised to sum to 1. The fit then reports how much
// of each is present, which is a physical quantity with an error bar.

// Smooth shapes that absorb what the library does not model: the sub-110 keV
// X-ray fluorescence forest, backscatter, cosmic continuum. Giving the fit
// somewhere to put these is what stops a nuclide being blamed for them.
//
// Implemented as broad overlapping bumps spaced across the energy range,
// rather than a few global exponentials. A handful of exponentials is too
// rigid to follow a real continuum, and the fit then recruits nuclide
// templates as continuum filler - measured on real background, where every
// nuclide with a low-energy line scored above 30 sigma purely by absorbing
// the X-ray forest.
//
// Each bump is far wider than the detector resolution (see CONTINUUM_WIDTH
// in templates.cpp), so the basis can model any smooth shape but cannot
// imitate a photopeak and steal real evidence from a nuclide.
#define CONTINUUM_BASIS 16

// Composite columns for the two natural decay chains, with their members
// locked at secular-equilibrium branching. See templates.cpp.
#define CHAIN_COLUMNS 2

namespace Templates {

  // Rebuilds the matrix for the current calibration and library. Cheap to
  // call; returns immediately when nothing relevant has changed.
  bool build(const SpectrometerData& d);

  // Total columns = nuclides + CONTINUUM_BASIS (+1 when a stored background
  // reference is in use).
  int count();
  int nuclideCount();          // columns [0, nuclideCount) map to library indices
  int continuumFirst();        // first continuum column
  // Column holding the whole chain in secular equilibrium, or -1.
  int chainColumn(DecayChain c);
  int backgroundColumn();      // -1 when no stored reference is active

  // Column j, SPECTRUM_CHANNELS long, normalised to unit sum.
  const float* column(int j);

  // Human-readable name for a column, for logging.
  const char* nameOf(int j);

  // Fraction of chain decays that pass through this member (Tl-208 is only
  // reached via Bi-212's 35.94% alpha branch). Needed by anything ranking
  // chain members by how much of the chain's signal they actually carry.
  float chainBranch(const char* symbol);

  void invalidate();           // force a rebuild (library reload, calib change)
}
