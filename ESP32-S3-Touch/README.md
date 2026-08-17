# ESP32-S3 + JC4827W543 (NV3041A QSPI) + GT911 Touch

A fully replicable guide to bring up the JC4827W543 4.3" 480x272 IPS display (NV3041A) via QSPI on ESP32‑S3 with GT911 touch. This README explains exactly what to wire, how to configure PlatformIO, and the precise source patch you must apply to the Arduino_GFX NV3041A driver so partial updates work reliably.

## Hardware wiring

- Board: ESP32‑S3 DevKitC‑1
- Display: JC4827W543, NV3041A controller, QSPI (4‑line)
- Touch: GT911 over I2C
- Backlight: PWM on GPIO1

QSPI display pins
- CS  -> GPIO45
- SCK -> GPIO47
- IO0 (MOSI) -> GPIO21
- IO1 (MISO) -> GPIO48
- IO2 (WP)   -> GPIO40
- IO3 (HD)   -> GPIO39
- RST -> GPIO17 (recommended, used for hard reset)
- BLK -> GPIO1 (driven with LEDC PWM)

GT911 touch pins
- SDA -> GPIO8
- SCL -> GPIO4
- INT -> GPIO3
- RST -> GPIO38

Panel resolution: 480x272 (landscape by default)

## PlatformIO configuration

In your new project's `platformio.ini` use:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 460800

; Make sure PlatformIO sees the patched libraries folder you use
lib_extra_dirs = Libraries

lib_deps =
    bblanchon/ArduinoJson @ ^6.21.3

build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=1

; Avoid duplicate or unpatched Arduino_GFX copies
; If you keep only one patched copy in Libraries/, you can skip this.
; Otherwise, explicitly ignore other names that might resolve:
; lib_ignore = GFX Library for Arduino, Arduino_GFX
```

Important
- Keep only one copy of Arduino_GFX in your include path, and ensure it contains the patch below. The safest route is to copy this repo’s `Libraries/Arduino_GFX-1.4.4/` into your new project’s `Libraries/` and remove other Arduino_GFX copies from `lib/` or `.pio/libdeps`.

## Mandatory driver patch (NV3041A over QSPI)

Symptom without this patch: only the first line updates or you see flicker/tearing on sub‑rectangle updates. Cause: the address window commands must use contiguous 16‑bit transfers over QSPI for this panel/controller.

Patched file path (use the one your build actually includes):
- Libraries/Arduino_GFX-1.4.4/src/display/Arduino_NV3041A.cpp

Exact function body required for reliable updates:

```cpp
void Arduino_NV3041A::writeAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
  if ((x != _currentX) || (w != _currentW))
  {
    _currentX = x;
    _currentW = w;
    x += _xStart;
    // Use contiguous 16-bit writes for QSPI reliability
    _bus->writeC8D16D16(NV3041A_CASET, x, x + w - 1);
  }

  if ((y != _currentY) || (h != _currentH))
  {
    _currentY = y;
    _currentH = h;
    y += _yStart;
    // Use contiguous 16-bit writes for QSPI reliability
    _bus->writeC8D16D16(NV3041A_RASET, y, y + h - 1);
  }

  _bus->writeCommand(NV3041A_RAMWR); // write to RAM
}
```

Notes
- Some upstream variants used split writes (e.g., `writeC8D16D16Split`). Replace those with the exact `writeC8D16D16` calls shown.
- Ensure the panel exits sleep and turns on. The stock init table in Arduino_GFX 1.4.4 already issues SLPOUT (0x11) and DISPON (0x29). If your copy does not turn on the panel, add this to `tftInit()` after `invertDisplay(false);`:

```cpp
  _bus->sendCommand(NV3041A_DISPON);
  delay(NV3041A_SLPOUT_DELAY);
```

## Minimal application setup (copyable)

Display and bus construction (QSPI pinout shown above):

```cpp
#include <Arduino_GFX_Library.h>

