#include "peak_search.h"
#include "../data/isotope_library.h"
#include "../data/background.h"
#include <math.h>
#include <string.h>

// Prints the detected peak list and the winning nuclide scores to Serial on
// every analysis pass. Invaluable for diagnosing a questionable ID against a
// known sample - set to 0 to silence.
#define PEAK_SEARCH_DEBUG 1

#if PEAK_SEARCH_DEBUG
#include <Arduino.h>
#endif

static float smoothed[SPECTRUM_CHANNELS];
static float continuum[SPECTRUM_CHANNELS];
static float net[SPECTRUM_CHANNELS];

// Stored-background counts expected in this acquisition, and the sample with
// them removed. Zero-filled when no reference is stored or subtraction is off,
// so the whole pipeline below is identical either way.
static float bgCounts[SPECTRUM_CHANNELS];
static float corrected[SPECTRUM_CHANNELS];
static float bgVarScale = 0.0f; // acquisition time / background measurement time

static void prepareSpectra(const SpectrometerData& d) {
  float T = (float)d.spectrumDurationSec;
  bool useBg = Background::active() && T > 0.0f;
  uint32_t bgDur = Background::durationSec();
  // A reference measured for far longer than the sample contributes almost no
  // uncertainty of its own; a short one does. This is why record() insists on
  // a minimum duration.
  bgVarScale = (useBg && bgDur > 0) ? (T / (float)bgDur) : 0.0f;

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    bgCounts[i] = useBg ? Background::rateAt(i) * T : 0.0f;
    corrected[i] = (float)d.spectrum[i] - bgCounts[i];
  }
}

// Counting uncertainty of a background-subtracted net area over [lo,hi]:
//   var = var(sample) + var(stored background) + var(continuum estimate)
// Keeping all three terms is what stops subtraction from silently inflating
// apparent significance - removing counts also removes certainty.
static float significanceOver(const SpectrometerData& d, int lo, int hi, float netArea) {
  float sampleArea = 0.0f, bgArea = 0.0f, contArea = 0.0f;
  for (int j = lo; j <= hi; j++) {
    if (j < 0 || j >= SPECTRUM_CHANNELS) continue;
    sampleArea += (float)d.spectrum[j];
    bgArea += bgCounts[j];
    contArea += continuum[j];
  }
  float var = sampleArea + bgArea * bgVarScale + fabsf(contArea) + 1.0f;
  if (var <= 0.0f) return 0.0f;
  return netArea / sqrtf(var);
}

// Peak list from the most recent analyzeSpectrum(), so the spectrum chart can
// draw exactly the peaks the identification actually used.
static DetectedPeak g_lastPeaks[MAX_DETECTED_PEAKS];
static int g_lastPeakCount = 0;

const DetectedPeak* lastPeaks() { return g_lastPeaks; }
int lastPeakCount() { return g_lastPeakCount; }

const float* correctedSpectrum() { return corrected; }
const float* backgroundCounts() { return bgCounts; }
bool backgroundApplied() { return bgVarScale > 0.0f; }

float fwhmKeVAt(float energyKeV) {
  if (energyKeV < 20.0f) energyKeV = 20.0f; // model breaks down near zero
  return 0.084f * sqrtf(662.0f * energyKeV);
}

// Peak width in channels at a given channel, from the resolution model and
// the local energy dispersion dE/dch. This matters a lot: at 100 keV a peak
// is ~9 channels wide but at 2614 keV it is ~46 channels wide, so every
// window below has to scale rather than being a fixed number of channels.
static float fwhmChannelsAt(const SpectrometerData& d, int ch) {
  float e = channelToKeV(d, ch);
  float dEdCh = d.calibA1 + 2.0f * d.calibA2 * (float)ch;
  if (dEdCh < 0.01f) dEdCh = 0.01f;
  float fw = fwhmKeVAt(e) / dEdCh;
  if (fw < 2.0f) fw = 2.0f;
  if (fw > 120.0f) fw = 120.0f;
  return fw;
}

