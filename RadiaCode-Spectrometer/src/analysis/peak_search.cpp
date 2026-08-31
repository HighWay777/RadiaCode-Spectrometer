#include "peak_search.h"
#include "../data/isotope_library.h"
#include "../data/background.h"
#include "../data/settings.h"
#include <math.h>
#include <string.h>

// Prints the detected peak list and the winning nuclide scores to Serial on
// every analysis pass. Invaluable for diagnosing a questionable ID against a
// known sample - set to 0 to silence.
#ifndef PEAK_SEARCH_DEBUG
#define PEAK_SEARCH_DEBUG 0
#endif

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

// Multiplicative correction applied to the energy axis before matching;
// see "gain-drift correction" below. 1.0 means the reported calibration is
// taken at face value.
static float gainCorr = 1.0f;

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
  // Area of the strongest matched peak and the detectability expected for
  // that line. Their quotient is the activity this nuclide would need to
  // explain what was seen - the basis of the intensity check below.
  float bestNet;
  float bestDetect;
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
  // The caller passes a library energy, which lives on the corrected
  // axis; undo the correction to reach the raw channel it sits in.
  int ch = channelForEnergy(d, energyKeV / gainCorr);
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
    r.centroidKeV = (d.calibA0 + d.calibA1 * c + d.calibA2 * c * c) * gainCorr;
    r.valid = true;
  }
  return r;
}

// For each detected peak, the distance to the closest line of ANY nuclide.
// Without this, one real peak is counted as a confirmed match for every
// nuclide that happens to have a line within tolerance, which is how a pure
// thorium sample was scoring a full uranium chain.
static float bestClaimDist[MAX_DETECTED_PEAKS];

// Which nuclides are allowed to contest a peak. Filled by the first
// scoring pass; see scoreIsotopes() for why this exists.
static bool eligible[ISOTOPE_LIBRARY_MAX];
// Nuclides entitled to contest a peak: those that matched their own
// defining line with real significance. Deliberately NOT the same as being
// listed - see scoreIsotopes().
static bool contender[ISOTOPE_LIBRARY_MAX];
// Per-nuclide count of independent peaks matched, and which decay chains
// have a member confirmed by two or more of them. Members of a chain are
// in secular equilibrium - they are physically inseparable - so a firmly
// identified member is real corroboration for its siblings, and the
// absence of any anchor means a lone marginal line is all we have.
static uint8_t distinctPeaks[ISOTOPE_LIBRARY_MAX];
// Significance of each contender's best matched line, kept even for
// nuclides that were not reportable on their own.
static float contenderSigma[ISOTOPE_LIBRARY_MAX];
static bool chainAnchored[3];   // indexed by DecayChain
// Chains anchored earlier in THIS acquisition. Evidence near a threshold
// crosses it back and forth as counts arrive, and because everything else
// hangs off the anchor, one marginal pass emptied the candidate list and
// dropped a correct THORIUM back to "measuring". Once a chain has been
// established it is kept while its members remain plausible; RESET clears
// it, so a new sample never inherits the last one's conclusion.
static bool chainHeld[3];

