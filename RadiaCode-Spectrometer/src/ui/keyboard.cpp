#include "keyboard.h"
#include "cards.h"
#include "ui_theme.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace Keyboard {

static const int16_t HEADER_H = 24;
static const int16_t FIELD_Y = 28, FIELD_H = 30;

// 4 rows of keys. Row 3 carries shift and backspace, row 4 the space bar and
// the accept/cancel actions, so everything fits 480x272 with keys large
// enough to hit reliably.
static const char* ROW_LOWER[4] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm"
};
static const char* ROW_UPPER[4] = {
  "!@#$%^&*()",
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNM"
};
// Extra symbols reachable via the shift layer's number row plus these.
static const char* SYMBOLS = "-_.:/+=?,';\"\\|<>[]{}~`";

static const int16_t KEY_H = 34;
static const int16_t ROW_Y[4] = {66, 104, 142, 180};
static const int16_t KEY_W = 44;
static const int16_t KEY_GAP = 4;

static const int16_t BOT_Y = 220, BOT_H = 40;

static char buf[80];
static size_t maxLen = 63;
static bool open_ = false, shift = false, masked = false, symbols = false;
static bool accepted = false, cancelled = false;
static char titleBuf[32];

static const char* rowChars(int r) {
  if (symbols && r == 0) return SYMBOLS;
  return shift ? ROW_UPPER[r] : ROW_LOWER[r];
}

static int16_t rowStartX(int r) {
  int n = (int)strlen(rowChars(r));
  int16_t w = n * KEY_W + (n - 1) * KEY_GAP;
  int16_t x = (480 - w) / 2;
  return x < 4 ? 4 : x;
}

void open(const char* title, const char* initial, bool mask, size_t maxLength) {
  snprintf(titleBuf, sizeof(titleBuf), "%s", title ? title : "Input");
  strncpy(buf, initial ? initial : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  maxLen = (maxLength < sizeof(buf) - 1) ? maxLength : sizeof(buf) - 1;
  masked = mask;
  shift = false;
  symbols = false;
  open_ = true;
  accepted = cancelled = false;
  draw();
}

bool isOpen() { return open_; }
bool wasAccepted() { return accepted; }
bool wasCancelled() { return cancelled; }
void consumeResult() { accepted = cancelled = false; }
const char* text() { return buf; }

static void drawField() {
  gfx->fillRect(0, FIELD_Y, 480, FIELD_H, currentTheme->background);
  gfx->drawRoundRect(8, FIELD_Y, 464, FIELD_H - 2, 6, currentTheme->cardBorder);

  char shown[80];
  if (masked) {
    size_t n = strlen(buf);
    if (n > sizeof(shown) - 1) n = sizeof(shown) - 1;
    memset(shown, '*', n);
    shown[n] = '\0';
  } else {
    strncpy(shown, buf, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = '\0';
  }

  // Keep the caret visible on long entries by showing the tail.
  const char* p = shown;
  while (Cards::textWidth(p, 2) > 440 && *p) p++;
  Cards::leftText(16, FIELD_Y + 7, p, 2, currentTheme->text);
  int16_t cx = 16 + Cards::textWidth(p, 2);
  if (cx < 466) gfx->fillRect(cx + 2, FIELD_Y + 6, 2, 16, currentTheme->accent);
}

static void drawKey(int16_t x, int16_t y, int16_t w, int16_t h,
                    const char* label, uint16_t fill, uint8_t size = 2) {
  gfx->fillRoundRect(x, y, w, h, 5, fill);
  gfx->drawRoundRect(x, y, w, h, 5, currentTheme->cardBorder);
  int16_t tw = Cards::textWidth(label, size);
  Cards::leftText(x + (w - tw) / 2, y + (h - size * 8) / 2, label, size, currentTheme->text);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 5, titleBuf, 2, currentTheme->text);
  drawField();

  for (int r = 0; r < 4; r++) {
    const char* chars = rowChars(r);
    int16_t x = rowStartX(r);
    for (int i = 0; chars[i]; i++) {
      char lbl[2] = {chars[i], '\0'};
      drawKey(x, ROW_Y[r], KEY_W, KEY_H, lbl, currentTheme->card);
      x += KEY_W + KEY_GAP;
    }
  }

  // Right-hand modifier column on rows 2 and 3.
  drawKey(430, ROW_Y[2], 46, KEY_H, "DEL", currentTheme->warm, 1);
  drawKey(430, ROW_Y[3], 46, KEY_H, shift ? "abc" : "ABC", currentTheme->cool, 1);
  drawKey(4,   ROW_Y[3], 46, KEY_H, symbols ? "123" : "#+=", currentTheme->cool, 1);

  drawKey(8,   BOT_Y, 150, BOT_H, "SPACE", currentTheme->card, 1);
  drawKey(170, BOT_Y, 140, BOT_H, "CANCEL", currentTheme->danger, 2);
  drawKey(322, BOT_Y, 150, BOT_H, "OK", currentTheme->accent, 2);
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void append(char c) {
  size_t n = strlen(buf);
  if (n >= maxLen) return;
  buf[n] = c;
  buf[n + 1] = '\0';
  drawField();
}

bool handleTap(int16_t x, int16_t y) {
  if (!open_) return false;

  if (inRect(x, y, 322, BOT_Y, 150, BOT_H)) { open_ = false; accepted = true; return true; }
  if (inRect(x, y, 170, BOT_Y, 140, BOT_H)) { open_ = false; cancelled = true; return true; }
  if (inRect(x, y, 8, BOT_Y, 150, BOT_H)) { append(' '); return true; }

  if (inRect(x, y, 430, ROW_Y[2], 46, KEY_H)) {
    size_t n = strlen(buf);
    if (n > 0) { buf[n - 1] = '\0'; drawField(); }
    return true;
  }
  if (inRect(x, y, 430, ROW_Y[3], 46, KEY_H)) { shift = !shift; draw(); return true; }
  if (inRect(x, y, 4, ROW_Y[3], 46, KEY_H)) { symbols = !symbols; draw(); return true; }

  for (int r = 0; r < 4; r++) {
    if (y < ROW_Y[r] || y > ROW_Y[r] + KEY_H) continue;
    const char* chars = rowChars(r);
    int16_t kx = rowStartX(r);
    for (int i = 0; chars[i]; i++) {
      if (x >= kx && x <= kx + KEY_W) { append(chars[i]); return true; }
      kx += KEY_W + KEY_GAP;
    }
  }
  return true; // swallow stray taps while the keyboard owns the screen
}

} // namespace Keyboard
