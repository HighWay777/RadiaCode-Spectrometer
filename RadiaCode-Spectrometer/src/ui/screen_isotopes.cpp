#include "screen_isotopes.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/isotope_library.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenIsotopes {

static const int16_t HEADER_H = 26;
static const int16_t SLOT_W = 228, SLOT_H = 74;
static const int16_t COL_X[2] = {8, 244};
static const int16_t ROW_Y[3] = {30, 110, 190};

// Per-slot cache so a slot is only repainted when its content actually
// changed - avoids the whole grid visibly flashing every update tick.
static char lastSymbol[MAX_CANDIDATES][16];
static int lastConfidencePct[MAX_CANDIDATES];
static bool lastHasData[MAX_CANDIDATES];

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Isotope Candidates", 2, currentTheme->text);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

static void drawSlot(int index, bool force) {
  bool hasData = index < g_data.candidateCount;
  const char* symbol = hasData ? g_data.candidates[index].symbol : "";
  int confidencePct = hasData ? (int)(g_data.candidates[index].confidence * 100.0f) : -1;

  if (!force && hasData == lastHasData[index] && confidencePct == lastConfidencePct[index] &&
      strcmp(symbol, lastSymbol[index]) == 0) {
    return;
  }
  lastHasData[index] = hasData;
  lastConfidencePct[index] = confidencePct;
  strncpy(lastSymbol[index], symbol, sizeof(lastSymbol[index]) - 1);
  lastSymbol[index][sizeof(lastSymbol[index]) - 1] = '\0';

  int16_t x = COL_X[index % 2];
  int16_t y = ROW_Y[index / 2];
  bool isTop = (index == 0) && hasData;

  Cards::frame(x, y, SLOT_W, SLOT_H, nullptr, isTop);

  if (!hasData) {
    Cards::centeredText(x + SLOT_W / 2, y + SLOT_H / 2 - 4, "--", 2, currentTheme->textSecondary);
    return;
  }

  const IsotopeCandidate& c = g_data.candidates[index];
  uint16_t symColor = isTop ? currentTheme->highlight : currentTheme->text;

  Cards::leftText(x + 10, y + 8, c.symbol, 2, symColor);

  char pct[8];
  snprintf(pct, sizeof(pct), "%.0f%%", c.confidence * 100.0f);
  int16_t pw = Cards::textWidth(pct, 1);
  Cards::leftText(x + SLOT_W - 10 - pw, y + 12, pct, 1, currentTheme->textSecondary);

  Cards::progressBar(x + 10, y + 30, SLOT_W - 20, 8, c.confidence, isTop ? currentTheme->highlight : currentTheme->accent);

  Cards::leftText(x + 10, y + 46, c.name, 1, currentTheme->textSecondary);

  // Tag the origin so a lab/medical nuclide can't be mistaken for something
  // geologically plausible at a glance.
  const char* tag = "";
  uint16_t tagColor = currentTheme->textSecondary;
  for (uint16_t i = 0; i < IsotopeLibrary::count(); i++) {
    if (strcmp(IsotopeLibrary::at(i).symbol, c.symbol) != 0) continue;
    switch (IsotopeLibrary::at(i).category) {
      case CAT_NATURAL:
        tag = (IsotopeLibrary::at(i).chain == CHAIN_U238)  ? "U chain"
            : (IsotopeLibrary::at(i).chain == CHAIN_TH232) ? "Th chain" : "natural";
        tagColor = currentTheme->cool;
        break;
      case CAT_ARTIFICIAL:  tag = "artificial"; tagColor = currentTheme->danger; break;
      case CAT_MEDICAL:     tag = "medical";    tagColor = currentTheme->warning; break;
      case CAT_CALIBRATION: tag = "lab source"; tagColor = currentTheme->warning; break;
    }
    break;
  }

  char energyBuf[24];
  snprintf(energyBuf, sizeof(energyBuf), "%.0f keV  %u/%u", c.matchedEnergyKeV,
           c.linesMatched, c.linesTotal);
  Cards::leftText(x + 10, y + 58, energyBuf, 1, currentTheme->textSecondary);

  int16_t tw = Cards::textWidth(tag, 1);
  Cards::leftText(x + SLOT_W - 10 - tw, y + 58, tag, 1, tagColor);
}

static void refreshAll(bool force) {
  // candidateCount and the candidate array are rebuilt on core 0 by
  // analyzeSpectrum(); without this a UI tick landing mid-rebuild can render
  // one nuclide's symbol beside another's confidence.
  DataGuard guard;
  for (int i = 0; i < MAX_CANDIDATES; i++) drawSlot(i, force);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  refreshAll(/* force */ true);
}

void update() {
  refreshAll(false);
}

int handleTap(int16_t x, int16_t y) {
  for (int i = 0; i < MAX_CANDIDATES; i++) {
    int16_t sx = COL_X[i % 2];
    int16_t sy = ROW_Y[i / 2];
    if (x >= sx && x <= sx + SLOT_W && y >= sy && y <= sy + SLOT_H) {
      return (i < g_data.candidateCount) ? i : -1;
    }
  }
  return -1;
}

} // namespace ScreenIsotopes