static void computeBestClaims(const DetectedPeak* peaks, int peakCount) {
  for (int p = 0; p < peakCount; p++) bestClaimDist[p] = 1e9f;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (!eligible[iso]) continue;
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
// A nuclide identified from a line that never rose into a visible peak is
// standing on evidence nobody can see, so it must be overwhelming. At 4
// sigma the tail of a neighbouring peak was enough: on plain background the
// shoulder of a 1335 keV feature satisfied Na-22's 1274.5 keV line through
// this path - the peak itself being 61 keV away, well outside the matching
// window - and background reported "Na-22 100%". Corroborating lines keep
// the lower bar, which is what makes weak sources detectable at all.
#define ROI_PRIMARY_SIGMA 8.0f
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

// A single chain member this significant anchors its chain alone. Well
// above the 3 sigma listing bar, so a marginal detection still cannot
// bootstrap a whole chain out of nothing.
#define CHAIN_ANCHOR_SOLO_SIGMA 8.0f

// How much worse one chain's internal intensity agreement must be before
// it is discarded in favour of the other. 1.4 separated real uranium ore
// (1.11) from the thorium misreading of the same peaks (1.93) with room to
// spare, while leaving a genuinely mixed sample alone.
#define CHAIN_SPREAD_MARGIN 1.4f

// How much stronger one chain must be before it is named instead of
// reporting a mixture. Real ore sits at 97% against 70% (ratio 1.39); a
// genuinely mixed sample at 93% against 98% (ratio 1.05).
#define CHAIN_DOMINANCE_RATIO 1.25f

// Below this, unmodelled X-ray fluorescence and backscatter dominate; a line
// down here cannot by itself identify a nuclide (see scoreIsotopes()).
#define XRAY_REGION_KEV 110.0f

// A nuclide identified entirely from below that region is standing on
// ground we do not model - lead and thorium K X-rays, backscatter, the
// Compton edge - so it must clear a far higher bar. On plain background a
// 4.9 sigma bump at 63 keV (really Th-234 at 63.29, ordinary building
// material) was reported as Am-241 at 100%, a smoke-detector source in an
// empty room.
#define LOW_ENERGY_ONLY_SIGMA 9.0f

// Positron annihilation at 511 keV appears in almost every spectrum -
// cosmic-ray muons and pair production make positrons whatever the sample
// is - so it identifies nothing on its own. It is also Na-22's most
// detectable line by a factor of five, which is how ordinary background
// came to report "Na-22 100%". A nuclide with other lines must be
// identified by one of those; 511 may still corroborate.
#define ANNIHILATION_KEV 511.0f

// --- non-nuclide spectral features -----------------------------------------
//
// A gamma spectrum contains peaks that are not gamma lines. Standard features
// of any scintillator spectrum:
//
//   backscatter   photons scattered ~180 degrees off surrounding material
//                 arrive at E/(1 + 2E/511), asymptotically ~200-255 keV
//   Compton edge  the maximum energy a single Compton scatter can deposit,
//                 2E^2/(511 + 2E) below each strong photopeak
//   escape peaks  pair production with one or both annihilation photons
//                 escaping, at E-511 and E-1022
//   sum peaks     two photons arriving within the shaping time, at E1+E2
//
// The matcher assumed every peak was a gamma line, so these were handed to
// whichever nuclide sat nearest. That is one cause behind several separate
// false positives - Na-22 on a Compton feature, Ba-133 and Am-241 on the
// low-energy forest - rather than several unrelated bugs.
//
// A feature is only credited to a parent that is clearly stronger than it,
// so a genuine weak line is never explained away by a neighbour of similar
// size.
#define ARTEFACT_TOL_FWHM     0.5f
#define ARTEFACT_PARENT_RATIO 1.5f

static bool artefactPeak[MAX_DETECTED_PEAKS];

static void markArtefacts(const DetectedPeak* peaks, int n) {
  for (int i = 0; i < n && i < MAX_DETECTED_PEAKS; i++) artefactPeak[i] = false;

  for (int i = 0; i < n && i < MAX_DETECTED_PEAKS; i++) {
    float e = peaks[i].energyKeV;
    float tol = fwhmKeVAt(e) * ARTEFACT_TOL_FWHM;

    for (int j = 0; j < n && !artefactPeak[i]; j++) {
      if (j == i) continue;
      if (peaks[j].significance < peaks[i].significance * ARTEFACT_PARENT_RATIO) continue;
      float P = peaks[j].energyKeV;

      if (P > 1022.0f && (fabsf(e - (P - 511.0f)) <= tol ||
                          fabsf(e - (P - 1022.0f)) <= tol)) {
        artefactPeak[i] = true; break;
      }
      if (fabsf(e - 2.0f * P * P / (511.0f + 2.0f * P)) <= tol) {
        artefactPeak[i] = true; break;   // Compton edge of this parent
      }
      if (fabsf(e - P / (1.0f + 2.0f * P / 511.0f)) <= tol) {
        artefactPeak[i] = true; break;   // backscatter from this parent
      }
      for (int k = j; k < n; k++) {
        if (peaks[k].significance < peaks[i].significance * ARTEFACT_PARENT_RATIO) continue;
        if (fabsf(e - (P + peaks[k].energyKeV)) <= tol) { artefactPeak[i] = true; break; }
      }
    }
  }
}

static bool nearAnnihilation(float energyKeV) {
  return fabsf(energyKeV - ANNIHILATION_KEV) <= fwhmKeVAt(ANNIHILATION_KEV) * 0.5f;
}

// Lines that can establish a nuclide on their own: above the X-ray forest
// and not the ubiquitous annihilation peak.
static bool isDiagnosticLine(float energyKeV) {
  return energyKeV >= XRAY_REGION_KEV && !nearAnnihilation(energyKeV);
}

// --- gain-drift correction ------------------------------------------------
//
// The detector reports factory calibration coefficients, but a scintillator's
// gain moves with temperature, so the real peaks drift away from where those
// coefficients say they should be. Measured on hardware: every line from a
// uranium sample read 3-4% low - Bi-214's 609.3 keV landed at 582.8, close
// enough to Tl-208's 583.2 keV that competitive assignment handed it over and
// a uranium sample reported thorium. The peak search itself is sound; the
// regression tests recover synthetic lines to better than 1%.
//
// So estimate one multiplicative gain factor and apply it to the energy axis
// before any matching. The estimate is a consensus vote, not a fit to an
// assumed nuclide: every significant peak proposes the ratio line/peak for
// each library line within reach, and the ratio that most peaks agree on
// wins. That needs no prior idea of what the sample is.
//
// Guards, because a wrong correction is worse than none:
//   - at least GAIN_MIN_VOTES independent peaks must agree
//   - the factor is clamped to +/-8%, beyond which something else is wrong
//   - it is smoothed over time, so it cannot jump between passes
#define GAIN_SEARCH_SPAN   0.08f   // consider drift up to +/-8%
#define GAIN_STEP          0.002f  // 0.2% resolution
#define GAIN_VOTE_TOL_FWHM 0.12f   // how close counts as agreement
#define GAIN_MIN_VOTES     2       // isolated references that must agree
#define GAIN_MIN_SIGMA     3.0f    // only peaks worth trusting may vote
#define GAIN_SMOOTHING     0.25f   // fraction of a new estimate adopted

float gainCorrection() { return gainCorr; }

void resetGainCorrection() { gainCorr = 1.0f; }

// Adopt whatever this detector previously taught us. A short measurement of a
// weak sample contains no isolated reference lines, so it cannot derive the
// correction itself - but the drift is the instrument's, not the sample's, and
// a long background run does contain the references (K-40 1460.8, Bi-212
// 1620.5, Tl-208 2614.5). Learn it when the spectrum allows, keep it
// afterwards.
void loadGainCorrection() {
  // Deliberately not restoring a stored value while the estimator is
  // disabled: a bad one was persisted and would keep skewing the axis.
  gainCorr = 1.0f;
  Settings::setEnergyGain(1.0f);
}

// A line is usable as a gain reference only if nothing else in the library
// sits near it. Voting against every line made the estimate meaningless:
// the table is dense enough that almost any gain factor finds something to
// agree with, so the consensus carried no information and the axis barely
// moved on a spectrum that was 4% out. Isolated lines - K-40 1460.8,
// Cs-137 661.7, Tl-208 2614.5 and the like - can only be matched by the
// factor that is actually right.
static bool lineIsIsolated(float energyKeV) {
  // 0.6 FWHM: roughly what this detector can actually separate. At 1.5 only
  // four lines in the whole table qualified and the estimator got no votes
  // at all; at 0.6 there are nine, including K-40 1460.8, Bi-214 1764.5 and
  // 2204.2 and Tl-208 2614.5 - present in exactly the natural samples where
  // drift does the most damage.
  float guard = fwhmKeVAt(energyKeV) * 0.6f;
  int near = 0;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    const IsotopeDef& def = IsotopeLibrary::at(iso);
    for (uint8_t li = 0; li < def.lineCount; li++) {
      if (fabsf(def.lines[li].energyKeV - energyKeV) <= guard) {
        if (++near > 1) return false;   // itself, plus a neighbour
      }
    }
  }
  return true;
}

