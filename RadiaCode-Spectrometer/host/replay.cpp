// Offline replay harness.
//
// Runs the real analysis code (src/analysis/peak_search.cpp and the data
// layer it depends on) against spectra captured from the device, so a change
// can be judged in seconds against known samples instead of by pointing the
// detector at a rock and reading the screen.
//
// Input is the CSV that SdStorage::exportSpectrum() already writes, so a
// capture needs no conversion. The header carries duration and calibration:
//
//   # duration_s,3600
//   # calib_a0,3.839503
//   ...
//   channel,energy_keV,counts
//   0,3.84,127
//
// Usage:
//   replay <spectrum.csv> [...]                 analyse each file
//   replay --expect VERDICT <spectrum.csv>      also assert the verdict
//   replay --mix <background.csv> <source.csv> <bg_seconds>
//       Rebuilds "a long background run, then a source is presented": the
//       background scaled to bg_seconds, plus the source file's excess over
//       its own background. This is the case that currently needs a RESET.

#include "../src/analysis/peak_search.h"
#include "../src/analysis/templates.h"
#include "../src/analysis/fit.h"
#include "../src/analysis/identify.h"
#include "../src/data/spectrum_window.h"
#include "../src/data/spectrometer_data.h"
#include "../src/data/isotope_library.h"
#include "../src/data/background.h"

#include <Arduino.h>
#include <SD.h>
#include <math.h>
#include <string>
#include <random>

// Globals the shims declare.
HostSerial Serial;
SDClass SD;

// Host stub for the NVS-backed preferences the analyser touches.
namespace Settings {
  static float s_gain = 1.0f;
  float energyGain() { return s_gain; }
  void setEnergyGain(float g) { s_gain = g; }
}

// --- CSV loading ------------------------------------------------------------

struct Capture {
  std::string name;
  uint32_t durationSec = 0;
  uint32_t counts[SPECTRUM_CHANNELS] = {0};
  float a0 = 0.0f, a1 = 3.0f, a2 = 0.0f;
  float cps = 0.0f, dose = 0.0f;
  uint32_t total = 0;
};

static bool loadCsv(const char* path, Capture& out) {
  FILE* f = fopen(path, "rb");
  if (!f) { printf("  !! cannot open %s\n", path); return false; }
  out.name = path;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#') {
      float v = 0.0f;
      if      (sscanf(line, "# duration_s,%f", &v) == 1)  out.durationSec = (uint32_t)v;
      else if (sscanf(line, "# calib_a0,%f", &v) == 1)    out.a0 = v;
      else if (sscanf(line, "# calib_a1,%f", &v) == 1)    out.a1 = v;
      else if (sscanf(line, "# calib_a2,%f", &v) == 1)    out.a2 = v;
      else if (sscanf(line, "# cps,%f", &v) == 1)         out.cps = v;
      else if (sscanf(line, "# dose_uSv_h,%f", &v) == 1)  out.dose = v;
      continue;
    }
    int ch; float keV; unsigned long c;
    if (sscanf(line, "%d,%f,%lu", &ch, &keV, &c) == 3) {
      if (ch >= 0 && ch < SPECTRUM_CHANNELS) {
        out.counts[ch] = (uint32_t)c;
        out.total += (uint32_t)c;
      }
    }
  }
  fclose(f);

  // Counting statistics are meaningless without the acquisition time, and
  // guessing one would silently produce plausible-looking wrong sigmas.
  if (out.durationSec == 0) {
    printf("  !! %s has no duration_s header - refusing to guess\n", path);
    return false;
  }
  return true;
}

static void install(const Capture& c, SpectrometerData& d) {
  d.connection = CONN_CONNECTED;
  d.spectrumDurationSec = c.durationSec;
  d.calibA0 = c.a0; d.calibA1 = c.a1; d.calibA2 = c.a2;
  d.calibValid = true;
  d.cps = c.cps > 0.0f ? c.cps : (float)c.total / (float)c.durationSec;
  d.doseRateUSvH = c.dose;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) d.spectrum[i] = c.counts[i];
}

