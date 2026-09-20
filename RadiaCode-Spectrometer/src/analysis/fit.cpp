#include "fit.h"
#include "../data/isotope_library.h"   // ISOTOPE_LIBRARY_MAX
#include <math.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace Fit {

// Sized by the template ceiling rather than the live count, so a bigger
// library cannot overrun these. File-scope, not stack: this runs on the BLE
// task, whose stack is 12 KB.
#define FIT_MAX_COLS (ISOTOPE_LIBRARY_MAX + CONTINUUM_BASIS + 1)

static FitResult s_results[FIT_MAX_COLS];
static float     s_x[FIT_MAX_COLS];
static float     s_colSum[FIT_MAX_COLS];
static int       s_cols = 0;

static float s_model[SPECTRUM_CHANNELS];
static float s_residualFraction = 0.0f;
static float s_chi2 = 0.0f;

// Iterations of the ML-EM update. The likelihood climbs quickly at first and
// then crawls; past ~60 the amplitudes move by less than their own error
// bars, so more iterations buy nothing but time on a 240 MHz core.
#define EM_ITERATIONS 60

// Floor on the model in a channel, so a channel the model currently explains
// as zero cannot produce a division by zero or an infinite update step.
#define MODEL_FLOOR 1e-6f

// Channels below this energy are excluded from the likelihood.
// Set to 45 keV to capture critical low-energy lines (Am-241 59.5 keV,
// Co-57 122/136 keV, Tc-99m 140.5 keV, Th-234 63/93 keV) while cutting off
// sub-45 keV electronic noise and severe detector attenuation.
#define FIT_MIN_KEV 45.0f

static int s_firstChannel = 0;

static void computeFitRange(const SpectrometerData& d) {
  s_firstChannel = 0;
  for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
    float e = d.calibA0 + d.calibA1 * ch + d.calibA2 * ch * ch;
    if (e >= FIT_MIN_KEV) { s_firstChannel = ch; return; }
  }
  s_firstChannel = 0;
}

// --- marginal uncertainties -------------------------------------------------
//
// Inverting the Fisher matrix costs O(n^3), so it is done over an "active
// set" of columns that actually carry amplitude rather than all of them. A
// 300-nuclide library would otherwise mean a 317x317 inversion on every
// analysis pass; in practice only a few dozen columns are ever non-trivial.
#define ACTIVE_MAX 64
// Columns holding less than this fraction of the total are pinned to zero
// rather than fitted, so they cannot inflate the matrix or its condition.
#define ACTIVE_MIN_FRACTION 1e-5f

static int   s_active[ACTIVE_MAX];
static int   s_nActive = 0;
static float s_sigma[FIT_MAX_COLS];

// Fisher matrix and its Cholesky factor, over the active set only. Two
// ACTIVE_MAX^2 double matrices are 64 KB, which is far too much static RAM on
// a board that must also hold the Bluedroid and WiFi stacks - allocate them
// in PSRAM on demand instead, as the template matrix already does.
static double* s_fisher = nullptr;
static double* s_chol = nullptr;

static bool ensureMatrices() {
  if (s_fisher && s_chol) return true;
  size_t bytes = (size_t)ACTIVE_MAX * ACTIVE_MAX * sizeof(double);
#ifdef ARDUINO
  if (!s_fisher) s_fisher = (double*)ps_malloc(bytes);
  if (!s_fisher) s_fisher = (double*)malloc(bytes);
  if (!s_chol)   s_chol   = (double*)ps_malloc(bytes);
  if (!s_chol)   s_chol   = (double*)malloc(bytes);
#else
  if (!s_fisher) s_fisher = (double*)malloc(bytes);
  if (!s_chol)   s_chol   = (double*)malloc(bytes);
#endif
  return s_fisher && s_chol;
}

