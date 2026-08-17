#include "custom_pages.h"
#include <vector>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "page_home.h"
#include "page_boiler.h"
#include "page_stats.h"
#include "weather_icons.h"
#include <Fonts/GFXFF/FreeSans9pt7b.h>
#include <Fonts/GFXFF/FreeSans12pt7b.h>
#include <Fonts/GFXFF/FreeSansBold12pt7b.h>
#include <Fonts/GFXFF/FreeSansBold18pt7b.h>
#include <Fonts/GFXFF/FreeSansBold24pt7b.h>

static std::vector<CustomPage> PAGES;
static std::vector<CustomPage> OVERLAYS; // per-builtin overlays loaded from overlayN.json

static String defaultPagesJson() {
  // One sample custom page overlaying Home with a big label
  StaticJsonDocument<1024> doc;
  JsonArray pages = doc.createNestedArray("pages");
  JsonObject p = pages.createNestedObject();
  p["name"] = "My Custom";
  p["base"] = (uint8_t)PAGE_HOME;
  JsonArray widgets = p.createNestedArray("widgets");
  JsonObject w1 = widgets.createNestedObject();
  w1["type"] = "value";
  w1["binding"] = "currentTemp";
  w1["color"] = 0xF800; // red
  w1["fontSize"] = 3;
  w1["x"] = 320; w1["y"] = 40; w1["w"] = 140; w1["h"] = 40;
  String out; serializeJson(doc, out); return out;
}

static bool loadPagesJson(String &json) {
  if (!SPIFFS.exists("/pages.json")) {
    json = defaultPagesJson();
    File f = SPIFFS.open("/pages.json", FILE_WRITE);
    if (f) { f.print(json); f.close(); }
    return true;
  }
  File f = SPIFFS.open("/pages.json", FILE_READ);
  if (!f) return false;
  json = f.readString();
  f.close();
  return true;
}

static uint16_t toColor(JsonVariant v) {
  // Handle numeric RGB565 color directly
  if (v.is<uint16_t>()) {
    return v.as<uint16_t>();
  }
  if (v.is<int>()) {
    return (uint16_t)v.as<int>();
  }
  
  // Handle hex string like "#RRGGBB" or "#RGB"
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s || s[0] != '#') {
      return currentTheme ? currentTheme->text : 0xFFFF;
    }
    
    uint32_t rgb = 0;
    size_t len = strlen(s + 1);
    
    if (len == 6) {
      // Full #RRGGBB format
      rgb = (uint32_t)strtoul(s + 1, nullptr, 16);
    } else if (len == 3) {
      // Short #RGB format - expand to #RRGGBB
      char expanded[7];
      expanded[0] = s[1]; expanded[1] = s[1];
      expanded[2] = s[2]; expanded[3] = s[2];
      expanded[4] = s[3]; expanded[5] = s[3];
      expanded[6] = '\0';
      rgb = (uint32_t)strtoul(expanded, nullptr, 16);
    } else if (len == 8 && s[1] == '0' && s[2] == 'x') {
      // Handle 0xRRGGBB hex literal (8 chars including '#0x')
      char noAlpha[7];
      strncpy(noAlpha, s + 1, 6);
      noAlpha[6] = '\0';
      rgb = (uint32_t)strtoul(noAlpha, nullptr, 16);
    } else {
      // Invalid format
      return currentTheme ? currentTheme->text : 0xFFFF;
    }
    
    // Convert 24-bit RGB to RGB565
    // RGB888: RRRRRRRRGGGGGGGGBBBBBBBB
    // RGB565: RRRRRGGGGGGBBBBB
    uint16_t r = (rgb >> 19) & 0x1F;  // Take top 5 bits of R
    uint16_t g = (rgb >> 10) & 0x3F;  // Take top 6 bits of G
    uint16_t b = (rgb >> 3) & 0x1F;   // Take top 5 bits of B
    return (r << 11) | (g << 5) | b;
  }
  
  // Default to theme text color
  return currentTheme ? currentTheme->text : 0xFFFF;
}

void customPagesInit() {
  customPagesReload();
}

