#include "screen_home.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/ble_source.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenHome {

static const int16_t HEADER_H = 26;

static const int16_t DOSE_X = 8,   DOSE_Y = 30,  DOSE_W = 228, DOSE_H = 88;
static const int16_t CPM_X  = 244, CPM_Y  = 30,  CPM_W  = 228, CPM_H  = 88;
static const int16_t SRC_X  = 8,   SRC_Y  = 122, SRC_W  = 228, SRC_H  = 88;
static const int16_t STAT_X = 244, STAT_Y = 122, STAT_W = 228, STAT_H = 88;
// Bottom row: connect, an explicit RESET (the only thing that discards an
// accumulated run - see BleSource::requestConnect), and settings.
static const int16_t BTN_X  = 8,   BTN_Y  = 216, BTN_W  = 288, BTN_H  = 44;
static const int16_t RST_X  = 302, RST_W  = 84;
static const int16_t SET_X  = 392, SET_W  = 80;

static bool resetArmed = false;
static unsigned long resetArmedUntil = 0;

// Dose rate spans orders of magnitude — 0.1 uSv/h indoors, ~28 on uranium ore
// — so the scale is logarithmic and the colour moves continuously rather than
// snapping between three states. Green means "ordinary background", and it
// climbs green -> lime -> amber -> orange -> red -> magenta from there.
static const struct { float t; uint8_t r, g, b; } DOSE_STOPS[] = {
  {0.00f,   0, 200,  90},   // safe: normal background
  {0.28f, 160, 210,  40},   // slightly elevated
  {0.48f, 245, 210,   0},   // noticeably above background
  {0.66f, 255, 140,  10},   // hot specimen
  {0.85f, 235,  45,  45},   // do not linger
  {1.00f, 255,   0, 140},   // very high
};

static float doseScalePosition(float uSvH) {
  // 0.05 uSv/h (below any real background) to 100 uSv/h maps onto 0..1.
  if (uSvH < 0.05f) uSvH = 0.05f;
  float t = log10f(uSvH / 0.05f) / log10f(100.0f / 0.05f);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t;
}

static uint16_t doseColor(float uSvH) {
  float t = doseScalePosition(uSvH);
  const int n = sizeof(DOSE_STOPS) / sizeof(DOSE_STOPS[0]);
  for (int i = 1; i < n; i++) {
    if (t > DOSE_STOPS[i].t && i < n - 1) continue;
    const auto& a = DOSE_STOPS[i - 1];
    const auto& b = DOSE_STOPS[i];
    float span = b.t - a.t;
    float f = span > 0.0f ? (t - a.t) / span : 0.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    uint8_t r = (uint8_t)(a.r + (b.r - a.r) * f);
    uint8_t g = (uint8_t)(a.g + (b.g - a.g) * f);
    uint8_t bl = (uint8_t)(a.b + (b.b - a.b) * f);
    return rgb565(r, g, bl);
  }
  return currentTheme->accent;
}

// A thin gradient strip with a marker showing where the current reading sits.
// The number alone does not tell a newcomer whether 0.3 uSv/h is fine.
static void drawDoseScale(int16_t x, int16_t y, int16_t w, int16_t h, float uSvH) {
  for (int16_t i = 0; i < w; i++) {
    float t = (float)i / (float)(w - 1);
    // Invert doseScalePosition to colour the strip evenly across the range.
    float v = 0.05f * powf(100.0f / 0.05f, t);
    gfx->drawFastVLine(x + i, y, h, doseColor(v));
  }
  int16_t mx = x + (int16_t)(doseScalePosition(uSvH) * (w - 1));
  gfx->fillRect(mx - 1, y - 2, 3, h + 4, currentTheme->text);
}

static void formatTime(uint32_t totalSeconds, char* buf, size_t bufSize) {
  uint32_t h = totalSeconds / 3600;
  uint32_t m = (totalSeconds / 60) % 60;
  snprintf(buf, bufSize, "%02u:%02u", (unsigned)h, (unsigned)m);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  // Shows whichever RadiaCode is actually connected, not a hardcoded model.
  Cards::leftText(10, 6, g_data.deviceModel, 2, currentTheme->text);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

// Everything below is redrawn on a timer (see main.cpp), but the QSPI
// clear-then-draw sequence is visibly flashy if repeated when nothing
// actually changed. Each updateXxx() caches what it last drew and skips the
// redraw entirely when the new value would look identical.

static ConnectionState lastHeaderConn = (ConnectionState)-1;

static void updateHeaderStatus(bool force = false) {
  if (!force && g_data.connection == lastHeaderConn) return;
  lastHeaderConn = g_data.connection;

  gfx->fillRect(300, 0, 180, HEADER_H, currentTheme->header);
  const char* label = "OFFLINE";
  uint16_t dotColor = currentTheme->textSecondary;
  if (g_data.connection == CONN_CONNECTING) { label = "CONNECTING"; dotColor = currentTheme->warning; }
  else if (g_data.connection == CONN_CONNECTED) { label = "CONNECTED"; dotColor = currentTheme->accent; }

  int16_t tw = Cards::textWidth(label, 1);
  int16_t dotX = 468, dotY = 13;
  Cards::leftText(dotX - 8 - tw, 9, label, 1, currentTheme->textSecondary);
  gfx->fillCircle(dotX, dotY, 6, dotColor);
}

static char lastDoseBuf[16] = "";

static void updateDoseCard(bool force = false) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.2f", g_data.doseRateUSvH);
  if (!force && strcmp(buf, lastDoseBuf) == 0) return;
  strncpy(lastDoseBuf, buf, sizeof(lastDoseBuf) - 1);
  lastDoseBuf[sizeof(lastDoseBuf) - 1] = '\0';
  Cards::statValueArea(DOSE_X + 10, DOSE_Y + 26, DOSE_W - 20, 36, buf, "uSv/h",
                       doseColor(g_data.doseRateUSvH));
  drawDoseScale(DOSE_X + 10, DOSE_Y + 68, DOSE_W - 20, 6, g_data.doseRateUSvH);
}