static void buildActiveSet(int cols) {
  double total = 0.0;
  for (int j = 0; j < cols; j++) total += s_x[j];
  float floorAmp = (float)(total * ACTIVE_MIN_FRACTION);

  s_nActive = 0;
  for (int j = 0; j < cols; j++) {
    if (s_x[j] <= floorAmp) continue;
    if (s_nActive < ACTIVE_MAX) {
      s_active[s_nActive++] = j;
      continue;
    }
    // Full: displace the weakest member if this column is stronger.
    //
    // Taking simply the first ACTIVE_MAX columns above the floor - which is
    // what this did - selects by library index, so with an SD library of up
    // to ISOTOPE_LIBRARY_MAX (300) nuclides any nuclide past column 64 got no
    // covariance entry, kept sigma 0, and could never be identified no matter
    // how much of the spectrum it explained. Selection must be by amplitude.
    int weakest = 0;
    for (int k = 1; k < s_nActive; k++) {
      if (s_x[s_active[k]] < s_x[s_active[weakest]]) weakest = k;
    }
    if (s_x[j] > s_x[s_active[weakest]]) s_active[weakest] = j;
  }
}

// Cholesky inverse of the (symmetric, positive-definite) Fisher matrix.
// Only the diagonal of the inverse is needed, but Cholesky gives it cheaply
// and is far more numerically stable here than a general inversion: the
// matrix is close to singular whenever two templates are nearly degenerate,
// which is exactly the case this exists to handle.
static void computeCovariance() {
  int n = s_nActive;
  for (int j = 0; j < FIT_MAX_COLS; j++) s_sigma[j] = 0.0f;
  if (n <= 0 || !ensureMatrices()) return;

  for (int a = 0; a < n; a++) {
    const float* ca = Templates::column(s_active[a]);
    for (int b = a; b < n; b++) {
      const float* cb = Templates::column(s_active[b]);
      double acc = 0.0;
      for (int ch = s_firstChannel; ch < SPECTRUM_CHANNELS; ch++) {
        float m = s_model[ch];
        if (m < MODEL_FLOOR) m = MODEL_FLOOR;
        if (ca[ch] <= 0.0f || cb[ch] <= 0.0f) continue;
        acc += (double)ca[ch] * (double)cb[ch] / m;
      }
      s_fisher[a * n + b] = acc;
      s_fisher[b * n + a] = acc;
    }
    // Ridge term. Two templates can be genuinely indistinguishable in this
    // data (U-235 at 185.7 keV against Ra-226 at 186.2 keV, half a keV apart
    // under a 33 keV FWHM). Without it the inversion blows up and reports an
    // absurd significance for whichever of the pair rounding happens to
    // favour; with it, both correctly come out uncertain.
    s_fisher[a * n + a] *= 1.0 + 1e-6;
    s_fisher[a * n + a] += 1e-12;
  }

  // Cholesky: F = L L^T
  double* L = s_chol;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= i; j++) {
      double sum = s_fisher[i * n + j];
      for (int k = 0; k < j; k++) sum -= L[i * n + k] * L[j * n + k];
      if (i == j) {
        if (sum <= 0.0) {
#ifdef ARDUINO
          Serial.printf("[FIT] Cholesky failed at col %d (sum=%.3e)\n", i, sum);
#endif
          return;          // not positive definite; give up safely
        }
        L[i * n + i] = sqrt(sum);
      } else {
        L[i * n + j] = sum / L[j * n + j];
      }
    }
    for (int j = i + 1; j < n; j++) L[i * n + j] = 0.0;
  }

  // (F^-1)_jj by solving F z = e_j for each j and taking z_j.
  static double y[ACTIVE_MAX], z[ACTIVE_MAX];   // 512 B each, fine as statics
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) y[i] = (i == j) ? 1.0 : 0.0;
    for (int i = 0; i < n; i++) {              // forward substitution
      double sum = y[i];
      for (int k = 0; k < i; k++) sum -= L[i * n + k] * y[k];
      y[i] = sum / L[i * n + i];
    }
    for (int i = n - 1; i >= 0; i--) {         // back substitution
      double sum = y[i];
      for (int k = i + 1; k < n; k++) sum -= L[k * n + i] * z[k];
      z[i] = sum / L[i * n + i];
    }
    double var = z[j];
    s_sigma[s_active[j]] = (var > 0.0) ? (float)sqrt(var) : 0.0f;
  }
}

int count() { return s_cols; }
const FitResult& at(int j) {
  static const FitResult empty = {0.0f, 0.0f, 0.0f};
  if (j < 0 || j >= s_cols) return empty;
  return s_results[j];
}
float residualFraction() { return s_residualFraction; }
float reducedChiSquare() { return s_chi2; }
const float* model() { return s_model; }
float minEnergyKeV() { return FIT_MIN_KEV; }