static void smoothSpectrum(const SpectrometerData& d) {
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    int w = (int)(fwhmChannelsAt(d, i) * 0.25f);
    if (w < 1) w = 1;
    if (w > 12) w = 12;
    int lo = i - w < 0 ? 0 : i - w;
    int hi = i + w >= SPECTRUM_CHANNELS ? SPECTRUM_CHANNELS - 1 : i + w;
    float sum = 0;
    // Smooth the background-corrected spectrum, so everything downstream
    // (continuum, peaks, ROIs) works on the sample's own excess.
    for (int j = lo; j <= hi; j++) sum += corrected[j];
    smoothed[i] = sum / (float)(hi - lo + 1);
  }
}

// Continuum from two side-bands that straddle the peak region, rather than a
// boxcar centred on it. The original centred boxcar averaged the peak into
// its own background estimate and, on a steeply falling spectrum, biased the
// apparent peak position low - that is why measured energies were reading
// several keV under the true line (344 vs 352 keV, 184 vs 186 keV).
static void estimateContinuum(const SpectrometerData& d) {
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    // Side bands start a full FWHM out from the centre. At 0.75 FWHM they
    // still sat inside the peak's own tails (~2.4 sigma, several % of peak
    // height), inflating the background estimate and dragging fitted peak
    // positions low.
    float fw = fwhmChannelsAt(d, i);
    int inner = (int)(fw * 1.0f);
    if (inner < 3) inner = 3;
    int outer = (int)(fw * 2.0f);
    if (outer <= inner) outer = inner + 2;

    float leftSum = 0; int leftN = 0;
    for (int j = i - outer; j <= i - inner; j++) {
      if (j >= 0) { leftSum += smoothed[j]; leftN++; }
    }
    float rightSum = 0; int rightN = 0;
    for (int j = i + inner; j <= i + outer; j++) {
      if (j < SPECTRUM_CHANNELS) { rightSum += smoothed[j]; rightN++; }
    }

    if (leftN > 0 && rightN > 0) {
      // Linear interpolation across the peak region.
      continuum[i] = 0.5f * (leftSum / leftN + rightSum / rightN);
    } else if (leftN > 0) {
      continuum[i] = leftSum / leftN;
    } else if (rightN > 0) {
      continuum[i] = rightSum / rightN;
    } else {
      continuum[i] = smoothed[i];
    }
  }

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    net[i] = smoothed[i] - continuum[i];
  }
}

