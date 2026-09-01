#include "spectrum_window.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifndef SPECTRUM_WINDOW_DEBUG
#define SPECTRUM_WINDOW_DEBUG 0
#endif

namespace SpectrumWindow {

// Baseline: the accumulation as it stood when the current evidence window
// opened. Analysis sees (now - baseline).
// 4 KB each. Allocated in PSRAM rather than kept as static RAM: this board
// must hold the Bluedroid stack and, during an update check, a TLS session,
// and heap exhaustion there has already caused BLE to fail outright once.
static uint32_t* s_base = nullptr;
static uint32_t s_baseDuration = 0;
static uint32_t s_baseTotal = 0;
static bool     s_haveBase = false;
static bool     s_cumulative = false;

// One poll back, so a rate step can be attributed to the interval before it
// rather than the interval that contains it. Without this the rising edge
// itself lands inside the new window and dilutes it.
static uint32_t* s_prev = nullptr;
static uint32_t s_prevDuration = 0;
static uint32_t s_prevTotal = 0;
static bool     s_havePrev = false;

// Running estimate of the count rate in the current window, and how much
// counting it is based on.
static double s_windowCounts = 0.0;
static double s_windowSeconds = 0.0;
// Consecutive intervals that crossed the step threshold, signed by direction.
static int    s_pendingStep = 0;

// A step must exceed the Poisson noise of the established rate by this much
// before the window rolls. Counting noise on a rate measured over T seconds
// at R cps is sqrt(R/T), so this is a real statistical test rather than a
// fixed percentage - which would either miss a weak source or retrigger
// constantly on a strong one.
// 4 sigma rather than 6: at ~20 cps over a 5 s poll, 6 sigma demanded roughly
// a 60% rate jump, so a weak or shielded sample - the case this device is for
// - never triggered a window and stayed diluted in the accumulation.
#define STEP_SIGMA 4.0f

// Below this the rate estimate is too green to judge a step against.
#define MIN_ESTABLISH_SECONDS 20.0

// A window must be allowed to grow before anything may reset it. Every reset
// throws away the counting done so far, and a weak sample needs minutes: if
// noise can rebase every ~25 s the window never accumulates enough to identify
// anything and the device reports MEASURING or BACKGROUND forever.
#define MIN_WINDOW_SECONDS 90.0

// And the step must still be there on the next poll. A single interval
// crossing the threshold is a fluctuation as often as it is a sample being
// presented - especially now that a drop counts too, which doubles the
// opportunities to be fooled.
#define STEP_CONFIRMATIONS 2

// Returns false if the buffers could not be obtained, in which case the
// window degrades to cumulative mode rather than failing the measurement.
static bool ensureBuffers() {
  if (s_base && s_prev) return true;
  size_t bytes = SPECTRUM_CHANNELS * sizeof(uint32_t);
#ifdef ARDUINO
  if (!s_base) s_base = (uint32_t*)ps_malloc(bytes);
  if (!s_base) s_base = (uint32_t*)malloc(bytes);
  if (!s_prev) s_prev = (uint32_t*)ps_malloc(bytes);
  if (!s_prev) s_prev = (uint32_t*)malloc(bytes);
#else
  if (!s_base) s_base = (uint32_t*)malloc(bytes);
  if (!s_prev) s_prev = (uint32_t*)malloc(bytes);
#endif
  return s_base && s_prev;
}

void begin() {
  ensureBuffers();
  s_haveBase = false;
  s_havePrev = false;
  s_cumulative = false;
  s_windowCounts = 0.0;
  s_windowSeconds = 0.0;
  s_pendingStep = 0;
}

void setCumulative(bool on) { s_cumulative = on; }
bool cumulative() { return s_cumulative; }

static void snapshot(const SpectrometerData& full, uint32_t* dst,
                     uint32_t& dstDuration, uint32_t& dstTotal) {
  uint32_t total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    dst[i] = full.spectrum[i];
    total += full.spectrum[i];
  }
  dstDuration = full.spectrumDurationSec;
  dstTotal = total;
}

void rebase(const SpectrometerData& full) {
  if (!ensureBuffers()) { s_haveBase = false; return; }
  snapshot(full, s_base, s_baseDuration, s_baseTotal);
  s_haveBase = true;
  s_havePrev = false;
  s_windowCounts = 0.0;
  s_windowSeconds = 0.0;
}

