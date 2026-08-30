#include "screen_spectrum.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/background.h"
#include "../analysis/peak_search.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Arduino_GFX* gfx;

namespace ScreenSpectrum {

static const int16_t HEADER_H = 26;
static const int16_t MARK_Y   = 28;   // peak labels + markers live above the plot,
static const int16_t MARK_H   = 16;   // so they never overlap (or repaint) the bars
static const int16_t PLOT_X = 8, PLOT_Y = 46, PLOT_W = 464, PLOT_H = 150;
static const int16_t FOOTER_Y = PLOT_Y + PLOT_H + 6;

// Gamma spectra span orders of magnitude: on uranium ore the low-energy region
// is ~100x the 1764 keV Bi-214 line, so on a linear axis the diagnostic
// high-energy peaks are invisible. Log is the standard view and the default.
static bool logScale = true;

static uint32_t bins[PLOT_W];
// Rebinned background reference, drawn as a dim line behind the net spectrum
// when subtraction is active so the two can be compared directly.
static uint32_t bgBins[PLOT_W];
static bool showingNet = false;

// --- flicker avoidance ---------------------------------------------------
// The plot is redrawn every 500 ms. Clearing the whole 464x150 area and
// repainting it was the source of the visible flicker, so instead each
// column remembers its last height and only the changed segment is touched.
// That also means the y-scale must be *stable*: if it were rescaled to the
// live maximum on every tick, every column would change height every tick
// and we would be back to a full repaint. So the scale snaps to whole
// decades (log) or 1/2/5 x 10^n (linear) and only moves when the data
// genuinely outgrows it.
static int16_t lastBarH[PLOT_W];
static int16_t lastBgH[PLOT_W];
static bool haveLastBars = false;
static float scaleMax = 0.0f;    // log: decades; linear: counts

static void rebin() {
  // Snapshot under the lock: the BLE task rewrites these arrays from core 0.
  DataGuard guard;
  // With a reference active, plot the sample's own excess — that is what the
  // identification works on, and plotting the raw spectrum instead made
  // subtraction look like it did nothing at all.
  showingNet = Background::active() && backgroundApplied();
  const float* net = showingNet ? correctedSpectrum() : nullptr;
  const float* bg = showingNet ? backgroundCounts() : nullptr;

  for (int px = 0; px < PLOT_W; px++) {
    int chStart = (px * SPECTRUM_CHANNELS) / PLOT_W;
    int chEnd = ((px + 1) * SPECTRUM_CHANNELS) / PLOT_W;
    if (chEnd <= chStart) chEnd = chStart + 1;

    uint32_t sum = 0, bgSum = 0;
    for (int ch = chStart; ch < chEnd && ch < SPECTRUM_CHANNELS; ch++) {
      if (showingNet) {
        float v = net[ch];
        if (v > 0.0f) sum += (uint32_t)v;   // negative excess reads as zero
        float b = bg[ch];
        if (b > 0.0f) bgSum += (uint32_t)b;
      } else {
        sum += g_data.spectrum[ch];
      }
    }
    bins[px] = sum;
    bgBins[px] = bgSum;
  }
}

static float niceLinearMax(uint32_t v) {
  if (v < 10) return 10.0f;
  float mag = powf(10.0f, floorf(log10f((float)v)));
  float norm = (float)v / mag;
  float step = (norm <= 1.0f) ? 1.0f : (norm <= 2.0f) ? 2.0f : (norm <= 5.0f) ? 5.0f : 10.0f;
  return step * mag;
}

// Returns true if the scale moved (which forces a full repaint).
static bool updateScale() {
  uint32_t maxVal = 1;
  for (int px = 0; px < PLOT_W; px++) if (bins[px] > maxVal) maxVal = bins[px];

  float wanted;
  if (logScale) {
    wanted = ceilf(log10f((float)maxVal + 1.0f));
    if (wanted < 1.0f) wanted = 1.0f;
  } else {
    wanted = niceLinearMax(maxVal);
  }

  // Only grow, or shrink when the data has dropped well below the current
  // scale — prevents oscillating between two scales on noisy data.
  bool changed = false;
  if (wanted > scaleMax) { scaleMax = wanted; changed = true; }
  else if (logScale && wanted < scaleMax - 1.0f) { scaleMax = wanted; changed = true; }
  else if (!logScale && wanted < scaleMax * 0.4f) { scaleMax = wanted; changed = true; }
  if (scaleMax <= 0.0f) { scaleMax = logScale ? 1.0f : 10.0f; changed = true; }
  return changed;
}

static int16_t barHeightFor(uint32_t v) {
  if (v == 0) return 0;
  int16_t h;
  if (logScale) h = (int16_t)((log10f((float)v + 1.0f) / scaleMax) * PLOT_H);
  else          h = (int16_t)(((float)v / scaleMax) * PLOT_H);
  if (h > PLOT_H) h = PLOT_H;
  if (h < 1) h = 1;
  return h;
}

// y positions of the decade gridlines, so a cleared column segment can put
// back the pixels it just erased instead of leaving gaps in the grid.
static bool isGridlineY(int16_t y) {
  if (!logScale) return false;
  for (int decade = 1; decade < (int)scaleMax + 1; decade++) {
    int16_t gy = PLOT_Y + PLOT_H - (int16_t)((decade / scaleMax) * PLOT_H);
    if (gy == y) return true;
  }
  return false;
}

static void clearColumnSegment(int16_t x, int16_t yTop, int16_t h) {
  if (h <= 0) return;
  gfx->drawFastVLine(x, yTop, h, currentTheme->background);
  if (!logScale) return;
  if ((x - PLOT_X) % 6 != 0) return;
  for (int16_t y = yTop; y < yTop + h; y++) {
    if (isGridlineY(y)) gfx->drawPixel(x, y, currentTheme->cardBorder);
  }
}

static void drawGrid() {
  if (!logScale) return;
  for (int decade = 1; decade < (int)scaleMax + 1; decade++) {
    int16_t y = PLOT_Y + PLOT_H - (int16_t)((decade / scaleMax) * PLOT_H);
    if (y <= PLOT_Y || y >= PLOT_Y + PLOT_H) continue;
    for (int16_t x = PLOT_X; x < PLOT_X + PLOT_W; x += 6) {
      gfx->drawPixel(x, y, currentTheme->cardBorder);
    }
  }
}

static void drawPlot(bool full) {
  if (full) {
    gfx->fillRect(PLOT_X, PLOT_Y, PLOT_W, PLOT_H, currentTheme->background);
    gfx->drawRect(PLOT_X - 1, PLOT_Y - 1, PLOT_W + 2, PLOT_H + 2, currentTheme->cardBorder);
    drawGrid();
  }

  // With a reference shown, each column carries two series (net bar plus the
  // background marker) so a changed column is repainted whole rather than
  // delta-patched. Still per-column, so no whole-plot flicker.
  if (showingNet) {
    for (int px = 0; px < PLOT_W; px++) {
      int16_t netH = barHeightFor(bins[px]);
      int16_t bgH = barHeightFor(bgBins[px]);
      int16_t x = PLOT_X + px;
      if (!full && haveLastBars && lastBarH[px] == netH && lastBgH[px] == bgH) continue;

      clearColumnSegment(x, PLOT_Y, PLOT_H);
      if (bgH > 0) {
        // Where the room background sits, for direct comparison.
        gfx->drawPixel(x, PLOT_Y + PLOT_H - bgH, currentTheme->cardBorder);
      }
      if (netH > 0) {
        gfx->drawFastVLine(x, PLOT_Y + PLOT_H - netH, netH, currentTheme->accent);
      }
      lastBarH[px] = netH;
      lastBgH[px] = bgH;
    }
    haveLastBars = true;
    return;
  }

  for (int px = 0; px < PLOT_W; px++) {
    int16_t newH = barHeightFor(bins[px]);
    int16_t oldH = (haveLastBars && !full) ? lastBarH[px] : -1;
    int16_t x = PLOT_X + px;
    lastBgH[px] = -1;

    if (oldH == newH) continue;

    if (oldH < 0) {
      // Full paint of this column.
      clearColumnSegment(x, PLOT_Y, PLOT_H - newH);
      if (newH > 0) gfx->drawFastVLine(x, PLOT_Y + PLOT_H - newH, newH, currentTheme->accent);
    } else if (newH > oldH) {
      // Bar grew: paint only the new segment on top of the old one.
      gfx->drawFastVLine(x, PLOT_Y + PLOT_H - newH, newH - oldH, currentTheme->accent);
    } else {
      // Bar shrank: clear only the vacated segment.
      clearColumnSegment(x, PLOT_Y + PLOT_H - oldH, oldH - newH);
    }
    lastBarH[px] = newH;
  }
  haveLastBars = true;
}

// --- peak markers --------------------------------------------------------
// Drawn in their own strip above the plot so they can be repainted
// independently of the chart, and only when the peak set actually changes.
static int lastMarkerSignature = -1;

static void drawMarkers(bool force) {
  const DetectedPeak* peaks = lastPeaks();
  int peakCount = lastPeakCount();

  int signature = peakCount * 7919;
  for (int i = 0; i < peakCount; i++) signature += peaks[i].channel * (i + 1);
  if (!force && signature == lastMarkerSignature) return;
  lastMarkerSignature = signature;

  gfx->fillRect(0, MARK_Y, 480, MARK_H, currentTheme->background);

  for (int i = 0; i < peakCount; i++) {
    int16_t px = (int16_t)(((int64_t)peaks[i].channel * PLOT_W) / SPECTRUM_CHANNELS);
    int16_t cx = PLOT_X + px;
    if (cx < PLOT_X || cx >= PLOT_X + PLOT_W) continue;
    gfx->fillTriangle(cx - 3, MARK_Y + MARK_H - 6, cx + 3, MARK_Y + MARK_H - 6,
                      cx, MARK_Y + MARK_H - 1, currentTheme->highlight);
  }

  // Label the three most significant peaks (rank = how many are stronger).
  for (int rank = 0; rank < 3; rank++) {
    int bestIdx = -1;
    for (int i = 0; i < peakCount; i++) {
      int stronger = 0;
      for (int j = 0; j < peakCount; j++) {
        if (peaks[j].significance > peaks[i].significance) stronger++;
      }
      if (stronger == rank) { bestIdx = i; break; }
    }
    if (bestIdx < 0) continue;
    int16_t px = (int16_t)(((int64_t)peaks[bestIdx].channel * PLOT_W) / SPECTRUM_CHANNELS);
    char lbl[12];
    snprintf(lbl, sizeof(lbl), "%.0f", peaks[bestIdx].energyKeV);
    int16_t lw = Cards::textWidth(lbl, 1);
    int16_t lx = PLOT_X + px - lw / 2;
    if (lx < PLOT_X) lx = PLOT_X;
    if (lx + lw > PLOT_X + PLOT_W) lx = PLOT_X + PLOT_W - lw;
    Cards::leftText(lx, MARK_Y, lbl, 1, currentTheme->highlight);
  }
}

// --- chrome --------------------------------------------------------------
static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, showingNet ? "Spectrum - NET" : "Spectrum - RAW", 2,
                  showingNet ? currentTheme->accent : currentTheme->text);
  const char* mode = logScale ? "LOG" : "LIN";
  int16_t mw = Cards::textWidth(mode, 1);
  Cards::leftText(480 - 12 - mw, 10, mode, 1, currentTheme->accent);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

