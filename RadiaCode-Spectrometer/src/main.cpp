// ESP32-S3 + JC4827W543 (NV3041A QSPI, 480x272) + GT911 touch
// RadiaCode spectrometer UI. Data comes from BleSource (src/data/ble_source.*),
// a real RC-110 BLE link via the vendored mkgeiger/RadiaCode library
// (Libraries/RadiaCode/). src/data/mock_source.* is kept for offline UI
// testing without hardware — swap the main.cpp include/tick() call to switch.
// See ../CLAUDE.md for the project plan, architecture notes, and BLE caveats.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "ui/touch_handler.h"
#include "ui/touch_feedback.h"
#include "ui/page_transitions.h"
#include "ui/ui_theme.h"
#include "ui/cards.h"
#include "ui/screen_home.h"
#include "ui/screen_isotopes.h"
#include "ui/screen_isotope_detail.h"
#include "ui/screen_spectrum.h"
#include "ui/screen_analysis.h"
#include "ui/screen_background.h"
#include "ui/screen_data.h"
#include "ui/screen_bkg_library.h"
#include "ui/screen_import.h"
#include "ui/screen_settings.h"
#include "data/settings.h"
#include "data/updater.h"
#include "ui/keyboard.h"
#include "data/background.h"
#include "data/sd_storage.h"
#include "data/isotope_library.h"
#include "data/rc_import.h"
#include <SD.h>
#include "data/spectrometer_data.h"
#include "data/ble_source.h"

#ifndef RESET_SPECTRUM_ON_BOOT
#define RESET_SPECTRUM_ON_BOOT 0
#endif

// QSPI pin mapping for display
#define PIN_CS  45
#define PIN_SCK 47
#define PIN_D0  21 // MOSI
#define PIN_D1  48 // MISO
#define PIN_D2  40 // WP
#define PIN_D3  39 // HOLD
#define PIN_RST 17

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272

// Backlight PWM
#define LCD_BL 1
#define LEDC_CHANNEL_0    0
#define LEDC_TIMER_12_BIT 12
#define LEDC_BASE_FREQ    5000

// Touch pins
static const uint8_t TOUCH_SDA = 8, TOUCH_SCL = 4, TOUCH_INT = 3, TOUCH_RST = 38;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_CS, PIN_SCK, PIN_D0, PIN_D1, PIN_D2, PIN_D3);
Arduino_GFX *gfx = new Arduino_NV3041A(bus, PIN_RST, 0 /* rotation */, true /* IPS */);

void setBrightness(uint8_t value) {
  uint32_t duty = (1 << LEDC_TIMER_12_BIT) - 1;
  duty = duty * value / 255;
  ledcWrite(LEDC_CHANNEL_0, duty);
}

// SCR_COUNT bounds the swipe-cycle (Home/Isotopes/Spectrum/Analysis).
// SCR_ISOTOPE_DETAIL is a drill-down reached only by tapping a candidate
// card, and left only via its Back button — deliberately outside the swipe
// cycle so it doesn't confuse that mental model.
enum AppScreen {
  SCR_HOME = 0,
  SCR_ISOTOPES = 1,
  SCR_SPECTRUM = 2,
  SCR_ANALYSIS = 3,
  SCR_BACKGROUND = 4,
  SCR_DATA = 5,
  SCR_SETTINGS = 6,       // last screen in the cycle: WiFi, updates, reset
  SCR_COUNT = 7,          // swipe cycle covers 0..6
  SCR_ISOTOPE_DETAIL = 7, // tap-only drill-downs live beyond the cycle
  SCR_BKG_LIBRARY = 8,
  SCR_IMPORT = 9
};
static AppScreen currentScreen = SCR_HOME;
// The library is reachable from both Background and Data, so BACK has to
// return where the user actually came from.
static AppScreen libraryReturnScreen = SCR_DATA;

