#include "screen_isotope_detail.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/isotope_db.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Arduino_GFX* gfx;

namespace ScreenIsotopeDetail {

static const int16_t HEADER_H = 26;
static const int16_t BTN_X = 8, BTN_Y = 224, BTN_W = 464, BTN_H = 40;

static IsotopeCandidate snapshot;
static bool hasSnapshot = false;

void setCandidateIndex(int index) {
  hasSnapshot = false;
  if (index >= 0 && index < g_data.candidateCount) {
    snapshot = g_data.candidates[index];
    hasSnapshot = true;
  }
}

static const IsotopeDef* findIsotopeDef(const char* symbol) {
  for (uint8_t i = 0; i < ISOTOPE_DB_COUNT; i++) {
    if (strcmp(ISOTOPE_DB[i].symbol, symbol) == 0) return &ISOTOPE_DB[i];
  }
  return nullptr;
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Isotope Detail", 2, currentTheme->text);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);

  if (!hasSnapshot) {
    Cards::centeredText(240, 120, "No selection", 2, currentTheme->textSecondary);
    Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, "BACK", currentTheme->accent);
    return;
  }

  Cards::leftText(20, 34, snapshot.symbol, 4, currentTheme->highlight);
  Cards::leftText(20, 78, snapshot.name, 2, currentTheme->text);

  char pct[24];
  snprintf(pct, sizeof(pct), "Confidence: %.0f%%", snapshot.confidence * 100.0f);
  Cards::leftText(20, 106, pct, 1, currentTheme->textSecondary);
  Cards::progressBar(20, 118, 440, 8, snapshot.confidence, currentTheme->highlight);

  char matched[32];
  snprintf(matched, sizeof(matched), "Matched peak: %.0f keV", snapshot.matchedEnergyKeV);
  Cards::leftText(20, 134, matched, 1, currentTheme->textSecondary);

  Cards::leftText(20, 156, "Known gamma lines:", 1, currentTheme->textSecondary);

  const IsotopeDef* def = findIsotopeDef(snapshot.symbol);
  int16_t ly = 172;
  if (def != nullptr) {
    for (uint8_t i = 0; i < def->lineCount && ly < BTN_Y - 12; i++) {
      const IsotopeLine& line = def->lines[i];
      bool isMatched = fabsf(line.energyKeV - snapshot.matchedEnergyKeV) < 5.0f;
      char lineBuf[32];
      snprintf(lineBuf, sizeof(lineBuf), "%.1f keV   (rel. %.0f%%)", line.energyKeV, line.intensity * 100.0f);
      Cards::leftText(20, ly, lineBuf, 1, isMatched ? currentTheme->highlight : currentTheme->text);
      ly += 14;
    }
  }

  Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, "BACK", currentTheme->accent);
}

void update() {
  // Static snapshot view — nothing to refresh between draws.
}

bool handleTap(int16_t x, int16_t y) {
  return (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN_Y && y <= BTN_Y + BTN_H);
}

} // namespace ScreenIsotopeDetail