static float scoreGain(const DetectedPeak* peaks, int peakCount, float g,
                       int* votesOut) {
  int votes = 0;
  float quality = 0.0f;
  for (int i = 0; i < peakCount; i++) {
    if (peaks[i].significance < GAIN_MIN_SIGMA) continue;
    float e = peaks[i].energyKeV * g;
    if (e < XRAY_REGION_KEV) continue;   // that region is not line-like
    float best = 1e9f;
    for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
      const IsotopeDef& def = IsotopeLibrary::at(iso);
      for (uint8_t li = 0; li < def.lineCount; li++) {
        float le = def.lines[li].energyKeV;
        if (!lineIsIsolated(le)) continue;
        float d = fabsf(le - e);
        if (d < best) best = d;
      }
    }
    float tol = fwhmKeVAt(e) * GAIN_VOTE_TOL_FWHM;
    if (best <= tol) {
      votes++;
      // Closer agreement, and stronger peaks, count for more.
      quality += peaks[i].significance * (1.0f - best / tol);
    }
  }
  if (votesOut) *votesOut = votes;
  return quality;
}

// Returns the factor to multiply raw energies by. 1.0 when there is not
// enough evidence to justify moving the axis at all.
static float estimateGain(const DetectedPeak* peaks, int peakCount) {
  float bestG = 1.0f, bestQ = 0.0f;
  int bestVotes = 0;
  for (float g = 1.0f - GAIN_SEARCH_SPAN; g <= 1.0f + GAIN_SEARCH_SPAN; g += GAIN_STEP) {
    int votes = 0;
    float q = scoreGain(peaks, peakCount, g, &votes);
    if (votes < GAIN_MIN_VOTES) continue;
    // How many peaks agree matters more than how tightly a couple of them
    // do. Ranking on quality alone let two well-matched peaks outvote a
    // broader consensus, and the axis settled a long way from the truth.
    if (votes > bestVotes || (votes == bestVotes && q > bestQ)) {
      bestQ = q; bestG = g; bestVotes = votes;
    }
  }
  if (bestVotes < GAIN_MIN_VOTES) return gainCorr;   // keep what we had

  // Ease towards the new estimate so the energy axis never jumps.
  return gainCorr + (bestG - gainCorr) * GAIN_SMOOTHING;
}

// Competitive assignment for the ROI path.
//
// Peak matching already refuses to credit a nuclide whose line sits further
// from a peak than some other nuclide's line. The ROI fallback had no such
// check, so a nuclide rejected by that competition simply won the same
// excess through ROI instead - which is how Ba-133 kept claiming the real
// Ac-228/Pb-214 blend near 340 keV on uranium ore and scored 89%. The excess
// found in a window has to belong to whichever line best explains where it
// actually sits.
// A peak that some other line explains FAR better is not this nuclide's,
// whether or not that other nuclide is itself identifiable. Competitive
// assignment is otherwise restricted to nuclides that pass their own gates,
// which is deliberate - a rival failing every test must not veto a real
// nuclide over a couple of keV. But that left a hole: on plain background the
// K-40 Compton edge near 1243 keV sat 45.8 keV from Na-22's 1274.5 keV line
// and only 12.2 keV from Co-60's 1332.5 keV. Co-60 was not a contender, so
// nothing objected, and background reported "Na-22 100%".
//
// The margins here are deliberately wide apart: this only fires when the
// nuclide's own line is more than half a FWHM out AND something else is inside
// a quarter of one. Marginal disputes are still settled by the contender-only
// rule, so Pb-214 - 0.21 FWHM from its peak on real ore - is untouched.
#define FAR_CLAIM_OWN_FWHM   0.50f
#define FAR_CLAIM_RIVAL_FWHM 0.25f

static bool farBetterExplainedElsewhere(uint16_t ownIso, float peakKeV,
                                        float lineKeV, float fwhm) {
  if (fabsf(peakKeV - lineKeV) <= fwhm * FAR_CLAIM_OWN_FWHM) return false;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (iso == ownIso) continue;
    const IsotopeDef& other = IsotopeLibrary::at(iso);
    for (uint8_t li = 0; li < other.lineCount; li++) {
      if (fabsf(peakKeV - other.lines[li].energyKeV) <= fwhm * FAR_CLAIM_RIVAL_FWHM) {
        return true;
      }
    }
  }
  return false;
}

static bool roiBetterExplainedElsewhere(uint16_t ownIso, float centroidKeV,
                                        float lineEnergyKeV, float fwhm) {
  float ownDist = fabsf(centroidKeV - lineEnergyKeV);
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (iso == ownIso || !eligible[iso]) continue;
    const IsotopeDef& other = IsotopeLibrary::at(iso);
    for (uint8_t li = 0; li < other.lineCount; li++) {
      float d = fabsf(centroidKeV - other.lines[li].energyKeV);
      if (d + fwhm * SHARE_MARGIN_FWHM < ownDist) return true;
    }
  }
  return false;
}

// Says why a nuclide with real evidence was still not listed. Reasoning
// about this from the outside was how Pb-214 stayed missing through one
// wrong fix. Only reported for the competitive pass, and only when the
// nuclide actually had evidence, so the log stays readable.
static void rejectLog(bool useCompetition, const char* sym, const char* why,
                      uint8_t nm, uint8_t nem, uint8_t exp, float sig) {
#if PEAK_SEARCH_DEBUG
  if (!useCompetition || nm == 0) return;
  Serial.printf("[ANL] drop %-8s %-20s matched=%u expected=%u/%u sig=%.1f\n",
                sym, why, (unsigned)nm, (unsigned)nem, (unsigned)exp, sig);
#else
  (void)useCompetition; (void)sym; (void)why;
  (void)nm; (void)nem; (void)exp; (void)sig;
#endif
}