int findPeaks(const SpectrometerData& d, DetectedPeak* out, int maxPeaks) {
  prepareSpectra(d);
  smoothSpectrum(d);
  estimateContinuum(d);

  // Collect every candidate, then rank - never truncate by scan order.
  static DetectedPeak candidates[96];
  int nCand = 0;
  const int minChannel = 4; // skip electronic noise pile-up at the very bottom

  for (int i = minChannel; i < SPECTRUM_CHANNELS - 1 && nCand < 96; i++) {
    if (net[i] <= 0.0f) continue;

    // Local maximum of the CONTINUUM-SUBTRACTED spectrum, over a window that
    // scales with the local peak width. The original code tested the raw
    // smoothed spectrum over a fixed +-3 channels, which both biased the
    // position low and reported shoulders of broad peaks as separate peaks.
    float fw = fwhmChannelsAt(d, i);
    int hw = (int)(fw * 0.5f);
    if (hw < 2) hw = 2;

    bool isLocalMax = true;
    for (int j = i - hw; j <= i + hw; j++) {
      if (j < 0 || j >= SPECTRUM_CHANNELS || j == i) continue;
      // Ties must be broken, or a flat top reports every one of its channels
      // as a separate peak (seen on hardware as duplicate 1543/1544 keV
      // entries). Lowest channel of a tie wins.
      if (net[j] > net[i] || (net[j] == net[i] && j < i)) { isLocalMax = false; break; }
    }
    if (!isLocalMax) continue;

    // Integrate net and background over ~1 FWHM for a proper area-based
    // significance, instead of comparing single-channel heights.
    int aw = (int)(fw * 0.5f);
    if (aw < 1) aw = 1;
    float netArea = 0.0f, bkgArea = 0.0f;
    for (int j = i - aw; j <= i + aw; j++) {
      if (j < 0 || j >= SPECTRUM_CHANNELS) continue;
      if (net[j] > 0.0f) netArea += net[j];
      bkgArea += continuum[j];
    }
    if (netArea < 8.0f) continue; // absolute floor against empty-spectrum noise

    // Proper counting uncertainty (see significanceOver). The original
    // net/sqrt(bkg) ignored both the signal's own Poisson noise and the
    // uncertainty of the background estimate, overstating significance by
    // ~1.5-2x precisely where it matters most - weak peaks, where net and
    // background are comparable.
    float sig = significanceOver(d, i - aw, i + aw, netArea);
    if (sig < 2.0f) continue;

    // Intensity-weighted centroid over the peak, rather than a 3-point
    // parabola. These peaks are 10-45 channels wide, so fitting a parabola
    // to three adjacent channels samples an almost-flat top and is dominated
    // by noise and by any residual background slope - which biased measured
    // energies low by 1-3% against known lines on real samples.
    int cw = (int)(fw * 0.5f);
    if (cw < 1) cw = 1;
    float sumW = 0.0f, sumWX = 0.0f;
    for (int j = i - cw; j <= i + cw; j++) {
      if (j < 0 || j >= SPECTRUM_CHANNELS) continue;
      if (net[j] <= 0.0f) continue;
      sumW += net[j];
      sumWX += net[j] * (float)j;
    }
    float refined = (sumW > 0.0f) ? (sumWX / sumW) : (float)i;
    // Guard against a neighbouring peak dragging the centroid away.
    if (fabsf(refined - (float)i) > fw * 0.5f) refined = (float)i;
    float energy = d.calibA0 + d.calibA1 * refined + d.calibA2 * refined * refined;

    candidates[nCand].channel = i;
    candidates[nCand].energyKeV = energy;
    candidates[nCand].netCounts = netArea;
    candidates[nCand].significance = sig;
    nCand++;
  }

  // Keep the most significant peaks, then restore energy order for display.
  int keep = nCand < maxPeaks ? nCand : maxPeaks;
  for (int a = 0; a < keep; a++) {
    int best = a;
    for (int b = a + 1; b < nCand; b++) {
      if (candidates[b].significance > candidates[best].significance) best = b;
    }
    DetectedPeak tmp = candidates[a];
    candidates[a] = candidates[best];
    candidates[best] = tmp;
  }
  for (int a = 0; a < keep; a++) {
    for (int b = a + 1; b < keep; b++) {
      if (candidates[b].energyKeV < candidates[a].energyKeV) {
        DetectedPeak tmp = candidates[a];
        candidates[a] = candidates[b];
        candidates[b] = tmp;
      }
    }
  }
  for (int a = 0; a < keep; a++) out[a] = candidates[a];
  return keep;
}

struct IsotopeScore {
  float value;
  float bestEnergy;
  float bestSigma;
  float weakestSigma;
  uint8_t matched;
};

static IsotopeScore scores[ISOTOPE_LIBRARY_MAX];

// Matching window. 0.75 FWHM proved far too permissive on a real thorium
// mantle (2026-08-22): at 609 keV that is +-40 keV, wide enough for Tl-208's
// real 583 keV peak to satisfy Bi-214's 609 keV line and fake a uranium chain.
#define MATCH_TOLERANCE_FWHM 0.6f
// A nuclide may still share a peak that another nuclide explains better, but
// only if its line is nearly as close - otherwise the closer nuclide owns it.
#define SHARE_MARGIN_FWHM 0.15f

// Crude relative full-energy-peak efficiency for a small CsI(Tl) crystal,
// normalised to 1.0 at 200 keV. Photoelectric absorption falls steeply with
// energy, so a nuclide's most *intense* line is not necessarily its most
// *detectable* one. Confirmed on a real thorium mantle (2026-08-22): Tl-208's
// 2614 keV line (99.8% emission) produces no visible peak while its 583 keV
// line (85%) is clear, because efficiency at 2614 keV is ~5x lower.
// Gating on raw emission intensity therefore rejected genuinely present
// nuclides. Rough model, but far better than assuming flat efficiency.
static float relEfficiency(float energyKeV) {
  float e = energyKeV;
  if (e < 30.0f) e = 30.0f;
  float eff = (e >= 200.0f) ? powf(200.0f / e, 1.2f) : 1.0f;
  if (e < 80.0f) eff *= e / 80.0f; // housing/crystal-surface attenuation
  return eff;
}

