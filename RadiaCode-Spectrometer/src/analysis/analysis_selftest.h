#pragma once

// Offline regression tests for the identification pipeline.
//
// Why this exists: every identification bug in this project was found by
// putting a rock in front of the detector, and every fix was verified the same
// way - which means a change that fixed one nuclide could silently break
// another and nobody would know until the next field test. That happened:
// suppressing a Ba-133 false positive removed Pb-214 from real uranium ore,
// and it took a whole session on hardware to find.
//
// These tests build synthetic spectra from the nuclide library itself, with
// the detector's own resolution model, a realistic Compton continuum and
// reproducible Poisson noise, then run the real analyzeSpectrum() over them
// and assert what must and must not be identified. No detector, no sources,
// no waiting for counts.
//
// Enable with -DANALYSIS_SELFTEST=1; results print to Serial at boot and the
// device continues normally afterwards. Never enabled in a release build.
namespace AnalysisSelfTest {
  // Returns true if every case passed.
  bool run();
}