static void scorePass(const SpectrometerData& d, const DetectedPeak* peaks,
                      int peakCount, bool useCompetition) {
  memset(scores, 0, sizeof(scores));
  memset(contender, 0, sizeof(contender));
  memset(distinctPeaks, 0, sizeof(distinctPeaks));
  memset(contenderSigma, 0, sizeof(contenderSigma));

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
      if (!isDiagnosticLine(def.lines[li].energyKeV)) continue;
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
    // Measured against the best DIAGNOSTIC line, and only diagnostic lines
    // qualify. Otherwise an unmodelled always-present feature sets the
    // scale and then satisfies the requirement it created: Na-22's 511 keV
    // annihilation line is five times more detectable than its real
    // 1274.5 keV signature, so 1274.5 fell below the threshold, the
    // expected-line count collapsed to one, and ordinary background
    // annihilation alone corroborated "Na-22 100%". Barring 511 from being
    // the defining line was not enough while it still defined the scale.
    uint8_t expectedCount = 0;
    for (uint8_t li = 0; li < def.lineCount; li++) {
      if (!lowEnergyOnly && !isDiagnosticLine(def.lines[li].energyKeV)) continue;
      if (detectabilityOf(def.lines[li]) >= definingDetect * EXPECTED_LINE_FRACTION) {
        expectedCount++;
      }
    }

    float matchedDetect = 0.0f;
    float bestEnergy = 0.0f, bestNet = 0.0f, bestSigma = 0.0f, bestDetect = 0.0f;
    // Area and expected weight of the strongest peak this nuclide is the
    // SOLE best claimant of. The share margin deliberately lets a rival
    // within a keV or two also count a peak as evidence, which is right for
    // identification but ruinous for an intensity comparison: Pb-214's
    // 242.0 keV line sits 3.4 keV from Pb-212's 238.6 keV, so on a mixed
    // sample Pb-214 inherited Pb-212's peak area and the uranium chain
    // appeared wildly inconsistent with itself.
    float soleNet = 0.0f, soleDetect = 0.0f;
    float weakestSigma = 1e9f;
    uint8_t nMatched = 0;
    uint8_t nExpectedMatched = 0;
    bool strongestMatched = false;
    // Which peaks this nuclide has already used. At 8.4% FWHM several of
    // a nuclide's lines can fall inside one peak - U-235's 143.8, 163.3,
    // 185.7 and 205.3 keV lines all sit within about one FWHM - and
    // without this each of them counted as independent corroboration, so
    // one noise bump on plain background scored "U-235 80%, 2/4 lines".
    // One peak is one piece of evidence, however many lines it satisfies.
    bool peakUsed[MAX_DETECTED_PEAKS];
    memset(peakUsed, 0, sizeof(peakUsed));
    uint8_t nDistinctPeaks = 0;
    // Evidence above the X-ray region only. A chain must not be anchored
    // on the sub-110 keV forest: Th-234 emits only at 63 and 92.6 keV, and
    // letting those two anchor the U-238 chain made the whole verdict
    // flicker between URANIUM ORE and BACKGROUND every five seconds as
    // that noisy region moved around.
    uint8_t nAnchorPeaks = 0;

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
      bool peakMatched = (bestPeakIdx >= 0) && (bestDist <= tolerance);
      if (peakMatched && useCompetition) {
        peakMatched = (bestDist <= bestClaimDist[bestPeakIdx] + fwhm * SHARE_MARGIN_FWHM);
        if (peakMatched &&
            farBetterExplainedElsewhere(iso, peaks[bestPeakIdx].energyKeV,
                                        line.energyKeV, fwhm)) {
          peakMatched = false;
        }
      }

      float dt = detectabilityOf(line);
      bool isPrimary = (dt >= definingDetect * 0.999f) &&
                       (lowEnergyOnly || isDiagnosticLine(line.energyKeV));

      // A backscatter, Compton-edge, escape or sum feature may corroborate
      // a nuclide already established by real lines, but it must never be
      // the thing that establishes one.
      if (peakMatched && isPrimary && bestPeakIdx >= 0 &&
          bestPeakIdx < MAX_DETECTED_PEAKS && artefactPeak[bestPeakIdx]) {
        peakMatched = false;
      }
      float matchEnergy = 0.0f, matchNet = 0.0f, matchSigma = 0.0f;

      if (peakMatched && bestPeakIdx >= 0 && bestPeakIdx < MAX_DETECTED_PEAKS &&
          peakUsed[bestPeakIdx]) {
        // Already counted for another line of this same nuclide.
        continue;
      }

      if (peakMatched) {
        if (bestPeakIdx >= 0 && bestPeakIdx < MAX_DETECTED_PEAKS) {
          peakUsed[bestPeakIdx] = true;
        }
        nDistinctPeaks++;
        if (line.energyKeV >= XRAY_REGION_KEV) nAnchorPeaks++;
        matchEnergy = peaks[bestPeakIdx].energyKeV;
        matchNet = peaks[bestPeakIdx].netCounts;
        matchSigma = peaks[bestPeakIdx].significance;
      } else {
        // No standalone peak here - fall back to testing the line's own
        // energy window, which is far more sensitive for weak sources.
        RoiResult roi = testRoi(d, line.energyKeV);
        float need = isPrimary ? ROI_PRIMARY_SIGMA : ROI_SUPPORT_SIGMA;
#if PEAK_SEARCH_DEBUG
        // Why an expected line that should be visible was not found.
        if (useCompetition && dt >= definingDetect * EXPECTED_LINE_FRACTION &&
            (!roi.valid || roi.sigma < need)) {
          Serial.printf("[ANL] roi- %-8s %.1f keV valid=%d net=%.0f sig=%.1f need=%.1f cen=%.1f\n",
                        def.symbol, line.energyKeV, (int)roi.valid, roi.net,
                        roi.sigma, need, roi.centroidKeV);
        }
#endif
        if (!roi.valid || roi.net <= 0.0f || roi.sigma < need) continue;
        // The excess must actually be centred on this line, not be the
        // shoulder of a stronger neighbouring peak leaking into the window.
        if (fabsf(roi.centroidKeV - line.energyKeV) > fwhm * ROI_CENTROID_FWHM) continue;
        // ...and it must not be better explained by another nuclide's line.
        if (useCompetition &&
            roiBetterExplainedElsewhere(iso, roi.centroidKeV, line.energyKeV, fwhm)) continue;
        nDistinctPeaks++;   // an ROI excess is its own independent evidence
        if (line.energyKeV >= XRAY_REGION_KEV) nAnchorPeaks++;
        matchEnergy = line.energyKeV;
        matchNet = roi.net;
        matchSigma = roi.sigma;
      }

      matchedDetect += dt;
      nMatched++;
      bool countsAsExpected = (lowEnergyOnly || isDiagnosticLine(line.energyKeV)) &&
                              (dt >= definingDetect * EXPECTED_LINE_FRACTION);
      if (countsAsExpected) nExpectedMatched++;
      if (isPrimary) strongestMatched = true;
      if (matchNet > bestNet) {
        bestNet = matchNet;
        bestEnergy = matchEnergy;
        bestDetect = dt;
      }
      // Unambiguous only: an actual peak, and no rival line closer to it.
      if (peakMatched && bestPeakIdx >= 0 && matchNet > soleNet &&
          bestDist <= bestClaimDist[bestPeakIdx] + 0.01f) {
        soleNet = matchNet;
        soleDetect = dt;
      }
      if (matchSigma > bestSigma) bestSigma = matchSigma;
      if (matchSigma < weakestSigma) weakestSigma = matchSigma;
    }

    // Gate 1: the nuclide's most *detectable* line must be present. Matching
    // only a minor line is not evidence (this produced the original Ba-133
    // false positive on real ore).
    if (!strongestMatched) {
      rejectLog(useCompetition, def.symbol, "no defining line", nMatched, nExpectedMatched,
                 expectedCount, bestSigma);
      continue;
    }
    // Gate 2: corroboration from a second line, but only counting lines that
    // could plausibly be seen at all. A nuclide whose signature is genuinely
    // a single detectable line (Pb-212, Cs-137, K-40) is accepted on that one.
    // Entitled to contest from here on: the defining line is matched and
    // significant. Whether a second line corroborates it decides only
    // whether we are willing to *report* the nuclide.
    if (bestSigma >= MIN_CANDIDATE_SIGMA) {
      contender[iso] = true;
      distinctPeaks[iso] = nAnchorPeaks;
      contenderSigma[iso] = bestSigma;
    }

    uint8_t required = expectedCount >= 2 ? 2 : 1;
    // A confirmed sibling in the same decay chain supplies the
    // corroboration instead. Real uranium ore shows Pb-214's 351.9 keV
    // line at 18 sigma while its 295.2 keV line stays buried in the
    // Compton continuum; refusing the nuclide then also cost the chain a
    // member and dropped the whole verdict to BACKGROUND.
    bool chainSupport = (def.chain != CHAIN_NONE) && chainAnchored[(int)def.chain];
    if (chainSupport && nExpectedMatched >= 1) required = 1;

    // Conversely, a chain member resting on a SINGLE peak cannot certify
    // itself. Ra-226 has only its 186.2 keV line, so one bump read
    // "1/1 lines = 100%" and turned plain background into uranium; U-235
    // did the same, its other three lines all lying inside one FWHM of
    // 185.7 keV. Such a nuclide needs a sibling to vouch for it.
    //
    // The test is deliberately how much independent evidence this nuclide
    // actually has, not how many lines it might in principle show. Phrasing
    // it as "few detectable lines" instead collapsed both chains: Bi-214
    // and Tl-208 have one dominant line each, so they demanded support -
    // but they are precisely the members that anchor their chains, and
    // nothing could then anchor anything. Two independent peaks stands on
    // its own; one does not.
    if (nDistinctPeaks < 2 && def.chain != CHAIN_NONE && !chainSupport) {
      rejectLog(useCompetition, def.symbol, "single line, no chain", nMatched,
                nExpectedMatched, expectedCount, bestSigma);
      continue;
    }

    if (nExpectedMatched < required) {
      rejectLog(useCompetition, def.symbol, "needs corroboration", nMatched, nExpectedMatched,
                 expectedCount, bestSigma);
      continue;
    }
    // Gate 3: the evidence itself must be statistically real, and much
    // more so when it all comes from the unmodelled low-energy region.
    float needSigma = lowEnergyOnly ? LOW_ENERGY_ONLY_SIGMA : MIN_CANDIDATE_SIGMA;
    if (bestSigma < needSigma) {
      rejectLog(useCompetition, def.symbol, "below 3 sigma", nMatched, nExpectedMatched,
                 expectedCount, bestSigma);
      continue;
    }

    scores[iso].value = matchedDetect / totalDetect;
    scores[iso].bestEnergy = bestEnergy;
    scores[iso].bestNet = soleNet;
    scores[iso].bestDetect = soleDetect;
    scores[iso].bestSigma = bestSigma;
    scores[iso].weakestSigma = (weakestSigma > 1e8f) ? 0.0f : weakestSigma;
    scores[iso].matched = nMatched;
  }
}