// How visible a line should be: emission probability x detector efficiency.
static float detectabilityOf(const IsotopeLine& line) {
  return line.intensity * relEfficiency(line.energyKeV);
}

// --- region-of-interest testing -----------------------------------------
// Blind peak-finding needs a line to rise into a clean local maximum, which
// weak sources near background never manage. But we know exactly where each
// library line should be, so we can test that energy window directly: a
// statistically significant excess over the fitted continuum counts as a
// detection even with no visible peak. This is what makes trace/weak-source
// identification work at all, and it is standard practice for exactly this
// reason. Peak finding is still used for discovery and for the chart.
struct RoiResult {
  float net;
  float sigma;
  float centroidKeV;
  bool valid;
};

static int channelForEnergy(const SpectrometerData& d, float e) {
  float a = d.calibA2, b = d.calibA1, c0 = d.calibA0 - e;
  float ch;
  if (fabsf(a) < 1e-9f) {
    if (fabsf(b) < 1e-9f) return -1;
    ch = -c0 / b;
  } else {
    float disc = b * b - 4.0f * a * c0;
    if (disc < 0.0f) return -1;
    ch = (-b + sqrtf(disc)) / (2.0f * a);
  }
  if (ch < 0.0f || ch >= (float)SPECTRUM_CHANNELS) return -1;
  return (int)(ch + 0.5f);
}

// Assumes smoothSpectrum()/estimateContinuum() have already run for d.
static RoiResult testRoi(const SpectrometerData& d, float energyKeV) {
  RoiResult r = {0.0f, 0.0f, 0.0f, false};
  int ch = channelForEnergy(d, energyKeV);
  if (ch < 4 || ch >= SPECTRUM_CHANNELS - 4) return r;

  float fw = fwhmChannelsAt(d, ch);
  int hw = (int)(fw * 0.7f);
  if (hw < 1) hw = 1;

  float excess = 0.0f, sumW = 0.0f, sumWX = 0.0f;
  for (int j = ch - hw; j <= ch + hw; j++) {
    if (j < 0 || j >= SPECTRUM_CHANNELS) continue;
    // corrected[] already has the stored background removed; continuum[] is
    // the Compton/scatter shelf underneath.
    excess += corrected[j] - continuum[j];
    float w = net[j];
    if (w > 0.0f) { sumW += w; sumWX += w * (float)j; }
  }

  r.net = excess;
  r.sigma = significanceOver(d, ch - hw, ch + hw, excess);
  if (sumW > 0.0f) {
    float c = sumWX / sumW;
    r.centroidKeV = d.calibA0 + d.calibA1 * c + d.calibA2 * c * c;
    r.valid = true;
  }
  return r;
}

// For each detected peak, the distance to the closest line of ANY nuclide.
// Without this, one real peak is counted as a confirmed match for every
// nuclide that happens to have a line within tolerance, which is how a pure
// thorium sample was scoring a full uranium chain.
static float bestClaimDist[MAX_DETECTED_PEAKS];

static void computeBestClaims(const DetectedPeak* peaks, int peakCount) {
  for (int p = 0; p < peakCount; p++) bestClaimDist[p] = 1e9f;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    const IsotopeDef& def = IsotopeLibrary::at(iso);
    for (uint8_t li = 0; li < def.lineCount; li++) {
      float e = def.lines[li].energyKeV;
      float tol = fwhmKeVAt(e) * MATCH_TOLERANCE_FWHM;
      for (int p = 0; p < peakCount; p++) {
        float d = fabsf(peaks[p].energyKeV - e);
        if (d <= tol && d < bestClaimDist[p]) bestClaimDist[p] = d;
      }
    }
  }
}

// ROI evidence thresholds. A nuclide's defining line needs stronger evidence
// than a corroborating one, since accepting it on noise creates a whole false
// identification. The centroid check guards against an ROI simply catching
// the flank of a neighbouring nuclide's strong peak.
#define ROI_PRIMARY_SIGMA 4.0f
#define ROI_SUPPORT_SIGMA 2.5f
#define ROI_CENTROID_FWHM 0.35f