// --- reporting --------------------------------------------------------------

static void report(const SpectrometerData& d, const char* label) {
  printf("\n=== %s ===\n", label);
  printf("  %u s   %u counts   %.1f cps   %.3f uSv/h   calib a1=%.4f\n",
         (unsigned)d.spectrumDurationSec, (unsigned)d.quality.totalCounts,
         d.cps, d.doseRateUSvH, d.calibA1);

  printf("  peaks (%d):", lastPeakCount());
  const DetectedPeak* p = lastPeaks();
  for (int i = 0; i < lastPeakCount(); i++) {
    printf(" %.0f(%.1f)", p[i].energyKeV, p[i].significance);
  }
  printf("\n");

  printf("  candidates:\n");
  if (d.candidateCount == 0) printf("    (none)\n");
  for (uint8_t i = 0; i < d.candidateCount; i++) {
    const IsotopeCandidate& c = d.candidates[i];
    printf("    %-9s %3.0f%%  %u/%u lines  best %.1f keV  %.1f sigma\n",
           c.symbol, c.confidence * 100.0f, c.linesMatched, c.linesTotal,
           c.matchedEnergyKeV, c.significance);
  }

  printf("  U-238 chain %3.0f%% (%u/%u)   Th-232 chain %3.0f%% (%u/%u)\n",
         d.uranium.confidence * 100.0f, d.uranium.membersDetected, d.uranium.membersTotal,
         d.thorium.confidence * 100.0f, d.thorium.membersDetected, d.thorium.membersTotal);
  printf("  quality %s  sigma %.1f  peaks %u  recommended %us\n",
         qualityGradeLabel(d.quality.grade), d.quality.weakestKeySigma,
         d.quality.peaksFound, (unsigned)d.quality.recommendedSeconds);
  printf("  VERDICT: %s  (%.0f%%)\n",
         sourceClassLabel(d.sourceClass), d.verdictConfidence * 100.0f);
}

static int runOne(const char* path, const char* expect) {
  Capture c;
  if (!loadCsv(path, c)) return 1;

  resetVerdictHysteresis();
  install(c, g_data);
  analyzeSpectrum(g_data);
  report(g_data, path);

  if (expect) {
    const char* got = sourceClassLabel(g_data.sourceClass);
    if (strcmp(got, expect) != 0) {
      printf("  FAIL: expected %s, got %s\n", expect, got);
      return 1;
    }
    printf("  ok: %s\n", expect);
  }
  return 0;
}

// The regression this harness exists for: a source presented to an already
// long acquisition. Background is scaled up to bgSeconds, then the source
// capture's excess over its own background is added on top.
static int runMix(const char* bgPath, const char* srcPath, uint32_t bgSeconds) {
  Capture bg, src;
  if (!loadCsv(bgPath, bg) || !loadCsv(srcPath, src)) return 1;

  float bgScale = (float)bgSeconds / (float)bg.durationSec;
  // The source capture already contains its own background; add only its
  // excess, so the result reads as "long background, then this appears".
  float srcBgEq = (float)src.durationSec / (float)bg.durationSec;

  Capture mixed;
  mixed.durationSec = bgSeconds + src.durationSec;
  mixed.a0 = src.a0; mixed.a1 = src.a1; mixed.a2 = src.a2;
  mixed.dose = src.dose;

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    float base = (float)bg.counts[i] * bgScale;
    float excess = (float)src.counts[i] - (float)bg.counts[i] * srcBgEq;
    if (excess < 0.0f) excess = 0.0f;
    float v = base + excess;
    mixed.counts[i] = (uint32_t)(v < 0.0f ? 0.0f : v);
    mixed.total += mixed.counts[i];
  }

  char label[300];
  snprintf(label, sizeof(label), "MIX: %us of %s, then %s presented",
           (unsigned)bgSeconds, bgPath, srcPath);

  resetVerdictHysteresis();
  install(mixed, g_data);
  analyzeSpectrum(g_data);
  report(g_data, label);
  return 0;
}