// Competitive assignment must only be contested by nuclides that are
// themselves credible.
//
// The single-pass version let *every* line in the library set the bar. On real
// uranium ore that lost Pb-214 entirely: Eu-152's 344.3 keV line sat 0.9 keV
// from the measured 343.4 keV peak and Pb-214's 351.9 keV line 8.5 keV away,
// so Pb-214 was denied its own strongest line - and I-131's 284.3 keV did the
// same to Pb-214's 295.2 keV. Neither Eu-152 nor I-131 passed a single gate or
// appeared as a candidate; they vetoed a real nuclide purely by existing in the
// table. Adding nuclides would make that steadily worse, which is the opposite
// of what a library should do.
//
// So: pass 1 scores with competition disabled, which says which nuclides are
// plausible at all. Pass 2 recomputes the claims using only those, then scores
// properly. A nuclide can now only lose a peak to a rival that is itself
// supported by the data.
static void scoreIsotopes(const SpectrometerData& d, const DetectedPeak* peaks, int peakCount) {
  // Pass 1 is permissive - no competition - purely to find out which nuclides
  // are plausible at all.
  for (uint16_t i = 0; i < ISOTOPE_LIBRARY_MAX; i++) eligible[i] = true;
  memset(chainAnchored, 0, sizeof(chainAnchored));
  scorePass(d, peaks, peakCount, /* useCompetition */ false);

  // Then iterate to a fixed point, on who may CONTEST rather than who gets
  // listed. Those are different questions and conflating them created a
  // vicious circle on real uranium ore: Pb-214 matched its 351.9 keV line at
  // 16 sigma but lacked a second line, so it was dropped, so it stopped
  // contesting - and Ba-133, whose 356 keV line is further from the same
  // peak, won it by default and was reported at 89%. The real nuclide was
  // invisible and a false one took its place.
  //
  // Corroboration decides whether we are willing to report a nuclide. It
  // must not decide whether that nuclide is allowed to explain a peak.
  // Nuclides rejected outright (I-131, Eu-152 here - no defining line at
  // all) still drop out, which is what stops them vetoing real ones. The
  // contender set only shrinks, so this converges in a couple of rounds.
  for (int round = 0; round < 4; round++) {
    bool changed = false, any = false;
    for (uint16_t i = 0; i < IsotopeLibrary::count(); i++) {
      bool nowEligible = contender[i];
      if (nowEligible != eligible[i]) changed = true;
      eligible[i] = nowEligible;
      if (nowEligible) any = true;
    }
    // Nothing plausible: keep the empty result rather than running a pass in
    // which no line may contest anything.
    if (!any) return;

    // A chain is anchored by a member identified from two or more
    // independent peaks. That is strong enough to vouch for siblings whose
    // own second line is buried, without letting a single ambiguous line
    // bootstrap a whole chain out of nothing.
    // A chain is anchored either by one member carrying two independent
    // peaks, or by two different members each carrying one. The second form
    // matters: on a real thorium mantle Pb-212 (238.6 keV at 10 sigma) and
    // Tl-208 (583.2 keV) each had a single line, so each was refused for
    // want of corroboration the other could not give, and the chain never
    // formed. Two independent members IS decay-chain corroboration - that is
    // the whole meaning of a chain - and demanding it come from one nuclide
    // was stricter than the physics requires.
    memset(chainAnchored, 0, sizeof(chainAnchored));
    int chainMembers[3] = {0, 0, 0};
    for (uint16_t i = 0; i < IsotopeLibrary::count(); i++) {
      DecayChain c = IsotopeLibrary::at(i).chain;
      if (c == CHAIN_NONE) continue;
      if (scores[i].value > 0.0f && distinctPeaks[i] >= 2) {
        chainAnchored[(int)c] = true;
      }
      // Members that matched their own defining line significantly, whether
      // or not they could be reported on their own.
      if (contender[i] && distinctPeaks[i] >= 1) chainMembers[(int)c]++;
      // One member can carry the chain when its own evidence is not
      // marginal at all. On a real thorium mantle Pb-212's 238.6 keV line
      // came in at 8.4 sigma while Tl-208 and Ac-228 sat around 2 sigma -
      // genuinely present but not yet able to corroborate anything - so
      // nothing formed and an obviously thorium sample read BACKGROUND.
      // Nothing else in nature emits at 238.6 keV with that strength.
      if (contender[i] && distinctPeaks[i] >= 1 &&
          contenderSigma[i] >= CHAIN_ANCHOR_SOLO_SIGMA) {
        chainAnchored[(int)c] = true;
      }
    }
    for (int c = 1; c < 3; c++) {
      if (chainMembers[c] >= 2) chainAnchored[c] = true;
      // Keep an established chain while at least one member is still
      // credible, rather than requiring the full anchor every pass.
      if (!chainAnchored[c] && chainHeld[c] && chainMembers[c] >= 1) {
        chainAnchored[c] = true;
      }
      if (chainAnchored[c]) chainHeld[c] = true;
    }

    if (!changed && round > 0) break;

    computeBestClaims(peaks, peakCount);
    scorePass(d, peaks, peakCount, /* useCompetition */ true);
  }
}