static char lastCpsBuf[16] = "";

static void updateCpsCard(bool force = false) {
  char buf[16];
  // One decimal below 100 CPS so slow background changes are still visible.
  if (g_data.cps < 100.0f) snprintf(buf, sizeof(buf), "%.1f", g_data.cps);
  else                     snprintf(buf, sizeof(buf), "%.0f", g_data.cps);
  if (!force && strcmp(buf, lastCpsBuf) == 0) return;
  strncpy(lastCpsBuf, buf, sizeof(lastCpsBuf) - 1);
  lastCpsBuf[sizeof(lastCpsBuf) - 1] = '\0';
  Cards::statValueArea(CPM_X + 10, CPM_Y + 30, CPM_W - 20, 40, buf, "cps", currentTheme->text);
}

static char lastSourceSymbol[16] = "";
static int lastSourceConfidencePct = -1;
static SourceClass lastSourceClass = (SourceClass)-1;

// Shows the sample-level verdict ("URANIUM ORE") rather than a single
// daughter nuclide. For natural samples the individual nuclides are the
// evidence, not the answer — nobody reading a rock wants to be told
// "Bi-214". The nuclide breakdown lives on the Isotopes screen.
static void updateSourceCard(bool force = false) {
  int confidencePct = g_data.candidateCount > 0 ? (int)(g_data.candidates[0].confidence * 100.0f) : -1;
  const char* symbol = g_data.candidateCount > 0 ? g_data.candidates[0].symbol : "";
  if (!force && confidencePct == lastSourceConfidencePct &&
      g_data.sourceClass == lastSourceClass && strcmp(symbol, lastSourceSymbol) == 0) {
    return;
  }
  lastSourceConfidencePct = confidencePct;
  lastSourceClass = g_data.sourceClass;
  strncpy(lastSourceSymbol, symbol, sizeof(lastSourceSymbol) - 1);
  lastSourceSymbol[sizeof(lastSourceSymbol) - 1] = '\0';

  gfx->fillRect(SRC_X + 10, SRC_Y + 26, SRC_W - 20, SRC_H - 34, currentTheme->card);

  if (g_data.sourceClass == SRC_INSUFFICIENT && g_data.candidateCount == 0) {
    Cards::leftText(SRC_X + 10, SRC_Y + 40, "No source", 2, currentTheme->textSecondary);
    return;
  }

  uint16_t color = currentTheme->highlight;
  if (g_data.sourceClass == SRC_ARTIFICIAL) color = currentTheme->danger;
  else if (g_data.sourceClass == SRC_BACKGROUND) color = currentTheme->accent;
  else if (g_data.sourceClass == SRC_INSUFFICIENT) color = currentTheme->textSecondary;

  Cards::leftText(SRC_X + 10, SRC_Y + 28, sourceClassLabel(g_data.sourceClass), 2, color);
  Cards::leftText(SRC_X + 10, SRC_Y + 50, sourceClassDetail(g_data.sourceClass), 1,
                  currentTheme->textSecondary);

  if (g_data.candidateCount > 0) {
    const IsotopeCandidate& top = g_data.candidates[0];
    char line[32];
    snprintf(line, sizeof(line), "top: %s  %.0f%%", top.symbol, top.confidence * 100.0f);
    Cards::leftText(SRC_X + 10, SRC_Y + 66, line, 1, currentTheme->textSecondary);
  }
}

static int lastBattery = -2;
static char lastLiveBuf[24] = "";