static void drawHomeFn()           { ScreenHome::draw(); }
static void drawIsotopesFn()       { ScreenIsotopes::draw(); }
static void drawSpectrumFn()       { ScreenSpectrum::draw(); }
static void drawAnalysisFn()       { ScreenAnalysis::draw(); }
static void drawBackgroundFn()     { ScreenBackground::draw(); }
static void drawDataFn()           { ScreenData::draw(); }
static void drawIsotopeDetailFn()  { ScreenIsotopeDetail::draw(); }
static void drawBkgLibraryFn()     { ScreenBkgLibrary::draw(); }
static void drawImportFn()         { ScreenImport::draw(); }
static void drawSettingsFn()       { ScreenSettings::draw(); }

// Six swipeable screens is a lot to hold in your head, so mark position with
// a dot row in the header strip. Drill-downs (detail/library/import/settings)
// are outside the cycle and show none.
static void drawPageDots() {
  if (currentScreen >= SCR_COUNT) return;
  const int16_t r = 2, gap = 9;
  int16_t totalW = (SCR_COUNT - 1) * gap;
  int16_t x0 = 240 - totalW / 2;
  for (int i = 0; i < SCR_COUNT; i++) {
    uint16_t c = (i == (int)currentScreen) ? currentTheme->accent : currentTheme->cardBorder;
    gfx->fillCircle(x0 + i * gap, 20, r, c);
  }
}

// Shown while SD, NVS and the nuclide library initialise, so the boot delay
// reads as deliberate rather than as a blank screen.
static const uint16_t SPLASH_MIN_MS = 3200;

static void drawSplash() {
  gfx->fillScreen(currentTheme->background);

  const char* title = "RadiaCode";
  const char* sub   = "PORTABLE SPECTROMETER";
  int16_t tw = Cards::textWidth(title, 5);
  Cards::leftText(240 - tw / 2, 78, title, 5, currentTheme->text);

  int16_t sw = Cards::textWidth(sub, 1);
  Cards::leftText(240 - sw / 2, 126, sub, 1, currentTheme->accent);

  gfx->fillRect(240 - 90, 142, 180, 2, currentTheme->accent);

  const char* tag = "Live gamma spectrum + isotope ID";
  int16_t gw = Cards::textWidth(tag, 1);
  Cards::leftText(240 - gw / 2, 158, tag, 1, currentTheme->textSecondary);

  char ver[32];
  snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
  int16_t vw = Cards::textWidth(ver, 1);
  Cards::leftText(240 - vw / 2, 240, ver, 1, currentTheme->textSecondary);
}

static void goToScreen(AppScreen target, TransitionType type) {
  // "Test my credentials" on the Settings screen leaves the WiFi station
  // up so the IP can be displayed. WiFi and Bluedroid cannot both hold
  // their buffers on this board, so it must not survive the screen: leaving
  // Settings powers it down and the measurement resumes.
  if (currentScreen == SCR_SETTINGS && target != SCR_SETTINGS &&
      Updater::wifiConnected() && !Updater::busy()) {
    Updater::releaseWifiNow();
  }
  currentScreen = target;
  void (*fn)() = nullptr;
  switch (target) {
    case SCR_HOME:            fn = drawHomeFn;           break;
    case SCR_ISOTOPES:        fn = drawIsotopesFn;        break;
    case SCR_SPECTRUM:        fn = drawSpectrumFn;        break;
    case SCR_ANALYSIS:        fn = drawAnalysisFn;        break;
    case SCR_BACKGROUND:      fn = drawBackgroundFn;      break;
    case SCR_DATA:            fn = drawDataFn;            break;
    case SCR_ISOTOPE_DETAIL:  fn = drawIsotopeDetailFn;   break;
    case SCR_BKG_LIBRARY:     fn = drawBkgLibraryFn;      break;
    case SCR_IMPORT:          fn = drawImportFn;          break;
    case SCR_SETTINGS:        fn = drawSettingsFn;        break;
    default: return;
  }
  PageTransitions::start(type, fn);
  drawPageDots();
}