// --- template fit -----------------------------------------------------------

static void reportFit(const SpectrometerData& d, const char* label) {
  if (!Templates::build(d)) { printf("  !! template build failed\n"); return; }
  if (!Fit::run(d))         { printf("  !! fit failed\n"); return; }

  printf("\n=== FIT %s ===\n", label);
  printf("  %u s  %.1f cps   residual %.1f%%  reduced chi2 %.2f\n",
         (unsigned)d.spectrumDurationSec, d.cps,
         Fit::residualFraction() * 100.0f, Fit::reducedChiSquare());

  // Rank nuclide columns by significance; the continuum columns are shown
  // separately because they are bookkeeping, not findings.
  int n = Templates::nuclideCount();
  int order[64];
  int m = 0;
  for (int j = 0; j < n && m < 64; j++) order[m++] = j;
  for (int a = 0; a < m; a++) {
    for (int b = a + 1; b < m; b++) {
      if (Fit::at(order[b]).significance > Fit::at(order[a]).significance) {
        int t = order[a]; order[a] = order[b]; order[b] = t;
      }
    }
  }

  printf("  nuclides (by significance):\n");
  double total = 0.0;
  for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) total += d.spectrum[ch];
  for (int i = 0; i < m && i < 10; i++) {
    const FitResult& r = Fit::at(order[i]);
    if (r.significance < 1.0f) break;
    printf("    %-9s %10.0f +- %-8.0f %6.1f sigma   %5.1f%% of counts\n",
           Templates::nameOf(order[i]), r.amplitude, r.sigma, r.significance,
           total > 0.0 ? r.amplitude / total * 100.0f : 0.0f);
  }

  for (int c = 0; c < 2; c++) {
    int j = Templates::chainColumn(c == 0 ? CHAIN_U238 : CHAIN_TH232);
    if (j < 0) continue;
    const FitResult& r = Fit::at(j);
    printf("  CHAIN %-12s %10.0f +- %-8.0f %6.1f sigma   %5.1f%% of counts\n",
           Templates::nameOf(j), r.amplitude, r.sigma, r.significance,
           total > 0.0 ? r.amplitude / total * 100.0f : 0.0f);
  }

  printf("  continuum:");
  for (int j = Templates::continuumFirst(); j < Templates::count(); j++) {
    printf("  %s=%.0f", Templates::nameOf(j), Fit::at(j).amplitude);
  }
  printf("\n");
}

// --- time-to-verdict sweep --------------------------------------------------
//
// The verdict at full acquisition is only half the question; the other half is
// how long the user has to wait for it. A real capture can be thinned to a
// shorter effective run by keeping each count with probability t/T - binomial
// thinning of a Poisson process is exactly another Poisson process, so the
// result has the correct statistics for a genuine shorter measurement rather
// than merely scaled-down counts.
static void thin(const Capture& src, uint32_t seconds, Capture& out,
                 std::mt19937& rng) {
  double keep = (double)seconds / (double)src.durationSec;
  if (keep > 1.0) keep = 1.0;

  out.durationSec = seconds;
  out.a0 = src.a0; out.a1 = src.a1; out.a2 = src.a2;
  out.dose = src.dose;
  out.total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    std::binomial_distribution<uint32_t> b(src.counts[i], keep);
    out.counts[i] = b(rng);
    out.total += out.counts[i];
  }
  out.cps = (float)out.total / (float)seconds;
}

