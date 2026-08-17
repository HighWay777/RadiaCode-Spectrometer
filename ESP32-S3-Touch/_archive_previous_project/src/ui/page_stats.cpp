#include "page_stats.h"
#include "ui_framework.h"
#include "page_home.h"
#include "style_overrides.h"
#include <math.h>

static void drawMetric(Arduino_GFX* d, int16_t x, int16_t y, const char* title, const String& value, uint16_t labelColor, uint16_t valueColor) {
  d->setFont(nullptr);
  uint8_t labelSize = uiScaleText(1);
  uint8_t valueSize = uiScaleText(2);
  d->setTextSize(labelSize);
  d->setTextColor(labelColor);
  d->setCursor(x, y);
  d->print(title);
  int16_t valueY = y + 8 * labelSize + 4;
  d->setTextSize(valueSize);
  d->setTextColor(valueColor);
  d->setCursor(x, valueY);
  d->print(value);
}

// Helper to draw a styled card with StyleOverride support
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

void drawPageStatsEnergy(Arduino_GFX* gfx, AppConfig* config) {
  gfx->setTextWrap(false);
  drawHeader("Energy");

  WeatherData* wd = getCurrentWeatherData();
  int m = 10; int y = 24 + m; int h = 80; int w = (SCREEN_WIDTH - m*3)/2; int x = m;
  
  // Draw cards with StyleOverrides support
  drawStyledCard(gfx, x, y, w, h, "Efficiency", "stats.efficiency");
  drawStyledCard(gfx, x + w + m, y, w, h, "Heat Loss", "stats.loss");
  drawStyledCard(gfx, x, y + h + m, w, h, "kWh Total", "stats.kwh");
  drawStyledCard(gfx, x + w + m, y + h + m, w, h, "Cost Total", "stats.cost");
  drawStyledCard(gfx, x, y + (h + m)*2, w, h, "Daily kWh", "stats.dailyKwh");
  drawStyledCard(gfx, x + w + m, y + (h + m)*2, w, h, "Daily Cost", "stats.dailyCost");

  if (!wd) return;
  char buf[32];
  
  if (!StyleOverrides::isHidden("stats.efficiency")) {
    if (!isnan(wd->efficiency) && wd->efficiency >= 0) snprintf(buf, sizeof(buf), "%.1f %%", wd->efficiency);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.efficiency", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.efficiency", currentTheme->text);
    drawMetric(gfx, x + 12, y + 28, "Boiler Efficiency", buf, labelCol, valueCol);
  }

  if (!StyleOverrides::isHidden("stats.loss")) {
    if (!isnan(wd->heatLossRate)) snprintf(buf, sizeof(buf), "%.2f", wd->heatLossRate);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.loss", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.loss", currentTheme->text);
    drawMetric(gfx, x + w + m + 12, y + 28, "Heat Loss Rate", buf, labelCol, valueCol);
  }

  if (!StyleOverrides::isHidden("stats.kwh")) {
    if (!isnan(wd->kwhTotal)) snprintf(buf, sizeof(buf), "%.3f kWh", wd->kwhTotal);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.kwh", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.kwh", currentTheme->text);
    drawMetric(gfx, x + 12, y + h + m + 28, "Total Energy", buf, labelCol, valueCol);
  }

  if (!StyleOverrides::isHidden("stats.cost")) {
    if (!isnan(wd->costTotal)) snprintf(buf, sizeof(buf), "%.2f", wd->costTotal);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.cost", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.cost", currentTheme->text);
    drawMetric(gfx, x + w + m + 12, y + h + m + 28, "Total Cost", buf, labelCol, valueCol);
  }

  if (!StyleOverrides::isHidden("stats.dailyKwh")) {
    if (!isnan(wd->dailyKWh)) snprintf(buf, sizeof(buf), "%.3f kWh", wd->dailyKWh);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.dailyKwh", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.dailyKwh", currentTheme->text);
    drawMetric(gfx, x + 12, y + (h + m)*2 + 28, "Today Energy", buf, labelCol, valueCol);
  }

  if (!StyleOverrides::isHidden("stats.dailyCost")) {
    if (!isnan(wd->dailyCost)) snprintf(buf, sizeof(buf), "%.2f", wd->dailyCost);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t labelCol = StyleOverrides::getTextColor("stats.dailyCost", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("stats.dailyCost", currentTheme->text);
    drawMetric(gfx, x + w + m + 12, y + (h + m)*2 + 28, "Today Cost", buf, labelCol, valueCol);
  }
}
