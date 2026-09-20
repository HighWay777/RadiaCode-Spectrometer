#include "screen_background.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/background.h"
#include "../data/ble_source.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenBackground {

// The original version exposed "RECORD" as a snapshot of whatever had already
// accumulated. That reads as "start recording", so pressing it with an empty
// or short acquisition appeared to do nothing at all. This version models an
// explicit recording *session* — start it, watch it fill, stop and save —
// so the button state always matches what the device is doing.

static const int16_t HEADER_H = 26;

static const int16_t PANEL_X = 8, PANEL_Y = 30, PANEL_W = 464, PANEL_H = 88;
static const int16_t ROW_A = PANEL_Y + 8;    // state headline
static const int16_t ROW_B = PANEL_Y + 30;   // primary value
static const int16_t BAR_Y = PANEL_Y + 52;   // progress
static const int16_t ROW_D = PANEL_Y + 64;   // detail
static const int16_t ROW_E = PANEL_Y + 76;   // detail

static const int16_t PRI_X = 8,   PRI_Y = 124, PRI_W = 464, PRI_H = 44;
// Three secondary actions: the subtraction toggle, a route into the slot
// library/import, and erase. Library used to be reachable only from the Data
// screen, which nobody would guess.
static const int16_t SEC_W = 150, SEC_H = 40, SEC_Y = 174;
static const int16_t SEC1_X = 8, SEC2_X = 165, SEC3_X = 322;

static const int16_t TOAST_Y = 222;
static const int16_t HELP_Y  = 238;
static const int16_t HELP2_Y = 252;

// Recording is NOT capped — it runs until you stop it, for hours if you want.
// This is only the point at which the progress bar reads full, chosen because
// a reference contributes negligible extra uncertainty once it is roughly 10x
// longer than the samples you intend to measure (see bgVarScale in
// peak_search.cpp). 10 min comfortably covers 1-minute sample runs; the panel
// separately reports what sample length the current reference actually
// supports, which is more useful than any fixed target.
static const uint32_t BAR_FULL_SEC = 600;

static bool s_recording = false;

static char toast[56] = "";
static unsigned long toastUntil = 0;

// Cached rows — nothing repaints unless its text actually changed.
static char cA[48], cB[48], cD[64], cE[64], cToast[56], cHelp[80], cHelp2[80];
static int16_t cBarW = -1;
static char cPri[32], cSec1[32], cSec2[32], cSec3[32];
static bool cPriEn = false, cSec1En = false, cSec2En = false, cSec3En = false;

static void setToast(const char* m) {
  strncpy(toast, m, sizeof(toast) - 1);
  toast[sizeof(toast) - 1] = '\0';
  toastUntil = millis() + 4500;
}