static void updateCurrentScreen() {
  switch (currentScreen) {
    case SCR_HOME:            ScreenHome::update();           break;
    case SCR_ISOTOPES:        ScreenIsotopes::update();        break;
    case SCR_SPECTRUM:        ScreenSpectrum::update();        break;
    case SCR_ANALYSIS:        ScreenAnalysis::update();        break;
    case SCR_BACKGROUND:      ScreenBackground::update();      break;
    case SCR_DATA:            ScreenData::update();            break;
    case SCR_ISOTOPE_DETAIL:  ScreenIsotopeDetail::update();   break;
    case SCR_BKG_LIBRARY:     ScreenBkgLibrary::update();      break;
    case SCR_IMPORT:          ScreenImport::update();          break;
    case SCR_SETTINGS:        ScreenSettings::update();        break;
    default: break;
  }
}

void setup() {
  // The analysis task emits a burst of ~15 debug lines every few seconds from
  // core 0 while core 1 logs too; on the stock 256-byte TX buffer those bursts
  // came out interleaved and with characters missing. 1 KB is enough to fix
  // that - deliberately not more, because heap here is contended with
  // Bluedroid and that is a far more expensive thing to run short of.
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  delay(300);
  Serial.println("\nRadiaCode Spectrometer UI starting...");

  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LCD_BL, LEDC_CHANNEL_0);
  setBrightness(200);

  if (!gfx->begin(16000000)) {
    Serial.println("Display initialization failed!");
    while (1) delay(1000);
  }
  gfx->invertDisplay(false);
  gfx->setRotation(0);

  Cards::init(gfx);
  drawSplash();
  unsigned long splashStart = millis();

  Settings::begin();
  setBrightness(Settings::brightness());
  Background::begin();      // stored reference spectrum, before first analysis
  IsotopeLibrary::begin();  // built-in nuclide table as the baseline
  SdStorage::begin();       // optional - everything works without a card

  // A nuclide library on the card supersedes the built-in table. If it is
  // absent, drop a template next to it so the card is immediately editable.
  if (SdStorage::available()) {
    if (!IsotopeLibrary::loadFromSd("/isotopes.csv")) {
      IsotopeLibrary::exportTemplate("/isotopes_template.csv");
      // Round-trip self-check: parse the template we just wrote. It is
      // identical to the built-in table, so a mismatch means the writer and
      // reader disagree and any user-supplied library would misbehave.
      if (IsotopeLibrary::loadFromSd("/isotopes_template.csv")) {
        Serial.printf("[LIB] Self-check parsed template: %u nuclides, %u lines (expect 21/60)\n",
                      (unsigned)IsotopeLibrary::count(), (unsigned)IsotopeLibrary::lineCount());
      } else {
        Serial.println("[LIB] Self-check FAILED to parse its own template");
      }
      IsotopeLibrary::begin(); // back to the built-in table
    }
  }
  SdStorage::logInventory();
  Serial.printf("[BKG] stored=%d enabled=%d duration=%us total=%u\n",
                (int)Background::available(), (int)Background::enabled(),
                (unsigned)Background::durationSec(), (unsigned)Background::totalCounts());

  Serial.printf("[LIB] Using %s: %u nuclides, %u lines\n",
                IsotopeLibrary::sourceName(), (unsigned)IsotopeLibrary::count(),
                (unsigned)IsotopeLibrary::lineCount());