void customPagesReload() {
  PAGES.clear();
  OVERLAYS.clear();
  String json;
  if (!loadPagesJson(json)) {
    Serial.println("[CUSTOM_PAGES] Failed to load pages.json");
    return;
  }
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("[CUSTOM_PAGES] JSON parse error: %s\n", err.c_str());
    return;
  }
  JsonArray pages = doc["pages"].as<JsonArray>();
  uint8_t baseId = PAGE_COUNT;
  for (JsonObject p : pages) {
    CustomPage cp; 
    cp.id = baseId++; 
    cp.name = p["name"].as<String>();
    cp.base = p.containsKey("base") ? p["base"].as<uint8_t>() : 255;
    JsonArray ws = p["widgets"].as<JsonArray>();
    for (JsonObject w : ws) {
      CustomWidget cw;
      cw.type = w["type"].as<String>();
      cw.binding = w["binding"].as<String>();
      cw.text = w["text"].as<String>();
      cw.color = toColor(w["color"]);
      cw.textColor = w.containsKey("textColor") ? toColor(w["textColor"]) : (currentTheme ? currentTheme->text : 0xFFFF);
      cw.fontSize = w["fontSize"].as<uint8_t>();
      cw.x = w["x"].as<int>(); 
      cw.y = w["y"].as<int>();
      cw.w = w["w"].as<int>(); 
      cw.h = w["h"].as<int>();
      cp.widgets.push_back(cw);
    }
    PAGES.push_back(cp);
  }
  
  // Load overlays for each builtin page if present
  for (uint8_t base = 0; base < PAGE_COUNT; ++base) {
    char path[24]; 
    snprintf(path, sizeof(path), "/overlay%u.json", base);
    if (!SPIFFS.exists(path)) continue;
    
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) {
      Serial.printf("[OVERLAY%u] Failed to open file\n", base);
      continue;
    }
    
    DynamicJsonDocument odoc(4096);
    DeserializationError err = deserializeJson(odoc, f);
    f.close();
    
    if (err) {
      Serial.printf("[OVERLAY%u] JSON parse error: %s\n", base, err.c_str());
      continue;
    }
    
    // DEBUG: Print the entire overlay JSON
    Serial.printf("[OVERLAY%u] Loaded successfully, parsing widgets...\n", base);
    
    CustomPage cp; 
    cp.id = 254; 
    cp.name = String("overlay") + base; 
    cp.base = base;
    
    JsonArray ws = odoc["widgets"].as<JsonArray>();
    for (JsonObject w : ws) {
      // DEBUG: Print each widget's color field
      const char* typeStr = w["type"].is<const char*>() ? w["type"].as<const char*>() : "?";
      const char* colorStr = "NULL";
      if (w["color"].is<const char*>()) {
        colorStr = w["color"].as<const char*>();
      } else if (w["color"].is<uint16_t>()) {
        colorStr = "UINT16";
      }
      Serial.printf("  Widget: type=%s, color=%s\n", typeStr, colorStr);
      
      CustomWidget cw; 
      cw.type = w["type"].as<String>(); 
      cw.binding = w["binding"].as<String>(); 
      cw.text = w["text"].as<String>();
      cw.color = toColor(w["color"]);
      cw.textColor = w.containsKey("textColor") ? toColor(w["textColor"]) : (currentTheme ? currentTheme->text : 0xFFFF);
      cw.fontSize = w["fontSize"].as<uint8_t>();
      cw.x = w["x"].as<int>(); 
      cw.y = w["y"].as<int>(); 
      cw.w = w["w"].as<int>(); 
      cw.h = w["h"].as<int>();
      cp.widgets.push_back(cw);
    }
    OVERLAYS.push_back(cp);
    Serial.printf("[OVERLAY%u] Loaded %d widgets\n", base, cp.widgets.size());
  }
  
  Serial.printf("[CUSTOM_PAGES] Reload complete: %d custom pages, %d overlays\n", PAGES.size(), OVERLAYS.size());
}

uint8_t customPagesCount() { return (uint8_t)PAGES.size(); }

bool isCustomPageId(uint8_t pageId) {
  return pageId >= PAGE_COUNT && (pageId - PAGE_COUNT) < PAGES.size();
}

