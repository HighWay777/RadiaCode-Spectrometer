#pragma once

#include <stdint.h>

// Shared live state for the whole UI. Populated today by MockSource
// (src/data/mock_source.*); the intended real data source is the RC-110 BLE
// link (mkgeiger/RadiaCode) — swap the producer, keep this struct and every
// screen that reads it unchanged.

#define SPECTRUM_CHANNELS 1024
#define MAX_CANDIDATES 6

enum ConnectionState {
  CONN_DISCONNECTED = 0,
  CONN_CONNECTING,
  CONN_CONNECTED
};

struct IsotopeCandidate {
  const char* symbol;        // "Cs-137"
  const char* name;          // "Caesium-137"
  float confidence;          // 0..1, sorted descending in SpectrometerData::candidates
  float matchedEnergyKeV;    // strongest matched line, for display
};

struct SpectrometerData {
  ConnectionState connection = CONN_DISCONNECTED;
  int batteryPercent = -1;   // -1 = unknown
  uint32_t liveTimeSeconds = 0;

  float doseRateUSvH = 0.0f;
  float cpm = 0.0f;

  uint32_t spectrum[SPECTRUM_CHANNELS] = {0};
  // Energy(keV) = calibA0 + calibA1*channel + calibA2*channel^2
  float calibA0 = 0.0f;
  float calibA1 = 3.0f; // ~0-3072 keV across 1024 channels, placeholder until real calib read from device
  float calibA2 = 0.0f;

  IsotopeCandidate candidates[MAX_CANDIDATES];
  uint8_t candidateCount = 0;
};

extern SpectrometerData g_data;

inline float channelToKeV(const SpectrometerData& d, int channel) {
  return d.calibA0 + d.calibA1 * channel + d.calibA2 * channel * channel;
}