// A line counts as "expected to be visible" only if its detectability is at
// least this fraction of the nuclide's best line. Set from hardware: at 0.25
// Tl-208 was required to show its 511 keV line, which sits on the Compton
// edge and is genuinely hard, so the definitive thorium marker never
// registered even with its 583 keV line clearly present.
#define EXPECTED_LINE_FRACTION 0.35f

// No nuclide may be claimed on sub-3-sigma evidence, however well its line
// happens to line up. Observed on hardware (2026-08-22): on plain background
// a 2.0-sigma noise bump at 110 keV landed within tolerance of Co-57's
// 122 keV line and produced an "89% Co-57" candidate out of nothing. The
// verdict guard caught it, but the candidate list should never have shown it.
#define MIN_CANDIDATE_SIGMA 3.0f

// A single chain member this strong, with this many independently matched
// lines, establishes the chain without a second member (see scoreChain()).
#define CHAIN_SOLO_CONFIDENCE 0.75f
#define CHAIN_SOLO_LINES      3

// Below this, unmodelled X-ray fluorescence and backscatter dominate; a line
// down here cannot by itself identify a nuclide (see scoreIsotopes()).
#define XRAY_REGION_KEV 110.0f

// Competitive assignment for the ROI path.
//
// Peak matching already refuses to credit a nuclide whose line sits further
// from a peak than some other nuclide's line. The ROI fallback had no such
// check, so a nuclide rejected by that competition simply won the same
// excess through ROI instead - which is how Ba-133 kept claiming the real
// Ac-228/Pb-214 blend near 340 keV on uranium ore and scored 89%. The excess
// found in a window has to belong to whichever line best explains where it
// actually sits.
static bool roiBetterExplainedElsewhere(uint16_t ownIso, float centroidKeV,
                                        float lineEnergyKeV, float fwhm) {
  float ownDist = fabsf(centroidKeV - lineEnergyKeV);
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (iso == ownIso) continue;
    const IsotopeDef& other = IsotopeLibrary::at(iso);
    for (uint8_t li = 0; li < other.lineCount; li++) {
      float d = fabsf(centroidKeV - other.lines[li].energyKeV);
      if (d + fwhm * SHARE_MARGIN_FWHM < ownDist) return true;
    }
  }
  return false;
}