bool run(const SpectrometerData& d) {
  int cols = Templates::count();
  if (cols <= 0 || cols > FIT_MAX_COLS) return false;
  s_cols = cols;

  // Total observed counts, and the per-column sums used by the EM update.
  double totalObs = 0.0;
  computeFitRange(d);
  for (int ch = s_firstChannel; ch < SPECTRUM_CHANNELS; ch++) totalObs += d.spectrum[ch];
  if (totalObs <= 0.0) return false;

  for (int j = 0; j < cols; j++) {
    const float* col = Templates::column(j);
    double s = 0.0;
    for (int ch = s_firstChannel; ch < SPECTRUM_CHANNELS; ch++) s += col[ch];
    s_colSum[j] = (float)s;
    // Start every column with an equal share of the observed counts. A
    // uniform start matters: seeding from a peak search would import exactly
    // the bias the fit exists to avoid.
    s_x[j] = (float)(totalObs / cols);
  }

  // --- ML-EM iterations -----------------------------------------------------
  // x_j <- x_j / sum_c A_cj  *  sum_c A_cj y_c / (Ax)_c
  //
  // Non-negative by construction (all terms positive), so no projection or
  // active-set bookkeeping is needed - which is why this is preferred here
  // over a general NNLS solver.
  for (int it = 0; it < EM_ITERATIONS; it++) {
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) s_model[ch] = 0.0f;
    for (int j = 0; j < cols; j++) {
      if (s_x[j] <= 0.0f) continue;
      const float* col = Templates::column(j);
      float xj = s_x[j];
      for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) s_model[ch] += col[ch] * xj;
    }

    for (int j = 0; j < cols; j++) {
      if (s_colSum[j] <= 0.0f) { s_x[j] = 0.0f; continue; }
      const float* col = Templates::column(j);
      double acc = 0.0;
      for (int ch = s_firstChannel; ch < SPECTRUM_CHANNELS; ch++) {
        if (col[ch] <= 0.0f) continue;
        float m = s_model[ch];
        if (m < MODEL_FLOOR) m = MODEL_FLOOR;
        acc += (double)col[ch] * (double)d.spectrum[ch] / m;
      }
      s_x[j] = (float)(s_x[j] * acc / s_colSum[j]);
    }
  }

  // Final model after the last update.
  for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) s_model[ch] = 0.0f;
  for (int j = 0; j < cols; j++) {
    const float* col = Templates::column(j);
    float xj = s_x[j];
    if (xj <= 0.0f) continue;
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) s_model[ch] += col[ch] * xj;
  }

  // --- uncertainties --------------------------------------------------------
  // Fisher information for Poisson data:  I_jk = sum_c A_cj A_ck / model_c
  // and sigma_j = sqrt( (I^-1)_jj ).
  //
  // The FULL matrix is required, not just its diagonal. Templates in this
  // library overlap heavily - at 8.4% FWHM many nuclides share lines within
  // one resolution width - and the diagonal alone pretends each amplitude is
  // measured independently. Measured on real background, that produced
  // Co-60 at 12.8 sigma and Mn-54 at 5.6 sigma from a spectrum containing
  // neither: the counts were genuinely there, but they were equally well
  // explained by neighbours, and only the off-diagonal terms say so.
  //
  // Inverting the marginal covariance is what makes "3 sigma" mean "this
  // nuclide specifically, and not something else that would fit as well".
  buildActiveSet(cols);
  computeCovariance();

  for (int j = 0; j < cols; j++) {
    s_results[j].amplitude = s_x[j];
    s_results[j].sigma = s_sigma[j];
    s_results[j].significance = (s_sigma[j] > 0.0f) ? (s_x[j] / s_sigma[j]) : 0.0f;
  }

  // --- goodness of fit ------------------------------------------------------
  double resid = 0.0, chi2 = 0.0;
  int dof = 0;
  for (int ch = s_firstChannel; ch < SPECTRUM_CHANNELS; ch++) {
    double y = d.spectrum[ch];
    double m = s_model[ch];
    resid += fabs(y - m);
    double var = (y > 1.0) ? y : 1.0;
    chi2 += (y - m) * (y - m) / var;
    if (y > 0.0) dof++;
  }
  s_residualFraction = (float)(resid / totalObs);
  s_chi2 = (dof > cols) ? (float)(chi2 / (dof - cols)) : 0.0f;

  return true;
}

} // namespace Fit