static String bindingToString(const String& binding) {
  WeatherData* wd = getCurrentWeatherData();
  if (!wd) return String("");
  
  // Temperature fields
  if (binding == "currentTemp") return String(wd->currentTemp, 1) + " °C";
  if (binding == "targetTemp") return String(wd->targetTemp, 1) + " °C";
  if (binding == "dayTemp") return String(wd->dayTemp, 1) + " °C";
  if (binding == "nightTemp") return String(wd->nightTemp, 1) + " °C";
  if (binding == "boostTemp") return String(wd->boostTemp, 1) + " °C";
  if (binding == "extTemp") return String(wd->extTemp, 1) + " °C";
  
  // Weather fields
  if (binding == "humidity") return String(wd->humidity) + "%";
  if (binding == "pressure") return String(wd->pressure) + " hPa";
  if (binding == "windKph") return String(wd->windKph, 1) + " km/h";
  if (binding == "solar") return String(wd->solar) + " W/m²";
  if (binding == "weatherCode") return String(wd->weatherCode);
  if (binding == "weatherDesc") return wd->weatherDesc;
  
  // Energy/Efficiency fields
  if (binding == "efficiency") return String(wd->efficiency, 1) + "%";
  if (binding == "heatLossRate") return String(wd->heatLossRate, 2) + " W";
  if (binding == "kwhTotal") return String(wd->kwhTotal, 3) + " kWh";
  if (binding == "costTotal") return String(wd->costTotal, 2) + " €";
  if (binding == "dailyKWh") return String(wd->dailyKWh, 3) + " kWh";
  if (binding == "costToday") return String(wd->costToday, 2) + " €";
  if (binding == "dailyCost") return String(wd->dailyCost, 2) + " €";
  
  // Boolean/State fields
  if (binding == "heaterOn") return wd->heaterOn ? "ON" : "OFF";
  if (binding == "manualOverride") return wd->manualOverride ? "YES" : "NO";
  if (binding == "aiEnabled") return wd->aiEnabled ? "ENABLED" : "DISABLED";
  if (binding == "isNightTime") return wd->isNightTime ? "NIGHT" : "DAY";
  if (binding == "isBoostTime") return wd->isBoostTime ? "BOOST" : "NORMAL";
  
  // Mode/Status fields
  if (binding == "heaterControlMode") {
    switch (wd->heaterControlMode) {
      case 0: return "MANUAL";
      case 1: return "SCHEDULE";
      case 2: return "AUTO";
      default: return "UNKNOWN";
    }
  }
  if (binding == "targetSource") return wd->targetSource;
  if (binding == "wifiRSSI") return String(wd->wifiRSSI) + " dBm";
  
  // Unknown binding - return empty
  return String("");
}

