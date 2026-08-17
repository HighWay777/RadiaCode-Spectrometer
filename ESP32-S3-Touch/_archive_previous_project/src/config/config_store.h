#pragma once

#include <Arduino.h>
#include <Preferences.h>

#define MAX_DASHBOARD_CARDS 6
#define MAX_CARD_TITLE_LEN 24
#define MAX_WIFI_SSID_LEN 32
#define MAX_WIFI_PASS_LEN 64
#define MAX_API_KEY_LEN 64
#define MAX_LOCATION_LEN 48
#define MAX_API_URL_LEN 128

/**
 * Configuration structure for the ESP32 Touch UI
 * Stores all user configurable settings
 */
struct AppConfig {
  // System settings
  uint8_t brightness;      // Display brightness (0-255)
  uint8_t autoRotatePages; // Auto rotate through pages when idle (0=off, 1=on)
  uint16_t autoRotateTime; // Time in seconds between page rotations
  uint16_t idleTimeout;    // Time in seconds before considering device idle

  // UI settings
  uint8_t fontSize;        // Font size for UI elements (1-3)
  uint8_t theme;           // Theme index (0=dark, 1=light)
  bool showClock;          // Show clock on home screen
  bool showDate;           // Show date on home screen
  bool showWeatherIcons;   // Show dynamic weather icons on home screen
  bool showBoilerTempOnHome; // Show small boiler temperature badge on Home page

  // UI pages & navigation
  uint8_t landingPage;     // Page to show on boot/reset (0=Home,1=Dashboard,2=Stats,3=SysInfo,4=Settings)
  uint8_t pageOrder[6];    // Order of the 6 built-in pages

  // Network settings
  char wifiSsid[MAX_WIFI_SSID_LEN];
  char wifiPassword[MAX_WIFI_PASS_LEN];
  bool useStaticIp;        // Use static IP instead of DHCP
  uint8_t ipAddress[4];    // Static IP address
  uint8_t gateway[4];      // Gateway address
  uint8_t subnet[4];       // Subnet mask
  uint8_t dns1[4];         // Primary DNS server
  uint8_t dns2[4];         // Secondary DNS server

  // Time settings
  char timeZone[40];       // e.g., "Europe/Sofia"

  // Weather API settings
  char apiKey[MAX_API_KEY_LEN];
  char location[MAX_LOCATION_LEN];
  char weatherApiUrl[MAX_API_URL_LEN];
  uint16_t weatherUpdateInterval; // Update interval in minutes
  uint8_t weatherProvider;       // 0=OpenWeather, 1=Open-Meteo
  
  // Data API settings (external boiler/sensor status endpoint)
  char dataApiUrl[MAX_API_URL_LEN];  // URL for external /api/status data

  // Dashboard card settings
  struct DashboardCard {
    bool enabled;
    char title[MAX_CARD_TITLE_LEN];
    uint8_t type;          // 0=temperature, 1=humidity, 2=pressure, etc.
    uint8_t size;          // Size of card (1=small, 2=medium, 3=large)
    uint8_t position;      // Position index on dashboard
    uint8_t icon;          // Icon to display (see ICON_SYSTEM_GUIDE.md)
    uint8_t refreshRate;   // Refresh rate in seconds
    bool showValue;        // Show numeric value
    bool showIcon;         // Show icon
    uint16_t color;        // Custom color (0=use theme default)
  };
  
  DashboardCard cards[MAX_DASHBOARD_CARDS];

  // Version tracking
  uint16_t configVersion;  // Configuration version for migrations
};

class ConfigStore {
public:
  // Initialize the configuration store
  static bool init();
  
  // Load configuration from persistent storage
  static bool load();
  
  // Save configuration to persistent storage
  static bool save();
  
  // Reset configuration to factory defaults
  static void resetToDefaults();
  
  // Get configuration pointer
  static AppConfig* getConfig();

private:
  static AppConfig config;
  static Preferences preferences;
  static bool initialized;
  static constexpr const char* NAMESPACE = "esp32touch";
  static constexpr const char* CONFIG_KEY = "appconfig";
  static constexpr uint16_t CURRENT_CONFIG_VERSION = 3;
};