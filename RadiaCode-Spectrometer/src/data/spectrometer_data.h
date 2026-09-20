#pragma once

#include <stdint.h>

// Shared live state for the whole UI. Populated by BleSource
// (src/data/ble_source.*) from the real RC-110; MockSource
// (src/data/mock_source.*) writes the same struct for offline UI testing.
// Every screen reads only from here.

#define SPECTRUM_CHANNELS 1024
#define MAX_CANDIDATES 6

enum ConnectionState {
  CONN_DISCONNECTED = 0,
  CONN_CONNECTING,
  CONN_CONNECTED
};

struct IsotopeCandidate {
  const char* symbol;        // "Bi-214"
  const char* name;          // "Bismuth-214"
  float confidence;          // 0..1, sorted descending in SpectrometerData::candidates
  float matchedEnergyKeV;    // strongest matched line, for display
  uint8_t linesMatched;
  uint8_t linesTotal;
  float significance;        // sigma of the strongest matched peak
};

// Plain-language verdict for the whole sample — what a non-specialist
// actually wants to know. Derived from decay-chain corroboration in
// analysis/peak_search.cpp, not from any single nuclide.
enum SourceClass {
  SRC_INSUFFICIENT = 0, // not enough counts yet to say anything
  SRC_BACKGROUND,       // nothing above normal natural background
  SRC_URANIUM,          // U-238 chain (uranium ore / pitchblende / autunite...)
  SRC_THORIUM,          // Th-232 chain (monazite, thorite, gas mantles...)
  SRC_MIXED_NATURAL,    // both chains, or chain + K-40
  SRC_POTASSIUM,        // K-40 dominant (feldspar, salt substitute, fertiliser)
  SRC_ARTIFICIAL        // man-made nuclide dominates
};

// Evidence for one decay chain: how many of its member nuclides were
// independently detected. Corroboration across members is far stronger
// evidence than any single line match.
struct ChainResult {
  float confidence;        // 0..1
  uint8_t membersDetected;
  uint8_t membersTotal;
};

// Counting statistics drive how long the user should keep measuring.
// Peak significance grows as sqrt(time), so the remaining-time estimate is
// a real extrapolation, not a guess. See computeQuality() in peak_search.cpp.
enum QualityGrade {
  QUAL_POOR = 0,   // < 3 sigma  — treat any ID as noise
  QUAL_FAIR,       // 3-6        — plausible, not confirmed
  QUAL_GOOD,       // 6-12       — solid for strong isolated sources
  QUAL_EXCELLENT   // >= 12      — as good as this detector gets
};

struct QualityInfo {
  uint32_t totalCounts;
  float weakestKeySigma;      // weakest peak the current top ID relies on
  float bestSigma;            // strongest peak found
  QualityGrade grade;
  uint32_t recommendedSeconds; // total acquisition to reach QUAL_EXCELLENT
  uint32_t remainingSeconds;   // recommended minus elapsed, 0 when done
  uint8_t peaksFound;
};

struct SpectrometerData {
  ConnectionState connection = CONN_DISCONNECTED;
  // Reported by the device, not assumed. The vendored library supports
  // RC-102/103/103G/110, so nothing here should hardcode one model.
  char deviceModel[20] = "RadiaCode";   // e.g. "RadiaCode-110"
  char deviceSerial[24] = "";           // e.g. "RC-110-000983"
  int batteryPercent = -1;   // -1 = unknown
  uint32_t liveTimeSeconds = 0;      // wall clock since connect
  uint32_t spectrumDurationSec = 0;  // device-reported spectrum acquisition time
  uint32_t windowDurationSec = 0;    // duration of active measurement window
  bool isWindowed = false;           // true when spectrum window is active

  float doseRateUSvH = 0.0f;
  // Counts per second, matching what the official RadiaCode app reports —
  // this detector's CPS runs in the tens for background and thousands for
  // ore, so CPM produced needlessly large numbers.
  float cps = 0.0f;

  uint32_t spectrum[SPECTRUM_CHANNELS] = {0};
  // Energy(keV) = calibA0 + calibA1*channel + calibA2*channel^2
  float calibA0 = 0.0f;
  float calibA1 = 3.0f; // placeholder until real calib is read from the device
  float calibA2 = 0.0f;
  // False until energyCalib() has actually been read over BLE. Anything that
  // compares against the device's calibration (e.g. importing a background
  // recorded elsewhere) must not draw conclusions from the placeholder.
  bool calibValid = false;

  IsotopeCandidate candidates[MAX_CANDIDATES];
  uint8_t candidateCount = 0;

  SourceClass sourceClass = SRC_INSUFFICIENT;
  // How well supported that verdict is, 0..1. A verdict computed from
  // counting statistics is never simply true or false, and presenting it as
  // a bare word made the display flip between URANIUM and BACKGROUND as the
  // evidence wandered across a threshold. The UI shows this instead of
  // pretending to certainty it does not have.
  float verdictConfidence = 0.0f;
  ChainResult uranium = {0.0f, 0, 6};
  ChainResult thorium = {0.0f, 0, 4};
  QualityInfo quality = {0, 0.0f, 0.0f, QUAL_POOR, 0, 0, 0};
};

extern SpectrometerData g_data;

// BLE polling and spectrum analysis run on core 0 (see BleSource::begin), so
// the UI on core 1 stays responsive during multi-second scans and blocking
// reads. Anything touching the bulk arrays (spectrum, candidates, analysis
// buffers) must hold this while it does so. Short critical sections only.
void dataLock();
void dataUnlock();

// RAII helper for UI code.
struct DataGuard {
  DataGuard()  { dataLock(); }
  ~DataGuard() { dataUnlock(); }
};

inline float channelToKeV(const SpectrometerData& d, int channel) {
  return d.calibA0 + d.calibA1 * channel + d.calibA2 * channel * channel;
}

const char* sourceClassLabel(SourceClass c);
const char* sourceClassLabelGraded(SourceClass c, float confidence);
const char* sourceClassDetail(SourceClass c);
const char* qualityGradeLabel(QualityGrade g);
