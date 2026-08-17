#include "peak_search.h"
#include "../data/isotope_db.h"
#include <math.h>
#include <string.h>

static float smoothed[SPECTRUM_CHANNELS];
static float continuum[SPECTRUM_CHANNELS];

static void smoothSpectrum(const SpectrometerData& d) {
  const int win = 2; // +-2 => 5-wide boxcar
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    int lo = i - win < 0 ? 0 : i - win;
    int hi = i + win >= SPECTRUM_CHANNELS ? SPECTRUM_CHANNELS - 1 : i + win;
    uint32_t sum = 0;
    for (int j = lo; j <= hi; j++) sum += d.spectrum[j];
    smoothed[i] = (float)sum / (hi - lo + 1);
  }
}

static void estimateContinuum() {
  const int win = 20; // +-20 => 41-wide boxcar, wide enough to average out narrow photopeaks
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    int lo = i - win < 0 ? 0 : i - win;
    int hi = i + win >= SPECTRUM_CHANNELS ? SPECTRUM_CHANNELS - 1 : i + win;
    float sum = 0;
    for (int j = lo; j <= hi; j++) sum += smoothed[j];
    continuum[i] = sum / (hi - lo + 1);
  }
}

int findPeaks(const SpectrometerData& d, DetectedPeak* out, int maxPeaks) {
  smoothSpectrum(d);
  estimateContinuum();

  int count = 0;
  const int minChannel = 3; // skip low-energy electronic noise pileup near ch0
  for (int i = minChannel; i < SPECTRUM_CHANNELS - 1 && count < maxPeaks; i++) {
    float net = smoothed[i] - continuum[i];
    if (net <= 0) continue;

    bool isLocalMax = true;
    for (int j = i - 3; j <= i + 3; j++) {
      if (j < 0 || j >= SPECTRUM_CHANNELS || j == i) continue;
      if (smoothed[j] > smoothed[i]) { isLocalMax = false; break; }
    }
    if (!isLocalMax) continue;

    // statistical significance: net signal vs Poisson noise of the continuum
    float sigma = sqrtf(continuum[i] + 1.0f);
    if (net < 3.0f * sigma) continue;
    if (net < 5.0f) continue; // absolute floor so a near-empty spectrum doesn't flag noise

    out[count].channel = i;
    out[count].energyKeV = channelToKeV(d, i);
    out[count].netCounts = (uint32_t)net;
    count++;
  }
  return count;
}

// Scintillator resolution degrades roughly as 1/sqrt(E) (Poisson photon-statistics
// dominated). Anchor to the measured RC-110 figure: 8.4% FWHM at 662 keV.
static float fwhmKeVAt(float energyKeV) {
  if (energyKeV < 20.0f) energyKeV = 20.0f; // avoid blowing up near zero
  const float refE = 662.0f;
  const float refFwhmPct = 8.4f;
  float fwhmPct = refFwhmPct * sqrtf(refE / energyKeV);
  return (fwhmPct / 100.0f) * energyKeV;
}

void analyzeSpectrum(SpectrometerData& d) {
  DetectedPeak peaks[MAX_DETECTED_PEAKS];
  int peakCount = findPeaks(d, peaks, MAX_DETECTED_PEAKS);

  struct Score { float value; float bestEnergy; };
  static Score scores[ISOTOPE_DB_COUNT];
  memset(scores, 0, sizeof(scores));

  for (uint8_t iso = 0; iso < ISOTOPE_DB_COUNT; iso++) {
    const IsotopeDef& def = ISOTOPE_DB[iso];
    float matchedIntensity = 0.0f;
    float totalIntensity = 0.0f;
    float bestEnergy = 0.0f;
    float bestNet = 0.0f;

    // A candidate must have its single strongest line matched — matching
    // only a minor secondary line (e.g. Ba-133's weak 81 keV line without
    // its dominant 356 keV line) isn't real evidence, just noise in a
    // crowded low-energy region. Require the real signature, not a fragment.
    float maxLineIntensity = 0.0f;
    for (uint8_t li = 0; li < def.lineCount; li++) {
      if (def.lines[li].intensity > maxLineIntensity) maxLineIntensity = def.lines[li].intensity;
    }
    bool strongestLineMatched = false;

    for (uint8_t li = 0; li < def.lineCount; li++) {
      const IsotopeLine& line = def.lines[li];
      totalIntensity += line.intensity;
      float tolerance = fwhmKeVAt(line.energyKeV) * 0.75f; // ~0.75 FWHM matching window

      float bestDist = tolerance + 1.0f;
      int bestPeakIdx = -1;
      for (int p = 0; p < peakCount; p++) {
        float dist = fabsf(peaks[p].energyKeV - line.energyKeV);
        if (dist < bestDist) { bestDist = dist; bestPeakIdx = p; }
      }
      if (bestPeakIdx >= 0 && bestDist <= tolerance) {
        matchedIntensity += line.intensity;
        if (line.intensity >= maxLineIntensity) strongestLineMatched = true;
        if ((float)peaks[bestPeakIdx].netCounts > bestNet) {
          bestNet = (float)peaks[bestPeakIdx].netCounts;
          bestEnergy = peaks[bestPeakIdx].energyKeV;
        }
      }
    }

    if (strongestLineMatched && matchedIntensity > 0.0f && totalIntensity > 0.0f) {
      scores[iso].value = matchedIntensity / totalIntensity;
      scores[iso].bestEnergy = bestEnergy;
    }
  }

  // Selection sort top MAX_CANDIDATES with score > 0 into d.candidates.
  d.candidateCount = 0;
  bool used[ISOTOPE_DB_COUNT];
  memset(used, 0, sizeof(used));

  for (int slot = 0; slot < MAX_CANDIDATES; slot++) {
    int bestIdx = -1;
    float bestVal = 0.0f;
    for (uint8_t iso = 0; iso < ISOTOPE_DB_COUNT; iso++) {
      if (used[iso]) continue;
      if (scores[iso].value > bestVal) { bestVal = scores[iso].value; bestIdx = iso; }
    }
    if (bestIdx < 0 || bestVal <= 0.0f) break;
    used[bestIdx] = true;

    IsotopeCandidate& c = d.candidates[d.candidateCount];
    c.symbol = ISOTOPE_DB[bestIdx].symbol;
    c.name = ISOTOPE_DB[bestIdx].name;
    c.confidence = bestVal > 1.0f ? 1.0f : bestVal;
    c.matchedEnergyKeV = scores[bestIdx].bestEnergy;
    d.candidateCount++;
  }
}
