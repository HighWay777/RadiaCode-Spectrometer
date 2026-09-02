#include "templates.h"
#include "peak_search.h"          // fwhmKeVAt()
#include "../data/isotope_library.h"
#include "../data/background.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdio.h>
#endif

namespace Templates {

static float* s_matrix = nullptr;      // [nCols][SPECTRUM_CHANNELS]
static int    s_cols = 0;
static int    s_nuclides = 0;
static int    s_contFirst = 0;
static int    s_chainFirst = 0;
static int    s_bgCol = -1;
static bool   s_valid = false;

// Cached inputs, so build() can no-op when nothing has changed.
static float  s_a0 = 0, s_a1 = 0, s_a2 = 0;
static uint16_t s_libCount = 0;
static bool   s_bgActive = false;

static char   s_names[CONTINUUM_BASIS + 3][16]; // names for non-nuclide columns

// Width of each continuum bump, as a fraction of its centre energy. The
// detector is ~8.4% FWHM, so 40% is about five resolution widths - broad
// enough to be unmistakably continuum, narrow enough that 16 of them can
// follow a real spectrum shape.
#define CONTINUUM_WIDTH 0.40f

void invalidate() { s_valid = false; }

// --- detector response ------------------------------------------------------
//
// Deliberately a separate model from the one in peak_search.cpp: that one
// exists to rank which line is most likely to be *visible*, this one has to
// predict an actual spectrum shape. Both are approximations of the same
// crystal, but they are used for different things and should not be shared.

// Full-energy peak efficiency of a small CsI(Tl) crystal, relative to its
// value at 200 keV. Photoelectric absorption falls steeply with energy;
// below ~80 keV the housing and crystal surface attenuate.
static float photopeakEff(float e) {
  if (e < 25.0f) e = 25.0f;
  float eff = (e >= 200.0f) ? powf(200.0f / e, 1.2f) : 1.0f;
  if (e < 80.0f) eff *= e / 80.0f;
  return eff;
}

// Ratio of Compton-continuum counts to photopeak counts. For a small crystal
// this grows steeply with energy - a 2614 keV gamma rarely deposits all its
// energy, which is exactly why Tl-208's 583 keV line is easier to see than
// its far more intense 2614 keV one.
static float comptonRatio(float e) {
  float r = 0.30f + e / 420.0f;
  return r > 6.0f ? 6.0f : r;
}

static float energyOf(int ch) { return s_a0 + s_a1 * ch + s_a2 * ch * ch; }

// Invert the quadratic calibration. a2 is tiny, so Newton converges at once.
static float channelOf(float e) {
  float ch = (e - s_a0) / (s_a1 != 0.0f ? s_a1 : 1.0f);
  for (int i = 0; i < 4; i++) {
    float f = s_a0 + s_a1 * ch + s_a2 * ch * ch - e;
    float d = s_a1 + 2.0f * s_a2 * ch;
    if (fabsf(d) < 1e-6f) break;
    ch -= f / d;
  }
  return ch;
}

static void addGaussian(float* col, float energyKeV, float area) {
  if (area <= 0.0f || energyKeV <= 0.0f) return;
  float dEdCh = s_a1 + 2.0f * s_a2 * channelOf(energyKeV);
  if (dEdCh < 0.01f) dEdCh = 0.01f;
  float sigmaCh = (fwhmKeVAt(energyKeV) / 2.3548f) / dEdCh;
  if (sigmaCh < 0.5f) sigmaCh = 0.5f;

  float centre = channelOf(energyKeV);
  int lo = (int)(centre - 4.0f * sigmaCh);
  int hi = (int)(centre + 4.0f * sigmaCh);
  if (lo < 0) lo = 0;
  if (hi >= SPECTRUM_CHANNELS) hi = SPECTRUM_CHANNELS - 1;

  float norm = area / (sigmaCh * 2.5066283f);   // sqrt(2*pi)
  for (int ch = lo; ch <= hi; ch++) {
    float d = ((float)ch - centre) / sigmaCh;
    col[ch] += norm * expf(-0.5f * d * d);
  }
}

// The continuum a single gamma line produces: a Compton plateau running from
// the Compton edge down to zero energy, plus the backscatter bump that the
// same scattering produces in surrounding material.
static void addCompton(float* col, float energyKeV, float photopeakArea) {
  if (photopeakArea <= 0.0f || energyKeV < 40.0f) return;

  float total = photopeakArea * comptonRatio(energyKeV);
  float edge = energyKeV * (1.0f - 1.0f / (1.0f + 2.0f * energyKeV / 511.0f));
  int edgeCh = (int)channelOf(edge);
  if (edgeCh > SPECTRUM_CHANNELS - 1) edgeCh = SPECTRUM_CHANNELS - 1;
  if (edgeCh < 4) return;

  // Klein-Nishina gives a roughly flat plateau rising slightly toward the
  // edge. Exact shape matters far less than having the counts accounted for
  // somewhere, which is what stops them being attributed to a nuclide.
  float per = total * 0.85f / (float)edgeCh;
  for (int ch = 1; ch < edgeCh; ch++) {
    float f = (float)ch / (float)edgeCh;
    col[ch] += per * (0.65f + 0.7f * f);
  }
  float bs = energyKeV / (1.0f + 2.0f * energyKeV / 511.0f);
  if (bs > 25.0f && bs < energyKeV) addGaussian(col, bs, total * 0.15f);
}

// Fraction of chain decays that actually pass through this member. Tl-208 is
// only reached via the 35.94% alpha branch of Bi-212, and U-235 accompanies
// natural uranium at 0.72% by atom but ~4.6% by activity.
float chainBranch(const char* symbol) {
  if (strcmp(symbol, "Tl-208") == 0) return 0.3594f;
  if (strcmp(symbol, "Bi-212") == 0) return 0.6406f;
  if (strcmp(symbol, "U-235") == 0)  return 0.046f;
  return 1.0f;
}

// Adds one nuclide's full response - photopeaks plus their Compton
// continua - into a column, scaled by weight.
static void addNuclideTo(float* col, const IsotopeDef& def, float weight) {
  if (weight <= 0.0f) return;
  for (uint8_t li = 0; li < def.lineCount; li++) {
    float e = def.lines[li].energyKeV;
    if (e <= 0.0f) continue;
    // Absolute emission probability x detection efficiency.
    float area = def.lines[li].intensity * photopeakEff(e) * weight;
    if (area <= 0.0f) continue;
    addGaussian(col, e, area);
    addCompton(col, e, area);
  }
}

// --- build ------------------------------------------------------------------

static bool allocate(int cols) {
  size_t bytes = (size_t)cols * SPECTRUM_CHANNELS * sizeof(float);
  if (s_matrix && s_cols == cols) return true;
  if (s_matrix) { free(s_matrix); s_matrix = nullptr; }

#ifdef ARDUINO
  s_matrix = (float*)ps_malloc(bytes);       // PSRAM first, as the library does
  if (!s_matrix) s_matrix = (float*)malloc(bytes);
#else
  s_matrix = (float*)malloc(bytes);
#endif
  if (!s_matrix) {
    s_cols = 0;
#ifdef ARDUINO
    // Silence here was a real failure mode: build() returning false makes
    // analyzeSpectrum() fall back to the old peak-search path, so the device
    // looks like it is working while running entirely different code.
    Serial.printf("[TPL] ALLOC FAILED: %u bytes for %d columns "
                  "(psram total %u free %u, heap free %u)\n",
                  (unsigned)bytes, cols, (unsigned)ESP.getPsramSize(),
                  (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
#endif
    return false;
  }
#ifdef ARDUINO
  Serial.printf("[TPL] matrix %u bytes for %d columns (psram free %u, heap free %u)\n",
                (unsigned)bytes, cols, (unsigned)ESP.getFreePsram(),
                (unsigned)ESP.getFreeHeap());
#endif
  s_cols = cols;
  return true;
}

bool build(const SpectrometerData& d) {
  uint16_t lib = IsotopeLibrary::count();
  bool bgActive = Background::active();

  if (s_valid && lib == s_libCount && bgActive == s_bgActive &&
      d.calibA0 == s_a0 && d.calibA1 == s_a1 && d.calibA2 == s_a2) {
    return true;                              // nothing relevant changed
  }

  s_a0 = d.calibA0; s_a1 = d.calibA1; s_a2 = d.calibA2;
  s_libCount = lib;
  s_bgActive = bgActive;

  // nuclides + 2 composite chains + continuum basis + optional stored ref
  int cols = (int)lib + CHAIN_COLUMNS + CONTINUUM_BASIS + (bgActive ? 1 : 0);
  if (!allocate(cols)) return false;

  memset(s_matrix, 0, (size_t)cols * SPECTRUM_CHANNELS * sizeof(float));
  s_nuclides = (int)lib;
  s_chainFirst = (int)lib;
  s_contFirst = s_chainFirst + CHAIN_COLUMNS;
  s_bgCol = bgActive ? s_contFirst + CONTINUUM_BASIS : -1;

  // --- one column per standalone nuclide ---
  //
  // Decay-chain members deliberately get NO individual column. They are
  // fitted only through the composite chain column below, which holds them
  // at secular-equilibrium branching.
  //
  // Giving a member both its own column and a share of the composite makes
  // the two exactly degenerate - they span the same space - and the Fisher
  // matrix becomes singular. Measured: every chain nuclide came back with a
  // sigma of ~10^5, i.e. the fit correctly reporting that it cannot tell the
  // two apart. Constraining the chain is the whole point, so the constraint
  // wins and the free members go.
  for (uint16_t iso = 0; iso < lib; iso++) {
    const IsotopeDef& def = IsotopeLibrary::at(iso);
    if (def.chain != CHAIN_NONE) continue;      // fitted via its chain
    float* col = s_matrix + (size_t)iso * SPECTRUM_CHANNELS;
    addNuclideTo(col, def, 1.0f);
  }

  // --- composite decay-chain columns ---
  //
  // Members of a natural chain are in secular equilibrium: they share one
  // activity, so their line intensities are fixed relative to each other.
  // Fitting six free amplitudes for the U-238 chain ignores that and lets
  // one member absorb another chain's counts - measured on the real ore,
  // where Tl-208's 583 keV template took 7.2 sigma out of Bi-214's 609 keV
  // line and produced a spurious thorium reading on pure uranium.
  //
  // A composite column carries every member at its correct branching, so it
  // can only fit the data if the WHOLE pattern is present. That is a far
  // stronger and more honest test of "is this uranium" than any threshold on
  // individual members, and it is what separates the two chains despite
  // their 609.3/351.9 and 583.2/338.3 ratios differing by only 0.4%.
  for (int c = 0; c < 2; c++) {
    DecayChain chain = (c == 0) ? CHAIN_U238 : CHAIN_TH232;
    float* col = s_matrix + (size_t)(s_chainFirst + c) * SPECTRUM_CHANNELS;
    for (uint16_t iso = 0; iso < lib; iso++) {
      const IsotopeDef& def = IsotopeLibrary::at(iso);
      if (def.chain != chain) continue;
      addNuclideTo(col, def, chainBranch(def.symbol));
    }
    snprintf(s_names[c], sizeof(s_names[0]),
             c == 0 ? "U238-chain" : "Th232-chain");
  }

  // --- smooth continuum basis ---
  // Broad overlapping bumps, logarithmically spaced so the low-energy region
  // (where the spectrum has the most structure) gets the most knots.
  float eLo = energyOf(0);
  float eHi = energyOf(SPECTRUM_CHANNELS - 1);
  if (eLo < 10.0f) eLo = 10.0f;
  if (eHi <= eLo) eHi = eLo * 100.0f;
  float logLo = logf(eLo), logHi = logf(eHi);

  for (int b = 0; b < CONTINUUM_BASIS; b++) {
    float* col = s_matrix + (size_t)(s_contFirst + b) * SPECTRUM_CHANNELS;
    float centre = expf(logLo + (logHi - logLo) * (float)b / (float)(CONTINUUM_BASIS - 1));
    // 40% of the centre energy: roughly 5x the detector FWHM, so these can
    // follow a continuum but never masquerade as a line.
    float width = CONTINUUM_WIDTH * centre;
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
      float e = energyOf(ch);
      if (e < 1.0f) e = 1.0f;
      float dz = (e - centre) / width;
      col[ch] = expf(-0.5f * dz * dz);
    }
    snprintf(s_names[CHAIN_COLUMNS + b], sizeof(s_names[0]), "c%.0f", centre);
  }

  // --- stored background reference, fitted rather than subtracted ---
  // Fitting its amplitude means a reference recorded at a different radon
  // level still helps, instead of over- or under-subtracting. It also keeps
  // its counting uncertainty inside the fit rather than inflating a variance
  // term after the fact.
  if (s_bgCol >= 0) {
    float* col = s_matrix + (size_t)s_bgCol * SPECTRUM_CHANNELS;
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) col[ch] = Background::rateAt(ch);
    snprintf(s_names[CHAIN_COLUMNS + CONTINUUM_BASIS], sizeof(s_names[0]), "stored-bkg");
  }

  // --- normalise every column to unit sum ---
  // Makes the fitted amplitudes directly comparable as "counts explained",
  // and keeps the solver well conditioned.
  for (int j = 0; j < cols; j++) {
    float* col = s_matrix + (size_t)j * SPECTRUM_CHANNELS;
    float sum = 0.0f;
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) sum += col[ch];
    if (sum <= 0.0f) continue;
    float inv = 1.0f / sum;
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) col[ch] *= inv;
  }

  s_valid = true;
  return true;
}

int count()           { return s_valid ? s_cols : 0; }
int chainColumn(DecayChain c) {
  if (!s_valid) return -1;
  if (c == CHAIN_U238)  return s_chainFirst;
  if (c == CHAIN_TH232) return s_chainFirst + 1;
  return -1;
}
int nuclideCount()    { return s_nuclides; }
int continuumFirst()  { return s_contFirst; }
int backgroundColumn(){ return s_bgCol; }

const float* column(int j) {
  if (!s_valid || j < 0 || j >= s_cols) return nullptr;
  return s_matrix + (size_t)j * SPECTRUM_CHANNELS;
}

const char* nameOf(int j) {
  if (!s_valid || j < 0 || j >= s_cols) return "?";
  if (j < s_nuclides) return IsotopeLibrary::at((uint16_t)j).symbol;
  return s_names[j - s_chainFirst];
}

} // namespace Templates