// Set to 1 to re-verify the RadiaCode XML scanner against a synthetic export
// at boot. Confirmed 2026-08-22: 1024 ch / 600 s / 3067 counts, mismatch=0.
#define RC_IMPORT_SELFTEST 0
#if RC_IMPORT_SELFTEST
  // One-off proof that the XML scanner handles the RadiaCode export shape.
  if (SdStorage::available()) {
    if (!SD.exists("/import")) SD.mkdir("/import");
    SD.remove("/import/_selftest.xml");
    File t = SD.open("/import/_selftest.xml", FILE_WRITE);
    if (t) {
      t.print("<?xml version=\"1.0\"?>\n<ResultDataFile><ResultDataList><ResultData>");
      t.print("<EnergySpectrum><NumberOfChannels>1024</NumberOfChannels>");
      t.print("<EnergyCalibration><Coefficients>");
      t.print("<Coefficient>3.839503</Coefficient><Coefficient>2.375420</Coefficient>");
      t.print("<Coefficient>0.000364</Coefficient></Coefficients></EnergyCalibration>");
      t.print("<MeasurementTime>600</MeasurementTime><Spectrum>");
      for (int i = 0; i < 1024; i++) t.printf("<DataPoint>%d</DataPoint>", i % 7);
      t.print("</Spectrum></EnergySpectrum></ResultData></ResultDataList></ResultDataFile>");
      t.close();

      RcImport::Result r = RcImport::importToSlot("/import/_selftest.xml", "SPARE", g_data);
      if (r.ok) {
        Serial.printf("[IMP] Self-test OK: %d ch, %us, %u counts, a1=%.4f, mismatch=%d "
                      "(expect 1024/600/3067)\n",
                      r.channels, (unsigned)r.durationSec, (unsigned)r.totalCounts,
                      r.a1, (int)r.calibMismatch);
      } else {
        Serial.printf("[IMP] Self-test FAILED: %s\n", r.error);
      }
      SD.remove("/import/_selftest.xml");
      SD.remove("/background/SPARE.csv");
    }
  }
#endif
  TouchHandler::init(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_WIDTH, SCREEN_HEIGHT);
  // TouchFeedback (the inherited tap-ripple) is deliberately left uninitialised
  // and never updated. It erases its ripple by painting the theme background
  // over the area, which punches a permanent dark blob into any card, button
  // or chart it lands on. Controls render their own pressed state instead.
  TouchFeedback::setEnabled(false);
  PageTransitions::init(gfx);
  PageTransitions::setDuration(200);

  // Hold the splash for its minimum, minus however long init already took.
  unsigned long shown = millis() - splashStart;
  if (shown < SPLASH_MIN_MS) delay(SPLASH_MIN_MS - shown);

  // First power-up lands on the setup screen so WiFi can be entered; once
  // that is marked done it never appears at startup again, and stays
  // reachable as the last screen in the swipe cycle.
  if (!Settings::setupDone()) {
    Serial.println("[SET] First run - showing setup screen");
    goToScreen(SCR_SETTINGS, TRANSITION_NONE);
  } else {
    goToScreen(SCR_HOME, TRANSITION_NONE);
  }

  BleSource::begin();          // BLE + analysis worker on core 0
  BleSource::beginAutoStart(); // performed by that task, so setup() never blocks

#if RESET_SPECTRUM_ON_BOOT
  // Bench aid: discard whatever the detector has accumulated so a test run
  // starts from zero without anyone tapping RESET. Never set in a release
  // build - the whole point of the normal path is that counts survive a
  // reboot. See "Continuous acquisition" in CLAUDE.md.
  Serial.println("[BOOT] RESET_SPECTRUM_ON_BOOT - clearing accumulated spectrum");
  BleSource::resetSpectrum();
#endif
  Updater::begin();            // own task; checks at boot only if enabled
}

