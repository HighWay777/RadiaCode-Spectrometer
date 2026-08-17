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
#include "data/spectrometer_data.h"
#include "data/ble_source.h"

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

static void setBrightness(uint8_t value) {
  uint32_t duty = (1 << LEDC_TIMER_12_BIT) - 1;
  duty = duty * value / 255;
  ledcWrite(LEDC_CHANNEL_0, duty);
}

// SCR_COUNT bounds the swipe-cycle (Home/Isotopes/Spectrum only).
// SCR_ISOTOPE_DETAIL is a drill-down reached only by tapping a candidate
// card, and left only via its Back button — deliberately outside the swipe
// cycle so it doesn't confuse that mental model.
enum AppScreen { SCR_HOME = 0, SCR_ISOTOPES = 1, SCR_SPECTRUM = 2, SCR_COUNT = 3, SCR_ISOTOPE_DETAIL = 3 };
static AppScreen currentScreen = SCR_HOME;

static void drawHomeFn()           { ScreenHome::draw(); }
static void drawIsotopesFn()       { ScreenIsotopes::draw(); }
static void drawSpectrumFn()       { ScreenSpectrum::draw(); }
static void drawIsotopeDetailFn()  { ScreenIsotopeDetail::draw(); }

static void goToScreen(AppScreen target, TransitionType type) {
  currentScreen = target;
  void (*fn)() = nullptr;
  switch (target) {
    case SCR_HOME:            fn = drawHomeFn;           break;
    case SCR_ISOTOPES:        fn = drawIsotopesFn;        break;
    case SCR_SPECTRUM:        fn = drawSpectrumFn;        break;
    case SCR_ISOTOPE_DETAIL:  fn = drawIsotopeDetailFn;   break;
    default: return;
  }
  PageTransitions::start(type, fn);
}

static void updateCurrentScreen() {
  switch (currentScreen) {
    case SCR_HOME:            ScreenHome::update();           break;
    case SCR_ISOTOPES:        ScreenIsotopes::update();        break;
    case SCR_SPECTRUM:        ScreenSpectrum::update();        break;
    case SCR_ISOTOPE_DETAIL:  ScreenIsotopeDetail::update();   break;
    default: break;
  }
}

void setup() {
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
  TouchHandler::init(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_WIDTH, SCREEN_HEIGHT);
  TouchFeedback::init(gfx);
  PageTransitions::init(gfx);
  PageTransitions::setDuration(200);

  goToScreen(SCR_HOME, TRANSITION_NONE);
}

void loop() {
  TouchHandler::update();
  bool transitionActive = PageTransitions::update();
  TouchFeedback::update();

  bool onSwipeableScreen = (currentScreen == SCR_HOME || currentScreen == SCR_ISOTOPES || currentScreen == SCR_SPECTRUM);

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
        consumed = ScreenHome::handleTap(x, y);
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
      } else if (currentScreen == SCR_ISOTOPE_DETAIL) {
        if (ScreenIsotopeDetail::handleTap(x, y)) {
          goToScreen(SCR_ISOTOPES, TRANSITION_SLIDE_RIGHT);
          consumed = true;
        }
      }

      if (consumed) TouchFeedback::trigger(x, y);
      TouchHandler::resetGestures();
    }
  }

  BleSource::tick();

  static unsigned long lastUiUpdate = 0;
  unsigned long now = millis();
  if (!transitionActive && now - lastUiUpdate >= 500) {
    lastUiUpdate = now;
    updateCurrentScreen();
  }

  delay(5);
}
