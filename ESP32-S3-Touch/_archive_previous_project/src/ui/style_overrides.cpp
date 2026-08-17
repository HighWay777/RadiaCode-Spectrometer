#include "style_overrides.h"
#include <ArduinoJson.h>

std::map<String, StyleOverride> StyleOverrides::overrides;
bool StyleOverrides::initialized = false;
const char* StyleOverrides::STYLES_FILE = "/styles.json";

void StyleOverrides::init() {
  if (initialized) return;
  initialized = true;
  
  if (!SPIFFS.exists(STYLES_FILE)) {
    Serial.println("[STYLES] No styles.json found, using defaults");
    return;
  }
  
  File f = SPIFFS.open(STYLES_FILE, FILE_READ);
  if (!f) {
    Serial.println("[STYLES] Failed to open styles.json");
    return;
  }
  
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if (err) {
    Serial.printf("[STYLES] JSON parse error: %s\n", err.c_str());
    return;
  }
  
  JsonObject styles = doc["styles"].as<JsonObject>();
  for (JsonPair kv : styles) {
    String id = kv.key().c_str();
    JsonObject s = kv.value().as<JsonObject>();
    
    StyleOverride so;
    
    if (s.containsKey("color")) {
      so.hasColor = true;
      if (s["color"].is<const char*>()) {
        so.color = hexToRgb565(s["color"].as<const char*>());
      } else {
        so.color = s["color"].as<uint16_t>();
      }
    }
    
    if (s.containsKey("textColor")) {
      so.hasTextColor = true;
      if (s["textColor"].is<const char*>()) {
        so.textColor = hexToRgb565(s["textColor"].as<const char*>());
      } else {
        so.textColor = s["textColor"].as<uint16_t>();
      }
    }
    
    if (s.containsKey("hidden")) {
      so.hidden = s["hidden"].as<bool>();
    }
    
    if (s.containsKey("fontSize")) {
      so.hasFontSize = true;
      so.fontSize = s["fontSize"].as<uint8_t>();
    }
    
    overrides[id] = so;
    Serial.printf("[STYLES] Loaded override: %s (color=%s, textColor=%s, hidden=%d, fontSize=%d)\n",
                  id.c_str(),
                  so.hasColor ? String(so.color, HEX).c_str() : "default",
                  so.hasTextColor ? String(so.textColor, HEX).c_str() : "default",
                  so.hidden,
                  so.hasFontSize ? so.fontSize : 0);
  }
  
  Serial.printf("[STYLES] Loaded %d style overrides\n", overrides.size());
}

bool StyleOverrides::save() {
  DynamicJsonDocument doc(4096);
  JsonObject styles = doc.createNestedObject("styles");
  
  for (const auto& kv : overrides) {
    JsonObject s = styles.createNestedObject(kv.first);
    const StyleOverride& so = kv.second;
    
    if (so.hasColor) {
      s["color"] = rgb565ToHex(so.color);
    }
    if (so.hasTextColor) {
      s["textColor"] = rgb565ToHex(so.textColor);
    }
    if (so.hidden) {
      s["hidden"] = true;
    }
    if (so.hasFontSize) {
      s["fontSize"] = so.fontSize;
    }
  }
  
  File f = SPIFFS.open(STYLES_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("[STYLES] Failed to open styles.json for writing");
    return false;
  }
  
  serializeJson(doc, f);
  f.close();
  
  Serial.printf("[STYLES] Saved %d style overrides\n", overrides.size());
  return true;
}

StyleOverride* StyleOverrides::get(const char* elementId) {
  auto it = overrides.find(String(elementId));
  if (it != overrides.end()) {
    return &it->second;
  }
  return nullptr;
}

void StyleOverrides::set(const char* elementId, const StyleOverride& style) {
  overrides[String(elementId)] = style;
}

void StyleOverrides::remove(const char* elementId) {
  overrides.erase(String(elementId));
}

void StyleOverrides::clear() {
  overrides.clear();
}

uint16_t StyleOverrides::getColor(const char* elementId, uint16_t defaultColor) {
  StyleOverride* so = get(elementId);
  if (so && so->hasColor) {
    return so->color;
  }
  return defaultColor;
}

uint16_t StyleOverrides::getTextColor(const char* elementId, uint16_t defaultColor) {
  StyleOverride* so = get(elementId);
  if (so && so->hasTextColor) {
    return so->textColor;
  }
  return defaultColor;
}

bool StyleOverrides::isHidden(const char* elementId) {
  StyleOverride* so = get(elementId);
  if (so) {
    return so->hidden;
  }
  return false;
}

uint8_t StyleOverrides::getFontSize(const char* elementId, uint8_t defaultSize) {
  StyleOverride* so = get(elementId);
  if (so && so->hasFontSize) {
    return so->fontSize;
  }
  return defaultSize;
}

uint16_t StyleOverrides::hexToRgb565(const char* hex) {
  if (!hex || hex[0] != '#') return 0xFFFF;
  
  uint32_t rgb = 0;
  size_t len = strlen(hex + 1);
  
  if (len == 6) {
    rgb = (uint32_t)strtoul(hex + 1, nullptr, 16);
  } else if (len == 3) {
    char expanded[7];
    expanded[0] = hex[1]; expanded[1] = hex[1];
    expanded[2] = hex[2]; expanded[3] = hex[2];
    expanded[4] = hex[3]; expanded[5] = hex[3];
    expanded[6] = '\0';
    rgb = (uint32_t)strtoul(expanded, nullptr, 16);
  } else {
    return 0xFFFF;
  }
  
  uint16_t r = (rgb >> 19) & 0x1F;
  uint16_t g = (rgb >> 10) & 0x3F;
  uint16_t b = (rgb >> 3) & 0x1F;
  return (r << 11) | (g << 5) | b;
}

String StyleOverrides::rgb565ToHex(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) << 3;
  uint8_t g = ((color >> 5) & 0x3F) << 2;
  uint8_t b = (color & 0x1F) << 3;
  
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}