void loop() {
  TouchHandler::update();
  bool transitionActive = PageTransitions::update();

  // The keyboard owns the whole screen while open: no swiping away mid-entry.
  bool onSwipeableScreen = (currentScreen < SCR_COUNT) && !Keyboard::isOpen();

  if (!transitionActive) {
    if (onSwipeableScreen && TouchHandler::wasSwipedLeft()) {
      goToScreen((AppScreen)((currentScreen + 1) % SCR_COUNT), TRANSITION_SLIDE_LEFT);
      TouchHandler::resetGestures();
    } else if (onSwipeableScreen && TouchHandler::wasSwipedRight()) {
      goToScreen((AppScreen)((currentScreen + SCR_COUNT - 1) % SCR_COUNT), TRANSITION_SLIDE_RIGHT);
      TouchHandler::resetGestures();
    } else if (TouchHandler::wasTapped()) {
      uint16_t x = TouchHandler::getTouchX();
      uint16_t y = TouchHandler::getTouchY();
      bool consumed = false;

      if (currentScreen == SCR_HOME) {
        bool toSettings = false;
        consumed = ScreenHome::handleTap(x, y, &toSettings);
        if (toSettings) goToScreen(SCR_SETTINGS, TRANSITION_SLIDE_LEFT);
      } else if (currentScreen == SCR_ISOTOPES) {
        int tapped = ScreenIsotopes::handleTap(x, y);
        if (tapped >= 0) {
          ScreenIsotopeDetail::setCandidateIndex(tapped);
          // SLIDE, not FADE: animateFade() redraws the whole screen in ~1000
          // tiny 4x4 fillRect calls per animation frame (vs. ~2 line draws
          // for a slide) — a plausible trigger for the reboot seen after
          // tapping into this screen. Slide is the same, already-proven path
          // the swipe navigation already exercises heavily.
          goToScreen(SCR_ISOTOPE_DETAIL, TRANSITION_SLIDE_LEFT);
          consumed = true;
        }
      } else if (currentScreen == SCR_SPECTRUM) {
        consumed = ScreenSpectrum::handleTap(x, y);
      } else if (currentScreen == SCR_BACKGROUND) {
        bool toLibrary = false;
        consumed = ScreenBackground::handleTap(x, y, &toLibrary);
        if (toLibrary) {
          libraryReturnScreen = SCR_BACKGROUND;
          goToScreen(SCR_BKG_LIBRARY, TRANSITION_SLIDE_LEFT);
        }
      } else if (currentScreen == SCR_DATA) {
        consumed = ScreenData::handleTap(x, y);
        if (ScreenData::wantsLibrary()) {
          libraryReturnScreen = SCR_DATA;
          goToScreen(SCR_BKG_LIBRARY, TRANSITION_SLIDE_LEFT);
        }
      } else if (currentScreen == SCR_BKG_LIBRARY) {
        bool back = false, toImport = false;
        consumed = ScreenBkgLibrary::handleTap(x, y, &back, &toImport);
        if (back) goToScreen(libraryReturnScreen, TRANSITION_SLIDE_RIGHT);
        else if (toImport) goToScreen(SCR_IMPORT, TRANSITION_SLIDE_LEFT);
      } else if (currentScreen == SCR_IMPORT) {
        bool back = false;
        consumed = ScreenImport::handleTap(x, y, &back);
        if (back) goToScreen(SCR_BKG_LIBRARY, TRANSITION_SLIDE_RIGHT);
      } else if (currentScreen == SCR_SETTINGS) {
        bool done = false;
        consumed = ScreenSettings::handleTap(x, y, &done);
        // Only "finish first-time setup" navigates away; otherwise Settings
        // is an ordinary screen you swipe off.
        if (done) goToScreen(SCR_HOME, TRANSITION_SLIDE_LEFT);
      } else if (currentScreen == SCR_ISOTOPE_DETAIL) {
        if (ScreenIsotopeDetail::handleTap(x, y)) {
          goToScreen(SCR_ISOTOPES, TRANSITION_SLIDE_RIGHT);
          consumed = true;
        }
      }

      // No TouchFeedback::trigger() here. Its ripple erases itself by
      // painting the theme background over the affected area, which is fine
      // on a flat screen but punches a permanent dark hole in any coloured
      // control it lands on (seen on hardware as a black blob stuck on the
      // RECORD button). Controls now render their own pressed state instead.
      (void)consumed;
      TouchHandler::resetGestures();
    }
  }

  SdStorage::pollPresence(); // notice a card being pulled or inserted

  // Periodic SD checkpoint. The RC-110 already carries an accumulating run
  // through link drops and ESP32 reboots, so this covers the remaining gap:
  // the RC-110 itself being switched off. Five minutes keeps card wear
  // negligible while bounding what a power loss can cost.
  static unsigned long lastAutosaveMs = 0;
  if (g_data.connection == CONN_CONNECTED && SdStorage::available() &&
      g_data.quality.totalCounts > 0 && millis() - lastAutosaveMs > 300000UL) {
    lastAutosaveMs = millis();
    SdStorage::autosaveSpectrum(g_data);
  }

  static unsigned long lastUiUpdate = 0;
  unsigned long now = millis();
  if (!transitionActive && now - lastUiUpdate >= 500) {
    lastUiUpdate = now;
    updateCurrentScreen();
  }

  delay(5);
}