static void scoreIsotopes(const SpectrometerData& d, const DetectedPeak* peaks, int peakCount) {
  memset(scores, 0, sizeof(scores));
  computeBestClaims(peaks, peakCount);

  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    const IsotopeDef& def = IsotopeLibrary::at(iso);

    // Everything below is weighted by detectability (emission x efficiency)
    // rather than raw emission intensity - see relEfficiency().
    float maxDetect = 0.0f;
    float totalDetect = 0.0f;
    for (uint8_t li = 0; li < def.lineCount; li++) {
      float dt = detectabilityOf(def.lines[li]);
      totalDetect += dt;
      if (dt > maxDetect) maxDetect = dt;
    }
    if (totalDetect <= 0.0f) continue;

    // Below ~110 keV the spectrum is a pile-up of lead/bismuth/thorium K
    // X-ray fluorescence, backscatter and the Compton edge - none of which
    // are in the library. Any dense, high-Z sample therefore grows a large
    // bump there, and on real ore Ba-133's 81 keV line claimed it and scored
    // 76%. So a nuclide that also emits above that region must be identified
    // by one of those higher lines; the low one may still corroborate.
    // Nuclides whose signature genuinely is low-energy only (Am-241 at 59.5)
    // keep their own strongest line, since there is nothing else to use.
    float definingDetect = 0.0f;
    for (uint8_t li = 0; li < def.lineCount; li++) {
      if (def.lines[li].energyKeV < XRAY_REGION_KEV) continue;
      float dt = detectabilityOf(def.lines[li]);
      if (dt > definingDetect) definingDetect = dt;
    }
    bool lowEnergyOnly = (definingDetect <= 0.0f);
    if (lowEnergyOnly) definingDetect = maxDetect;

    // Lines we could reasonably expect to see if this nuclide is present.
    // A line 10x less detectable than the best one needs ~100x the counting
    // time to reach the same significance, so requiring it as corroboration
    // would reject nuclides that are genuinely there (this rejected Pb-212,
    // the clearest thorium indicator of all, on a real mantle).
    uint8_t expectedCount = 0;
    for (uint8_t li = 0; li < def.lineCount; li++) {
      if (detectabilityOf(def.lines[li]) >= maxDetect * EXPECTED_LINE_FRACTION) expectedCount++;
    }

    float matchedDetect = 0.0f;
    float bestEnergy = 0.0f, bestNet = 0.0f, bestSigma = 0.0f;
    float weakestSigma = 1e9f;
    uint8_t nMatched = 0;
    uint8_t nExpectedMatched = 0;
    bool strongestMatched = false;

    for (uint8_t li = 0; li < def.lineCount; li++) {
      const IsotopeLine& line = def.lines[li];
      float fwhm = fwhmKeVAt(line.energyKeV);
      float tolerance = fwhm * MATCH_TOLERANCE_FWHM;

      float bestDist = tolerance + 1.0f;
      int bestPeakIdx = -1;
      for (int p = 0; p < peakCount; p++) {
        float dist = fabsf(peaks[p].energyKeV - line.energyKeV);
        if (dist < bestDist) { bestDist = dist; bestPeakIdx = p; }
      }

      // Competitive assignment: if some other nuclide's line sits materially
      // closer to this peak, that nuclide explains it and this one does not
      // get to count it as evidence.
      bool peakMatched = (bestPeakIdx >= 0) && (bestDist <= tolerance) &&
                         (bestDist <= bestClaimDist[bestPeakIdx] + fwhm * SHARE_MARGIN_FWHM);

      float dt = detectabilityOf(line);
      bool isPrimary = (dt >= definingDetect * 0.999f) &&
                       (lowEnergyOnly || line.energyKeV >= XRAY_REGION_KEV);
      float matchEnergy = 0.0f, matchNet = 0.0f, matchSigma = 0.0f;

      if (peakMatched) {
        matchEnergy = peaks[bestPeakIdx].energyKeV;
        matchNet = peaks[bestPeakIdx].netCounts;
        matchSigma = peaks[bestPeakIdx].significance;
      } else {
        // No standalone peak here - fall back to testing the line's own
        // energy window, which is far more sensitive for weak sources.
        RoiResult roi = testRoi(d, line.energyKeV);
        float need = isPrimary ? ROI_PRIMARY_SIGMA : ROI_SUPPORT_SIGMA;
        if (!roi.valid || roi.net <= 0.0f || roi.sigma < need) continue;
        // The excess must actually be centred on this line, not be the
        // shoulder of a stronger neighbouring peak leaking into the window.
        if (fabsf(roi.centroidKeV - line.energyKeV) > fwhm * ROI_CENTROID_FWHM) continue;
        // ...and it must not be better explained by another nuclide's line.
        if (roiBetterExplainedElsewhere(iso, roi.centroidKeV, line.energyKeV, fwhm)) continue;
        matchEnergy = line.energyKeV;
        matchNet = roi.net;
        matchSigma = roi.sigma;
      }

      matchedDetect += dt;
      nMatched++;
      if (dt >= maxDetect * EXPECTED_LINE_FRACTION) nExpectedMatched++;
      if (isPrimary) strongestMatched = true;
      if (matchNet > bestNet) { bestNet = matchNet; bestEnergy = matchEnergy; }
      if (matchSigma > bestSigma) bestSigma = matchSigma;
      if (matchSigma < weakestSigma) weakestSigma = matchSigma;
    }

    // Gate 1: the nuclide's most *detectable* line must be present. Matching
    // only a minor line is not evidence (this produced the original Ba-133
    // false positive on real ore).
    if (!strongestMatched) continue;
    // Gate 2: corroboration from a second line, but only counting lines that
    // could plausibly be seen at all. A nuclide whose signature is genuinely
    // a single detectable line (Pb-212, Cs-137, K-40) is accepted on that one.
    uint8_t required = expectedCount >= 2 ? 2 : 1;
    if (nExpectedMatched < required) continue;
    // Gate 3: the evidence itself must be statistically real.
    if (bestSigma < MIN_CANDIDATE_SIGMA) continue;

    scores[iso].value = matchedDetect / totalDetect;
    scores[iso].bestEnergy = bestEnergy;
    scores[iso].bestSigma = bestSigma;
    scores[iso].weakestSigma = (weakestSigma > 1e8f) ? 0.0f : weakestSigma;
    scores[iso].matched = nMatched;
  }
}

