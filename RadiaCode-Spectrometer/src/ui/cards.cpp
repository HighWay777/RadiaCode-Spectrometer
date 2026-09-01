#include "cards.h"
#include "ui_theme.h"
#include <string.h>

static Arduino_GFX* g_gfx = nullptr;

namespace Cards {

void init(Arduino_GFX* display) { g_gfx = display; }

static uint16_t cardBg()      { return currentTheme ? currentTheme->card : 0x0000; }
static uint16_t cardBorder()  { return currentTheme ? currentTheme->cardBorder : 0xFFFF; }
static uint16_t textColor()   { return currentTheme ? currentTheme->text : 0xFFFF; }
static uint16_t textSecColor(){ return currentTheme ? currentTheme->textSecondary : 0x8410; }
static uint16_t highlightColor() { return currentTheme ? currentTheme->highlight : 0xFFE0; }

// Built-in GFX font: 5x7 glyph + 1px spacing = 6px wide, 8px tall per size unit.
int16_t textWidth(const char* text, uint8_t textSize) {
  return (int16_t)(strlen(text) * 6 * textSize);
}

void leftText(int16_t x, int16_t y, const char* text, uint8_t textSize, uint16_t color) {
  if (!g_gfx) return;
  g_gfx->setTextSize(textSize);
  g_gfx->setTextColor(color);
  g_gfx->setCursor(x, y);
  g_gfx->print(text);
}

void centeredText(int16_t cx, int16_t y, const char* text, uint8_t textSize, uint16_t color) {
  int16_t w = textWidth(text, textSize);
  leftText(cx - w / 2, y, text, textSize, color);
}

void frame(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, bool highlighted) {
  if (!g_gfx) return;
  const int16_t radius = 8;
  g_gfx->fillRoundRect(x, y, w, h, radius, cardBg());
  uint16_t borderColor = highlighted ? highlightColor() : cardBorder();
  g_gfx->drawRoundRect(x, y, w, h, radius, borderColor);
  if (highlighted) {
    g_gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, radius - 1, borderColor);
  }
  if (title) {
    leftText(x + 10, y + 8, title, 1, textSecColor());
  }
}

void statValueArea(int16_t x, int16_t y, int16_t w, int16_t h, const char* value, const char* unit, uint16_t valueColor) {
  if (!g_gfx) return;
  g_gfx->fillRect(x, y, w, h, cardBg());
  leftText(x, y, value, 3, valueColor);
  if (unit && unit[0]) {
    int16_t vw = textWidth(value, 3);
    leftText(x + vw + 6, y + 14, unit, 2, textSecColor());
  }
}

// Scale an RGB565 colour by num/den, per channel.
static uint16_t scale565(uint16_t c, uint8_t num, uint8_t den) {
  uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  r = (uint16_t)(r * num / den);
  g = (uint16_t)(g * num / den);
  b = (uint16_t)(b * num / den);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
            uint16_t fillColor, bool enabled, bool pressed) {
  if (!g_gfx) return;
  const int16_t radius = 10;

  uint16_t fill = pressed ? scale565(fillColor, 1, 2) : fillColor;
  g_gfx->fillRoundRect(x, y, w, h, radius, fill);

  uint16_t border = pressed ? highlightColor() : (enabled ? textColor() : cardBorder());
  g_gfx->drawRoundRect(x, y, w, h, radius, border);
  if (pressed) g_gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, radius - 1, border);

  centeredText(x + w / 2, y + h / 2 - 8, label, 2, enabled ? textColor() : textSecColor());
}

void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction, uint16_t color) {
  if (!g_gfx) return;
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  g_gfx->fillRect(x, y, w, h, cardBorder());
  int16_t fillW = (int16_t)(w * fraction);
  if (fillW > 0) g_gfx->fillRect(x, y, fillW, h, color);
}

} // namespace Cards