// Combine member nuclides into chain-level evidence. Members of a decay chain
// in secular equilibrium must co-occur, so independent detections of several
// members is far stronger evidence than any single nuclide match - and it is
// the only way to report "uranium" at all, since U-238 itself emits no
// usable gammas.
// How consistent a chain's detected members are with each other in
// INTENSITY, not just in energy.
//
// Energy alone cannot separate the two natural chains from two peaks: their
// line spacings differ by 0.4% (609.3/351.9 = 1.7314 against 583.2/338.3 =
// 1.7238), far below what 8.4% FWHM resolves. Real uranium ore at 0.4 uSv/h
// therefore fitted both and was reported as NATURAL MIX.
//
// Intensity settles it. Chain members are in secular equilibrium, so they
// share one activity, and each matched peak implies an activity of
// area / (emission x efficiency). Under the right hypothesis those implied
// activities agree; under the wrong one they do not. On that same ore:
// uranium gave a spread of 1.11, thorium 1.93.
//
// Returns 1.0 when a chain has too few members to judge, so this can only
// discriminate, never invent evidence.
// Fraction of its chain's decays that actually pass through this nuclide.
// Library intensities are per decay OF THAT NUCLIDE, but members of a chain
// in secular equilibrium share the chain's activity, so comparing them
// requires the branch fraction as well.
//
//   Tl-208  reached only via the 35.94% alpha branch of Bi-212
//   U-235   a separate isotope at 0.72% abundance, about 4.6% of U-238's
//           activity in natural uranium
//
// Nuclear data, so it belongs in the nuclide library; it lives here until
// IsotopeDef carries a field for it, and defaults to 1.0 for everything else
// including any user-supplied entry.
static float chainBranchFraction(const char* symbol) {
  if (strcmp(symbol, "Tl-208") == 0) return 0.3594f;
  if (strcmp(symbol, "U-235") == 0)  return 0.046f;
  return 1.0f;
}