// Combine member nuclides into chain-level evidence. Members of a decay chain
// in secular equilibrium must co-occur, so independent detections of several
// members is far stronger evidence than any single nuclide match - and it is
// the only way to report "uranium" at all, since U-238 itself emits no
// usable gammas.
static ChainResult scoreChain(DecayChain chain) {
  ChainResult r = {0.0f, 0, 0};
  float pMiss = 1.0f;
  bool selfSufficient = false;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (IsotopeLibrary::at(iso).chain != chain) continue;
    r.membersTotal++;
    if (scores[iso].value >= 0.30f) {
      r.membersDetected++;
      pMiss *= (1.0f - scores[iso].value);
      // A member can be conclusive on its own. Bi-214 matching four of its
      // seven lines is uranium whatever else is or is not resolved, and
      // demanding a second member then reported 0% for the chain while that
      // nuclide sat at 92% in the candidate list. Corroboration is only
      // needed when the single member is itself ambiguous, so several
      // independently matched lines substitute for a second nuclide.
      if (scores[iso].value >= CHAIN_SOLO_CONFIDENCE &&
          scores[iso].matched >= CHAIN_SOLO_LINES) {
        selfSufficient = true;
      }
    }
  }
  if (r.membersDetected >= 2 || selfSufficient) r.confidence = 1.0f - pMiss;
  else r.confidence = 0.0f;
  return r;
}

static SourceClass classify(const SpectrometerData& d, float bestArtificial, float k40) {
  if (d.quality.peaksFound == 0 || d.quality.bestSigma < 3.0f) return SRC_INSUFFICIENT;

  bool u = d.uranium.confidence >= 0.5f;
  bool t = d.thorium.confidence >= 0.5f;

  if (bestArtificial >= 0.6f && bestArtificial > d.uranium.confidence &&
      bestArtificial > d.thorium.confidence) {
    return SRC_ARTIFICIAL;
  }
  if (u && t) return SRC_MIXED_NATURAL;
  if (u) return SRC_URANIUM;
  if (t) return SRC_THORIUM;
  if (k40 >= 0.5f) return SRC_POTASSIUM;
  return SRC_BACKGROUND;
}

// Counting statistics: peak significance grows as sqrt(acquisition time), so
// from the current significance we can extrapolate honestly to the time
// needed for a trustworthy identification. Target is 12 sigma on the weakest
// line the current top ID depends on.
static void computeQuality(SpectrometerData& d, const DetectedPeak* peaks, int peakCount) {
  uint32_t total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) total += d.spectrum[i];
  d.quality.totalCounts = total;
  d.quality.peaksFound = (uint8_t)peakCount;

  float best = 0.0f;
  for (int i = 0; i < peakCount; i++) {
    if (peaks[i].significance > best) best = peaks[i].significance;
  }
  d.quality.bestSigma = best;

  // The weakest link is what limits confidence in the reported ID.
  float weakest = (d.candidateCount > 0) ? d.candidates[0].significance : best;
  if (weakest <= 0.0f) weakest = best;
  d.quality.weakestKeySigma = weakest;

  if (weakest >= 12.0f)     d.quality.grade = QUAL_EXCELLENT;
  else if (weakest >= 6.0f) d.quality.grade = QUAL_GOOD;
  else if (weakest >= 3.0f) d.quality.grade = QUAL_FAIR;
  else                      d.quality.grade = QUAL_POOR;

  uint32_t elapsed = d.spectrumDurationSec;
  if (elapsed < 1) elapsed = 1;

  if (weakest <= 0.1f) {
    // Nothing measurable yet - suggest a sensible default rather than
    // extrapolating from noise.
    d.quality.recommendedSeconds = 300;
  } else {
    float factor = 12.0f / weakest;
    float needed = (float)elapsed * factor * factor;
    if (needed > 7200.0f) needed = 7200.0f;   // cap the advice at 2 h
    if (needed < (float)elapsed) needed = (float)elapsed;
    d.quality.recommendedSeconds = (uint32_t)needed;
  }

  d.quality.remainingSeconds = (d.quality.recommendedSeconds > elapsed)
                             ? (d.quality.recommendedSeconds - elapsed) : 0;
}

