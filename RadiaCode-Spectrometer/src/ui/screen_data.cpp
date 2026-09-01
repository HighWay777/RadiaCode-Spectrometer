#include "screen_data.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/background.h"
#include "../data/sd_storage.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenData {

static const int16_t HEADER_H = 26;
static const int16_t PANEL_X = 8, PANEL_Y = 30, PANEL_W = 464, PANEL_H = 82;
static const int16_t ROW_A = PANEL_Y + 8;
static const int16_t ROW_B = PANEL_Y + 30;
static const int16_t ROW_C = PANEL_Y + 48;
static const int16_t ROW_D = PANEL_Y + 62;

static const int16_t PRI_X = 8, PRI_Y = 120, PRI_W = 464, PRI_H = 44;
static const int16_t SEC_W = 228, SEC_H = 40, SEC_Y = 170;
static const int16_t SEC1_X = 8, SEC2_X = 244;

static const int16_t TOAST_Y = 218;
static const int16_t HELP_Y = 234;
static const int16_t HELP2_Y = 248;

static char toast[64] = "";
static unsigned long toastUntil = 0;

static char cA[48], cB[56], cC[64], cD[64], cToast[64];
static char cPri[32], cSec1[32], cSec2[32];
static bool cPriEn = false, cSec1En = false, cSec2En = false;

static void setToast(const char* m) {
  strncpy(toast, m, sizeof(toast) - 1);
  toast[sizeof(toast) - 1] = '\0';
  toastUntil = millis() + 5000;
}