static int runSweep(const char* path, const char* expect) {
  Capture c;
  if (!loadCsv(path, c)) return 1;

  static const uint32_t STEPS[] = {15, 30, 60, 120, 300, 600, 900, 1200, 1800, 3600};
  std::mt19937 rng(20260901u);

  printf("\n=== SWEEP %s (expect %s) ===\n", path, expect ? expect : "-");
  printf("  %6s  %8s  %-14s %-5s  %s\n", "time", "counts", "verdict", "conf", "top candidate");

  uint32_t firstCorrect = 0;
  for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
    if (STEPS[i] > c.durationSec) break;

    Capture t;
    thin(c, STEPS[i], t, rng);

    // Each point is an independent measurement, so the hysteresis must not
    // carry a verdict from the previous (longer-armed) step into it.
    resetVerdictHysteresis();
    install(t, g_data);
    analyzeSpectrum(g_data);

    const char* got = sourceClassLabel(g_data.sourceClass);
    char top[64] = "-";
    if (g_data.candidateCount > 0) {
      snprintf(top, sizeof(top), "%s %.0f%% (%.0f sigma)",
               g_data.candidates[0].symbol,
               g_data.candidates[0].confidence * 100.0f,
               g_data.candidates[0].significance);
    }
    bool ok = expect && strcmp(got, expect) == 0;
    if (ok && firstCorrect == 0) firstCorrect = STEPS[i];
    if (!ok) firstCorrect = 0;  // must stay correct from here on

    printf("  %5us  %8u  %-14s %4.0f%%  %-28s %s\n",
           (unsigned)STEPS[i], (unsigned)t.total, got,
           g_data.verdictConfidence * 100.0f, top, ok ? "ok" : "");
  }

  if (expect) {
    if (firstCorrect) printf("  -> settles on %s from %us\n", expect, (unsigned)firstCorrect);
    else              printf("  -> never settles on %s\n", expect);
  }
  return 0;
}

// --- presentation test ------------------------------------------------------
//
// Simulates the real sequence the device sees: background accumulating poll
// by poll for a while, then a sample is placed on the detector and the run
// continues. This is the case that forces a RESET today, and a single blended
// spectrum cannot test it - the window logic only works on a time series.
static int runPresent(const char* bgPath, const char* srcPath,
                      uint32_t preSeconds, const char* expect) {
  Capture bg, src;
  if (!loadCsv(bgPath, bg) || !loadCsv(srcPath, src)) return 1;

  std::mt19937 rng(4242u);
  const uint32_t POLL = 5;              // the device polls the spectrum every 5 s

  // Per-second rates for background alone and for the sample (which already
  // includes background, as a real capture does).
  double bgRate[SPECTRUM_CHANNELS], srcRate[SPECTRUM_CHANNELS];
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    bgRate[i] = (double)bg.counts[i] / bg.durationSec;
    srcRate[i] = (double)src.counts[i] / src.durationSec;
  }

  SpectrumWindow::begin();
  Identify::reset();
  resetVerdictHysteresis();

  Capture acc;
  acc.a0 = src.a0; acc.a1 = src.a1; acc.a2 = src.a2;
  acc.durationSec = 0;

  printf("\n=== PRESENT %s after %us of %s (expect %s) ===\n",
         srcPath, (unsigned)preSeconds, bgPath, expect ? expect : "-");
  printf("  %8s  %-9s %8s  %-14s %s\n", "elapsed", "phase", "window", "verdict", "top");

  uint32_t firstCorrect = 0;
  uint32_t totalSeconds = preSeconds + 1800;
  static SpectrometerData windowed;

  for (uint32_t t = POLL; t <= totalSeconds; t += POLL) {
    bool present = (t > preSeconds);
    const double* rate = present ? srcRate : bgRate;
    for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
      std::poisson_distribution<uint32_t> p(rate[i] * POLL);
      acc.counts[i] += p(rng);
    }
    acc.durationSec = t;

    install(acc, g_data);
    g_data.doseRateUSvH = present ? src.dose : bg.dose;
    SpectrumWindow::update(g_data);
    SpectrumWindow::view(g_data, windowed);
    analyzeSpectrum(windowed);

    // Report only at intervals that matter, and only once the sample is on.
    bool interesting = present &&
        ((t - preSeconds) == 30 || (t - preSeconds) == 60 ||
         (t - preSeconds) == 120 || (t - preSeconds) == 300 ||
         (t - preSeconds) == 600 || (t - preSeconds) == 1200 ||
         (t - preSeconds) == 1800);
    if (!interesting) continue;

    const char* got = sourceClassLabel(windowed.sourceClass);
    bool ok = expect && strcmp(got, expect) == 0;
    if (ok && firstCorrect == 0) firstCorrect = t - preSeconds;
    if (!ok) firstCorrect = 0;

    char top[48] = "-";
    if (windowed.candidateCount > 0) {
      snprintf(top, sizeof(top), "%s (%.0f sigma)", windowed.candidates[0].symbol,
               windowed.candidates[0].significance);
    }
    printf("  %6us  %-9s %7us  %-14s %-22s %s\n",
           (unsigned)(t - preSeconds), "sample on",
           (unsigned)windowed.spectrumDurationSec, got, top, ok ? "ok" : "");
  }

  if (expect) {
    if (firstCorrect) printf("  -> identified %us after presentation\n", (unsigned)firstCorrect);
    else              printf("  -> NEVER identified\n");
    return firstCorrect ? 0 : 1;
  }
  return 0;
}

