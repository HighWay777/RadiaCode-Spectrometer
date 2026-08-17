// ESP32-S3 + JC4827W543 (NV3041A QSPI, 480x272) + GT911 touch
// Minimal, cleaned bring-up. Display/touch driver layer only —
// see ../CLAUDE.md (project root) for the RadiaCode spectrometer UI plan
// that gets built on top of this.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "ui/touch_handler.h"
#include "ui/touch_feedback.h"
#include "ui/page_transitions.h"
#include "ui/ui_theme.h"

UITheme* currentTheme = nullptr; // set once the spectrometer UI defines a real theme

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

static void drawPlaceholder() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 90);
  gfx->println("RadiaCode Spectrometer UI");
  gfx->setTextSize(2);
  gfx->setCursor(20, 140);
  gfx->println("Display + touch driver ready.");
  gfx->setCursor(20, 165);
  gfx->println("Touch the screen to test.");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nESP32-S3-Touch display/touch bring-up");

  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LCD_BL, LEDC_CHANNEL_0);
  setBrightness(200);

  if (!gfx->begin(16000000)) {
    Serial.println("Display initialization failed!");
    while (1) delay(1000);
  }
  gfx->invertDisplay(false);
  gfx->setRotation(0);

  drawPlaceholder();

  TouchHandler::init(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_WIDTH, SCREEN_HEIGHT);
  TouchFeedback::init(gfx);
  PageTransitions::init(gfx);
}

void loop() {
  TouchHandler::update();
  TouchFeedback::update();
  PageTransitions::update();

  if (TouchHandler::wasTapped()) {
    uint16_t x = TouchHandler::getTouchX();
    uint16_t y = TouchHandler::getTouchY();
    Serial.printf("Tap at (%u, %u)\n", x, y);
    TouchFeedback::trigger(x, y);
    TouchHandler::resetGestures();
  }

  delay(5);
}