// Tests a whole chain hypothesis against the observed peaks.
//
// Energy alone cannot separate the two natural chains from a couple of peaks:
// their line ratios differ by 0.4% (609.3/351.9 against 583.2/338.3), far
// below what 8.4% FWHM resolves, so real uranium ore at 0.40 uSv/h fitted both
// and was reported as NATURAL MIX. Peak AREAS separate them cleanly.
//
// Chain members are in secular equilibrium and share one activity, so under
// the correct hypothesis every peak implies the same activity:
//     activity = area / (emission x efficiency x branch fraction)
// Under the wrong one they disagree. Measured on the real ore: uranium gave a
// spread of 1.11, thorium 1.93.
//
// Every peak is offered to every chain independently - deliberately NOT using
// the competitive assignment, which is about identification rather than about
// testing a hypothesis, and which excludes exactly the contested peaks that
// carry the answer. Sub-110 keV peaks and detector artefacts are skipped: the
// X-ray forest is not modelled and its area means nothing here.
//
// Returns 1.0 when fewer than two peaks fit, so this can only discriminate
// between hypotheses, never manufacture one.
static float chainHypothesisSpread(DecayChain chain, const DetectedPeak* peaks,
                                   int peakCount) {
  float lo = 1e30f, hi = 0.0f;
  int n = 0;
  for (int p = 0; p < peakCount && p < MAX_DETECTED_PEAKS; p++) {
    if (peaks[p].significance < MIN_CANDIDATE_SIGMA) continue;
    if (!isDiagnosticLine(peaks[p].energyKeV)) continue;
    if (artefactPeak[p]) continue;

    float bestDist = 1e9f, bestWeight = 0.0f;
    for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
      const IsotopeDef& def = IsotopeLibrary::at(iso);
      if (def.chain != chain) continue;
      float branch = chainBranchFraction(def.symbol);
      for (uint8_t li = 0; li < def.lineCount; li++) {
        float d = fabsf(def.lines[li].energyKeV - peaks[p].energyKeV);
        if (d > fwhmKeVAt(def.lines[li].energyKeV) * MATCH_TOLERANCE_FWHM) continue;
        if (d < bestDist) {
          bestDist = d;
          bestWeight = detectabilityOf(def.lines[li]) * branch;
        }
      }
    }
    if (bestWeight <= 0.0f) continue;

    float activity = peaks[p].netCounts / bestWeight;
    if (activity < lo) lo = activity;
    if (activity > hi) hi = activity;
    n++;
  }
  if (n < 2 || lo <= 0.0f) return 1.0f;
  return hi / lo;
}

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

// Hysteresis on the chain thresholds. Evidence near a single threshold
// crosses it back and forth as counts arrive, and on real ore that showed
// as the verdict alternating between URANIUM ORE and BACKGROUND every five
// seconds with the candidate list appearing and vanishing. A verdict has to
// be harder to lose than it was to gain: claim a chain at 0.5, keep it
// until it falls below 0.3.
#define CHAIN_CLAIM_CONFIDENCE 0.50f
#define CHAIN_HOLD_CONFIDENCE  0.30f

// Before declaring a sample to be nothing but background, require enough
// counting to have had a fair chance of seeing something. Roughly two
// minutes at a typical indoor rate.
#define MIN_BACKGROUND_SECONDS 120u
#define MIN_BACKGROUND_COUNTS  4000u

// K-40 must clearly dominate before the sample is called potassium, rather
// than merely being present as it is everywhere.
// Natural U-238 and Th-232 daughters are in every wall: concrete, brick and
// granite all carry them, and over a couple of hours their weak lines
// accumulate enough counts to be identified correctly. Detecting them is
// therefore not a finding - an ordinary room measured for two hours read
// "THORIUM" on genuine Tl-208 2614.5 keV and three Ac-228 lines. A chain
// only describes the SAMPLE once the sample is actually above ambient, so
// the verdict needs elevation, not mere presence. The nuclides remain
// listed as evidence either way; only the headline claim is withheld.
// Typical indoor background on this detector is 0.15-0.18 uSv/h at ~24 cps,
// so these sit about 1.5x above it - clear of natural variation without
// rejecting genuinely weak samples. 0.30 was too high and suppressed real
// uranium ore reading 0.28. Count rate is offered as an alternative because
// it is what the spectrum is actually built from: a sample rich in
// low-energy emission raises counts more than it raises dose.
#define SAMPLE_MIN_DOSE_USVH  0.20f
#define SAMPLE_MIN_CPS        35.0f

#define POTASSIUM_MIN_SIGMA   12.0f
#define POTASSIUM_MIN_SECONDS 300u

static bool heldUranium = false;
static bool heldThorium = false;

void resetVerdictHysteresis() {
  heldUranium = false;
  heldThorium = false;
  chainHeld[0] = chainHeld[1] = chainHeld[2] = false;
}