void analyzeSpectrum(SpectrometerData& d) {
  // findPeaks() leaves smoothed/continuum/net populated for d, which the ROI
  // tests inside scoreIsotopes() rely on - keep this ordering.
  g_lastPeakCount = findPeaks(d, g_lastPeaks, MAX_DETECTED_PEAKS);
  scoreIsotopes(d, g_lastPeaks, g_lastPeakCount);

  // Rank individual nuclides.
  d.candidateCount = 0;
  bool used[ISOTOPE_LIBRARY_MAX];
  memset(used, 0, sizeof(used));

  for (int slot = 0; slot < MAX_CANDIDATES; slot++) {
    int bestIdx = -1;
    float bestVal = 0.0f;
    for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
      if (used[iso]) continue;
      if (scores[iso].value > bestVal) { bestVal = scores[iso].value; bestIdx = iso; }
    }
    if (bestIdx < 0 || bestVal <= 0.0f) break;
    used[bestIdx] = true;

    IsotopeCandidate& c = d.candidates[d.candidateCount];
    c.symbol = IsotopeLibrary::at(bestIdx).symbol;
    c.name = IsotopeLibrary::at(bestIdx).name;
    c.confidence = bestVal > 1.0f ? 1.0f : bestVal;
    c.matchedEnergyKeV = scores[bestIdx].bestEnergy;
    c.linesMatched = scores[bestIdx].matched;
    c.linesTotal = IsotopeLibrary::at(bestIdx).lineCount;
    c.significance = scores[bestIdx].bestSigma;
    d.candidateCount++;
  }

  d.uranium = scoreChain(CHAIN_U238);
  d.thorium = scoreChain(CHAIN_TH232);

  float bestArtificial = 0.0f, k40 = 0.0f;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (IsotopeLibrary::at(iso).category == CAT_ARTIFICIAL && scores[iso].value > bestArtificial) {
      bestArtificial = scores[iso].value;
    }
    if (strcmp(IsotopeLibrary::at(iso).symbol, "K-40") == 0) k40 = scores[iso].value;
  }

  computeQuality(d, g_lastPeaks, g_lastPeakCount);
  d.sourceClass = classify(d, bestArtificial, k40);

#if PEAK_SEARCH_DEBUG
  Serial.printf("\n[ANL] --- %u peaks, %u counts, %us acquisition ---\n",
                (unsigned)g_lastPeakCount, (unsigned)d.quality.totalCounts,
                (unsigned)d.spectrumDurationSec);
  for (int i = 0; i < g_lastPeakCount; i++) {
    Serial.printf("[ANL] peak %7.1f keV  net=%-8.0f sigma=%.1f\n",
                  g_lastPeaks[i].energyKeV, g_lastPeaks[i].netCounts,
                  g_lastPeaks[i].significance);
  }
  for (uint8_t i = 0; i < d.candidateCount; i++) {
    Serial.printf("[ANL] cand %-8s %3.0f%%  %u/%u lines  best=%.1f keV\n",
                  d.candidates[i].symbol, d.candidates[i].confidence * 100.0f,
                  d.candidates[i].linesMatched, d.candidates[i].linesTotal,
                  d.candidates[i].matchedEnergyKeV);
  }
  Serial.printf("[ANL] U-238 chain %.0f%% (%u/%u)   Th-232 chain %.0f%% (%u/%u)   verdict=%s\n",
                d.uranium.confidence * 100.0f, d.uranium.membersDetected, d.uranium.membersTotal,
                d.thorium.confidence * 100.0f, d.thorium.membersDetected, d.thorium.membersTotal,
                sourceClassLabel(d.sourceClass));
#endif
}
