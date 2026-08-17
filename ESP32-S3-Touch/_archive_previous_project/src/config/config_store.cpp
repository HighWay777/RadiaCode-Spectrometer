#include "config_store.h"
#include <Preferences.h>
#include <cstring>
#include <Arduino.h>

// Single, consistent implementation of ConfigStore
AppConfig ConfigStore::config;
Preferences ConfigStore::preferences;
bool ConfigStore::initialized = false;

bool ConfigStore::init() {
  if (initialized) return true;
  // Initialize preferences (don't keep it open permanently)
  if (!preferences.begin(NAMESPACE, false)) {
    // Preferences may still work; continue but warn
    Serial.println("Warning: Preferences begin failed (may be first run)");
  }
  initialized = true;
  // Attempt to load persisted config; if missing, set defaults
  if (!load()) {
    resetToDefaults();
  }
  return true;
}

bool ConfigStore::load() {
  if (!initialized) init();
  // Check existence
  if (!preferences.isKey(CONFIG_KEY)) {
    return false;
  }
  size_t len = preferences.getBytesLength(CONFIG_KEY);
  if (len != sizeof(AppConfig)) {
    Serial.printf("Config size mismatch: %u != %u\n", (unsigned)len, (unsigned)sizeof(AppConfig));
    return false;
  }
  preferences.getBytes(CONFIG_KEY, &config, sizeof(AppConfig));
  if (config.configVersion != CURRENT_CONFIG_VERSION) {
    Serial.printf("Config version mismatch: %u != %u\n", config.configVersion, CURRENT_CONFIG_VERSION);
    return false;
  }
  Serial.println("Config loaded from NVS");
  return true;
}

bool ConfigStore::save() {
  if (!initialized) init();
  config.configVersion = CURRENT_CONFIG_VERSION;
  size_t written = preferences.putBytes(CONFIG_KEY, &config, sizeof(AppConfig));
  if (written != sizeof(AppConfig)) {
    Serial.printf("Failed to write config (%u != %u)\n", (unsigned)written, (unsigned)sizeof(AppConfig));
    return false;
  }
  Serial.println("Config saved to NVS");
  return true;
}

void ConfigStore::resetToDefaults() {
  memset(&config, 0, sizeof(AppConfig));

  // UI defaults
  config.brightness = 200;
  config.autoRotatePages = 0;
  config.autoRotateTime = 10;
  config.idleTimeout = 300;
  config.fontSize = 2;
  config.theme = 0;
  config.showClock = true;
  config.showDate = true;
  config.showWeatherIcons = true;
  config.showBoilerTempOnHome = true;
  config.landingPage = 0;
  for (int i = 0; i < (int)sizeof(config.pageOrder); ++i) config.pageOrder[i] = i;

  // Network defaults - WiFi credentials managed by wifi_credentials.h
  // Leave empty so scanAndConnectWiFi() will be triggered on first boot
  config.wifiSsid[0] = '\0';
  config.wifiPassword[0] = '\0';

  config.useStaticIp = false;
  memset(config.ipAddress, 0, sizeof(config.ipAddress));
  memset(config.gateway, 0, sizeof(config.gateway));
  memset(config.subnet, 0, sizeof(config.subnet));
  memset(config.dns1, 0, sizeof(config.dns1));
  memset(config.dns2, 0, sizeof(config.dns2));

  // Time & weather defaults
  strlcpy(config.timeZone, "Europe/Sofia", sizeof(config.timeZone));
  config.weatherUpdateInterval = 900;
  config.weatherProvider = 1;  // Open-Meteo (free, no API key needed)
  strlcpy(config.location, "42.6977,23.3219", sizeof(config.location));  // Sofia coordinates
  memset(config.apiKey, 0, sizeof(config.apiKey));  // Not needed for Open-Meteo
  
  // Data API default (external boiler/sensor status)
  strlcpy(config.dataApiUrl, "https://boiler101.vhelectronics.com/api/status", sizeof(config.dataApiUrl));

  // Dashboard defaults
  for (int i = 0; i < MAX_DASHBOARD_CARDS; ++i) {
    config.cards[i].enabled = false;
    config.cards[i].title[0] = '\0';
    config.cards[i].type = 0;
    config.cards[i].size = 1;
    config.cards[i].position = i;
    config.cards[i].icon = 0;
    config.cards[i].refreshRate = 60;
    config.cards[i].showValue = true;
    config.cards[i].showIcon = true;
    config.cards[i].color = 0;
  }

  config.configVersion = CURRENT_CONFIG_VERSION;

  // Do not auto-save defaults to NVS here (caller may choose to save)
  Serial.println("Config reset to defaults (in-memory)");
}

AppConfig* ConfigStore::getConfig() {
  if (!initialized) init();
  return &config;
}