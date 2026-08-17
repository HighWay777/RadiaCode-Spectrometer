#include "mock_source.h"
#include "spectrometer_data.h"
#include "../analysis/peak_search.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

// g_data is defined once in spectrometer_data.cpp.

namespace MockSource {

static unsigned long connectStartMs = 0;
static unsigned long lastSecondTickMs = 0;
static unsigned long lastSpectrumTickMs = 0;
static unsigned long connectedSinceMs = 0;

// Two synthetic sources: a Cs-137-like peak that ramps in over time, and a
// steady, weaker K-40 natural-background peak. Just enough to make the
// isotope-match and spectrum screens show something meaningful while the
// real BLE spectrum feed isn't wired up yet.
static float sourceStrength = 0.0f; // 0..1, ramps toward 1 while "connected"

static float resolutionSigmaChannels(float energyKeV, float a1) {
  float e = energyKeV < 20.0f ? 20.0f : energyKeV;
  float fwhmPct = 8.4f * sqrtf(662.0f / e);
  float fwhmKeV = (fwhmPct / 100.0f) * e;
  float sigmaKeV = fwhmKeV / 2.3548f;
  return sigmaKeV / a1;
}

static void resetSpectrum() {
  memset(g_data.spectrum, 0, sizeof(g_data.spectrum));
  g_data.candidateCount = 0;
  sourceStrength = 0.0f;
}

void requestConnect() {
  if (g_data.connection != CONN_DISCONNECTED) return;
  g_data.connection = CONN_CONNECTING;
  connectStartMs = millis();
}

void requestDisconnect() {
  g_data.connection = CONN_DISCONNECTED;
  g_data.batteryPercent = -1;
  g_data.liveTimeSeconds = 0;
  g_data.doseRateUSvH = 0;
  g_data.cpm = 0;
  resetSpectrum();
}

static void accumulateChannel(int ch, float meanCounts) {
  if (ch < 0 || ch >= SPECTRUM_CHANNELS) return;
  if (meanCounts <= 0.0f) return;
  long added = (long)random(0, (long)(2.0f * meanCounts) + 1);
  g_data.spectrum[ch] += (uint32_t)added;
}

static void addGaussianPeak(float centerKeV, float amplitude) {
  if (amplitude <= 0.0f) return;
  int centerCh = (int)((centerKeV - g_data.calibA0) / g_data.calibA1);
  float sigmaCh = resolutionSigmaChannels(centerKeV, g_data.calibA1);
  int span = (int)(sigmaCh * 3.0f) + 1;
  for (int ch = centerCh - span; ch <= centerCh + span; ch++) {
    float dx = (ch - centerCh) / sigmaCh;
    float meanCounts = amplitude * expf(-0.5f * dx * dx);
    accumulateChannel(ch, meanCounts);
  }
}

static void tickSpectrum() {
  // Compton-like continuum: falls off with channel/energy, small flat floor.
  for (int ch = 0; ch < SPECTRUM_CHANNELS; ch += 2) { // every other channel, cheap
    float meanCounts = 3.0f * expf(-ch / 260.0f) + 0.15f;
    accumulateChannel(ch, meanCounts);
    if (ch + 1 < SPECTRUM_CHANNELS) accumulateChannel(ch + 1, meanCounts);
  }

  // Cs-137 ramps in (simulates "a source is nearby"), K-40 is a steady weak
  // natural-background line always present at low level.
  if (sourceStrength < 1.0f) sourceStrength += 0.02f;
  addGaussianPeak(661.7f, 26.0f * sourceStrength);
  addGaussianPeak(1460.8f, 6.0f);

  analyzeSpectrum(g_data);
}

void tick() {
  unsigned long now = millis();

  if (g_data.connection == CONN_CONNECTING) {
    if (now - connectStartMs > 1200) {
      g_data.connection = CONN_CONNECTED;
      g_data.batteryPercent = 87;
      connectedSinceMs = now;
      resetSpectrum();
      lastSecondTickMs = now;
      lastSpectrumTickMs = now;
    }
    return;
  }

  if (g_data.connection != CONN_CONNECTED) return;

  if (now - lastSecondTickMs >= 1000) {
    lastSecondTickMs = now;
    g_data.liveTimeSeconds = (now - connectedSinceMs) / 1000;

    // gentle random walk around a plausible natural-background baseline,
    // nudged up while the simulated Cs-137 source is "present"
    float baselineUSvH = 0.11f + 0.35f * sourceStrength;
    float jitter = ((float)random(-15, 16)) / 1000.0f;
    g_data.doseRateUSvH = baselineUSvH + jitter;
    if (g_data.doseRateUSvH < 0.02f) g_data.doseRateUSvH = 0.02f;

    float baselineCpm = 18.0f + 70.0f * sourceStrength;
    g_data.cpm = baselineCpm + (float)random(-3, 4);
    if (g_data.cpm < 0) g_data.cpm = 0;
  }

  if (now - lastSpectrumTickMs >= 500) {
    lastSpectrumTickMs = now;
    tickSpectrum();
  }
}

} // namespace MockSource
