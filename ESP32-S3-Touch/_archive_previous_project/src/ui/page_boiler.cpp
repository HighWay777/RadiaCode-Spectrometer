#include "page_boiler.h"
#include "ui_framework.h"
#include "page_home.h"
#include "style_overrides.h"
#include <math.h>

static void drawLabeledValue(Arduino_GFX* d, int16_t x, int16_t y, const char* label, const String& value, uint16_t colorLabel, uint16_t colorValue) {
  d->setFont(nullptr);
  uint8_t labelSize = uiScaleText(1);
  uint8_t valueSize = uiScaleText(2);
  d->setTextSize(labelSize);
  d->setTextColor(colorLabel);
  d->setCursor(x, y);
  d->print(label);
  int16_t valueY = y + 8 * labelSize + 4;
  d->setTextSize(valueSize);
  d->setTextColor(colorValue);
  d->setCursor(x, valueY);
  d->print(value);
}

// Helper to draw a card with StyleOverride support
static void drawStyledCard(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, 
                           const char* title, const char* elementId) {
  if (StyleOverrides::isHidden(elementId)) return;
  
  uint16_t bgColor = StyleOverrides::getColor(elementId, currentTheme->card);
  uint16_t textColor = StyleOverrides::getTextColor(elementId, currentTheme->text);
  
  gfx->fillRoundRect(x, y, w, h, 8, bgColor);
  gfx->drawRoundRect(x, y, w, h, 8, currentTheme->cardBorder);
  
  if (title && title[0]) {
    gfx->setFont(nullptr);
    gfx->setTextSize(uiScaleText(1));
    gfx->setTextColor(textColor);
    gfx->setCursor(x + 8, y + 10);
    gfx->print(title);
  }
}

void drawPageBoiler(Arduino_GFX* gfx, AppConfig* config) {
  gfx->setTextWrap(false);
  drawHeader("Boiler");

  WeatherData* wd = getCurrentWeatherData();
  int m = 10; int y0 = 24 + m; int w = (SCREEN_WIDTH - m*3)/2; int h1 = 100; int x0 = m;

  // Current vs Target cards - with StyleOverrides support
  drawStyledCard(gfx, x0, y0, w, h1, "Current", "boiler.current");
  drawStyledCard(gfx, x0 + w + m, y0, w, h1, "Target", "boiler.target");

  char buf[32];
  if (!StyleOverrides::isHidden("boiler.current")) {
    if (wd && !isnan(wd->currentTemp)) { snprintf(buf, sizeof(buf), "%.1f C", wd->currentTemp); }
    else { snprintf(buf, sizeof(buf), "--.- C"); }
    uint16_t labelColor = StyleOverrides::getTextColor("boiler.current", currentTheme->textSecondary);
    uint16_t valueColor = StyleOverrides::getTextColor("boiler.current", currentTheme->text);
    drawLabeledValue(gfx, x0 + 12, y0 + 30, "Temp", String(buf), labelColor, valueColor);
  }

  if (!StyleOverrides::isHidden("boiler.target")) {
    if (wd && !isnan(wd->targetTemp)) { snprintf(buf, sizeof(buf), "%.1f C", wd->targetTemp); }
    else { snprintf(buf, sizeof(buf), "--.- C"); }
    uint16_t labelColor = StyleOverrides::getTextColor("boiler.target", currentTheme->textSecondary);
    uint16_t valueColor = StyleOverrides::getTextColor("boiler.target", currentTheme->text);
    drawLabeledValue(gfx, x0 + w + m + 12, y0 + 30, "Setpoint", String(buf), labelColor, valueColor);
  }

  // Delta card
  int y1 = y0 + h1 + m; int h2 = 70;
  drawStyledCard(gfx, x0, y1, w, h2, "Delta", "boiler.delta");
  if (!StyleOverrides::isHidden("boiler.delta")) {
    float delta = (wd && !isnan(wd->currentTemp) && !isnan(wd->targetTemp)) ? (wd->currentTemp - wd->targetTemp) : NAN;
    uint16_t dColor = (!isnan(delta) && delta >= 0) ? currentTheme->warm : currentTheme->cool;
    // Allow textColor override for delta
    dColor = StyleOverrides::getTextColor("boiler.delta", dColor);
    if (!isnan(delta)) { snprintf(buf, sizeof(buf), "%+.1f C", delta); }
    else { snprintf(buf, sizeof(buf), "--.- C"); }
    drawLabeledValue(gfx, x0 + 12, y1 + 26, "Current-Target", String(buf), currentTheme->textSecondary, dColor);
  }

  // Mode + Heater ON/OFF
  drawStyledCard(gfx, x0 + w + m, y1, w, h2, "Status", "boiler.status");
  if (!StyleOverrides::isHidden("boiler.status")) {
    const char* modeStr = "UNKNOWN";
    if (wd) { switch (wd->heaterControlMode) { case 0: modeStr = "OFF"; break; case 1: modeStr = "MANUAL"; break; case 2: modeStr = "AUTO"; break; case 3: modeStr = "AI"; break; } }
    uint16_t statusTextColor = StyleOverrides::getTextColor("boiler.status", currentTheme->text);
    drawLabeledValue(gfx, x0 + w + m + 12, y1 + 26, "Mode", String(modeStr), currentTheme->textSecondary, statusTextColor);
    drawLabeledValue(gfx, x0 + w + m + w/2, y1 + 26, "Heater", wd && wd->heaterOn ? String("ON") : String("OFF"), currentTheme->textSecondary, wd && wd->heaterOn ? currentTheme->warm : currentTheme->textSecondary);
  }

  // Extended card with day/night/boost (clamped to screen)
  int y2 = y1 + h2 + m;
  // ensure bottom margin with header (24px) and 2*m + small padding; clamp height strictly
  int bottomPadding = 8;
  int available = SCREEN_HEIGHT - y2 - m - bottomPadding;
  if (available < 40) available = 40;
  int h3 = available;
  drawStyledCard(gfx, x0, y2, w*2 + m, h3, "Schedule / Limits", "boiler.schedule");
  if (!StyleOverrides::isHidden("boiler.schedule") && wd) {
    int lineY = y2 + 28; int maxY = y2 + h3 - 10;
    uint16_t schedTextColor = StyleOverrides::getTextColor("boiler.schedule", currentTheme->text);
    auto put = [&](int16_t lx, const char* lab, const char* val){ if (lineY + 16 < maxY) drawLabeledValue(gfx, lx, lineY, lab, val, currentTheme->textSecondary, schedTextColor); };
    // Day/Night/Boost
    if (!isnan(wd->dayTemp)) { snprintf(buf, sizeof(buf), "%.1f C", wd->dayTemp); put(x0+12, "Day", buf); }
    else { put(x0+12, "Day", "--.- C"); }
    if (!isnan(wd->nightTemp)) { snprintf(buf, sizeof(buf), "%.1f C", wd->nightTemp); put(x0+116, "Night", buf); }
    else { put(x0+116, "Night", "--.- C"); }
    if (!isnan(wd->boostTemp)) { snprintf(buf, sizeof(buf), "%.1f C", wd->boostTemp); put(x0+220, "Boost", buf); }
    else { put(x0+220, "Boost", "--.- C"); }
    // Source (truncate harder and guard last line)
    String src = wd->targetSource.length() ? wd->targetSource : String("-");
    if (src.length() > 10) src = src.substring(0, 10) + "…";
    if (lineY + 16 < maxY) drawLabeledValue(gfx, x0 + 324, lineY, "Source", src, currentTheme->textSecondary, schedTextColor);
  }
}