static SourceClass classify(SpectrometerData& d, float bestArtificial, float k40,
                            float k40Sigma) {
  d.verdictConfidence = 0.0f;
  if (d.quality.peaksFound == 0 || d.quality.bestSigma < 3.0f) {
    resetVerdictHysteresis();
    return SRC_INSUFFICIENT;
  }

  heldUranium = heldUranium ? (d.uranium.confidence >= CHAIN_HOLD_CONFIDENCE)
                            : (d.uranium.confidence >= CHAIN_CLAIM_CONFIDENCE);
  heldThorium = heldThorium ? (d.thorium.confidence >= CHAIN_HOLD_CONFIDENCE)
                            : (d.thorium.confidence >= CHAIN_CLAIM_CONFIDENCE);
  bool u = heldUranium;
  bool t = heldThorium;

  if (bestArtificial >= 0.6f && bestArtificial > d.uranium.confidence &&
      bestArtificial > d.thorium.confidence) {
    d.verdictConfidence = bestArtificial;
    return SRC_ARTIFICIAL;
  }
  // Below ambient elevation, natural chains describe the room rather than
  // anything placed in front of the detector. Man-made nuclides are exempt:
  // a weak Cs-137 source barely lifts the dose rate and is still a genuine
  // find, whereas natural thorium at background level never is.
  // With a stored background reference in use, the chains were already
  // computed on the sample's own excess, so elevation is established by
  // construction and this crude dose floor would only get in the way - it
  // would suppress a genuine weak source, which is the whole case the
  // reference exists to handle. Without a reference there is nothing to
  // compare against, so fall back to requiring a dose clearly above
  // ordinary indoor ambient.
  bool elevated = backgroundApplied() ||
                  (d.doseRateUSvH >= SAMPLE_MIN_DOSE_USVH) ||
                  (d.cps >= SAMPLE_MIN_CPS);
  if (!elevated) { u = false; t = false; }

  if (u && t) {
    // "Mixture" should mean the two chains are comparably present. Real
    // uranium ore reads U 97% against Th 70% - the thorium being the same
    // two peaks read a second way, since at this resolution both chains fit
    // them - and calling that NATURAL MIX buries the answer. When one chain
    // is clearly stronger, name it; reserve MIXED for a genuine balance.
    float hi = d.uranium.confidence, lo = d.thorium.confidence;
    bool uranDominant = hi >= lo;
    if (!uranDominant) { hi = d.thorium.confidence; lo = d.uranium.confidence; }
    if (lo > 0.0f && hi >= lo * CHAIN_DOMINANCE_RATIO) {
      d.verdictConfidence = hi;
      return uranDominant ? SRC_URANIUM : SRC_THORIUM;
    }
    d.verdictConfidence = lo;
    return SRC_MIXED_NATURAL;
  }
  if (u) { d.verdictConfidence = d.uranium.confidence; return SRC_URANIUM; }
  if (t) { d.verdictConfidence = d.thorium.confidence; return SRC_THORIUM; }
  // K-40 is in every building material, every bag of soil and every human
  // body, so simply detecting it says nothing - and because it has a single
  // library line, its score is 1.0 the instant that line matches. Reporting
  // "K-40" for ordinary background is misleading. Call it only when the
  // line genuinely dominates, which needs both a strong peak and enough
  // counting time to establish that it is elevated rather than ambient.
  if (k40 >= 0.5f && k40Sigma >= POTASSIUM_MIN_SIGMA &&
      d.spectrumDurationSec >= POTASSIUM_MIN_SECONDS) {
    d.verdictConfidence = k40;
    return SRC_POTASSIUM;
  }

  // "BACKGROUND" is a positive claim - that there is nothing here - and
  // early in an acquisition it is one we cannot support. A real uranium
  // sample sitting beside the detector read BACKGROUND within a minute,
  // before its lines had the statistics to be identified, which is worse
  // than saying nothing: it invites the user to conclude the sample is
  // inert. Until there has been enough counting to have found something,
  // the honest answer is that we are still measuring.
  if (d.spectrumDurationSec < MIN_BACKGROUND_SECONDS ||
      d.quality.totalCounts < MIN_BACKGROUND_COUNTS) {
    return SRC_INSUFFICIENT;
  }

  // Never call an elevated sample "background". A thorium mantle at
  // 0.33 uSv/h read BACKGROUND for its first three minutes and only became
  // THORIUM at five - but during those three minutes the radiation was
  // plainly above ambient, so "background" was not an early answer, it was a
  // wrong one. Identifying which nuclide takes counting time; noticing that
  // there is something there does not. While the sample is elevated and
  // nothing is identified yet, keep saying so.
  if (elevated) return SRC_INSUFFICIENT;

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
  // This grade describes confidence in the IDENTIFICATION, so with nothing
  // identified there is no confidence to report. Falling back to the
  // strongest peak graded an unidentified 13.5 sigma X-ray bump as
  // EXCELLENT while the screen showed no source at all.
  if (d.candidateCount == 0) {
    d.quality.weakestKeySigma = 0.0f;
    d.quality.grade = QUAL_POOR;
    d.quality.recommendedSeconds = 0;
    d.quality.remainingSeconds = 0;   // or the screen keeps a stale estimate
    return;
  }

  float weakest = d.candidates[0].significance;
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

  // Align the energy axis with the library before anything is matched.
  // DISABLED. The consensus estimator never once produced a correct value on
  // real spectra: it sat at 1.000 whenever a short measurement contained no
  // isolated reference lines, and when it did move it locked onto 0.940 -
  // the wrong direction, at the edge of its search range - and turned
  // uranium ore into thorium. A wrong correction is far worse than none, so
  // the axis is left as the detector reports it until this can be done
  // properly, most likely against a known calibration source rather than by
  // voting against the library.
  float previous = gainCorr;
  gainCorr = 1.0f;
  (void)previous;
  if (false) gainCorr = estimateGain(g_lastPeaks, g_lastPeakCount);

  for (int i = 0; i < g_lastPeakCount; i++) {
    g_lastPeaks[i].energyKeV *= gainCorr;
  }

  // Identify which peaks are detector features rather than gamma lines,
  // on the corrected axis, before any nuclide may claim them.
  markArtefacts(g_lastPeaks, g_lastPeakCount);

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

  // When both chains claim the same evidence, the one whose members disagree
  // about their own activity is the wrong reading of it. Only applied when
  // both are actually claimed and one is clearly worse, so a genuine mixed
  // sample - where both chains are internally consistent - survives.
  if (d.uranium.confidence > 0.0f && d.thorium.confidence > 0.0f) {
    float su = chainHypothesisSpread(CHAIN_U238, g_lastPeaks, g_lastPeakCount);
    float st = chainHypothesisSpread(CHAIN_TH232, g_lastPeaks, g_lastPeakCount);
#if PEAK_SEARCH_DEBUG
    Serial.printf("[ANL] intensity spread  U %.2f  Th %.2f\n", su, st);
#endif
    // Logged, not acted on. The test is only valid for a single-chain
    // sample: on a genuine mixture each hypothesis is measured against the
    // other chain's peaks too, so both look inconsistent (U 13.51, Th 2.39
    // on a synthetic uranium+thorium case that is perfectly consistent by
    // construction). On single-chain samples it is clearly informative -
    // real uranium ore gives U 1.30 against Th 1.64 - and is the right
    // basis for this once peaks can be apportioned between chains first.
    (void)su; (void)st;
  }

  float bestArtificial = 0.0f, k40 = 0.0f, k40Sigma = 0.0f;
  for (uint16_t iso = 0; iso < IsotopeLibrary::count(); iso++) {
    if (IsotopeLibrary::at(iso).category == CAT_ARTIFICIAL && scores[iso].value > bestArtificial) {
      bestArtificial = scores[iso].value;
    }
    if (strcmp(IsotopeLibrary::at(iso).symbol, "K-40") == 0) {
      k40 = scores[iso].value;
      k40Sigma = scores[iso].bestSigma;
    }
  }

  computeQuality(d, g_lastPeaks, g_lastPeakCount);
  d.sourceClass = classify(d, bestArtificial, k40, k40Sigma);

#if PEAK_SEARCH_DEBUG
  Serial.printf("\n[ANL] --- %u peaks, %u counts, %us acq, %.2f uSv/h, gain %.3f ---\n",
                (unsigned)g_lastPeakCount, (unsigned)d.quality.totalCounts,
                (unsigned)d.spectrumDurationSec, d.doseRateUSvH, gainCorr);
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