static void row(int16_t y, int16_t h, const char* text, uint8_t size, uint16_t color,
                char* cache, size_t cn, bool inPanel) {
  if (strcmp(text, cache) == 0) return;
  strncpy(cache, text, cn - 1);
  cache[cn - 1] = '\0';
  if (inPanel) gfx->fillRect(PANEL_X + 6, y, PANEL_W - 12, h, currentTheme->card);
  else         gfx->fillRect(0, y, 480, h, currentTheme->background);
  if (text[0]) Cards::leftText(inPanel ? PANEL_X + 12 : 12, y, text, size, color);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Data & Storage", 2, currentTheme->text);
  const char* chip = SdStorage::available() ? "SD READY" : "NO SD";
  uint16_t cc = SdStorage::available() ? currentTheme->accent : currentTheme->textSecondary;
  int16_t w = Cards::textWidth(chip, 1);
  Cards::leftText(480 - 12 - w, 10, chip, 1, cc);
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

// Formatting wipes the card, so it takes two taps: the first arms it and
// relabels the button, the second commits. Arming times out.
static bool formatArmed = false;
static unsigned long formatArmedUntil = 0;

static void refreshPanel() {
  char a[48], b[56], c[64], d[64];

  if (SdStorage::cardPresentButUnreadable()) {
    snprintf(a, sizeof(a), "CARD UNREADABLE");
    snprintf(b, sizeof(b), "Filesystem not FAT32 - probably exFAT");
    snprintf(c, sizeof(c), "Windows formats cards over 32GB as exFAT, which this");
    snprintf(d, sizeof(d), "board cannot read. FORMAT rewrites it as FAT32.");
  } else if (!SdStorage::available()) {
    snprintf(a, sizeof(a), "NO CARD");
    snprintf(b, sizeof(b), "Insert a microSD and reboot");
    snprintf(c, sizeof(c), "Card is optional - the background reference lives in");
    snprintf(d, sizeof(d), "internal flash and works without it.");
  } else {
    snprintf(a, sizeof(a), "CARD READY");
    snprintf(b, sizeof(b), "%u free of %u MB", (unsigned)SdStorage::freeMB(),
             (unsigned)SdStorage::totalMB());
    snprintf(c, sizeof(c), "%d spectra saved in /spectra", SdStorage::countSpectra());
    snprintf(d, sizeof(d), "Live: %u counts over %us",
             (unsigned)g_data.quality.totalCounts, (unsigned)g_data.spectrumDurationSec);
  }

  row(ROW_A, 18, a, 2, SdStorage::available() ? currentTheme->accent : currentTheme->textSecondary,
      cA, sizeof(cA), true);
  row(ROW_B, 14, b, 1, currentTheme->text, cB, sizeof(cB), true);
  row(ROW_C, 11, c, 1, currentTheme->textSecondary, cC, sizeof(cC), true);
  row(ROW_D, 11, d, 1, currentTheme->textSecondary, cD, sizeof(cD), true);
}

static void refreshButtons(int pressed = -1) {
  bool sd = SdStorage::available();
  bool haveData = sd && g_data.quality.totalCounts > 0;
  bool haveRef = sd && Background::available();

  bool needsFormat = SdStorage::cardPresentButUnreadable();
  bool armed = formatArmed && millis() < formatArmedUntil;

  char pri[32], s1[32], s2[32];
  uint16_t priCol, s1Col, s2Col;
  bool priEn, s1En, s2En;

  if (needsFormat) {
    // The card is useless until it is reformatted, so that becomes the
    // primary action rather than a buried maintenance option.
    snprintf(pri, sizeof(pri), armed ? "TAP AGAIN TO ERASE ALL" : "FORMAT CARD (FAT32)");
    priEn = true;
    priCol = armed ? currentTheme->danger : currentTheme->warning;
    snprintf(s1, sizeof(s1), "-");   s1En = false; s1Col = currentTheme->card;
    snprintf(s2, sizeof(s2), "-");   s2En = false; s2Col = currentTheme->card;
  } else {
    snprintf(pri, sizeof(pri), "EXPORT SPECTRUM CSV");
    priEn = haveData;
    priCol = haveData ? currentTheme->accent : currentTheme->card;
    snprintf(s1, sizeof(s1), "BKG LIBRARY");
    s1En = sd;
    s1Col = sd ? currentTheme->cool : currentTheme->card;
    snprintf(s2, sizeof(s2), armed ? "TAP AGAIN: ERASE" : "ERASE DATA");
    s2En = sd;
    s2Col = armed ? currentTheme->danger : (sd ? currentTheme->card : currentTheme->card);
  }

  if (pressed == 0 || strcmp(pri, cPri) != 0 || priEn != cPriEn) {
    strncpy(cPri, pri, sizeof(cPri) - 1); cPri[sizeof(cPri) - 1] = '\0'; cPriEn = priEn;
    Cards::button(PRI_X, PRI_Y, PRI_W, PRI_H, pri, priCol, priEn, pressed == 0);
  }
  if (pressed == 1 || strcmp(s1, cSec1) != 0 || s1En != cSec1En) {
    strncpy(cSec1, s1, sizeof(cSec1) - 1); cSec1[sizeof(cSec1) - 1] = '\0'; cSec1En = s1En;
    Cards::button(SEC1_X, SEC_Y, SEC_W, SEC_H, s1, s1Col, s1En, pressed == 1);
  }
  if (pressed == 2 || strcmp(s2, cSec2) != 0 || s2En != cSec2En) {
    strncpy(cSec2, s2, sizeof(cSec2) - 1); cSec2[sizeof(cSec2) - 1] = '\0'; cSec2En = s2En;
    Cards::button(SEC2_X, SEC_Y, SEC_W, SEC_H, s2, s2Col, s2En, pressed == 2);
  }
}

static void refreshBottom() {
  const char* t = (toast[0] && millis() < toastUntil) ? toast : "";
  row(TOAST_Y, 11, t, 1, currentTheme->highlight, cToast, sizeof(cToast), false);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  cA[0] = cB[0] = cC[0] = cD[0] = cToast[0] = '\0';
  cPri[0] = cSec1[0] = cSec2[0] = '\0';

  drawHeader();
  Cards::frame(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, nullptr);
  refreshPanel();
  refreshButtons();
  Cards::leftText(12, HELP_Y, "CSV includes calibration coefficients, so the energy axis can be",
                  1, currentTheme->textSecondary);
  Cards::leftText(12, HELP2_Y, "rebuilt offline in Gamma-MCA or a spreadsheet.",
                  1, currentTheme->textSecondary);
  refreshBottom();
}

void update() {
  refreshPanel();
  refreshButtons();
  refreshBottom();
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void act(int index) {
  refreshButtons(index);
  delay(110);

  bool armed = formatArmed && millis() < formatArmedUntil;

  if (SdStorage::cardPresentButUnreadable() && index == 0) {
    if (!armed) {
      formatArmed = true;
      formatArmedUntil = millis() + 6000;
      setToast("This ERASES the whole card. Tap again within 6s to confirm.");
    } else {
      formatArmed = false;
      setToast("Formatting, please wait...");
      refreshBottom();
      if (SdStorage::formatCard()) setToast("Card formatted to FAT32 and ready.");
      else setToast("Format failed - try a different card.");
    }
  } else if (index == 0) {
    if (!SdStorage::available()) setToast("No card inserted.");
    else if (g_data.quality.totalCounts == 0) setToast("Nothing measured yet.");
    else {
      char path[40];
      if (SdStorage::exportSpectrum(g_data, path, sizeof(path))) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Saved %s", path);
        setToast(msg);
      } else setToast("Write failed - check the card.");
    }
  } else if (index == 1) {
    // Navigation is handled by main.cpp via wantsLibrary().
  } else if (index == 2) {
    if (!SdStorage::available()) setToast("No card inserted.");
    else if (!armed) {
      formatArmed = true;
      formatArmedUntil = millis() + 6000;
      setToast("Deletes all saved spectra and refs. Tap again within 6s.");
    } else {
      formatArmed = false;
      SdStorage::eraseAllData();
      setToast("Saved data deleted.");
    }
  }

  cPri[0] = cSec1[0] = cSec2[0] = '\0';
  cA[0] = cB[0] = cC[0] = cD[0] = '\0';
  refreshPanel();
  refreshButtons();
  refreshBottom();
}

static bool s_wantsLibrary = false;

bool wantsLibrary() {
  bool w = s_wantsLibrary;
  s_wantsLibrary = false;
  return w;
}

bool handleTap(int16_t x, int16_t y) {
  if (inRect(x, y, PRI_X, PRI_Y, PRI_W, PRI_H)) { act(0); return true; }
  if (inRect(x, y, SEC1_X, SEC_Y, SEC_W, SEC_H)) {
    if (!SdStorage::available()) { act(1); setToast("No card inserted."); refreshBottom(); return true; }
    refreshButtons(1);
    delay(110);
    s_wantsLibrary = true;
    return true;
  }
  if (inRect(x, y, SEC2_X, SEC_Y, SEC_W, SEC_H)) { act(2); return true; }
  return false;
}

} // namespace ScreenData