static void drawAxisLabels() {
  char loBuf[16], midBuf[16], hiBuf[16];
  snprintf(loBuf, sizeof(loBuf), "%.0f keV", channelToKeV(g_data, 0));
  snprintf(midBuf, sizeof(midBuf), "%.0f", channelToKeV(g_data, SPECTRUM_CHANNELS / 2));
  snprintf(hiBuf, sizeof(hiBuf), "%.0f keV", channelToKeV(g_data, SPECTRUM_CHANNELS - 1));
  Cards::leftText(PLOT_X, FOOTER_Y + 2, loBuf, 1, currentTheme->textSecondary);
  Cards::centeredText(PLOT_X + PLOT_W / 2, FOOTER_Y + 2, midBuf, 1, currentTheme->textSecondary);
  int16_t hw = Cards::textWidth(hiBuf, 1);
  Cards::leftText(PLOT_X + PLOT_W - hw, FOOTER_Y + 2, hiBuf, 1, currentTheme->textSecondary);
}

static char lastStatus[56] = "";

static void drawStatus(bool force) {
  char info[72];
  if (showingNet) {
    snprintf(info, sizeof(info), "sample minus %s   grey = background   %u peaks",
             Background::label(), (unsigned)lastPeakCount());
  } else {
    snprintf(info, sizeof(info), "%u counts   %u peaks   tap: log/lin",
             (unsigned)g_data.quality.totalCounts, (unsigned)lastPeakCount());
  }
  if (!force && strcmp(info, lastStatus) == 0) return;
  strncpy(lastStatus, info, sizeof(lastStatus) - 1);
  lastStatus[sizeof(lastStatus) - 1] = '\0';
  gfx->fillRect(0, FOOTER_Y + 16, 480, 14, currentTheme->background);
  Cards::centeredText(240, FOOTER_Y + 16, info, 1, currentTheme->textSecondary);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  haveLastBars = false;
  scaleMax = 0.0f;
  lastMarkerSignature = -1;
  lastStatus[0] = '\0';

  rebin();       // sets showingNet, which the header reports
  drawHeader();
  updateScale();
  drawPlot(true);
  drawMarkers(true);
  drawAxisLabels();
  drawStatus(true);
}

void update() {
  bool wasNet = showingNet;
  rebin();
  // Toggling subtraction changes what every column means, so redraw the
  // chrome and the whole plot rather than delta-patching between two
  // different series.
  bool modeChanged = (wasNet != showingNet);
  if (modeChanged) {
    drawHeader();
    haveLastBars = false;
    scaleMax = 0.0f;
  }
  bool scaleChanged = updateScale();
  drawPlot(scaleChanged || modeChanged);
  drawMarkers(modeChanged);
  drawStatus(modeChanged);
}

bool handleTap(int16_t x, int16_t y) {
  if (y < HEADER_H || y > FOOTER_Y + 24) return false;
  logScale = !logScale;
  scaleMax = 0.0f;      // force a rescale for the new axis type
  haveLastBars = false; // and a full repaint
  drawHeader();
  rebin();
  updateScale();
  drawPlot(true);
  drawMarkers(true);
  drawAxisLabels();
  drawStatus(true);
  return true;
}

} // namespace ScreenSpectrum