static const int8_t PIN_CS  = 45;
static const int8_t PIN_SCK = 47;
static const int8_t PIN_D0  = 21; // IO0 (MOSI)
static const int8_t PIN_D1  = 48; // IO1 (MISO)
static const int8_t PIN_D2  = 40; // IO2 (WP)
static const int8_t PIN_D3  = 39; // IO3 (HD)
static const int8_t PIN_RST = 17; // panel reset

Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_CS, PIN_SCK, PIN_D0, PIN_D1, PIN_D2, PIN_D3);
Arduino_GFX *gfx = new Arduino_NV3041A(bus, PIN_RST /* RST */, 0 /* rotation */, true /* IPS */);

void setup() {
  Serial.begin(115200);
  // Optional hard reset pulse before begin
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(10);
  digitalWrite(PIN_RST, LOW);  delay(20);
  digitalWrite(PIN_RST, HIGH); delay(120);

  // Start at a conservative QSPI clock (16 MHz). Increase to 24–32 MHz if stable.
  if (!gfx->begin(16000000)) {
    Serial.println("gfx->begin() failed");
    for(;;) delay(1000);
  }
  gfx->invertDisplay(false);
  gfx->setRotation(0); // landscape 480x272
  gfx->fillScreen(BLACK);
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->println("NV3041A QSPI OK");
}

void loop() {}
```

Backlight PWM on GPIO1 (LEDC):

```cpp
#define LCD_BL 1
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_12_BIT  12
#define LEDC_BASE_FREQ     5000

static void setBrightness(uint8_t value) {
  uint32_t duty = (1 << LEDC_TIMER_12_BIT) - 1;
  duty = duty * value / 255;
  ledcWrite(LEDC_CHANNEL_0, duty);
}

void setup() {
  // ...after Serial.begin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LCD_BL, LEDC_CHANNEL_0);
  setBrightness(255);
  // ...then display begin()
}
```

GT911 touch bring‑up (pins above; rotation to match display):

```cpp
#include "Touch_GT911.h"

static const uint8_t TOUCH_SDA = 8, TOUCH_SCL = 4, TOUCH_INT = 3, TOUCH_RST = 38;
static const uint16_t SCREEN_WIDTH = 480, SCREEN_HEIGHT = 272;
Touch_GT911 touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_WIDTH, SCREEN_HEIGHT);

static bool i2c_device_present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void touch_begin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);
  uint8_t addr = i2c_device_present(GT911_ADDR1) ? GT911_ADDR1 : GT911_ADDR2;
  touch.begin(addr);
  touch.setRotation(ROTATION_NORMAL); // matches display rotation(0)
}
```

## Build, upload, monitor

```powershell
pio run
pio run -t upload
pio device monitor
```

## Troubleshooting checklist

- Only the top line updates or sub‑rect draws flicker: your NV3041A driver isn’t patched. Apply the exact `writeAddrWindow()` above and ensure the patched Arduino_GFX copy is the one actually compiled. Delete duplicate Arduino_GFX copies from `lib/` and `.pio/libdeps/*` if needed.
- Random artifacts/tearing: lower QSPI speed in `gfx->begin(...)` to 12–16 MHz; avoid continuous full‑screen redraws.
- Black screen: verify wiring and that the panel is out of sleep and display is on (SLPOUT/DISPON). If your library doesn’t issue DISPON in the init table, add the two lines shown to `tftInit()`.
- Touch coordinates flipped: switch `touch.setRotation(...)` among ROTATION_NORMAL/RIGHT/INVERTED/LEFT to match your UI.

## What this project includes

- A patched Arduino_GFX 1.4.4 NV3041A driver in `Libraries/Arduino_GFX-1.4.4/` with contiguous 16‑bit address window writes.
- Working QSPI pin map and LEDC backlight control.
- Optional GT911 touch with a tested rotation mapping.

Use the code and patch above in any new ESP32‑S3 project with this panel to get a reliable, flicker‑free UI.