static void drawWidget(const CustomWidget& w, Arduino_GFX* d) {
  struct TextMetrics {
    int16_t x1;
    int16_t y1;
    uint16_t w;
    uint16_t h;
  };

  auto selectFont = [](uint8_t level, bool bold = false) -> const GFXfont* {
    if (bold) {
      if (level >= 5) return &FreeSansBold24pt7b;
      if (level >= 4) return &FreeSansBold18pt7b;
      return &FreeSansBold12pt7b;
    }
    if (level >= 4) return &FreeSansBold18pt7b;
    if (level >= 2) return &FreeSans12pt7b;
    return &FreeSans9pt7b;
  };

  auto measureText = [](Arduino_GFX* d, const GFXfont* font, const String& text) {
    TextMetrics m{0, 0, 0, 0};
    d->setFont(font);
    d->setTextSize(1);
    d->getTextBounds(text.c_str(), 0, 0, &m.x1, &m.y1, &m.w, &m.h);
    return m;
  };

  auto drawTextTopLeft = [&](Arduino_GFX* d, const GFXfont* font, uint16_t color, int16_t x, int16_t y, const String& text) {
    TextMetrics m = measureText(d, font, text);
    int16_t baseline = y - m.y1;
    d->setFont(font);
    d->setTextSize(1);
    d->setTextColor(color);
    d->setCursor(x, baseline);
    d->print(text);
  };

  auto drawTextCentered = [&](Arduino_GFX* d, const GFXfont* font, uint16_t color,
                              int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH,
                              const String& text) {
    TextMetrics m = measureText(d, font, text);
    int16_t left = areaX + (int16_t)((areaW - (int16_t)m.w) / 2);
    if (left < areaX) left = areaX;
    int16_t top = areaY + (int16_t)((areaH - (int16_t)m.h) / 2);
    if (top < areaY) top = areaY;
    int16_t baseline = top - m.y1;
    d->setFont(font);
    d->setTextSize(1);
    d->setTextColor(color);
    d->setCursor(left, baseline);
    d->print(text);
  };

  // Mask: draw an opaque rectangle to hide base elements
  if (w.type == "mask") {
    uint16_t col = (w.color ? w.color : 0x0000);
    d->fillRect(w.x, w.y, w.w, w.h, col);
    d->drawRect(w.x, w.y, w.w, w.h, currentTheme->cardBorder);
    return;
  }
  if (w.type == "label") {
    const GFXfont* font = selectFont(w.fontSize ? w.fontSize : 2, false);
    drawTextTopLeft(d, font, w.textColor ? w.textColor : currentTheme->text, w.x, w.y + 6, w.text);
  } else if (w.type == "value") {
    String v = bindingToString(w.binding);
    const GFXfont* font = selectFont(w.fontSize ? w.fontSize : 3, true);
    drawTextTopLeft(d, font, w.textColor ? w.textColor : currentTheme->text, w.x, w.y + 6, v.length() ? v : w.text);
  } else if (w.type == "panel") {
    uint16_t bg = w.color ? w.color : currentTheme->card;
    d->fillRoundRect(w.x, w.y, w.w, w.h, 18, bg);
    d->drawRoundRect(w.x, w.y, w.w, w.h, 18, currentTheme->cardBorder);
  } else if (w.type == "badge") {
    // filled rounded badge with value text
    uint16_t bg = w.color ? w.color : currentTheme->card;
    d->fillRoundRect(w.x, w.y, w.w, w.h, 8, bg);
    d->drawRoundRect(w.x, w.y, w.w, w.h, 8, currentTheme->cardBorder);
    String v = w.text.length() ? w.text : bindingToString(w.binding);
    const GFXfont* font = selectFont(w.fontSize ? w.fontSize : 2, false);
    drawTextCentered(d, font, w.textColor ? w.textColor : currentTheme->text, w.x + 6, w.y + 4, w.w - 12, w.h - 8, v);
  } else if (w.type == "weatherIcon") {
    WeatherData* wd = getCurrentWeatherData();
    int code = wd ? wd->weatherCode : 0;
    bool isNight = wd ? wd->isNightTime : false;
    int16_t size = min(w.w, w.h) - 12;
    if (size < 24) size = min(w.w, w.h) - 4;
    if (size < 16) size = min(w.w, w.h);
    int16_t iconX = w.x + (w.w - size) / 2;
    int16_t iconY = w.y + (w.h - size) / 2;
    // Use overlay color if specified, otherwise use theme
    uint16_t bg = w.color ? w.color : currentTheme->card;
    d->fillRoundRect(w.x, w.y, w.w, w.h, 16, bg);
    d->drawRoundRect(w.x, w.y, w.w, w.h, 16, currentTheme->cardBorder);
    drawWeatherIcon(d, iconX, iconY, size, code, isNight);
  } else if (w.type == "shape") {
    // Shape widget - rectangle, circle, or rounded rectangle
    uint16_t col = w.color ? w.color : currentTheme->card;
    // Draw filled shape (default to rect)
    d->fillRect(w.x, w.y, w.w, w.h, col);
  } else if (w.type == "button") {
    // Button widget - rounded rectangle with centered text
    uint16_t bg = w.color ? w.color : currentTheme->accent;
    d->fillRoundRect(w.x, w.y, w.w, w.h, 10, bg);
    d->drawRoundRect(w.x, w.y, w.w, w.h, 10, currentTheme->cardBorder);
    const GFXfont* font = selectFont(w.fontSize ? w.fontSize : 2, true);
    drawTextCentered(d, font, w.textColor ? w.textColor : WHITE, w.x, w.y, w.w, w.h, w.text);
  } else if (w.type == "icon") {
    // Icon widget - just draw the icon character
    uint16_t col = w.color ? w.color : currentTheme->text;
    const GFXfont* font = selectFont(w.fontSize ? w.fontSize : 3, true);
    drawTextCentered(d, font, col, w.x, w.y, w.w, w.h, w.text.length() ? w.text : "★");
  }
}

void drawCustomPageById(uint8_t pageId) {
  if (!isCustomPageId(pageId)) return;
  const CustomPage& p = PAGES[pageId - PAGE_COUNT];
  // draw base page if defined
  if (p.base < PAGE_COUNT) {
    switch ((UIPage)p.base) {
      case PAGE_HOME: drawPageHome(); break;
      case PAGE_DASHBOARD: drawPageDashboard(); break;
      case PAGE_STATS: drawPageStats(); break;
      case PAGE_CONTROLS: drawPageControls(); break;
      case PAGE_SETTINGS: drawPageSettings(); break;
      case PAGE_BOILER_CONTROL: drawPageBoilerControl(); break;
    }
  }
  // overlay widgets using UI display accessor
  Arduino_GFX* d = uiGetDisplay();
  if (!d) return;
  for (const auto& w : p.widgets) drawWidget(w, d);
}

// Render overlay for a built-in page if present
void drawBuiltinOverlay(uint8_t base) {
  if (OVERLAYS.empty()) return;
  for (const auto& p : OVERLAYS) {
    if (p.base == base) {
      Arduino_GFX* d = uiGetDisplay(); if (!d) return; for (const auto& w : p.widgets) drawWidget(w, d);
    }
  }
}