void update(const SpectrometerData& full) {
  if (s_cumulative || !ensureBuffers()) { s_haveBase = false; return; }

  uint32_t total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) total += full.spectrum[i];

  if (!s_haveBase) {
    rebase(full);
    snapshot(full, s_prev, s_prevDuration, s_prevTotal);
    s_havePrev = true;
    return;
  }

  // The accumulation went backwards: the detector was reset, or a different
  // device connected. Nothing before this point is comparable.
  if (total < s_baseTotal || full.spectrumDurationSec < s_baseDuration) {
    rebase(full);
    snapshot(full, s_prev, s_prevDuration, s_prevTotal);
    s_havePrev = true;
    return;
  }

  if (!s_havePrev) {
    snapshot(full, s_prev, s_prevDuration, s_prevTotal);
    s_havePrev = true;
    return;
  }

  double dt = (double)full.spectrumDurationSec - (double)s_prevDuration;
  double dc = (double)total - (double)s_prevTotal;
  if (dt <= 0.0) return;

  double instantRate = dc / dt;

  // Compare against the rate established over the window so far.
  if (s_windowSeconds >= MIN_ESTABLISH_SECONDS && s_windowCounts > 0.0) {
    double meanRate = s_windowCounts / s_windowSeconds;
    // Uncertainty of the difference between the new interval's rate and the
    // established mean, both Poisson.
    double var = meanRate / dt + meanRate / s_windowSeconds;
    double sd = (var > 0.0) ? sqrt(var) : 0.0;
    // Either direction. A drop matters as much as a rise: when the sample is
    // taken away, its counts stay banked in the open window and the device
    // goes on reporting an isotope that is no longer in front of it.
    double step = instantRate - meanRate;
    bool crossed = (sd > 0.0) && (fabs(step) > STEP_SIGMA * sd);

    // Require consecutive confirmation, and never reset a window that has not
    // had a fair chance to accumulate.
    if (crossed && (step > 0) == (s_pendingStep > 0)) s_pendingStep += (step > 0) ? 1 : -1;
    else if (crossed)                                 s_pendingStep = (step > 0) ? 1 : -1;
    else                                              s_pendingStep = 0;

    bool confirmed = (s_pendingStep >= STEP_CONFIRMATIONS) ||
                     (s_pendingStep <= -STEP_CONFIRMATIONS);
    if (confirmed && s_windowSeconds >= MIN_WINDOW_SECONDS) {
      s_pendingStep = 0;
      // Something has been presented to the detector. Open a new window at
      // the previous poll, so the window contains the source and not the
      // background that preceded it.
      for (int i = 0; i < SPECTRUM_CHANNELS; i++) s_base[i] = s_prev[i];
      s_baseDuration = s_prevDuration;
      s_baseTotal = s_prevTotal;
      s_windowCounts = 0.0;
      s_windowSeconds = 0.0;
#if SPECTRUM_WINDOW_DEBUG
      Serial.printf("[WIN] step %.1f -> %.1f cps, new window at %us\n",
                    meanRate, instantRate, (unsigned)s_baseDuration);
#endif
    }
  }

  s_windowCounts += dc;
  s_windowSeconds += dt;

  snapshot(full, s_prev, s_prevDuration, s_prevTotal);
}

void view(const SpectrometerData& full, SpectrometerData& out) {
  // Copy the live scalars; the spectrum is replaced below.
  out.connection = full.connection;
  out.calibA0 = full.calibA0;
  out.calibA1 = full.calibA1;
  out.calibA2 = full.calibA2;
  out.calibValid = full.calibValid;
  out.doseRateUSvH = full.doseRateUSvH;
  out.cps = full.cps;

  if (s_cumulative || !s_haveBase || s_base == nullptr ||
      full.spectrumDurationSec <= s_baseDuration) {
    for (int i = 0; i < SPECTRUM_CHANNELS; i++) out.spectrum[i] = full.spectrum[i];
    out.spectrumDurationSec = full.spectrumDurationSec;
    return;
  }

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    uint32_t now = full.spectrum[i];
    uint32_t was = s_base[i];
    out.spectrum[i] = (now > was) ? (now - was) : 0;
  }
  out.spectrumDurationSec = full.spectrumDurationSec - s_baseDuration;
}

uint32_t windowSeconds() { return s_baseDuration; }
bool windowed() { return s_haveBase && !s_cumulative; }

} // namespace SpectrumWindow
