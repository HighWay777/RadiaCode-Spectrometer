#include "screen_settings.h"
#include "cards.h"
#include "ui_theme.h"
#include "keyboard.h"
#include "../data/settings.h"
#include "../data/updater.h"
#include "../data/spectrometer_data.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;
extern void setBrightness(uint8_t value);

namespace ScreenSettings {

static const int16_t HEADER_H = 26;
static const int16_t ROW_X = 6, ROW_W = 468, ROW_H = 27;
static const int16_t ROW0_Y = 30;
static const int16_t STATUS_Y = 256;

// Compact rows so WiFi, connection, update and reset controls all fit one
// 480x272 screen without paging.
enum RowId {
  R_SSID = 0, R_PASS, R_AUTORECON, R_STARTCON, R_AUTOUPD, R_CHECKUPD,
  R_BRIGHT, R_RESET, ROW_COUNT
};

static int16_t rowY(int i) { return ROW0_Y + i * ROW_H; }

// Which field the on-screen keyboard is currently editing.
static int editingRow = -1;
static bool resetArmed = false;
static unsigned long resetArmedUntil = 0;
static char status[80] = "";

static void setStatus(const char* s) {
  strncpy(status, s, sizeof(status) - 1);
  status[sizeof(status) - 1] = '\0';
}

static void drawRow(int i, const char* label, const char* value,
                    uint16_t valueColor, bool pressed) {
  int16_t y = rowY(i);
  gfx->fillRoundRect(ROW_X, y, ROW_W, ROW_H - 2, 4, currentTheme->card);
  gfx->drawRoundRect(ROW_X, y, ROW_W, ROW_H - 2, 4,
                     pressed ? currentTheme->highlight : currentTheme->cardBorder);
  Cards::leftText(ROW_X + 10, y + 8, label, 1, currentTheme->text);
  int16_t vw = Cards::textWidth(value, 1);
  Cards::leftText(ROW_X + ROW_W - 10 - vw, y + 8, value, 1, valueColor);
}

static void drawAll(int pressed = -1) {
  bool setupMode = !Settings::setupDone();

  // WiFi. Entering credentials proves nothing on its own, so the row reports
  // whether the network actually came up, and shows the address once it has.
  const char* ssid = Settings::wifiSsid();
  char netVal[40];
  uint16_t netCol;
  if (!ssid[0]) {
    snprintf(netVal, sizeof(netVal), "not set");
    netCol = currentTheme->textSecondary;
  } else if (Updater::wifiConnected()) {
    snprintf(netVal, sizeof(netVal), "%s  %s", ssid, Updater::wifiIp());
    netCol = currentTheme->accent;
  } else if (Updater::state() == Updater::WIFI_CONNECTING) {
    snprintf(netVal, sizeof(netVal), "%s  connecting...", ssid);
    netCol = currentTheme->warning;
  } else {
    snprintf(netVal, sizeof(netVal), "%s  (tap to test)", ssid);
    netCol = currentTheme->textSecondary;
  }
  drawRow(R_SSID, "WiFi network (SSID)", netVal, netCol, pressed == R_SSID);

  char masked[24] = "not set";
  if (Settings::wifiPassword()[0]) {
    size_t n = strlen(Settings::wifiPassword());
    if (n > 16) n = 16;
    memset(masked, '*', n);
    masked[n] = '\0';
  }
  drawRow(R_PASS, "WiFi password", masked,
          Settings::wifiPassword()[0] ? currentTheme->accent : currentTheme->textSecondary,
          pressed == R_PASS);

  drawRow(R_AUTORECON, "Auto-reconnect / resume after reboot",
          Settings::autoReconnect() ? "ON" : "OFF",
          Settings::autoReconnect() ? currentTheme->accent : currentTheme->textSecondary,
          pressed == R_AUTORECON);

  drawRow(R_STARTCON, "Connect to detector at startup",
          Settings::connectAtStartup() ? "ON" : "OFF",
          Settings::connectAtStartup() ? currentTheme->accent : currentTheme->textSecondary,
          pressed == R_STARTCON);

  // Updates need the network. Offering "check for updates" before any WiFi
  // exists is a promise the device cannot keep, so both rows read as
  // unavailable until credentials are set.
  bool net = Settings::hasWifi();

  drawRow(R_AUTOUPD, "Check for firmware updates at startup",
          net ? (Settings::autoUpdate() ? "ON" : "OFF") : "needs WiFi",
          (net && Settings::autoUpdate()) ? currentTheme->accent : currentTheme->textSecondary,
          pressed == R_AUTOUPD);

  // Update row doubles as the action and its own progress readout.
  char upd[40];
  uint16_t updCol = currentTheme->cool;
  if (!net) {
    snprintf(upd, sizeof(upd), "set WiFi first");
    updCol = currentTheme->textSecondary;
  } else {
    switch (Updater::state()) {
      case Updater::DOWNLOADING:
        snprintf(upd, sizeof(upd), "%d%%", Updater::progressPercent());
        updCol = currentTheme->warning; break;
      case Updater::AVAILABLE:
        snprintf(upd, sizeof(upd), "INSTALL %s", Updater::latestVersion());
        updCol = currentTheme->highlight; break;
      case Updater::UP_TO_DATE: snprintf(upd, sizeof(upd), "up to date"); break;
      case Updater::FAILED:     snprintf(upd, sizeof(upd), "failed");
                                updCol = currentTheme->danger; break;
      case Updater::CHECKING:
      case Updater::WIFI_CONNECTING: snprintf(upd, sizeof(upd), "working..."); break;
      default: snprintf(upd, sizeof(upd), "CHECK NOW"); break;
    }
  }
  char verLabel[48];
  snprintf(verLabel, sizeof(verLabel), "Firmware %s  -  updates", FIRMWARE_VERSION);
  drawRow(R_CHECKUPD, verLabel, upd, updCol, pressed == R_CHECKUPD);

  char br[8];
  snprintf(br, sizeof(br), "%d%%", (int)(Settings::brightness() * 100 / 255));
  drawRow(R_BRIGHT, "Display brightness", br, currentTheme->cool, pressed == R_BRIGHT);

  bool armed = resetArmed && millis() < resetArmedUntil;
  if (setupMode) {
    drawRow(R_RESET, "Finish setup and start using the device", "DONE",
            currentTheme->accent, pressed == R_RESET);
  } else {
    drawRow(R_RESET, "Factory defaults (erases everything)",
            armed ? "TAP AGAIN" : "RESET",
            armed ? currentTheme->danger : currentTheme->warning, pressed == R_RESET);
  }
}

static void drawStatus() {
  gfx->fillRect(0, STATUS_Y, 480, 272 - STATUS_Y, currentTheme->background);
  const char* text = status[0] ? status : Updater::statusText();
  Cards::leftText(8, STATUS_Y, text, 1, currentTheme->textSecondary);
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  bool setupMode = !Settings::setupDone();
  Cards::leftText(10, 6, setupMode ? "First-time setup" : "Settings", 2,
                  setupMode ? currentTheme->highlight : currentTheme->text);
  const char* chip = setupMode ? "welcome" : "";
  if (chip[0]) {
    int16_t w = Cards::textWidth(chip, 1);
    Cards::leftText(480 - 12 - w, 10, chip, 1, currentTheme->textSecondary);
  }
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

void draw() {
  if (Keyboard::isOpen()) { Keyboard::draw(); return; }
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  drawAll();
  drawStatus();
}

void update() {
  if (Keyboard::isOpen()) return;
  // The update row animates during a download, and Updater runs on its own
  // task, so refresh while it is working.
  static int lastState = -1, lastProgress = -1;
  if ((int)Updater::state() != lastState || Updater::progressPercent() != lastProgress) {
    lastState = (int)Updater::state();
    lastProgress = Updater::progressPercent();
    drawAll();
    drawStatus();
  }
}

static bool inRow(int16_t x, int16_t y, int i) {
  int16_t ry = rowY(i);
  return x >= ROW_X && x <= ROW_X + ROW_W && y >= ry && y <= ry + ROW_H - 2;
}

// Applies whatever the keyboard produced, then returns to the list.
static void finishEditing() {
  if (Keyboard::wasAccepted() && editingRow >= 0) {
    if (editingRow == R_SSID) {
      Settings::setWifi(Keyboard::text(), Settings::wifiPassword());
      setStatus("WiFi network saved.");
    } else if (editingRow == R_PASS) {
      Settings::setWifi(Settings::wifiSsid(), Keyboard::text());
      setStatus("WiFi password saved.");
    }
  }
  Keyboard::consumeResult();
  editingRow = -1;
  draw();
}

bool handleTap(int16_t x, int16_t y, bool* backPressed) {
  if (backPressed) *backPressed = false;

  if (Keyboard::isOpen()) {
    Keyboard::handleTap(x, y);
    if (!Keyboard::isOpen()) finishEditing();
    return true;
  }

  for (int i = 0; i < ROW_COUNT; i++) {
    if (!inRow(x, y, i)) continue;
    drawAll(i);
    delay(100);

    switch (i) {
      case R_SSID:
        // Once an SSID exists, tapping tests it rather than immediately
        // reopening the keyboard — "did my credentials work?" is the more
        // common question. Clearing the field reopens the editor.
        if (Settings::wifiSsid()[0] && !Updater::wifiConnected() && !Updater::busy()) {
          Updater::connectWifi();
          setStatus("Testing WiFi connection...");
          break;
        }
        editingRow = i;
        Keyboard::open("WiFi network name", Settings::wifiSsid(), false, 32);
        return true;
      case R_PASS:
        editingRow = i;
        Keyboard::open("WiFi password", Settings::wifiPassword(), true, 63);
        return true;
      case R_AUTORECON:
        Settings::setAutoReconnect(!Settings::autoReconnect());
        setStatus(Settings::autoReconnect()
                  ? "Measurements resume automatically after a dropout or reboot."
                  : "Auto-reconnect off.");
        break;
      case R_STARTCON:
        Settings::setConnectAtStartup(!Settings::connectAtStartup());
        setStatus("");
        break;
      case R_AUTOUPD:
        // Refuse rather than store a preference that cannot take effect.
        if (!Settings::hasWifi()) {
          setStatus("Enter a WiFi network above first - updates need internet.");
        } else {
          Settings::setAutoUpdate(!Settings::autoUpdate());
          setStatus("");
        }
        break;
      case R_CHECKUPD:
        if (!Settings::hasWifi()) {
          setStatus("Enter a WiFi network above first - updates need internet.");
        } else if (Updater::state() == Updater::AVAILABLE) {
          Updater::installUpdate();
        } else {
          Updater::checkForUpdate();
        }
        break;
      case R_BRIGHT: {
        uint8_t b = Settings::brightness();
        b = (b >= 250) ? 60 : (uint8_t)(b + 65);
        Settings::setBrightness(b);
        setBrightness(Settings::brightness());
        break;
      }
      case R_RESET:
        if (!Settings::setupDone()) {
          Settings::setSetupDone(true);
          setStatus("Setup complete.");
          if (backPressed) *backPressed = true;   // hand control back to Home
        } else if (resetArmed && millis() < resetArmedUntil) {
          resetArmed = false;
          Settings::factoryReset();
          gfx->fillScreen(currentTheme->background);
          Cards::centeredText(240, 120, "Factory reset", 3, currentTheme->danger);
          Cards::centeredText(240, 150, "restarting...", 2, currentTheme->textSecondary);
          delay(1500);
          ESP.restart();
        } else {
          resetArmed = true;
          resetArmedUntil = millis() + 6000;
          setStatus("Erases WiFi, settings and the stored background. Tap again to confirm.");
        }
        break;
    }
    drawAll();
    drawStatus();
    return true;
  }
  return false;
}

} // namespace ScreenSettings
