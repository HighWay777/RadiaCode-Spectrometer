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

const char* sourceClassLabel(SourceClass c) {
  switch (c) {
    case SRC_BACKGROUND:    return "BACKGROUND";
    case SRC_URANIUM:       return "URANIUM ORE";
    case SRC_THORIUM:       return "THORIUM";
    case SRC_MIXED_NATURAL: return "NATURAL MIX";
    case SRC_POTASSIUM:     return "POTASSIUM";
    case SRC_ARTIFICIAL:    return "ARTIFICIAL";
    default:                return "MEASURING...";
  }
}

const char* sourceClassDetail(SourceClass c) {
  switch (c) {
    case SRC_BACKGROUND:    return "Normal natural background";
    case SRC_URANIUM:       return "Natural U-238 decay chain";
    case SRC_THORIUM:       return "Natural Th-232 decay chain";
    case SRC_MIXED_NATURAL: return "Multiple natural emitters";
    case SRC_POTASSIUM:     return "K-40 dominant";
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
