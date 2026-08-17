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
static const int16_t BTN_X  = 8,   BTN_Y  = 216, BTN_W  = 464, BTN_H  = 44;

static uint16_t doseColor(float uSvH) {
  if (uSvH >= 1.0f) return currentTheme->danger;
  if (uSvH >= 0.30f) return currentTheme->warning;
  return currentTheme->accent;
}

static void formatTime(uint32_t totalSeconds, char* buf, size_t bufSize) {
  uint32_t h = totalSeconds / 3600;
  uint32_t m = (totalSeconds / 60) % 60;
  snprintf(buf, bufSize, "%02u:%02u", (unsigned)h, (unsigned)m);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "RadiaCode110", 2, currentTheme->text);
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
  strncpy(lastDoseBuf, buf, sizeof(lastDoseBuf));
  Cards::statValueArea(DOSE_X + 10, DOSE_Y + 30, DOSE_W - 20, 40, buf, "uSv/h", doseColor(g_data.doseRateUSvH));
}

static char lastCpmBuf[16] = "";

static void updateCpmCard(bool force = false) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", g_data.cpm);
  if (!force && strcmp(buf, lastCpmBuf) == 0) return;
  strncpy(lastCpmBuf, buf, sizeof(lastCpmBuf));
  Cards::statValueArea(CPM_X + 10, CPM_Y + 30, CPM_W - 20, 40, buf, "cpm", currentTheme->text);
}

static char lastSourceSymbol[16] = "";
static int lastSourceConfidencePct = -1;

static void updateSourceCard(bool force = false) {
  int confidencePct = g_data.candidateCount > 0 ? (int)(g_data.candidates[0].confidence * 100.0f) : -1;
  const char* symbol = g_data.candidateCount > 0 ? g_data.candidates[0].symbol : "";
  if (!force && confidencePct == lastSourceConfidencePct && strcmp(symbol, lastSourceSymbol) == 0) return;
  lastSourceConfidencePct = confidencePct;
  strncpy(lastSourceSymbol, symbol, sizeof(lastSourceSymbol));

  gfx->fillRect(SRC_X + 10, SRC_Y + 26, SRC_W - 20, SRC_H - 34, currentTheme->card);
  if (g_data.candidateCount == 0) {
    Cards::leftText(SRC_X + 10, SRC_Y + 40, "No source", 2, currentTheme->textSecondary);
    return;
  }
  const IsotopeCandidate& top = g_data.candidates[0];
  Cards::leftText(SRC_X + 10, SRC_Y + 26, top.symbol, 3, currentTheme->highlight);

  char pct[8];
  snprintf(pct, sizeof(pct), "%.0f%%", top.confidence * 100.0f);
  Cards::leftText(SRC_X + 10 + Cards::textWidth(top.symbol, 3) + 10, SRC_Y + 34, pct, 2, currentTheme->textSecondary);

  Cards::leftText(SRC_X + 10, SRC_Y + 72, top.name, 1, currentTheme->textSecondary);
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

static void updateButton(bool force = false) {
  if (!force && g_data.connection == lastButtonConn) return;
  lastButtonConn = g_data.connection;

  const char* label = "CONNECT";
  uint16_t color = currentTheme->accent;
  if (g_data.connection == CONN_CONNECTING) { label = "CONNECTING..."; color = currentTheme->warning; }
  else if (g_data.connection == CONN_CONNECTED) { label = "DISCONNECT"; color = currentTheme->danger; }
  Cards::button(BTN_X, BTN_Y, BTN_W, BTN_H, label, color);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  updateHeaderStatus(/* force */ true);

  Cards::frame(DOSE_X, DOSE_Y, DOSE_W, DOSE_H, "DOSE RATE");
  Cards::frame(CPM_X, CPM_Y, CPM_W, CPM_H, "COUNT RATE");
  Cards::frame(SRC_X, SRC_Y, SRC_W, SRC_H, "LIKELY SOURCE");
  Cards::frame(STAT_X, STAT_Y, STAT_W, STAT_H, "RC-110 STATUS");

  // force=true: the frames above just cleared each card's content area, so
  // content must be redrawn even if it's identical to what was cached from
  // before this screen was last shown (diff-caching is for update(), not draw()).
  updateDoseCard(true);
  updateCpmCard(true);
  updateSourceCard(true);
  updateStatusCard(true);
  updateButton(true);
}

void update() {
  updateHeaderStatus();
  updateDoseCard();
  updateCpmCard();
  updateSourceCard();
  updateStatusCard();
  updateButton();
}

bool handleTap(int16_t x, int16_t y) {
  if (x < BTN_X || x > BTN_X + BTN_W || y < BTN_Y || y > BTN_Y + BTN_H) return false;

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
