#include "screen_import.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/sd_storage.h"
#include "../data/rc_import.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenImport {

static const int16_t HEADER_H = 26;
static const int16_t ROW_H = 24;
static const int16_t LIST_Y = 32;
static const int VISIBLE_ROWS = 5;

static const int16_t BTN_W = 150, BTN_H = 40, BTN_Y = 158;
static const int16_t SLOT_X = 8, PAGE_X = 165, BACK_X = 322;

static const int16_t MSG_Y = 206;
static const int16_t HELP_Y = 246;

static const char* SLOTS[] = {"HOME", "CAR", "BASEMENT", "GARDEN", "FIELD", "SPARE"};
static const int SLOT_COUNT = 6;

static RcImport::FileEntry files[RcImport::MAX_FILES];
static int fileCount = 0;
static int slotIndex = 0;
static int pageStart = 0;

static char msg1[64] = "";
static char msg2[64] = "";
static uint16_t msgColor;

// A 24-hour reference reads as "87713s" otherwise, which is unreadable.
static const char* durationText(uint32_t s) {
  static char buf[16];
  if (s >= 3600) snprintf(buf, sizeof(buf), "%uh%02um", (unsigned)(s / 3600),
                          (unsigned)((s % 3600) / 60));
  else if (s >= 60) snprintf(buf, sizeof(buf), "%um%02us", (unsigned)(s / 60),
                             (unsigned)(s % 60));
  else snprintf(buf, sizeof(buf), "%us", (unsigned)s);
  return buf;
}