static void updateStatusCard(bool force = false) {
  if (force || g_data.batteryPercent != lastBattery) {
    lastBattery = g_data.batteryPercent;
    gfx->fillRect(STAT_X + 10, STAT_Y + 26, STAT_W - 20, 20, currentTheme->card);
    char batBuf[16];
    if (g_data.batteryPercent >= 0) snprintf(batBuf, sizeof(batBuf), "BAT %d%%", g_data.batteryPercent);
    else snprintf(batBuf, sizeof(batBuf), "BAT --");
    Cards::leftText(STAT_X + 10, STAT_Y + 28, batBuf, 2, currentTheme->text);
  }

  char timeBuf[16];
  formatTime(g_data.liveTimeSeconds, timeBuf, sizeof(timeBuf));
  char liveBuf[24];
  snprintf(liveBuf, sizeof(liveBuf), "LIVE %s", timeBuf);
  if (force || strcmp(liveBuf, lastLiveBuf) != 0) {
    strncpy(lastLiveBuf, liveBuf, sizeof(lastLiveBuf));
    gfx->fillRect(STAT_X + 10, STAT_Y + 50, STAT_W - 20, 16, currentTheme->card);
    Cards::leftText(STAT_X + 10, STAT_Y + 52, liveBuf, 2, currentTheme->textSecondary);
  }
}

static ConnectionState lastButtonConn = (ConnectionState)-1;
static bool lastReconnecting = false;
static bool lastResetArmed = false;

static void updateButton(bool force = false) {
  bool reconnecting = BleSource::reconnectPending();
  bool armed = resetArmed && millis() < resetArmedUntil;
  if (!force && g_data.connection == lastButtonConn &&
      reconnecting == lastReconnecting && armed == lastResetArmed) {
    return;
  }
  lastButtonConn = g_data.connection;
  lastReconnecting = reconnecting;
  lastResetArmed = armed;

  const char* label = "CONNECT";
  uint16_t color = currentTheme->accent;
  if (g_data.connection == CONN_CONNECTING) { label = "CONNECTING..."; color = currentTheme->warning; }
  else if (g_data.connection == CONN_CONNECTED) { label = "DISCONNECT"; color = currentTheme->danger; }
  else if (reconnecting) { label = "RECONNECTING..."; color = currentTheme->warning; }
  Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, label, color);

  // Two-tap, because this is the one action that throws away a run.
  Cards::button(RST_X, BTN_Y, RST_W, BTN_H, armed ? "SURE?" : "RESET",
                armed ? currentTheme->danger : currentTheme->card, true, armed);
  Cards::button(SET_X, BTN_Y, SET_W, BTN_H, "SET", currentTheme->card);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  updateHeaderStatus(/* force */ true);

  Cards::frame(DOSE_X, DOSE_Y, DOSE_W, DOSE_H, "DOSE RATE");
  Cards::frame(CPM_X, CPM_Y, CPM_W, CPM_H, "COUNT RATE");
  Cards::frame(SRC_X, SRC_Y, SRC_W, SRC_H, "LIKELY SOURCE");
  Cards::frame(STAT_X, STAT_Y, STAT_W, STAT_H, "DEVICE STATUS");

  // force=true: the frames above just cleared each card's content area, so
  // content must be redrawn even if it's identical to what was cached from
  // before this screen was last shown (diff-caching is for update(), not draw()).
  updateDoseCard(true);
  updateCpsCard(true);
  updateSourceCard(true);
  updateStatusCard(true);
  updateButton(true);
}

void update() {
  // The model name only becomes known once the device identifies itself, so
  // repaint the title when it changes.
  static char lastModel[20] = "";
  if (strcmp(lastModel, g_data.deviceModel) != 0) {
    strncpy(lastModel, g_data.deviceModel, sizeof(lastModel) - 1);
    lastModel[sizeof(lastModel) - 1] = '\0';
    drawHeader();
    updateHeaderStatus(true);
  }
  updateHeaderStatus();
  updateDoseCard();
  updateCpsCard();
  updateSourceCard();
  updateStatusCard();
  updateButton();
}

bool handleTap(int16_t x, int16_t y, bool* settingsPressed) {
  if (settingsPressed) *settingsPressed = false;
  if (y < BTN_Y || y > BTN_Y + BTN_H) return false;

  if (x >= SET_X && x <= SET_X + SET_W) {
    if (settingsPressed) *settingsPressed = true;
    return true;
  }

  if (x >= RST_X && x <= RST_X + RST_W) {
    if (resetArmed && millis() < resetArmedUntil) {
      resetArmed = false;
      BleSource::resetSpectrum();
      updateButton(true);
      updateSourceCard(true);
      updateStatusCard(true);
    } else {
      resetArmed = true;
      resetArmedUntil = millis() + 5000;
      updateButton(true);
    }
    return true;
  }

  if (x < BTN_X || x > BTN_X + BTN_W) return false;

  if (g_data.connection == CONN_DISCONNECTED) {
    // BleSource::requestConnect() blocks for a few seconds (BLE scan + connect
    // + handshake — see CLAUDE.md). Paint the "connecting" state immediately,
    // before making that blocking call, so the tap has visible feedback.
    Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, "CONNECTING...", currentTheme->warning);
    BleSource::requestConnect();
  } else if (g_data.connection == CONN_CONNECTED) {
    BleSource::requestDisconnect();
  }
  // force=true: the manual "CONNECTING..." draw above bypassed the button's
  // cache, so the cache can't be trusted to reflect what's on screen now.
  updateHeaderStatus(true);
  updateButton(true);
  return true;
}

} // namespace ScreenHome
