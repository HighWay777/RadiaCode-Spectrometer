#include "spectrometer_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Recursive so a guarded function can safely call another guarded one.
static SemaphoreHandle_t s_lock = nullptr;

static void ensureLock() {
  if (s_lock == nullptr) s_lock = xSemaphoreCreateRecursiveMutex();
}

void dataLock() {
  ensureLock();
  xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
}

void dataUnlock() {
  if (s_lock) xSemaphoreGiveRecursive(s_lock);
}

// Single canonical definition — both MockSource and BleSource
// (data/mock_source.*, data/ble_source.*) write into this same instance;
// only one of them should be ticked from main.cpp at a time.
SpectrometerData g_data;

// Names what the evidence supports, not what the object is. The decay
// chain tells us uranium is present; it cannot tell us whether that is
// ore, dust, glaze, glass, a chemical compound or a contaminated surface.
// "URANIUM ORE" quietly asserted a form nothing in the spectrum shows.
const char* sourceClassLabel(SourceClass c) {
  switch (c) {
    case SRC_BACKGROUND:    return "BACKGROUND";
    case SRC_URANIUM:       return "URANIUM";
    case SRC_THORIUM:       return "THORIUM";
    case SRC_MIXED_NATURAL: return "NATURAL MIX";
    case SRC_POTASSIUM:     return "K-40";
    case SRC_ARTIFICIAL:    return "ARTIFICIAL";
    default:                return "MEASURING...";
  }
}

// Verdict wording graded by how well supported it is. A chain identified at
// 55% and one at 95% are not the same statement, and printing both as a
// bare "URANIUM" made the display contradict itself as evidence wandered
// across the threshold. Hedged wording turns that wobble into what it
// actually is - uncertainty - instead of a claim that keeps being retracted.
const char* sourceClassLabelGraded(SourceClass c, float confidence) {
  switch (c) {
    case SRC_URANIUM:
      return confidence >= 0.80f ? "URANIUM" : "URANIUM?";
    case SRC_THORIUM:
      return confidence >= 0.80f ? "THORIUM" : "THORIUM?";
    case SRC_MIXED_NATURAL:
      return confidence >= 0.80f ? "NATURAL MIX" : "NATURAL MIX?";
    case SRC_ARTIFICIAL:
      return confidence >= 0.80f ? "ARTIFICIAL" : "ARTIFICIAL?";
    default:
      return sourceClassLabel(c);
  }
}

const char* sourceClassDetail(SourceClass c) {
  switch (c) {
    case SRC_BACKGROUND:    return "Normal natural background";
    case SRC_URANIUM:       return "U-238 chain - ore, dust, glaze...";
    case SRC_THORIUM:       return "Th-232 chain - mantle, mineral...";
    case SRC_MIXED_NATURAL: return "Multiple natural emitters";
    case SRC_POTASSIUM:     return "Potassium-40 - salt, feldspar...";
    case SRC_ARTIFICIAL:    return "Man-made nuclide present";
    default:                return "Collecting counts";
  }
}

const char* qualityGradeLabel(QualityGrade g) {
  switch (g) {
    case QUAL_FAIR:      return "FAIR";
    case QUAL_GOOD:      return "GOOD";
    case QUAL_EXCELLENT: return "EXCELLENT";
    default:             return "POOR";
  }
}