static void rescan() {
  fileCount = SdStorage::available() ? RcImport::scan(files, RcImport::MAX_FILES) : 0;
  if (pageStart >= fileCount) pageStart = 0;
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Import RadiaCode File", 2, currentTheme->text);
  char chip[24];
  snprintf(chip, sizeof(chip), "%d found", fileCount);
  int16_t w = Cards::textWidth(chip, 1);
  Cards::leftText(480 - 12 - w, 10, chip, 1,
                  fileCount ? currentTheme->accent : currentTheme->textSecondary);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

static void drawList() {
  gfx->fillRect(0, LIST_Y, 480, VISIBLE_ROWS * ROW_H + 4, currentTheme->background);

  if (!SdStorage::available()) {
    Cards::leftText(12, LIST_Y + 20, "No SD card inserted.", 2, currentTheme->textSecondary);
    return;
  }
  if (fileCount == 0) {
    Cards::leftText(12, LIST_Y + 8, "No .xml files on the card.", 2, currentTheme->textSecondary);
    Cards::leftText(12, LIST_Y + 34,
                    "Copy RadiaCode exports into /import (or the card root),", 1,
                    currentTheme->textSecondary);
    Cards::leftText(12, LIST_Y + 46, "then tap RESCAN.", 1, currentTheme->textSecondary);
    return;
  }

  for (int i = 0; i < VISIBLE_ROWS; i++) {
    int idx = pageStart + i;
    if (idx >= fileCount) break;
    int16_t y = LIST_Y + i * ROW_H;
    gfx->fillRoundRect(8, y, 464, ROW_H - 3, 4, currentTheme->card);
    gfx->drawRoundRect(8, y, 464, ROW_H - 3, 4, currentTheme->cardBorder);

    char label[40];
    strncpy(label, files[idx].label, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
    // Keep long names from colliding with the size column.
    if (Cards::textWidth(label, 1) > 330) {
      label[54] = '\0';
      for (int k = 54; k > 4 && Cards::textWidth(label, 1) > 330; k--) label[k] = '\0';
    }
    Cards::leftText(16, y + 6, label, 1, currentTheme->text);

    char sz[16];
    snprintf(sz, sizeof(sz), "%u KB", (unsigned)(files[idx].size / 1024));
    int16_t sw = Cards::textWidth(sz, 1);
    Cards::leftText(472 - 8 - sw, y + 6, sz, 1, currentTheme->textSecondary);
  }
}

static void drawButtons(int pressed = -1) {
  char slotLabel[24];
  snprintf(slotLabel, sizeof(slotLabel), "TO: %s", SLOTS[slotIndex]);
  Cards::button(SLOT_X, BTN_Y, BTN_W, BTN_H, slotLabel, currentTheme->cool, true, pressed == 0);

  bool paged = fileCount > VISIBLE_ROWS;
  Cards::button(PAGE_X, BTN_Y, BTN_W, BTN_H, paged ? "NEXT PAGE" : "RESCAN",
                currentTheme->card, true, pressed == 1);
  Cards::button(BACK_X, BTN_Y, BTN_W, BTN_H, "BACK", currentTheme->card, true, pressed == 2);
}

static void drawMessages() {
  gfx->fillRect(0, MSG_Y, 480, 272 - MSG_Y, currentTheme->background);
  if (msg1[0]) Cards::leftText(12, MSG_Y, msg1, 1, msgColor);
  if (msg2[0]) Cards::leftText(12, MSG_Y + 13, msg2, 1, currentTheme->textSecondary);
  Cards::leftText(12, HELP_Y, "Tap a file to convert it into the selected slot.", 1,
                  currentTheme->textSecondary);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  rescan();
  drawHeader();
  drawList();
  drawButtons();
  drawMessages();
}

void update() {
  // Static listing; nothing changes without a tap.
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void doImport(int idx) {
  snprintf(msg1, sizeof(msg1), "Importing %s...", files[idx].label);
  msgColor = currentTheme->textSecondary;
  msg2[0] = '\0';
  drawMessages();

  RcImport::Result r = RcImport::importToSlot(files[idx].path, SLOTS[slotIndex], g_data);

  if (!r.ok) {
    snprintf(msg1, sizeof(msg1), "Failed: %s", r.error);
    msgColor = currentTheme->danger;
    msg2[0] = '\0';
    drawMessages();
    return;
  }

  // Importing a background file means "I want to use this". Stopping at
  // "file written" left the user with no visible change and two more hidden
  // steps on another screen, so activate it here and say so plainly.
  bool activated = SdStorage::loadBackgroundRef(SLOTS[slotIndex]);

  if (activated) {
    snprintf(msg1, sizeof(msg1), "ACTIVE: %s  -  %s, %.1f cps, subtraction ON",
             SLOTS[slotIndex], durationText(r.durationSec),
             r.durationSec ? (float)r.totalCounts / (float)r.durationSec : 0.0f);
    msgColor = currentTheme->accent;
  } else {
    snprintf(msg1, sizeof(msg1), "Saved to %s but could not activate it.", SLOTS[slotIndex]);
    msgColor = currentTheme->warning;
  }

  if (r.calibMismatch) {
    // Worth shouting about: a reference recorded under a different energy
    // calibration subtracts counts from the wrong channels.
    snprintf(msg2, sizeof(msg2),
             "WARNING calib a1=%.3f vs device %.3f - energies will not line up",
             r.a1, g_data.calibA1);
    msgColor = currentTheme->warning;
  } else if (!g_data.calibValid) {
    // Say so rather than imply the calibration was verified.
    snprintf(msg2, sizeof(msg2), "Calibration unchecked (not connected). File a1=%.3f", r.a1);
  } else {
    snprintf(msg2, sizeof(msg2), "%u counts. Nothing else to do - it is in use now.",
             (unsigned)r.totalCounts);
  }
  drawMessages();
}

bool handleTap(int16_t x, int16_t y, bool* backPressed) {
  if (backPressed) *backPressed = false;

  if (inRect(x, y, BACK_X, BTN_Y, BTN_W, BTN_H)) {
    drawButtons(2);
    delay(110);
    if (backPressed) *backPressed = true;
    return true;
  }
  if (inRect(x, y, SLOT_X, BTN_Y, BTN_W, BTN_H)) {
    drawButtons(0);
    delay(110);
    slotIndex = (slotIndex + 1) % SLOT_COUNT;
    drawButtons();
    return true;
  }
  if (inRect(x, y, PAGE_X, BTN_Y, BTN_W, BTN_H)) {
    drawButtons(1);
    delay(110);
    if (fileCount > VISIBLE_ROWS) {
      pageStart += VISIBLE_ROWS;
      if (pageStart >= fileCount) pageStart = 0;
    } else {
      rescan();
      drawHeader();
    }
    drawList();
    drawButtons();
    return true;
  }

  for (int i = 0; i < VISIBLE_ROWS; i++) {
    int idx = pageStart + i;
    if (idx >= fileCount) break;
    int16_t ry = LIST_Y + i * ROW_H;
    if (inRect(x, y, 8, ry, 464, ROW_H - 3)) {
      doImport(idx);
      return true;
    }
  }
  return false;
}

} // namespace ScreenImport
