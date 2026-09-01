#include "identify.h"
#include "templates.h"
#include "fit.h"
#include "peak_search.h"              // fwhmKeVAt(), lastPeaks() for quality
#include "../data/isotope_library.h"
#include "../data/background.h"

#include <math.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdio.h>
#endif

#ifndef IDENTIFY_DEBUG
#define IDENTIFY_DEBUG 0
#endif

namespace Identify {

// A fitted amplitude is "detected" at 3 sigma. This is the ONE detection
// threshold in the whole identification path, and it means the same thing for
// every nuclide because every amplitude is measured the same way. The old
// path needed a different rule per situation (MIN_CANDIDATE_SIGMA,
// LOW_ENERGY_ONLY_SIGMA, ROI_PRIMARY_SIGMA, ROI_SUPPORT_SIGMA, the
// strongest-line gate, the two-distinct-peaks gate) precisely because its
// score was not a measurement.
#define DETECT_SIGMA 3.0f

// A verdict is a stronger claim than a detection, so it needs more than the
// bare detection limit.
#define VERDICT_SIGMA 5.0f

// Naming one chain rather than "mixed" requires it to be clearly the larger.
// Both chains share lines within one FWHM at this resolution, so the weaker
// one always picks up some amplitude from the stronger.
#define CHAIN_DOMINANCE 1.6f

// Ambient radon daughters and thorium in building materials are real, so a
// significant chain amplitude alone does not mean the SAMPLE contains one.
// With a stored background reference in use this is handled properly - the
// reference column absorbs ambient and the chain amplitude is already an
// excess - and this fraction test is skipped.
//
// Without a reference there is nothing to compare against, so the chain must
// account for a share of the spectrum that ambient never reaches. Measured on
// real captures: ambient background gives the Th-232 chain 1.9% of counts and
// U-238 0.7%, while the thorium mantle gives 18.0% and the uranium ore gives
// 7.4%. This replaces the old dose-rate and count-rate floors, which were two
// tuned numbers standing in for the same idea less directly.
#define CHAIN_MIN_FRACTION 0.04f

// Same idea for a man-made nuclide, which has no ambient baseline but is
// still subject to the response model being imperfect. Lower than the chain
// figure because an artificial source is a single nuclide rather than a whole
// chain, so it explains proportionally fewer counts.
#define ARTIFICIAL_MIN_FRACTION 0.02f

// With a stored reference the right comparison is against ambient itself, not
// against total counts: the fit reports how much of the spectrum the stored
// background explains, so a chain is a finding only when it is a real
// fraction of that. Measured - background fitted against itself gives the
// Th-232 chain 0.05 of the reference amplitude, the mantle 0.87, the ore 0.35
// for uranium and 0.15 for the thorium it cross-talks into.
//
// Without this the reference merely bypassed the elevation test, and
// subtracting a background from itself reported THORIUM at 70%.
#define CHAIN_MIN_VS_REFERENCE 0.20f

// Counting required before the absence of a source may be asserted.
#define BACKGROUND_MIN_SECONDS 600u
#define BACKGROUND_MIN_COUNTS  20000.0

// How many members of a detected chain to list as evidence, and how weak a
// member may be relative to the chain's strongest before listing it would
// assert evidence that is not actually visible at this resolution.
#define CHAIN_MEMBERS_SHOWN    3
#define CHAIN_MEMBER_MIN_SHARE 0.10f

// Below this the spectrum is lead/bismuth K X-ray fluorescence, backscatter
// and the Compton edge - none of it modelled, so none of it diagnostic.
#define XRAY_REGION_KEV 110.0f

// Hysteresis: a verdict must be harder to lose than it was to gain, or it
// flickers as evidence crosses the line. Kept from the old implementation
// because the problem it solves is real and independent of how the evidence
// is computed.
static bool s_heldUranium = false;
static bool s_heldThorium = false;

void reset() {
  s_heldUranium = false;
  s_heldThorium = false;
}

// Maps a detection significance to a 0..1 confidence for display.
//
// This is a presentation choice, not a probability: it saturates smoothly so
// that 3 sigma reads as "borderline" and 20 sigma as "certain", without ever
// printing 100%. The old path could not do this - a single-line nuclide
// scored matchedDetect/totalDetect = 1.0 the instant its one line matched,
// so K-40 and Ra-226 always displayed 100% on a full bar regardless of how
// marginal the evidence was.
static float confidenceOf(float sigma) {
  if (sigma <= DETECT_SIGMA) return 0.0f;
  float c = 1.0f - 0.5f * expf(-(sigma - DETECT_SIGMA) / 4.0f);
  return c > 0.99f ? 0.99f : c;
}

struct Detection {
  int      column;
  uint16_t iso;         // library index, or 0xFFFF for a chain column
  float    sigma;
  float    amplitude;
  float    fraction;    // share of total counts
};

static int addCandidate(SpectrometerData& d, const char* symbol, const char* name,
                        float sigma, uint8_t matched, uint8_t total,
                        float energyKeV) {
  if (d.candidateCount >= MAX_CANDIDATES) return 0;
  IsotopeCandidate& c = d.candidates[d.candidateCount];
  c.symbol = symbol;
  c.name = name;
  c.confidence = confidenceOf(sigma);
  c.matchedEnergyKeV = energyKeV;
  c.linesMatched = matched;
  c.linesTotal = total;
  c.significance = sigma;
  d.candidateCount++;
  return 1;
}

// The most detectable line of a nuclide, for the "matched energy" the UI
// shows. Under fitting there is no single matched peak - the whole shape is
// fitted - so this reports the line that dominates the template instead of
// implying one peak was picked out.
static float dominantLine(const IsotopeDef& def) {
  float best = 0.0f, bestWeight = 0.0f;
  for (uint8_t li = 0; li < def.lineCount; li++) {
    float e = def.lines[li].energyKeV;
    if (e <= 0.0f) continue;
    // Only lines the fit actually used. Eu-152 was displayed as "best
    // 121.8 keV" while that line sits below the fit range and contributed
    // nothing - the evidence was really its 344 keV line overlapping the
    // uranium chain. Naming an unused line misrepresents what was measured.
    if (e < Fit::minEnergyKeV()) continue;
    float w = def.lines[li].intensity * (e >= 200.0f ? powf(200.0f / e, 1.2f) : 1.0f);
    if (w > bestWeight) { bestWeight = w; best = e; }
  }
  return best;
}

bool run(SpectrometerData& d) {
  if (!Templates::build(d)) return false;
  if (!Fit::run(d)) return false;

  double total = 0.0;
  for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) total += d.spectrum[ch];
  if (total <= 0.0) return false;

