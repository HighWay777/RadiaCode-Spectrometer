#include "screen_isotope_detail.h"
#include "../analysis/peak_search.h"   // fwhmKeVAt()
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/isotope_library.h"
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
  // The candidate array is rebuilt on core 0; copying a whole struct out of
  // it unguarded can mix two nuclides' fields into one snapshot.
  DataGuard guard;
  hasSnapshot = false;
  if (index >= 0 && index < g_data.candidateCount) {
    snapshot = g_data.candidates[index];
    hasSnapshot = true;
  }
}

static const IsotopeDef* findIsotopeDef(const char* symbol) {
  for (uint16_t i = 0; i < IsotopeLibrary::count(); i++) {
    if (strcmp(IsotopeLibrary::at(i).symbol, symbol) == 0) return &IsotopeLibrary::at(i);
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

  const IsotopeDef* def = findIsotopeDef(snapshot.symbol);

  Cards::leftText(20, 32, snapshot.symbol, 3, currentTheme->highlight);
  Cards::leftText(20, 60, snapshot.name, 2, currentTheme->text);

  // Origin context matters for judging plausibility: a 13-year calibration
  // nuclide or an 8-day medical tracer showing up in a rock sample is a red
  // flag, whereas a decay-chain member is exactly what ore should produce.
  if (def != nullptr) {
    const char* origin = "";
    switch (def->category) {
      case CAT_NATURAL:     origin = (def->chain == CHAIN_U238)  ? "Natural - U-238 chain"
                                   : (def->chain == CHAIN_TH232) ? "Natural - Th-232 chain"
                                                                 : "Natural - primordial"; break;
      case CAT_ARTIFICIAL:  origin = "Artificial - fission/activation"; break;
      case CAT_MEDICAL:     origin = "Medical tracer - short lived"; break;
      case CAT_CALIBRATION: origin = "Lab calibration source"; break;
    }
    Cards::leftText(20, 82, origin, 1, currentTheme->cool);
  }

  char pct[48];
  snprintf(pct, sizeof(pct), "Confidence %.0f%%   %u/%u lines   %.1f sigma",
           snapshot.confidence * 100.0f, snapshot.linesMatched, snapshot.linesTotal,
           snapshot.significance);
  Cards::leftText(20, 98, pct, 1, currentTheme->textSecondary);
  Cards::progressBar(20, 110, 440, 8, snapshot.confidence, currentTheme->highlight);

  char matched[40];
  snprintf(matched, sizeof(matched), "Strongest matched peak: %.1f keV", snapshot.matchedEnergyKeV);
  Cards::leftText(20, 126, matched, 1, currentTheme->textSecondary);

  Cards::leftText(20, 146, "Gamma lines (gold = matched in this spectrum):", 1,
                  currentTheme->textSecondary);

  // Two columns so nuclides with many lines (Bi-214 has 7) still fit.
  int16_t ly = 162;
  int16_t lx = 20;
  if (def != nullptr) {
    for (uint8_t i = 0; i < def->lineCount; i++) {
      const IsotopeLine& line = def->lines[i];
      // Scale with the detector resolution, as the matcher does. A fixed
      // 5 keV window disagreed with the analysis at both ends of the range -
      // the real tolerance is ~10 keV at 60 keV and ~64 keV at 2614 keV.
      float tol = fwhmKeVAt(line.energyKeV) * 0.6f;
      bool isMatched = fabsf(line.energyKeV - snapshot.matchedEnergyKeV) < tol;
      char lineBuf[32];
      snprintf(lineBuf, sizeof(lineBuf), "%.1f keV  (%.1f%%)", line.energyKeV, line.intensity * 100.0f);
      Cards::leftText(lx, ly, lineBuf, 1, isMatched ? currentTheme->highlight : currentTheme->text);
      ly += 13;
      if (ly > BTN_Y - 14) { ly = 162; lx += 240; if (lx > 260) break; }
    }
  }

  Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, "BACK", currentTheme->accent);
}

void update() {
  // Static snapshot view - nothing to refresh between draws.
}

bool handleTap(int16_t x, int16_t y) {
  return (x >= BTN_X && x <= BTN_X + BTN_W && y >= BTN_Y && y <= BTN_Y + BTN_H);
}

} // namespace ScreenIsotopeDetail