// h:mm:ss once past an hour, so multi-hour references read sensibly instead
// of turning into "185:32".
static void fmtClock(uint32_t s, char* b, size_t n) {
  if (s >= 3600) snprintf(b, n, "%u:%02u:%02u", (unsigned)(s / 3600),
                          (unsigned)((s % 3600) / 60), (unsigned)(s % 60));
  else snprintf(b, n, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

// A reference is "good enough" for samples about a tenth of its own length,
// where its contribution to the total variance drops below ~10%. Reporting
// that is far more actionable than a bare duration.
static void fmtSupports(uint32_t bgSec, char* b, size_t n) {
  uint32_t s = bgSec / 10;
  if (s < 60) snprintf(b, n, "%us", (unsigned)s);
  else if (s < 3600) snprintf(b, n, "%um", (unsigned)(s / 60));
  else snprintf(b, n, "%uh", (unsigned)(s / 3600));
}

static void fmtDur(uint32_t s, char* b, size_t n) {
  if (s >= 3600) snprintf(b, n, "%uh%02um", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
  else if (s >= 60) snprintf(b, n, "%um%02us", (unsigned)(s / 60), (unsigned)(s % 60));
  else snprintf(b, n, "%us", (unsigned)s);
}

static void row(int16_t y, int16_t h, const char* text, uint8_t size, uint16_t color,
                char* cache, size_t cn, bool insidePanel) {
  if (strcmp(text, cache) == 0) return;
  strncpy(cache, text, cn - 1);
  cache[cn - 1] = '\0';
  if (insidePanel) gfx->fillRect(PANEL_X + 6, y, PANEL_W - 12, h, currentTheme->card);
  else             gfx->fillRect(0, y, 480, h, currentTheme->background);
  if (text[0]) Cards::leftText(insidePanel ? PANEL_X + 12 : 12, y, text, size, color);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Background Reference", 2, currentTheme->text);
  const char* chip = s_recording ? "RECORDING"
                   : Background::active() ? "SUBTRACTING"
                   : Background::available() ? "REF STORED" : "NO REF";
  uint16_t cc = s_recording ? currentTheme->warning
              : Background::active() ? currentTheme->accent : currentTheme->textSecondary;
  int16_t w = Cards::textWidth(chip, 1);
  gfx->fillRect(480 - 16 - w, 6, w + 10, 14, currentTheme->header);
  Cards::leftText(480 - 12 - w, 10, chip, 1, cc);
}

static void refreshPanel() {
  char a[48], b[48], d[64], e[64];
  uint16_t aCol, bCol;
  float frac = -1.0f;
  uint16_t barCol = currentTheme->accent;

  if (s_recording) {
    uint32_t el = g_data.spectrumDurationSec;
    char clk[16], sup[12];
    fmtClock(el, clk, sizeof(clk));
    fmtSupports(el, sup, sizeof(sup));
    snprintf(a, sizeof(a), "RECORDING BACKGROUND");
    snprintf(b, sizeof(b), "%s", clk);
    snprintf(d, sizeof(d), "%u counts   %.1f cps   good for %s samples",
             (unsigned)g_data.quality.totalCounts, g_data.cps, sup);
    if (el < Background::MIN_RECORD_SECONDS) {
      snprintf(e, sizeof(e), "Keep samples away. Minimum %us before saving.",
               (unsigned)Background::MIN_RECORD_SECONDS);
    } else {
      snprintf(e, sizeof(e), "Runs as long as you like - stop whenever it is good enough.");
    }
    aCol = currentTheme->warning;
    bCol = currentTheme->text;
    frac = (float)el / (float)BAR_FULL_SEC;
    barCol = currentTheme->warning;
  } else if (!Background::available()) {
    snprintf(a, sizeof(a), "NO REFERENCE STORED");
    snprintf(b, sizeof(b), "Using raw counts");
    snprintf(d, sizeof(d), "Weak samples cannot be separated from room background.");
    snprintf(e, sizeof(e), "Remove samples, then tap START RECORDING.");
    aCol = currentTheme->textSecondary;
    bCol = currentTheme->textSecondary;
  } else {
    float refCps = (Background::durationSec() > 0)
                 ? (float)Background::totalCounts() / (float)Background::durationSec() : 0.0f;
    float excess = g_data.cps - refCps;
    char dur[16];
    fmtDur(Background::durationSec(), dur, sizeof(dur));

    char sup[12];
    fmtSupports(Background::durationSec(), sup, sizeof(sup));
    // Name the reference in the headline: "subtraction is on" is not useful
    // without knowing *what* is being subtracted.
    snprintf(a, sizeof(a), Background::enabled() ? "USING: %s" : "%s (OFF)",
             Background::label());
    snprintf(b, sizeof(b), "%+.1f cps over background", excess);
    snprintf(d, sizeof(d), "Ref %s, %.1f cps - good for %s sample runs",
             dur, refCps, sup);
    snprintf(e, sizeof(e), "Now measuring: %.1f cps", g_data.cps);
    aCol = Background::enabled() ? currentTheme->accent : currentTheme->textSecondary;
    // Colour the headline number by whether the sample is actually above
    // background — the one thing that matters for a weak specimen.
    bCol = (excess > 3.0f) ? currentTheme->highlight
         : (excess > 1.0f) ? currentTheme->warning : currentTheme->textSecondary;
  }

  row(ROW_A, 18, a, 2, aCol, cA, sizeof(cA), true);
  row(ROW_B, 18, b, 2, bCol, cB, sizeof(cB), true);
  row(ROW_D, 11, d, 1, currentTheme->textSecondary, cD, sizeof(cD), true);
  row(ROW_E, 11, e, 1, currentTheme->textSecondary, cE, sizeof(cE), true);

  if (frac >= 0.0f) {
    if (frac > 1.0f) frac = 1.0f;
    int16_t w = (int16_t)((PANEL_W - 24) * frac);
    if (w != cBarW) {
      cBarW = w;
      Cards::progressBar(PANEL_X + 12, BAR_Y, PANEL_W - 24, 8, frac, barCol);
    }
  } else if (cBarW != -1) {
    cBarW = -1;
    gfx->fillRect(PANEL_X + 12, BAR_Y, PANEL_W - 24, 8, currentTheme->card);
  }
}

static void refreshButtons(int pressed = -1) {
  bool conn = (g_data.connection == CONN_CONNECTED);
  char pri[32], s1[32], s2[32], s3[32];
  uint16_t priCol, s1Col, s2Col, s3Col;
  bool priEn, s1En, s2En, s3En;

  if (s_recording) {
    uint32_t el = g_data.spectrumDurationSec;
    bool longEnough = el >= Background::MIN_RECORD_SECONDS;
    if (longEnough) snprintf(pri, sizeof(pri), "STOP & SAVE");
    else snprintf(pri, sizeof(pri), "WAIT %us...",
                  (unsigned)(Background::MIN_RECORD_SECONDS - el));
    priEn = longEnough;
    priCol = longEnough ? currentTheme->accent : currentTheme->card;

    snprintf(s1, sizeof(s1), "CANCEL");
    s1En = true; s1Col = currentTheme->warm;
    snprintf(s2, sizeof(s2), "-");
    s2En = false; s2Col = currentTheme->card;
    snprintf(s3, sizeof(s3), "-");
    s3En = false; s3Col = currentTheme->card;
  } else {
    snprintf(pri, sizeof(pri), "START RECORDING");
    priEn = conn;
    priCol = conn ? currentTheme->accent : currentTheme->card;

    bool have = Background::available();
    snprintf(s1, sizeof(s1), Background::enabled() ? "SUBTRACT: ON" : "SUBTRACT: OFF");
    s1En = have;
    s1Col = have ? (Background::enabled() ? currentTheme->accent : currentTheme->card)
                 : currentTheme->card;

    snprintf(s2, sizeof(s2), "LIBRARY");
    s2En = true;
    s2Col = currentTheme->cool;

    snprintf(s3, sizeof(s3), "ERASE");
    s3En = have;
    s3Col = have ? currentTheme->danger : currentTheme->card;
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
  if (pressed == 3 || strcmp(s3, cSec3) != 0 || s3En != cSec3En) {
    strncpy(cSec3, s3, sizeof(cSec3) - 1); cSec3[sizeof(cSec3) - 1] = '\0'; cSec3En = s3En;
    Cards::button(SEC3_X, SEC_Y, SEC_W, SEC_H, s3, s3Col, s3En, pressed == 3);
  }
}

static void refreshBottom() {
  const char* t = (toast[0] && millis() < toastUntil) ? toast : "";
  row(TOAST_Y, 11, t, 1, currentTheme->highlight, cToast, sizeof(cToast), false);

  char h1[80], h2[80];
  if (s_recording) {
    snprintf(h1, sizeof(h1), "Measuring the room itself - nothing but background should be nearby.");
    snprintf(h2, sizeof(h2), "5 minutes gives a clean reference; 30s is the bare minimum.");
  } else if (g_data.connection != CONN_CONNECTED) {
    snprintf(h1, sizeof(h1), "Not connected. Connect on the Home screen first.");
    snprintf(h2, sizeof(h2), "");
  } else if (!Background::available()) {
    snprintf(h1, sizeof(h1), "Room air carries radon daughters; walls carry K-40. Both look like");
    snprintf(h2, sizeof(h2), "a real sample until you subtract them.");
  } else {
    snprintf(h1, sizeof(h1), "Place the sample, then use NEW MEASUREMENT on any screen to start.");
    snprintf(h2, sizeof(h2), "Re-record if the room or detector position changes.");
  }
  row(HELP_Y, 11, h1, 1, currentTheme->textSecondary, cHelp, sizeof(cHelp), false);
  row(HELP2_Y, 11, h2, 1, currentTheme->textSecondary, cHelp2, sizeof(cHelp2), false);
}

void draw() {
  gfx->fillScreen(currentTheme->background);
  cA[0] = cB[0] = cD[0] = cE[0] = '\0';
  cToast[0] = cHelp[0] = cHelp2[0] = '\0';
  cPri[0] = cSec1[0] = cSec2[0] = '\0';
  cBarW = -2;

  drawHeader();
  Cards::frame(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, nullptr);
  refreshPanel();
  refreshButtons();
  refreshBottom();
}

void update() {
  // Recording cannot continue through a dropped link.
  if (s_recording && g_data.connection != CONN_CONNECTED) {
    s_recording = false;
    setToast("Connection lost - recording cancelled.");
    drawHeader();
  }
  refreshPanel();
  refreshButtons();
  refreshBottom();
}

static bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void act(int index) {
  refreshButtons(index);
  delay(110); // make the press visible

  if (s_recording) {
    if (index == 0) {
      if (g_data.spectrumDurationSec < Background::MIN_RECORD_SECONDS) {
        setToast("Not long enough yet.");
      } else if (Background::record(g_data)) {
        s_recording = false;
        BleSource::resetSpectrum();
        setToast("Saved. Subtraction ON, ready for sample.");
      } else {
        setToast("Could not save reference.");
      }
    } else if (index == 1) {
      s_recording = false;
      setToast("Recording cancelled.");
    }
  } else {
    if (index == 0) {
      if (g_data.connection != CONN_CONNECTED) {
        setToast("Connect first.");
      } else {
        BleSource::resetSpectrum();
        s_recording = true;
        setToast("Recording started - keep samples away.");
      }
    } else if (index == 1) {
      if (!Background::available()) setToast("No reference yet - record or import one.");
      else {
        Background::setEnabled(!Background::enabled());
        setToast(Background::enabled() ? "Subtraction ON." : "Subtraction OFF.");
      }
    } else if (index == 3) {
      if (!Background::available()) setToast("Nothing stored.");
      else { Background::erase(); setToast("Active reference cleared."); }
    }
    // index == 2 (LIBRARY) is navigation, handled by the caller.
  }

  // State changed enough that cached button/panel text is stale.
  cPri[0] = cSec1[0] = cSec2[0] = '\0';
  cA[0] = cB[0] = cD[0] = cE[0] = '\0';
  cBarW = -2;
  drawHeader();
  refreshPanel();
  refreshButtons();
  refreshBottom();
}

bool handleTap(int16_t x, int16_t y, bool* libraryPressed) {
  if (libraryPressed) *libraryPressed = false;

  if (inRect(x, y, PRI_X, PRI_Y, PRI_W, PRI_H)) { act(0); return true; }
  if (inRect(x, y, SEC1_X, SEC_Y, SEC_W, SEC_H)) { act(1); return true; }
  if (inRect(x, y, SEC2_X, SEC_Y, SEC_W, SEC_H)) {
    if (s_recording) return true; // disabled mid-recording
    refreshButtons(2);
    delay(110);
    if (libraryPressed) *libraryPressed = true;
    return true;
  }
  if (inRect(x, y, SEC3_X, SEC_Y, SEC_W, SEC_H)) {
    if (!s_recording) { act(3); return true; }
    return true;
  }
  return false;
}

} // namespace ScreenBackground