  bool haveReference = (Templates::backgroundColumn() >= 0);

  // --- chains ---------------------------------------------------------------
  int uCol = Templates::chainColumn(CHAIN_U238);
  int tCol = Templates::chainColumn(CHAIN_TH232);
  float uSigma = (uCol >= 0) ? Fit::at(uCol).significance : 0.0f;
  float tSigma = (tCol >= 0) ? Fit::at(tCol).significance : 0.0f;
  float uFrac  = (uCol >= 0) ? (float)(Fit::at(uCol).amplitude / total) : 0.0f;
  float tFrac  = (tCol >= 0) ? (float)(Fit::at(tCol).amplitude / total) : 0.0f;

  // Ambient chains are present in every room. Without a reference to subtract
  // them, require the chain to be a large enough share of the spectrum that
  // ambient cannot explain it.
  float refAmp = 0.0f;
  if (haveReference) refAmp = Fit::at(Templates::backgroundColumn()).amplitude;

  bool uElevated, tElevated;
  if (haveReference && refAmp > 0.0f) {
    uElevated = (Fit::at(uCol).amplitude >= refAmp * CHAIN_MIN_VS_REFERENCE);
    tElevated = (Fit::at(tCol).amplitude >= refAmp * CHAIN_MIN_VS_REFERENCE);
  } else {
    uElevated = (uFrac >= CHAIN_MIN_FRACTION);
    tElevated = (tFrac >= CHAIN_MIN_FRACTION);
  }

  d.uranium.confidence = (uSigma >= DETECT_SIGMA && uElevated) ? confidenceOf(uSigma) : 0.0f;
  d.thorium.confidence = (tSigma >= DETECT_SIGMA && tElevated) ? confidenceOf(tSigma) : 0.0f;

