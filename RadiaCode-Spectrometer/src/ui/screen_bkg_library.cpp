#include "screen_bkg_library.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/background.h"
#include "../data/sd_storage.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenBkgLibrary {

// Fixed slot names: there is no keyboard on this device, and a handful of
// location presets covers the actual use case (a reference per place you
// measure in) without inventing a text-entry UI.
static const char* SLOT_NAMES[] = {"HOME", "CAR", "BASEMENT", "GARDEN", "FIELD", "SPARE"};
static const int SLOT_COUNT = 6;

static const int16_t HEADER_H = 26;
static const int16_t SLOT_W = 150, SLOT_H = 62;
static const int16_t COL_X[3] = {8, 165, 322};
static const int16_t ROW_Y[2] = {34, 102};

static const int16_t BTN_W = 150, BTN_H = 40, BTN_Y = 172;
static const int16_t MODE_X = 8, IMPORT_X = 165, BACK_X = 322;

static const int16_t TOAST_Y = 220;
static const int16_t HELP_Y = 236;

static bool saveMode = false; // false = LOAD, true = SAVE
static char toast[64] = "";
static unsigned long toastUntil = 0;

static void setToast(const char* m) {
  strncpy(toast, m, sizeof(toast) - 1);
  toast[sizeof(toast) - 1] = '\0';
  toastUntil = millis() + 4500;
}

static void fmtDur(uint32_t s, char* b, size_t n) {
  if (s >= 3600) snprintf(b, n, "%uh%02um", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
  else if (s >= 60) snprintf(b, n, "%um%02us", (unsigned)(s / 60), (unsigned)(s % 60));
  else snprintf(b, n, "%us", (unsigned)s);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Background Library", 2, currentTheme->text);
  const char* chip = saveMode ? "SAVE MODE" : "LOAD MODE";
  uint16_t cc = saveMode ? currentTheme->warm : currentTheme->accent;
  int16_t w = Cards::textWidth(chip, 1);
  Cards::leftText(480 - 12 - w, 10, chip, 1, cc);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

static void drawSlot(int i) {
  int16_t x = COL_X[i % 3];
  int16_t y = ROW_Y[i / 3];

  uint32_t dur = 0, tot = 0;
  bool occupied = SdStorage::refInfo(SLOT_NAMES[i], &dur, &tot);
  // Highlight whichever slot is currently driving the subtraction, so the
  // screen answers "which one am I using?" at a glance.
  bool isActive = Background::available() &&
                  strcmp(Background::label(), SLOT_NAMES[i]) == 0;

  Cards::frame(x, y, SLOT_W, SLOT_H, nullptr, isActive);

  Cards::leftText(x + 10, y + 8, SLOT_NAMES[i], 2,
                  isActive ? currentTheme->highlight
                           : (occupied ? currentTheme->text : currentTheme->textSecondary));

  if (isActive) {
    const char* tag = Background::enabled() ? "IN USE" : "LOADED";
    int16_t tw = Cards::textWidth(tag, 1);
    Cards::leftText(x + SLOT_W - 10 - tw, y + 12, tag, 1, currentTheme->highlight);
  }

  if (occupied) {
    char d[16];
    fmtDur(dur, d, sizeof(d));
    char line[32];
    snprintf(line, sizeof(line), "%s ref", d);
    Cards::leftText(x + 10, y + 30, line, 1, currentTheme->accent);
    snprintf(line, sizeof(line), "%u counts", (unsigned)tot);
    Cards::leftText(x + 10, y + 44, line, 1, currentTheme->textSecondary);
  } else {
    Cards::leftText(x + 10, y + 34, "empty", 1, currentTheme->textSecondary);
  }
}

static void drawButtons(int pressed = -1) {
  Cards::button(MODE_X, BTN_Y, BTN_W, BTN_H, saveMode ? "MODE: SAVE" : "MODE: LOAD",
                saveMode ? currentTheme->warm : currentTheme->accent, true, pressed == 0);
  Cards::button(IMPORT_X, BTN_Y, BTN_W, BTN_H, "IMPORT XML",
                SdStorage::available() ? currentTheme->cool : currentTheme->card,
                SdStorage::available(), pressed == 2);
  Cards::button(BACK_X, BTN_Y, BTN_W, BTN_H, "BACK", currentTheme->card, true, pressed == 1);
}

static void drawBottom() {
  gfx->fillRect(0, TOAST_Y, 480, 272 - TOAST_Y, currentTheme->background);
  if (toast[0] && millis() < toastUntil) {
    Cards::leftText(12, TOAST_Y, toast, 1, currentTheme->highlight);
  }
  const char* help = saveMode
    ? "Tap a slot to store the current reference there (overwrites)."
    : "Tap a filled slot to make it the active reference.";
  Cards::leftText(12, HELP_Y, help, 1, currentTheme->textSecondary);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  if (!SdStorage::available()) {
    Cards::centeredText(240, 100, "No SD card", 3, currentTheme->textSecondary);
    Cards::centeredText(240, 130, "The library needs a card; the active reference", 1,
                        currentTheme->textSecondary);
    Cards::centeredText(240, 142, "still lives in internal flash.", 1, currentTheme->textSecondary);
    drawButtons();
    return;
  }
  for (int i = 0; i < SLOT_COUNT; i++) drawSlot(i);
  drawButtons();
  drawBottom();
}

void update() {
  // Slot contents only change in response to taps, so nothing to poll —
  // just let a toast expire.
  static bool hadToast = false;
  bool has = toast[0] && millis() < toastUntil;
  if (hadToast && !has) { toast[0] = '\0'; drawBottom(); }
  hadToast = has;
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

bool handleTap(int16_t x, int16_t y, bool* backPressed, bool* importPressed) {
  if (backPressed) *backPressed = false;
  if (importPressed) *importPressed = false;

  if (inRect(x, y, IMPORT_X, BTN_Y, BTN_W, BTN_H)) {
    if (!SdStorage::available()) { setToast("No SD card."); drawBottom(); return true; }
    drawButtons(2);
    delay(110);
    if (importPressed) *importPressed = true;
    return true;
  }
  if (inRect(x, y, BACK_X, BTN_Y, BTN_W, BTN_H)) {
    drawButtons(1);
    delay(110);
    if (backPressed) *backPressed = true;
    return true;
  }
  if (inRect(x, y, MODE_X, BTN_Y, BTN_W, BTN_H)) {
    drawButtons(0);
    delay(110);
    saveMode = !saveMode;
    draw();
    return true;
  }
  if (!SdStorage::available()) return false;

  for (int i = 0; i < SLOT_COUNT; i++) {
    int16_t sx = COL_X[i % 3], sy = ROW_Y[i / 3];
    if (!inRect(x, y, sx, sy, SLOT_W, SLOT_H)) continue;

    if (saveMode) {
      if (!Background::available()) setToast("No active reference to save.");
      else if (SdStorage::saveBackgroundRef(SLOT_NAMES[i])) {
        char m[64];
        snprintf(m, sizeof(m), "Saved current reference to %s.", SLOT_NAMES[i]);
        setToast(m);
      } else setToast("Write failed - check the card.");
    } else {
      if (!SdStorage::refExists(SLOT_NAMES[i])) setToast("That slot is empty.");
      else if (SdStorage::loadBackgroundRef(SLOT_NAMES[i])) {
        char m[64];
        snprintf(m, sizeof(m), "%s is now the active reference.", SLOT_NAMES[i]);
        setToast(m);
      } else setToast("Could not read that reference.");
    }
    draw();
    return true;
  }
  return false;
}

} // namespace ScreenBkgLibrary
