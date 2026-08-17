#include "screen_spectrum.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../analysis/peak_search.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>

extern Arduino_GFX* gfx;

namespace ScreenSpectrum {

static const int16_t HEADER_H = 26;
static const int16_t PLOT_X = 8, PLOT_Y = 36, PLOT_W = 464, PLOT_H = 168;
static const int16_t FOOTER_Y = PLOT_Y + PLOT_H + 6;

static uint32_t bins[PLOT_W];

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Live Spectrum", 2, currentTheme->text);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

static void rebin() {
  for (int px = 0; px < PLOT_W; px++) {
    int chStart = (px * SPECTRUM_CHANNELS) / PLOT_W;
    int chEnd = ((px + 1) * SPECTRUM_CHANNELS) / PLOT_W;
    if (chEnd <= chStart) chEnd = chStart + 1;
    uint32_t sum = 0;
    for (int ch = chStart; ch < chEnd && ch < SPECTRUM_CHANNELS; ch++) sum += g_data.spectrum[ch];
    bins[px] = sum;
  }
}

static void drawPlot() {
  gfx->fillRect(PLOT_X, PLOT_Y, PLOT_W, PLOT_H, currentTheme->background);
  gfx->drawRect(PLOT_X - 1, PLOT_Y - 1, PLOT_W + 2, PLOT_H + 2, currentTheme->cardBorder);

  uint32_t maxVal = 1;
  for (int px = 0; px < PLOT_W; px++) if (bins[px] > maxVal) maxVal = bins[px];

  for (int px = 0; px < PLOT_W; px++) {
    if (bins[px] == 0) continue;
    int16_t barH = (int16_t)(((uint64_t)bins[px] * PLOT_H) / maxVal);
    if (barH > PLOT_H) barH = PLOT_H;
    gfx->drawFastVLine(PLOT_X + px, PLOT_Y + PLOT_H - barH, barH, currentTheme->accent);
  }

  // Mark statistically significant peaks found by the same classical
  // peak-search used for isotope matching (see analysis/peak_search.*).
  DetectedPeak peaks[MAX_DETECTED_PEAKS];
  int peakCount = findPeaks(g_data, peaks, MAX_DETECTED_PEAKS);
  for (int i = 0; i < peakCount; i++) {
    int16_t px = (int16_t)(((int64_t)peaks[i].channel * PLOT_W) / SPECTRUM_CHANNELS);
    int16_t cx = PLOT_X + px;
    gfx->fillTriangle(cx - 3, PLOT_Y - 2, cx + 3, PLOT_Y - 2, cx, PLOT_Y + 4, currentTheme->highlight);
  }
}

static void drawFooter() {
  gfx->fillRect(0, FOOTER_Y, 480, 272 - FOOTER_Y, currentTheme->background);

  char loBuf[16], hiBuf[16];
  snprintf(loBuf, sizeof(loBuf), "%.0f keV", channelToKeV(g_data, 0));
  snprintf(hiBuf, sizeof(hiBuf), "%.0f keV", channelToKeV(g_data, SPECTRUM_CHANNELS - 1));
  Cards::leftText(PLOT_X, FOOTER_Y + 2, loBuf, 1, currentTheme->textSecondary);
  int16_t hw = Cards::textWidth(hiBuf, 1);
  Cards::leftText(PLOT_X + PLOT_W - hw, FOOTER_Y + 2, hiBuf, 1, currentTheme->textSecondary);

  char liveBuf[24];
  snprintf(liveBuf, sizeof(liveBuf), "Live %us", (unsigned)g_data.liveTimeSeconds);
  Cards::centeredText(PLOT_X + PLOT_W / 2, FOOTER_Y + 2, liveBuf, 1, currentTheme->textSecondary);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  rebin();
  drawPlot();
  drawFooter();
}

void update() {
  rebin();
  drawPlot();
  drawFooter();
}

} // namespace ScreenSpectrum