  // membersDetected is now "how many members the fitted chain amplitude
  // implies are contributing measurably", which under an equilibrium
  // constraint means all of them once the chain itself is detected.
  d.uranium.membersTotal = 0;
  d.thorium.membersTotal = 0;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    DecayChain ch = IsotopeLibrary::at(iso).chain;
    if (ch == CHAIN_U238) d.uranium.membersTotal++;
    if (ch == CHAIN_TH232) d.thorium.membersTotal++;
  }
  d.uranium.membersDetected = (d.uranium.confidence > 0.0f) ? d.uranium.membersTotal : 0;
  d.thorium.membersDetected = (d.thorium.confidence > 0.0f) ? d.thorium.membersTotal : 0;

  // --- candidates -----------------------------------------------------------
  // Standalone nuclides ranked by significance, then the detected chains
  // represented by their most diagnostic member so the list stays readable.
  d.candidateCount = 0;

  // File scope, not stack: ISOTOPE_LIBRARY_MAX entries is ~6 KB and this
  // runs on the BLE task, which has 12 KB and a deep Bluedroid call chain
  // beneath it.
  static Detection order[ISOTOPE_LIBRARY_MAX];
  int n = 0;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count() && n < ISOTOPE_LIBRARY_MAX; iso++) {
    if (IsotopeLibrary::at(iso).chain != CHAIN_NONE) continue;   // fitted via chain
    const FitResult& r = Fit::at((int)iso);
    if (r.significance < DETECT_SIGMA) continue;
    // The same test the verdict applies. Without it the candidate CARD showed
    // Co-60 at 76% and Na-22 at 60% on clean background - nuclides that
    // reached 3 sigma by fitting continuum residuals while explaining well
    // under 1% of the counts. The headline verdict rejected them; the list
    // the user actually reads did not, which is the more alarming of the two.
    const IsotopeDef& cdef = IsotopeLibrary::at(iso);
    bool cManMade = (cdef.category == CAT_ARTIFICIAL || cdef.category == CAT_MEDICAL ||
                     cdef.category == CAT_CALIBRATION);
    if (cManMade && (float)(r.amplitude / total) < ARTIFICIAL_MIN_FRACTION) continue;
    order[n].column = (int)iso;
    order[n].iso = iso;
    order[n].sigma = r.significance;
    order[n].amplitude = r.amplitude;
    order[n].fraction = (float)(r.amplitude / total);
    n++;
  }
  for (int a = 0; a < n; a++) {
    for (int b = a + 1; b < n; b++) {
      if (order[b].sigma > order[a].sigma) {
        Detection t = order[a]; order[a] = order[b]; order[b] = t;
      }
    }
  }

  // Chains first when they are the dominant finding, since a natural sample
  // is described by its chain rather than by any one daughter.
  struct { float sigma; DecayChain chain; } chains[2] = {
    { d.uranium.confidence > 0.0f ? uSigma : 0.0f, CHAIN_U238 },
    { d.thorium.confidence > 0.0f ? tSigma : 0.0f, CHAIN_TH232 },
  };
  if (chains[1].sigma > chains[0].sigma) {
    float s = chains[0].sigma; DecayChain c = chains[0].chain;
    chains[0].sigma = chains[1].sigma; chains[0].chain = chains[1].chain;
    chains[1].sigma = s; chains[1].chain = c;
  }

  for (int k = 0; k < 2; k++) {
    if (chains[k].sigma < DETECT_SIGMA) continue;

    // List the members that actually carry the chain's signal, not just one.
    //
    // The identification rests on the WHOLE chain pattern fitting, so showing
    // a single daughter hides the corroboration that makes it trustworthy -
    // and reads exactly like the single-line evidence this pipeline replaced.
    // A thorium mantle should show Pb-212, Tl-208 and Ac-228 together.
    //
    // They share one significance because they share one fitted amplitude:
    // under the secular-equilibrium constraint there is one measurement, not
    // three independent ones. The screens tag them "Th chain"/"U chain", so
    // that reads correctly rather than as three separate confirmations.
    struct { uint16_t iso; float weight; } member[ISOTOPE_LIBRARY_MAX];
    int nm = 0;
    for (uint16_t iso = 0; iso < IsotopeLibrary::count() && nm < ISOTOPE_LIBRARY_MAX; iso++) {
      const IsotopeDef& def = IsotopeLibrary::at(iso);
      if (def.chain != chains[k].chain) continue;
      float best = 0.0f;
      for (uint8_t li = 0; li < def.lineCount; li++) {
        float e = def.lines[li].energyKeV;
        // Below the X-ray region nothing is diagnostic: that part of the
        // spectrum is unmodelled fluorescence, so a member whose only lines
        // are there (Th-234 at 63/92 keV) is not evidence a user can act on.
        if (e < XRAY_REGION_KEV) continue;
        // Weighted by branch as well as intensity and efficiency. A member on
        // a minor branch carries little of the chain: U-235 otherwise led the
        // U-238 chain on its intense 185.7 keV line despite reaching only
        // 4.6% of decays, naming real ore "U-235" instead of Bi-214.
        float w = def.lines[li].intensity * Templates::chainBranch(def.symbol) *
                  (e >= 200.0f ? powf(200.0f / e, 1.2f) : 1.0f);
        if (w > best) best = w;
      }
      if (best <= 0.0f) continue;
      member[nm].iso = iso;
      member[nm].weight = best;
      nm++;
    }
    for (int a = 0; a < nm; a++) {
      for (int b = a + 1; b < nm; b++) {
        if (member[b].weight > member[a].weight) {
          uint16_t ti = member[a].iso; float tw = member[a].weight;
          member[a] = member[b];
          member[b].iso = ti; member[b].weight = tw;
        }
      }
    }

    // Keep room for the other chain and for any standalone nuclide.
    int shown = 0;
    for (int m = 0; m < nm && shown < CHAIN_MEMBERS_SHOWN; m++) {
      // A member far weaker than the chain's strongest would not be visible
      // in this spectrum even though the chain is present, so listing it
      // would claim evidence that is not there.
      if (member[m].weight < member[0].weight * CHAIN_MEMBER_MIN_SHARE) break;
      if (d.candidateCount >= MAX_CANDIDATES - 1) break;
      const IsotopeDef& def = IsotopeLibrary::at(member[m].iso);
      addCandidate(d, def.symbol, def.name, chains[k].sigma,
                   def.lineCount, def.lineCount, dominantLine(def));
      shown++;
    }
  }

  for (int i = 0; i < n && d.candidateCount < MAX_CANDIDATES; i++) {
    const IsotopeDef& def = IsotopeLibrary::at(order[i].iso);
    addCandidate(d, def.symbol, def.name, order[i].sigma,
                 def.lineCount, def.lineCount, dominantLine(def));
  }

  // --- verdict --------------------------------------------------------------
  s_heldUranium = s_heldUranium ? (uSigma >= DETECT_SIGMA && uElevated)
                                : (uSigma >= VERDICT_SIGMA && uElevated);
  s_heldThorium = s_heldThorium ? (tSigma >= DETECT_SIGMA && tElevated)
                                : (tSigma >= VERDICT_SIGMA && tElevated);

  float bestArtificial = 0.0f;
  const IsotopeDef* artificial = nullptr;
  float k40Sigma = 0.0f;
  for (int i = 0; i < n; i++) {
    const IsotopeDef& def = IsotopeLibrary::at(order[i].iso);
    if (strcmp(def.symbol, "K-40") == 0) k40Sigma = order[i].sigma;
    bool manMade = (def.category == CAT_ARTIFICIAL || def.category == CAT_MEDICAL ||
                    def.category == CAT_CALIBRATION);
    // A nuclide claiming to be a finding must account for a real share of the
    // spectrum, not merely reach significance. The response model is
    // approximate, and on a long background run its small mismatches at high
    // energy accumulated enough statistical weight for Co-60 to cross 5 sigma
    // while explaining 0.8% of the counts - reported as ARTIFICIAL on a
    // spectrum containing nothing man-made. Significance says the amplitude
    // is not zero; it does not say the model is right.
    if (manMade && order[i].fraction < ARTIFICIAL_MIN_FRACTION) continue;
    if (manMade && order[i].sigma > bestArtificial) {
      bestArtificial = order[i].sigma;
      artificial = &def;
    }
  }

  d.verdictConfidence = 0.0f;
  SourceClass verdict = SRC_INSUFFICIENT;

  if (bestArtificial >= VERDICT_SIGMA && bestArtificial > uSigma && bestArtificial > tSigma) {
    // A man-made nuclide is a finding at any level: unlike a natural chain it
    // has no ambient baseline to rise above, so no elevation test applies.
    verdict = SRC_ARTIFICIAL;
    d.verdictConfidence = confidenceOf(bestArtificial);
    (void)artificial;
  } else if (s_heldUranium && s_heldThorium) {
    if (uSigma >= tSigma * CHAIN_DOMINANCE) {
      verdict = SRC_URANIUM;
      d.verdictConfidence = d.uranium.confidence;
    } else if (tSigma >= uSigma * CHAIN_DOMINANCE) {
      verdict = SRC_THORIUM;
      d.verdictConfidence = d.thorium.confidence;
    } else {
      verdict = SRC_MIXED_NATURAL;
      d.verdictConfidence = confidenceOf(uSigma < tSigma ? uSigma : tSigma);
    }
  } else if (s_heldUranium) {
    verdict = SRC_URANIUM;
    d.verdictConfidence = d.uranium.confidence;
  } else if (s_heldThorium) {
    verdict = SRC_THORIUM;
    d.verdictConfidence = d.thorium.confidence;
  } else if (k40Sigma >= 12.0f) {
    // K-40 is in every wall and every bag of fertiliser, so detecting it is
    // not a finding. Only call it when it genuinely dominates.
    verdict = SRC_POTASSIUM;
    d.verdictConfidence = confidenceOf(k40Sigma);
  }

  // --- quality --------------------------------------------------------------
  // The significance the verdict actually rests on. The old implementation
  // stored candidates[0].significance here while calling it "weakest", so the
  // grade, the progress bar and the "measurement complete" claim were all
  // computed from the strongest line instead of the limiting one.
  float driving = 0.0f;
  switch (verdict) {
    case SRC_URANIUM:       driving = uSigma; break;
    case SRC_THORIUM:       driving = tSigma; break;
    case SRC_MIXED_NATURAL: driving = (uSigma < tSigma) ? uSigma : tSigma; break;
    case SRC_ARTIFICIAL:    driving = bestArtificial; break;
    case SRC_POTASSIUM:     driving = k40Sigma; break;
    default:                driving = (d.candidateCount > 0) ? d.candidates[0].significance : 0.0f;
  }

  d.quality.totalCounts = (uint32_t)total;
  d.quality.peaksFound = (uint8_t)lastPeakCount();
  d.quality.weakestKeySigma = driving;
  d.quality.bestSigma = (d.candidateCount > 0) ? d.candidates[0].significance : 0.0f;

  if (driving >= 12.0f)     d.quality.grade = QUAL_EXCELLENT;
  else if (driving >= 6.0f) d.quality.grade = QUAL_GOOD;
  else if (driving >= 3.0f) d.quality.grade = QUAL_FAIR;
  else                      d.quality.grade = QUAL_POOR;

  // Significance grows as sqrt(t), so the time to reach a target is a real
  // extrapolation from where we are now.
  uint32_t elapsed = d.spectrumDurationSec;
  if (driving <= 0.1f || elapsed == 0) {
    d.quality.recommendedSeconds = 300;
  } else {
    float factor = 12.0f / driving;
    float needed = (float)elapsed * factor * factor;
    if (needed > 7200.0f) needed = 7200.0f;
    if (needed < (float)elapsed) needed = (float)elapsed;
    d.quality.recommendedSeconds = (uint32_t)needed;
  }
  d.quality.remainingSeconds = (d.quality.recommendedSeconds > elapsed)
                             ? (d.quality.recommendedSeconds - elapsed) : 0;

  // "BACKGROUND" is a positive claim - that there is nothing here - and early
  // in a run it is not one we can support. Until there has been enough
  // counting to have found something, the honest answer is still measuring.
  // "BACKGROUND" asserts that nothing is there, and that is only honest once
  // there has been enough counting to have seen something if it were.
  //
  // Measured on the real ore capture: the uranium chain does not reach the
  // verdict threshold until ~600 s. The old floor of 120 s therefore printed
  // BACKGROUND with a genuine uranium source in front of the detector for
  // eight minutes before changing its mind - worse than saying nothing,
  // because it invites the user to conclude the sample is inert.
  if (verdict == SRC_INSUFFICIENT) {
    if (elapsed >= BACKGROUND_MIN_SECONDS && total >= BACKGROUND_MIN_COUNTS) {
      verdict = SRC_BACKGROUND;
    }
  }
  d.sourceClass = verdict;

#if IDENTIFY_DEBUG
  Serial.printf("[ID] U %.1f sigma (%.1f%%)  Th %.1f sigma (%.1f%%)  art %.1f  -> %s\n",
                uSigma, uFrac * 100.0f, tSigma, tFrac * 100.0f, bestArtificial,
                sourceClassLabel(d.sourceClass));
#endif
  return true;
}

} // namespace Identify

bool sampleIsElevated(const SpectrometerData& d) {
  // A verdict, or any listed candidate, means the analysis found something.
  if (d.sourceClass == SRC_URANIUM || d.sourceClass == SRC_THORIUM ||
      d.sourceClass == SRC_MIXED_NATURAL || d.sourceClass == SRC_ARTIFICIAL ||
      d.sourceClass == SRC_POTASSIUM) {
    return true;
  }
  if (d.candidateCount > 0) return true;
  // Before the first analysis lands there is only the live rate. These are
  // about 1.5x typical indoor ambient on this detector (0.13-0.18 uSv/h at
  // ~22 cps), so they read "something is here" without waiting for counts.
  return d.doseRateUSvH >= 0.20f || d.cps >= 35.0f;
}