int main(int argc, char** argv) {
  IsotopeLibrary::begin();
  Background::begin();

  printf("library: %u nuclides / %u lines\n",
         IsotopeLibrary::count(), IsotopeLibrary::lineCount());

  if (argc < 2) {
    printf("usage: replay <spectrum.csv> [...]\n"
           "       replay --expect VERDICT <spectrum.csv>\n"
           "       replay --mix <background.csv> <source.csv> <bg_seconds>\n");
    return 2;
  }

  int failures = 0;
  const char* expect = nullptr;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--expect") == 0 && i + 1 < argc) {
      expect = argv[++i];
      continue;
    }
    // Install a capture as the active background reference, exactly as the
    // device does when a reference is loaded from its library.
    if (strcmp(argv[i], "--bkg") == 0 && i + 1 < argc) {
      Capture b;
      if (loadCsv(argv[++i], b)) {
        static float rates[SPECTRUM_CHANNELS];
        for (int k = 0; k < SPECTRUM_CHANNELS; k++) {
          rates[k] = (float)b.counts[k] / (float)b.durationSec;
        }
        bool ok = Background::setFromRates(rates, b.durationSec, b.total);
        Background::setEnabled(true);
        Templates::invalidate();
        printf("background reference: %s (%us, %.1f cps) %s\n", argv[i],
               (unsigned)b.durationSec, (float)b.total / (float)b.durationSec,
               ok ? "installed" : "REJECTED");
      } else failures++;
      continue;
    }
    if (strcmp(argv[i], "--fit") == 0 && i + 1 < argc) {
      Capture c;
      if (loadCsv(argv[++i], c)) { install(c, g_data); reportFit(g_data, argv[i]); }
      else failures++;
      continue;
    }
    if (strcmp(argv[i], "--present") == 0 && i + 3 < argc) {
      failures += runPresent(argv[i + 1], argv[i + 2],
                             (uint32_t)atol(argv[i + 3]), expect);
      i += 3;
      expect = nullptr;
      continue;
    }
    if (strcmp(argv[i], "--sweep") == 0 && i + 1 < argc) {
      failures += runSweep(argv[++i], expect);
      expect = nullptr;
      continue;
    }
    if (strcmp(argv[i], "--mix") == 0 && i + 3 < argc) {
      failures += runMix(argv[i + 1], argv[i + 2], (uint32_t)atol(argv[i + 3]));
      i += 3;
      continue;
    }
    failures += runOne(argv[i], expect);
    expect = nullptr;
  }

  printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
  return failures ? 1 : 0;
}
