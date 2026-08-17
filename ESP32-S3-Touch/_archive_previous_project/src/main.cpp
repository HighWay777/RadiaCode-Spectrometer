// Core
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <memory>
#include <string.h>
#include <time.h>
#include <Update.h>  // ESP32 OTA update library

// App modules
#include "config/config_store.h"
#include "network/network_manager.h"
#include "network/wifi_credentials.h"
#include "network/weather_service.h"
#include "ui/ui_framework.h"
#include "ui/touch_handler.h"
#include "ui/touch_feedback.h"
#include "ui/page_transitions.h"
#include "ui/page_home.h"
#include "ui/base_layouts.h"
#include "ui/style_overrides.h"
#include "utils/time_manager.h"
#include "ui/custom_pages.h"
#include "ui/boiler_credentials.h"

// Web server
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Display configuration
// QSPI pin mapping for display
#define PIN_CS  45 // CS
#define PIN_SCK 47 // SCK
#define PIN_D0  21 // IO0 (MOSI)
#define PIN_D1  48 // IO1 (MISO)
#define PIN_D2  40 // IO2 (WP)
#define PIN_D3  39 // IO3 (HOLD)
#define PIN_RST 17 // Reset pin

// Display resolution
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// Backlight configuration
#define LCD_BL 1
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_12_BIT  12
#define LEDC_BASE_FREQ     5000

// Global variables
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  PIN_CS, 
  PIN_SCK, 
  PIN_D0, 
  PIN_D1, 
  PIN_D2, 
  PIN_D3);
Arduino_GFX *gfx = new Arduino_NV3041A(bus, PIN_RST, 0 /* rotation */, true /* IPS */);

static AsyncWebServer server(80);

// Flags to defer actions from async callbacks to the main loop task
static volatile bool pendingRestart  = false;
static volatile bool pendingUiReinit = false;

// Touch pins (from README)
static const uint8_t TOUCH_SDA = 8, TOUCH_SCL = 4, TOUCH_INT = 3, TOUCH_RST = 38;

void setBrightness(uint8_t value) {
  uint32_t duty = (1 << LEDC_TIMER_12_BIT) - 1; // 4095
  duty = duty * value / 255;
  ledcWrite(LEDC_CHANNEL_0, duty);
}

static void drawWelcome(const char* line2 = nullptr) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(30, 90);
  gfx->println("Home IOT Controller");
  gfx->setTextSize(2);
  gfx->setCursor(30, 140);
  gfx->println(line2 ? line2 : "Initializing...");
}

// Simple status JSON for web UI
static void setupApiRoutes();
static void setupFileRoutes();

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(500); // Give serial monitor time to start
  Serial.println("\n\nESP32-S3 Touch UI Starting...");
  
  // Configure LED PWM for backlight control
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LCD_BL, LEDC_CHANNEL_0);
  setBrightness(200);
  
  // Initialize the display
  if (!gfx->begin(16000000)) {
    Serial.println("Display initialization failed!");
    while (1) delay(1000);
  }
  gfx->invertDisplay(false);
  gfx->setRotation(0);
  
  // Welcome screen
  drawWelcome();

  // Init config store (loads or defaults)
  ConfigStore::init();

  // Mount SPIFFS for web UI
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted");
  }

  // Initialize style overrides (must be after SPIFFS)
  StyleOverrides::init();

  // Initialize touch
  TouchHandler::init(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_WIDTH, SCREEN_HEIGHT);

  // Initialize networking
  NetworkManager::init();
  WeatherService::init();
  
  // Auto-connect to WiFi
  // If NVRAM has saved credentials, use those; otherwise scan and connect to best available network
  {
    AppConfig* cfg = ConfigStore::getConfig();
    
    // Validate saved credentials (check for corruption/invalid data)
    bool hasValidCredentials = false;
    bool ssidInKnownList = false;
    if (strlen(cfg->wifiSsid) > 0) {
      // Check if SSID contains only printable characters
      bool validChars = true;
      for (size_t i = 0; i < strlen(cfg->wifiSsid); i++) {
        if (cfg->wifiSsid[i] < 32 || cfg->wifiSsid[i] > 126) {
          validChars = false;
          break;
        }
      }

      for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
        if (strcmp(cfg->wifiSsid, WIFI_NETWORKS[i].ssid) == 0) {
          ssidInKnownList = true;
          break;
        }
      }

      // Consider saved credentials valid only when they match a configured known network.
      hasValidCredentials = validChars && strlen(cfg->wifiSsid) >= 2 && ssidInKnownList;
    }
    
    if (!hasValidCredentials) {
      // No valid saved credentials - scan for known networks from wifi_credentials.h
      Serial.println("[SETUP] No valid WiFi credentials in NVRAM, scanning for available networks...");
      if (strlen(cfg->wifiSsid) > 0) {
        if (!ssidInKnownList) {
          Serial.printf("[SETUP] Warning: Saved SSID '%s' is not in wifi_credentials.h (clearing)\n", cfg->wifiSsid);
        } else {
          Serial.printf("[SETUP] Warning: Corrupted SSID detected (clearing): '%s'\n", cfg->wifiSsid);
        }
        cfg->wifiSsid[0] = '\0';
        cfg->wifiPassword[0] = '\0';
        ConfigStore::save();
      }
      NetworkManager::scanAndConnectWiFi();
    } else {
      // Use saved credentials
      Serial.printf("[SETUP] Using saved WiFi credentials: %s\n", cfg->wifiSsid);
      NetworkManager::connectWiFi();
    }
  }

  // Configure NTP once WiFi is up (non-blocking)
  {
    AppConfig* cfg = ConfigStore::getConfig();
    Serial.printf("[SETUP] Initializing timezone: %s\n", cfg->timeZone);
    TimeManager::init(cfg->timeZone);
    // Debug time info after a brief delay for NTP sync
    delay(2000);
    if (TimeManager::timeIsSet()) {
      TimeManager::debugTimeInfo();
    } else {
      Serial.println("[SETUP] Time not yet synchronized via NTP");
    }
  }

  // Start web server
  setupFileRoutes();
  setupApiRoutes();
  server.begin();
  Serial.println("Web server started on :80");

  // Keep welcome for ~10s while WiFi comes up
  unsigned long start = millis();
  while (millis() - start < 10000) {
    // heartbeat block
    gfx->fillRect(10, 10, 20, 20, ((millis() / 500) % 2) ? GREEN : BLACK);
    NetworkManager::loop();
    WeatherService::loop();
    delay(10);
  }

  // Initialize UI framework and draw the first page
  customPagesInit();
  uiInit(gfx);
  
  // Initialize touch feedback and page transitions
  TouchFeedback::init(gfx);
  PageTransitions::init(gfx);
  
  Serial.println("Initialization complete");
}

void loop() {
  // Deferred restart requested from async callback
  if (pendingRestart) { delay(100); ESP.restart(); }
  
  // Deferred UI reinit requested from async callback
  if (pendingUiReinit) {
    pendingUiReinit = false;
    uiInit(gfx);
  }
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("rescan") || cmd.equalsIgnoreCase("scanwifi")) {
      Serial.println("\n[DEBUG] Force WiFi re-scan requested...");
      AppConfig* cfg = ConfigStore::getConfig();
      cfg->wifiSsid[0] = '\0';
      cfg->wifiPassword[0] = '\0';
      ConfigStore::save();
      Serial.println("[DEBUG] Cleared saved credentials, restarting...");
      delay(1000);
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("reset") || cmd.equalsIgnoreCase("factory")) {
      Serial.println("\n[DEBUG] Factory reset requested...");
      ConfigStore::resetToDefaults();
      ConfigStore::save();
      Serial.println("[DEBUG] Reset complete, restarting...");
      delay(1000);
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("help") || cmd.equals("?")) {
      Serial.println("\n=== Debug Commands ===");
      Serial.println("rescan   - Clear WiFi and re-scan networks");
      Serial.println("factory  - Factory reset all settings");
      Serial.println("help     - Show this menu");
    }
  }
  
  // Periodic modules
  TouchHandler::update();
  NetworkManager::loop();
  WeatherService::loop();
  uiLoop();

  // Update touch feedback animations
  TouchFeedback::update();
  
  // Update page transition animations
  PageTransitions::update();

  // Trigger touch feedback on tap (visual feedback)
  if (TouchHandler::wasTapped()) {
    int16_t tx = TouchHandler::getTouchX();
    int16_t ty = TouchHandler::getTouchY();
    TouchFeedback::trigger(tx, ty, FEEDBACK_RIPPLE);
    
    // Invoke registered page-specific touch handlers (buttons, controls, etc.)
    invokeTouchHandlers(tx, ty);
    
    // Reset gesture flags after handling tap
    TouchHandler::resetGestures();
  }
  
  // Handle gestures for page navigation with slide transitions
  // Note: swipe handlers are swapped because touch X is un-inverted for button accuracy
  if (TouchHandler::wasSwipedLeft()) {
    // Physical left swipe goes to next page
    navigateNextWithTransition();
    TouchHandler::resetGestures();
  } else if (TouchHandler::wasSwipedRight()) {
    // Physical right swipe goes to previous page
    navigatePrevWithTransition();
    TouchHandler::resetGestures();
  }

  delay(5);
}

// ---------------- Web server routes ----------------
static void jsonResponse(AsyncWebServerRequest* request, const String& json, int code = 200) {
  AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

static String statusJson() {
  AppConfig* cfg = ConfigStore::getConfig();
  StaticJsonDocument<1536> doc;  // Increased from 1024 to fit all ~30 fields
  
  // Basic system/network status
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifiRSSI"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
  doc["wifiSsid"] = String(cfg->wifiSsid);
  doc["ipAddress"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("");
  doc["uptime"] = (uint32_t)(millis() / 1000);
  doc["freeHeap"] = (uint32_t)ESP.getFreeHeap();
  doc["apiStatus"] = (int)NetworkManager::getApiStatus();
  doc["apiFailCount"] = NetworkManager::getApiFailureCount();
  doc["timeZone"] = String(cfg->timeZone);
  time_t now = time(nullptr);
  doc["epoch"] = (uint32_t)now;
  struct tm timeinfo; memset(&timeinfo, 0, sizeof(timeinfo));
  localtime_r(&now, &timeinfo);
  if (TimeManager::timeIsSet()) {
    char formatted[32];
    strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    doc["localTime"] = formatted;
  } else {
    doc["localTime"] = "--";
  }

  // Mirror of last parsed boiler data exposed via UI layer
  WeatherData* wd = getCurrentWeatherData();
  if (wd) {
    // Weather data (from weather service)
    doc["extTemp"] = wd->extTemp;
    doc["weatherCode"] = wd->weatherCode;
    doc["weatherDesc"] = wd->weatherDesc;
    doc["humidity"] = wd->humidity;
    doc["pressure"] = wd->pressure;
    doc["windKph"] = wd->windKph;
    doc["solar"] = wd->solar;
    doc["isNightTime"] = wd->isNightTime;
    
    // Boiler temperature data
    doc["currentTemp"] = wd->currentTemp;
    doc["targetTemp"] = wd->targetTemp;
    doc["dayTemp"] = wd->dayTemp;
    doc["nightTemp"] = wd->nightTemp;
    doc["boostTemp"] = wd->boostTemp;
    
    // Boiler state
    doc["heaterOn"] = wd->heaterOn;
    doc["heaterControlMode"] = wd->heaterControlMode;
    doc["aiEnabled"] = wd->aiEnabled;
    doc["manualOverride"] = wd->manualOverride;
    doc["isBoostTime"] = wd->isBoostTime;
    doc["targetSource"] = wd->targetSource;
    
    // Energy metrics
    doc["efficiency"] = wd->efficiency;
    doc["heatLossRate"] = wd->heatLossRate;
    doc["kwhTotal"] = wd->kwhTotal;
    doc["costTotal"] = wd->costTotal;
    doc["dailyKWh"] = wd->dailyKWh;
    doc["costToday"] = wd->costToday;
    doc["dailyCost"] = wd->dailyCost;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

static void setupApiRoutes() {
  // Status endpoint used by web UI polling
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request){
    request->send(200, "application/json", statusJson());
  });

  // Config GET/POST
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request){
    AppConfig* cfg = ConfigStore::getConfig();
    StaticJsonDocument<1536> doc;
    doc["brightness"] = cfg->brightness;
    doc["theme"] = cfg->theme;
    doc["fontSize"] = cfg->fontSize;
    doc["autoRotatePages"] = cfg->autoRotatePages;
    doc["autoRotateTime"] = cfg->autoRotateTime;
    doc["idleTimeout"] = cfg->idleTimeout;
    doc["wifiSsid"] = cfg->wifiSsid;
    doc["useStaticIp"] = cfg->useStaticIp;
    JsonArray ip = doc.createNestedArray("ipAddress"); for (int i=0;i<4;i++) ip.add(cfg->ipAddress[i]);
    JsonArray gw = doc.createNestedArray("gateway"); for (int i=0;i<4;i++) gw.add(cfg->gateway[i]);
    JsonArray sn = doc.createNestedArray("subnet"); for (int i=0;i<4;i++) sn.add(cfg->subnet[i]);
    JsonArray d1 = doc.createNestedArray("dns1"); for (int i=0;i<4;i++) d1.add(cfg->dns1[i]);
    JsonArray d2 = doc.createNestedArray("dns2"); for (int i=0;i<4;i++) d2.add(cfg->dns2[i]);
    doc["showClock"] = cfg->showClock;
    doc["showDate"] = cfg->showDate;
    doc["showWeatherIcons"] = cfg->showWeatherIcons;
  doc["showBoilerTempOnHome"] = cfg->showBoilerTempOnHome;
  doc["landingPage"] = cfg->landingPage;
  // pageOrder as array
  JsonArray po = doc.createNestedArray("pageOrder"); for (int i=0;i<PAGE_COUNT;i++) po.add(cfg->pageOrder[i]);
    doc["weatherUpdateInterval"] = cfg->weatherUpdateInterval;
    doc["weatherProvider"] = cfg->weatherProvider;
  JsonArray wp = doc.createNestedArray("weatherProviders");
  wp.add("OpenWeather");
  wp.add("Open-Meteo");
    doc["apiKey"] = cfg->apiKey;
    doc["location"] = cfg->location;
  doc["timeZone"] = cfg->timeZone;
  doc["dataApiUrl"] = cfg->dataApiUrl;
    String out; serializeJson(doc, out); request->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request){ request->send(200, "application/json", "{\"status\":\"ok\"}"); },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<2048> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) { jsonResponse(request, String("{\"error\":\"json\"}"), 400); return; }
      AppConfig* cfg = ConfigStore::getConfig();
      if (doc.containsKey("brightness")) cfg->brightness = doc["brightness"].as<uint8_t>();
      if (doc.containsKey("theme")) cfg->theme = doc["theme"].as<uint8_t>();
      if (doc.containsKey("fontSize")) cfg->fontSize = doc["fontSize"].as<uint8_t>();
      if (doc.containsKey("autoRotatePages")) cfg->autoRotatePages = doc["autoRotatePages"].as<uint8_t>();
      if (doc.containsKey("autoRotateTime")) cfg->autoRotateTime = doc["autoRotateTime"].as<uint16_t>();
      if (doc.containsKey("idleTimeout")) cfg->idleTimeout = doc["idleTimeout"].as<uint16_t>();
      if (doc.containsKey("wifiSsid")) strlcpy(cfg->wifiSsid, doc["wifiSsid"], MAX_WIFI_SSID_LEN);
      if (doc.containsKey("wifiPassword")) strlcpy(cfg->wifiPassword, doc["wifiPassword"], MAX_WIFI_PASS_LEN);
      if (doc.containsKey("useStaticIp")) cfg->useStaticIp = doc["useStaticIp"].as<bool>();
      auto copyIp = [](JsonArray arr, uint8_t out[4]){ for (int i=0;i<4 && i<(int)arr.size(); i++) out[i] = arr[i].as<uint8_t>(); };
      if (doc.containsKey("ipAddress")) copyIp(doc["ipAddress"].as<JsonArray>(), cfg->ipAddress);
      if (doc.containsKey("gateway")) copyIp(doc["gateway"].as<JsonArray>(), cfg->gateway);
      if (doc.containsKey("subnet")) copyIp(doc["subnet"].as<JsonArray>(), cfg->subnet);
      if (doc.containsKey("dns1")) copyIp(doc["dns1"].as<JsonArray>(), cfg->dns1);
      if (doc.containsKey("dns2")) copyIp(doc["dns2"].as<JsonArray>(), cfg->dns2);
      if (doc.containsKey("showClock")) cfg->showClock = doc["showClock"].as<bool>();
      if (doc.containsKey("showDate")) cfg->showDate = doc["showDate"].as<bool>();
      if (doc.containsKey("showWeatherIcons")) cfg->showWeatherIcons = doc["showWeatherIcons"].as<bool>();
      if (doc.containsKey("showBoilerTempOnHome")) cfg->showBoilerTempOnHome = doc["showBoilerTempOnHome"].as<bool>();
      if (doc.containsKey("landingPage")) cfg->landingPage = doc["landingPage"].as<uint8_t>();
      if (doc.containsKey("pageOrder")) {
        JsonArray arr = doc["pageOrder"].as<JsonArray>();
        for (int i=0;i<PAGE_COUNT && i<(int)arr.size(); i++) cfg->pageOrder[i] = arr[i].as<uint8_t>();
      }
      if (doc.containsKey("weatherUpdateInterval")) cfg->weatherUpdateInterval = doc["weatherUpdateInterval"].as<uint16_t>();
      if (doc.containsKey("weatherProvider")) cfg->weatherProvider = doc["weatherProvider"].as<uint8_t>();
      if (doc.containsKey("apiKey")) strlcpy(cfg->apiKey, doc["apiKey"], MAX_API_KEY_LEN);
      if (doc.containsKey("location")) strlcpy(cfg->location, doc["location"], MAX_LOCATION_LEN);
      if (doc.containsKey("dataApiUrl")) {
        strlcpy(cfg->dataApiUrl, doc["dataApiUrl"], MAX_API_URL_LEN);
        Serial.printf("[CONFIG] Data API URL updated to: %s\n", cfg->dataApiUrl);
      }
      if (doc.containsKey("timeZone")) {
        strlcpy(cfg->timeZone, doc["timeZone"], sizeof(cfg->timeZone));
        Serial.printf("[CONFIG] Timezone updated to: %s\n", cfg->timeZone);
      }
      ConfigStore::save();
      // Apply changes immediately
      // 1) Brightness
      setBrightness(cfg->brightness);
      // 2) Timezone / clock - force refresh
      Serial.println("[CONFIG] Forcing timezone refresh...");
      TimeManager::forceRefresh(cfg->timeZone);
      // 3) UI re-init deferred to main task to avoid cross-task rendering crash
      pendingUiReinit = true;
      // 4) Kick weather refresh to quickly reflect changes in provider/interval
      WeatherService::forceUpdate();
      // 5) Fetch data from (possibly new) API URL
      NetworkManager::fetchApiData();
      jsonResponse(request, "{\"status\":\"ok\"}");
    }
  );

  // Network settings endpoint (dedicated for WiFi changes that may need reconnect)
  server.on("/api/network", HTTP_POST, [](AsyncWebServerRequest* request){ request->send(200, "application/json", "{\"status\":\"ok\"}"); },
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) { jsonResponse(request, String("{\"error\":\"json\"}"), 400); return; }
      AppConfig* cfg = ConfigStore::getConfig();
      bool needReconnect = false;
      if (doc.containsKey("wifiSsid")) {
        const char* newSsid = doc["wifiSsid"];
        if (strcmp(cfg->wifiSsid, newSsid) != 0) {
          strlcpy(cfg->wifiSsid, newSsid, MAX_WIFI_SSID_LEN);
          needReconnect = true;
        }
      }
      if (doc.containsKey("wifiPassword")) {
        const char* newPass = doc["wifiPassword"];
        if (strlen(newPass) > 0) {  // Only update if new password provided
          strlcpy(cfg->wifiPassword, newPass, MAX_WIFI_PASS_LEN);
          needReconnect = true;
        }
      }
      ConfigStore::save();
      Serial.printf("[NETWORK] WiFi settings saved (SSID: %s, reconnect: %d)\n", cfg->wifiSsid, needReconnect);
      
      if (needReconnect) {
        // Schedule reconnect after response is sent
        Serial.println("[NETWORK] Scheduling WiFi reconnect...");
        // We can't disconnect here as it would break the response
        // The device should be restarted or manually reconnected
      }
      jsonResponse(request, "{\"status\":\"ok\",\"message\":\"WiFi settings saved. Restart device to apply.\"}");
    }
  );

  // Endpoint to force weather refresh
  server.on("/api/weather/refresh", HTTP_POST, [](AsyncWebServerRequest* request){ WeatherService::forceUpdate(); jsonResponse(request, "{\"status\":\"ok\"}"); });
  // Endpoint to force boiler API refresh
  server.on("/api/boiler/refresh", HTTP_POST, [](AsyncWebServerRequest* request){ NetworkManager::fetchApiData(); jsonResponse(request, "{\"status\":\"ok\"}"); });
  // Redraw current UI page immediately
  server.on("/api/ui/redraw", HTTP_POST, [](AsyncWebServerRequest* request){ pendingUiReinit = true; jsonResponse(request, "{\"status\":\"ok\"}"); });

  // Helper: Forward a control command to the remote boiler API
  // Returns true if successful, false otherwise
  auto forwardToBoiler = [](const char* endpoint, const String& payload, bool isFormData = false) -> bool {
    AppConfig* cfg = ConfigStore::getConfig();
    if (strlen(cfg->dataApiUrl) == 0) {
      Serial.println("[CONTROL] No boiler API URL configured");
      return false;
    }
    
    // Build the base URL (remove /api/status if present and add the endpoint)
    String baseUrl = String(cfg->dataApiUrl);
    int apiIdx = baseUrl.indexOf("/api/");
    if (apiIdx > 0) {
      baseUrl = baseUrl.substring(0, apiIdx);
    }
    String url = baseUrl + endpoint;
    
    Serial.printf("[CONTROL] Forwarding to: %s\n", url.c_str());
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);  // 5 second timeout
    http.setAuthorization(BOILER_USER, BOILER_PASS);
    
    int httpCode;
    if (isFormData) {
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      httpCode = http.POST(payload);
    } else {
      http.addHeader("Content-Type", "application/json");
      httpCode = http.POST(payload);
    }
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      String response = http.getString();
      Serial.printf("[CONTROL] Boiler response: %s\n", response.c_str());
      http.end();
      return true;
    } else {
      Serial.printf("[CONTROL] Boiler request failed: %d\n", httpCode);
      http.end();
      return false;
    }
  };

  // Mode control - forwards to remote boiler if configured
  server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest* request){ jsonResponse(request, "{\"status\":\"ok\",\"note\":\"Mode change would be forwarded to boiler\"}"); });

  // Control mode endpoint (alias for /api/mode - used by Web UI control.js)
  server.on("/api/control/mode", HTTP_POST,
    [](AsyncWebServerRequest* request){ request->send(200, "application/json", "{\"status\":\"ok\"}"); },
    nullptr,
    [forwardToBoiler](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) { jsonResponse(request, "{\"error\":\"json\"}", 400); return; }
      int mode = doc["mode"] | -1;
      if (mode < 0 || mode > 3) { jsonResponse(request, "{\"error\":\"invalid mode\"}", 400); return; }
      
      // Map mode numbers to boiler API mode strings
      const char* modeStr[] = {"off", "manual", "auto", "auto-ai"};
      String formData = "mode=" + String(modeStr[mode]);
      
      Serial.printf("[CONTROL] Mode change requested: %d (%s)\n", mode, modeStr[mode]);
      
      // Forward to boiler (async would be better but keep it simple for now)
      bool success = forwardToBoiler("/api/mode", formData, true);
      if (success) {
        jsonResponse(request, "{\"status\":\"ok\",\"forwarded\":true}");
      } else {
        jsonResponse(request, "{\"status\":\"ok\",\"forwarded\":false,\"note\":\"Boiler offline or not configured\"}");
      }
    }
  );

  // Control temperature endpoint (used by Web UI control.js)
  server.on("/api/control/temp", HTTP_POST,
    [](AsyncWebServerRequest* request){ request->send(200, "application/json", "{\"status\":\"ok\"}"); },
    nullptr,
    [forwardToBoiler](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) { jsonResponse(request, "{\"error\":\"json\"}", 400); return; }
      float targetTemp = doc["targetTemp"] | -1.0f;
      if (targetTemp < 10.0f || targetTemp > 70.0f) { jsonResponse(request, "{\"error\":\"invalid temp\"}", 400); return; }
      
      Serial.printf("[CONTROL] Target temp change requested: %.1f\n", targetTemp);
      
      // Build JSON payload for boiler API
      String payload = "{\"target\":" + String(targetTemp, 1) + "}";
      
      // Forward to boiler
      bool success = forwardToBoiler("/api/temperature", payload, false);
      if (success) {
        jsonResponse(request, "{\"status\":\"ok\",\"forwarded\":true}");
      } else {
        jsonResponse(request, "{\"status\":\"ok\",\"forwarded\":false,\"note\":\"Boiler offline or not configured\"}");
      }
    }
  );

  // Restart
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request){ jsonResponse(request, "{\"status\":\"restarting\"}"); pendingRestart = true; });

  // Reset config
  server.on("/api/reset-config", HTTP_POST, [](AsyncWebServerRequest* request){ ConfigStore::resetToDefaults(); pendingUiReinit = true; jsonResponse(request, "{\"status\":\"ok\"}"); });

  server.on("/api/update-check", HTTP_POST, [](AsyncWebServerRequest* request){
    StaticJsonDocument<256> doc;
    doc["status"] = "ok";
    doc["message"] = "Firmware is up to date";
    doc["timestamp"] = millis();
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // ----- Custom Pages CRUD -----
  
  // Combined listing: built-in page descriptors + custom pages from pages.json
  server.on("/api/pages/all", HTTP_GET, [](AsyncWebServerRequest* request){
    StaticJsonDocument<2048> outDoc;
    // builtins
    JsonArray builtinsArr = outDoc.createNestedArray("builtins");
    const char* names[] = {"Home","Boiler","Stats","Controls","Settings","Boiler Control"};
    for (uint8_t i=0; i<PAGE_COUNT; ++i) {
      JsonObject o = builtinsArr.createNestedObject();
      o["id"] = i;
      o["name"] = names[i];
    }
    // pages (custom)
    JsonArray pagesArr = outDoc.createNestedArray("pages");
    if (SPIFFS.exists("/pages.json")) {
      File f = SPIFFS.open("/pages.json", FILE_READ);
      if (f) {
        DynamicJsonDocument inDoc(8192);
        if (!deserializeJson(inDoc, f)) {
          JsonArray src = inDoc["pages"].as<JsonArray>();
          if (!src.isNull()) {
            for (JsonObject p : src) {
              JsonObject dst = pagesArr.createNestedObject();
              if (p.containsKey("name")) dst["name"] = p["name"].as<const char*>();
              if (p.containsKey("base")) dst["base"] = p["base"].as<uint8_t>();
              if (p.containsKey("widgets")) dst["widgets"] = p["widgets"];
            }
          }
        }
        f.close();
      }
    }
    String body; serializeJson(outDoc, body);
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", body);
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });

  // Base (built-in) page elements for the Builder preview/editor
  server.on("/api/pages/base", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("base")) { jsonResponse(request, "{\"error\":\"missing base\"}", 400); return; }
    uint8_t base = (uint8_t)request->getParam("base")->value().toInt();
    const std::vector<BaseLayoutWidget>& layout = getBaseLayout(base);
    StaticJsonDocument<4096> doc;
    JsonArray ws = doc.createNestedArray("widgets");
    if (layout.empty()) {
      // fallback to legacy minimal response if unknown base requested
      String body = "{\"widgets\":[]}";
      AsyncWebServerResponse* res = request->beginResponse(200, "application/json", body);
      res->addHeader("Cache-Control", "no-store");
      request->send(res);
      return;
    }
    for (const auto& w : layout) {
      JsonObject obj = ws.createNestedObject();
      if (w.id) obj["id"] = w.id;
      if (w.type) obj["type"] = w.type;
      if (w.binding && strlen(w.binding)) obj["binding"] = w.binding;
      if (w.text && strlen(w.text)) obj["text"] = w.text;
      if (w.color && strlen(w.color)) obj["color"] = w.color;
      obj["fontSize"] = w.fontSize ? w.fontSize : 2;
      obj["x"] = w.x;
      obj["y"] = w.y;
      obj["w"] = w.w;
      obj["h"] = w.h;
    }
    String body; serializeJson(doc, body);
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", body);
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });

  // Get pages.json
  server.on("/api/pages", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!SPIFFS.exists("/pages.json")) { request->send(200, "application/json", "{\"pages\":[]}"); return; }
    File f = SPIFFS.open("/pages.json", FILE_READ);
    if (!f) { request->send(500, "application/json", "{\"error\":\"fs\"}"); return; }
    String body = f.readString(); f.close();
    request->send(200, "application/json", body);
  });
  // Save pages.json (full replace)
  server.on("/api/pages", HTTP_POST, 
    [](AsyncWebServerRequest* request){ 
      request->send(200, "application/json", "{\"status\":\"ok\"}"); 
    }, 
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
      // Per-request accumulator stored in _tempObject to avoid race conditions
      if (index == 0) {
        // Free any stale allocation from a previous interrupted request
        if (request->_tempObject) {
          delete static_cast<String*>(request->_tempObject);
          request->_tempObject = nullptr;
        }
        String* buf = new String();
        buf->reserve(total > 0 ? total : 4096);
        request->_tempObject = buf;
      }
      
      String* accumulator = static_cast<String*>(request->_tempObject);
      if (accumulator) {
        accumulator->concat((const char*)data, len);
      }
      
      // On final chunk: write to file
      if (index + len >= total) {
        if (!accumulator) {
          Serial.println("[PAGES] Error: No accumulator");
          return;
        }
        
        // Quick validation
        if (!accumulator->startsWith("{")) {
          Serial.println("[PAGES] Error: Invalid JSON");
          delete accumulator;
          request->_tempObject = nullptr;
          return;
        }
        
        // Write directly to file
        File f = SPIFFS.open("/pages.json", FILE_WRITE);
        if (!f) { 
          Serial.println("[PAGES] Failed to open pages.json");
          delete accumulator;
          request->_tempObject = nullptr;
          return; 
        }
        
        size_t written = f.print(*accumulator);
        f.close();
        
        Serial.printf("[PAGES] Saved pages.json (%d bytes)\n", written);
        
        // Clean up
        delete accumulator;
        request->_tempObject = nullptr;
        
        // Reload (flag-based: uiInit will be called from loop via pending flag)
        customPagesReload();
        uiRequestRedraw();
      }
    }
  );

  // Per-built-in overlay widgets: GET current overlay and POST to save
  server.on("/api/pages/overlay", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("base")) { jsonResponse(request, "{\"error\":\"missing base\"}", 400); return; }
    uint8_t base = (uint8_t)request->getParam("base")->value().toInt();
    char path[24]; snprintf(path, sizeof(path), "/overlay%u.json", base);
    if (!SPIFFS.exists(path)) { request->send(200, "application/json", "{\"widgets\":[]}"); return; }
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) { jsonResponse(request, "{\"error\":\"fs\"}", 500); return; }
    String body = f.readString(); f.close();
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", body);
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  // DELETE overlay for a base id
  server.on("/api/pages/overlay", HTTP_DELETE, [](AsyncWebServerRequest* request){
    if (!request->hasParam("base")) { 
      jsonResponse(request, "{\"error\":\"missing base\"}", 400); 
      return; 
    }
    uint8_t base = (uint8_t)request->getParam("base")->value().toInt();
    
    if (base >= PAGE_COUNT) {
      jsonResponse(request, "{\"error\":\"invalid base id\"}", 400);
      return;
    }
    
    char path[24]; 
    snprintf(path, sizeof(path), "/overlay%u.json", base);
    
    bool existed = SPIFFS.exists(path);
    bool ok = true;
    
    if (existed) {
      ok = SPIFFS.remove(path);
      if (!ok) {
        Serial.printf("[OVERLAY] Failed to delete %s\n", path);
        jsonResponse(request, "{\"error\":\"delete failed\"}", 500);
        return;
      }
      Serial.printf("[OVERLAY] Deleted %s\n", path);
    }
    
    customPagesReload(); 
    uiRequestRedraw();
    
    if (ok && existed) {
      jsonResponse(request, "{\"status\":\"deleted\"}");
    } else {
      jsonResponse(request, "{\"status\":\"absent\"}");
    }
  });
  server.on("/api/pages/overlay", HTTP_POST, 
    [](AsyncWebServerRequest* request){ 
      request->send(200, "application/json", "{\"status\":\"ok\"}"); 
    }, 
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
      // Per-request state: pack base id + String buffer into a small heap struct
      struct OverlayAccum { uint8_t base; String buf; };
      
      if (index == 0) {
        // Free any stale allocation from a previous interrupted request
        if (request->_tempObject) {
          delete static_cast<OverlayAccum*>(request->_tempObject);
          request->_tempObject = nullptr;
        }
        if (!request->hasParam("base")) {
          Serial.println("[OVERLAY] Error: Missing base parameter");
          return;
        }
        OverlayAccum* acc = new OverlayAccum();
        acc->base = (uint8_t)request->getParam("base")->value().toInt();
        acc->buf.reserve(total > 0 ? total : 2048);
        request->_tempObject = acc;
      }
      
      OverlayAccum* acc = static_cast<OverlayAccum*>(request->_tempObject);
      if (acc) {
        acc->buf.concat((const char*)data, len);
      }
      
      // On final chunk: validate and write
      if (index + len >= total) {
        if (!acc) {
          Serial.println("[OVERLAY] Error: Invalid state");
          return;
        }
        
        // Quick JSON validation
        bool validJson = acc->buf.startsWith("{") && acc->buf.endsWith("}");
        if (!validJson) {
          Serial.println("[OVERLAY] Error: Invalid JSON format");
          delete acc;
          request->_tempObject = nullptr;
          return;
        }
        
        char path[24]; 
        snprintf(path, sizeof(path), "/overlay%u.json", acc->base);
        
        File f = SPIFFS.open(path, FILE_WRITE);
        if (!f) {
          Serial.printf("[OVERLAY] Failed to open %s\n", path);
          delete acc;
          request->_tempObject = nullptr;
          return;
        }
        
        size_t written = f.print(acc->buf);
        f.close();
        Serial.printf("[OVERLAY] Saved %s (%d bytes)\n", path, written);
        
        delete acc;
        request->_tempObject = nullptr;
        
        customPagesReload(); 
        uiRequestRedraw();
      }
    }
  );

  // Preset management endpoints
  server.on("/api/presets/list", HTTP_GET, [](AsyncWebServerRequest* request){
    // List saved layout presets from NVRAM/SPIFFS
    File presetsFile = SPIFFS.open("/presets.json", FILE_READ);
    if (!presetsFile) {
      jsonResponse(request, "{\"presets\":[]}");
      return;
    }
    String content = presetsFile.readString();
    presetsFile.close();
    
    // If file is empty or invalid, return empty presets
    if (content.length() == 0 || content[0] != '{') {
      jsonResponse(request, "{\"presets\":[]}");
      return;
    }
    jsonResponse(request, content);
  });

  server.on("/api/presets/save", HTTP_POST, 
    [](AsyncWebServerRequest* request){ 
      request->send(200, "application/json", "{\"status\":\"ok\"}"); 
    }, 
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
      // Accumulate body chunks
      if (index == 0) {
        String* buf = new String();
        buf->reserve(total ? total : len);
        request->_tempObject = buf;
      }
      String* buf = static_cast<String*>(request->_tempObject);
      if (buf) buf->concat((const char*)data, len);
      
      // Process on last chunk
      if (index + len == total) {
        std::unique_ptr<DynamicJsonDocument> payloadDoc(new DynamicJsonDocument(8192));
        DeserializationError err = deserializeJson(*payloadDoc, *buf);
        delete buf;
        request->_tempObject = nullptr;
        
        if (err) {
          jsonResponse(request, "{\"error\":\"invalid json\"}", 400);
          return;
        }
        
        // Extract name and widgets from payload
        if (!payloadDoc->containsKey("name")) {
          jsonResponse(request, "{\"error\":\"missing name\"}", 400);
          return;
        }
        
        String name = (*payloadDoc)["name"].as<String>();
        if (name.length() > 32) {
          jsonResponse(request, "{\"error\":\"name too long\"}", 400);
          return;
        }
        
        if (!payloadDoc->containsKey("widgets")) {
          jsonResponse(request, "{\"error\":\"missing widgets\"}", 400);
          return;
        }
        
        // Read existing presets
        std::unique_ptr<DynamicJsonDocument> presetsDoc(new DynamicJsonDocument(12288));
        JsonArray presets = presetsDoc->createNestedArray("presets");
        
        File presetsFile = SPIFFS.open("/presets.json", FILE_READ);
        if (presetsFile) {
          std::unique_ptr<DynamicJsonDocument> existing(new DynamicJsonDocument(12288));
          if (!deserializeJson(*existing, presetsFile)) {
            if (existing->containsKey("presets")) {
              for (const auto& p : (*existing)["presets"].as<JsonArray>()) {
                // Skip preset with same name (replace it)
                if (p["name"].as<String>() != name) {
                  presets.add(p);
                }
              }
            }
          }
          presetsFile.close();
        }
        
        // Add new preset
        JsonObject preset = presets.createNestedObject();
        preset["name"] = name;
        preset["timestamp"] = millis();
        preset["widgets"] = (*payloadDoc)["widgets"];
        
        // Save presets
        presetsFile = SPIFFS.open("/presets.json", FILE_WRITE);
        if (!presetsFile) {
          jsonResponse(request, "{\"error\":\"fs\"}", 500);
          return;
        }
        serializeJson(*presetsDoc, presetsFile);
        presetsFile.close();
        
        jsonResponse(request, "{\"status\":\"ok\",\"name\":\"" + name + "\"}");
      }
    }
  );

  server.on("/api/presets/load", HTTP_POST, [](AsyncWebServerRequest* request){
    // Load a preset
    if (!request->hasParam("name")) {
      jsonResponse(request, "{\"error\":\"missing name\"}", 400);
      return;
    }
    String name = request->getParam("name")->value();
    
    // Read presets and find matching
    File presetsFile = SPIFFS.open("/presets.json", FILE_READ);
    if (!presetsFile) {
      jsonResponse(request, "{\"error\":\"not found\"}", 404);
      return;
    }
    
    std::unique_ptr<DynamicJsonDocument> doc(new DynamicJsonDocument(12288));
    DeserializationError err = deserializeJson(*doc, presetsFile);
    presetsFile.close();
    
    if (err || !doc->containsKey("presets")) {
      jsonResponse(request, "{\"error\":\"not found\"}", 404);
      return;
    }
    
    for (const auto& preset : (*doc)["presets"].as<JsonArray>()) {
      if (preset["name"].as<String>() == name) {
        // Found it - return the widgets array
        if (preset.containsKey("widgets")) {
          String response;
          serializeJson(preset["widgets"], response);
          jsonResponse(request, response);
          return;
        }
      }
    }
    
    jsonResponse(request, "{\"error\":\"not found\"}", 404);
  });

  server.on("/api/presets/delete", HTTP_POST, [](AsyncWebServerRequest* request){
    // Delete a preset
    if (!request->hasParam("name")) {
      jsonResponse(request, "{\"error\":\"missing name\"}", 400);
      return;
    }
    String name = request->getParam("name")->value();
    
    // Read, filter, and rewrite presets
    File presetsFile = SPIFFS.open("/presets.json", FILE_READ);
    if (!presetsFile) {
      jsonResponse(request, "{\"error\":\"not found\"}", 404);
      return;
    }
    
    std::unique_ptr<DynamicJsonDocument> doc(new DynamicJsonDocument(12288));
    DeserializationError err = deserializeJson(*doc, presetsFile);
    presetsFile.close();
    
    if (err || !doc->containsKey("presets")) {
      jsonResponse(request, "{\"error\":\"not found\"}", 404);
      return;
    }
    
    JsonArray presets = (*doc)["presets"];
    bool found = false;
    for (size_t i = 0; i < presets.size(); i++) {
      if (presets[i]["name"].as<String>() == name) {
        presets.remove(i);
        found = true;
        break;
      }
    }
    
    if (!found) {
      jsonResponse(request, "{\"error\":\"not found\"}", 404);
      return;
    }
        
    presetsFile = SPIFFS.open("/presets.json", FILE_WRITE);
    if (!presetsFile) {
      jsonResponse(request, "{\"error\":\"fs\"}", 500);
      return;
    }
    serializeJson(*doc, presetsFile);
    presetsFile.close();
    
    jsonResponse(request, "{\"status\":\"ok\"}");
  });

  // Delete a custom page by index
  server.on("/api/pages/delete", HTTP_POST, [](AsyncWebServerRequest* request){
    if (!request->hasParam("index")) {
      jsonResponse(request, "{\"error\":\"missing index\"}", 400);
      return;
    }
    
    uint8_t index = (uint8_t)request->getParam("index")->value().toInt();
    
    // Load current pages.json
    if (!SPIFFS.exists("/pages.json")) {
      jsonResponse(request, "{\"error\":\"no custom pages\"}", 400);
      return;
    }
    
    // Use heap allocation for large JSON parsing
    DynamicJsonDocument* doc = new DynamicJsonDocument(16384);
    File f = SPIFFS.open("/pages.json", FILE_READ);
    DeserializationError err = deserializeJson(*doc, f);
    f.close();
    
    if (err || !doc->containsKey("pages")) {
      delete doc;
      jsonResponse(request, "{\"error\":\"invalid pages.json\"}", 400);
      return;
    }
    
    JsonArray pages = (*doc)["pages"];
    if (index >= pages.size()) {
      delete doc;
      jsonResponse(request, "{\"error\":\"index out of range\"}", 400);
      return;
    }
    
    // Delete the page at index
    pages.remove(index);
    
    // Save updated pages.json
    f = SPIFFS.open("/pages.json", FILE_WRITE);
    if (!f) {
      delete doc;
      jsonResponse(request, "{\"error\":\"fs\"}", 500);
      return;
    }
    serializeJson(*doc, f);
    f.close();
    delete doc;
    
    // Reload and redraw
    customPagesReload();
    uiRequestRedraw();
    
    jsonResponse(request, "{\"status\":\"deleted\",\"index\":" + String(index) + "}");
  });

  // Reset to default layouts
  server.on("/api/pages/reset", HTTP_POST, [](AsyncWebServerRequest* request){
    // Delete all custom pages and presets
    SPIFFS.remove("/pages.json");
    SPIFFS.remove("/presets.json");
    
    // Reset all overlay files for builtin pages
    for (int i = 0; i < PAGE_COUNT; i++) {
      char path[24];
      snprintf(path, sizeof(path), "/overlay%d.json", i);
      SPIFFS.remove(path);
    }
    
    // Also clear style overrides
    SPIFFS.remove("/styles.json");
    StyleOverrides::clear();
    
    customPagesReload();
    uiRequestRedraw();
    jsonResponse(request, "{\"status\":\"ok\",\"message\":\"Reset to defaults\"}");
  });

  // ===== STYLE OVERRIDES API =====
  
  // GET current styles
  server.on("/api/styles", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!SPIFFS.exists("/styles.json")) {
      jsonResponse(request, "{\"styles\":{}}");
      return;
    }
    File f = SPIFFS.open("/styles.json", FILE_READ);
    if (!f) {
      jsonResponse(request, "{\"error\":\"fs\"}", 500);
      return;
    }
    String body = f.readString();
    f.close();
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", body);
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  
  // POST to update a single style
  server.on("/api/styles", HTTP_POST,
    [](AsyncWebServerRequest* request){ request->send(200, "application/json", "{\"status\":\"ok\"}"); },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
      // Per-request accumulator stored in _tempObject
      if (index == 0) {
        if (request->_tempObject) {
          delete static_cast<String*>(request->_tempObject);
          request->_tempObject = nullptr;
        }
        String* buf = new String();
        buf->reserve(total > 0 ? total : 512);
        request->_tempObject = buf;
      }
      
      String* accumulator = static_cast<String*>(request->_tempObject);
      if (accumulator) {
        accumulator->concat((const char*)data, len);
      }
      
      if (index + len >= total) {
        if (!accumulator) return;
        
        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, *accumulator);
        delete accumulator;
        request->_tempObject = nullptr;
        
        if (err) {
          Serial.printf("[STYLES API] JSON parse error: %s\n", err.c_str());
          return;
        }
        
        // Expected format: { "id": "home.clock", "color": "#FF0088", "textColor": "#FFFFFF", "hidden": false, "fontSize": 3 }
        if (!doc.containsKey("id")) {
          Serial.println("[STYLES API] Missing id field");
          return;
        }
        
        const char* elementId = doc["id"].as<const char*>();
        StyleOverride so;
        
        if (doc.containsKey("color")) {
          so.hasColor = true;
          if (doc["color"].is<const char*>()) {
            so.color = StyleOverrides::hexToRgb565(doc["color"].as<const char*>());
          } else {
            so.color = doc["color"].as<uint16_t>();
          }
        }
        
        if (doc.containsKey("textColor")) {
          so.hasTextColor = true;
          if (doc["textColor"].is<const char*>()) {
            so.textColor = StyleOverrides::hexToRgb565(doc["textColor"].as<const char*>());
          } else {
            so.textColor = doc["textColor"].as<uint16_t>();
          }
        }
        
        if (doc.containsKey("hidden")) {
          so.hidden = doc["hidden"].as<bool>();
        }
        
        if (doc.containsKey("fontSize")) {
          so.hasFontSize = true;
          so.fontSize = doc["fontSize"].as<uint8_t>();
        }
        
        // Apply and save
        StyleOverrides::set(elementId, so);
        StyleOverrides::save();
        
        // Redraw UI to show changes
        uiRequestRedraw();
        
        Serial.printf("[STYLES API] Updated style for %s\n", elementId);
      }
    }
  );
  
  // DELETE a style override
  server.on("/api/styles", HTTP_DELETE, [](AsyncWebServerRequest* request){
    if (!request->hasParam("id")) {
      jsonResponse(request, "{\"error\":\"missing id\"}", 400);
      return;
    }
    
    const char* elementId = request->getParam("id")->value().c_str();
    StyleOverrides::remove(elementId);
    StyleOverrides::save();
    uiRequestRedraw();
    
    jsonResponse(request, "{\"status\":\"deleted\"}");
  });

  // ===== OTA UPDATE API =====
  
  // Get system info for OTA page
  server.on("/api/system/info", HTTP_GET, [](AsyncWebServerRequest* request){
    StaticJsonDocument<512> doc;
    
    // Firmware info
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["sketchMD5"] = ESP.getSketchMD5();
    doc["freeSketchSpace"] = ESP.getFreeSketchSpace();
    doc["flashChipSize"] = ESP.getFlashChipSize();
    doc["flashChipSpeed"] = ESP.getFlashChipSpeed();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    
    // SPIFFS info
    doc["spiffsTotal"] = SPIFFS.totalBytes();
    doc["spiffsUsed"] = SPIFFS.usedBytes();
    doc["spiffsFree"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    
    // Uptime
    doc["uptimeMs"] = millis();
    doc["uptimeSec"] = millis() / 1000;
    
    String output;
    serializeJson(doc, output);
    jsonResponse(request, output);
  });
  
  // OTA firmware update upload
  server.on("/api/ota/firmware", HTTP_POST, 
    [](AsyncWebServerRequest* request){
      // Final response after upload completes
      if (Update.hasError()) {
        String error = "Update failed: " + String(Update.errorString());
        Serial.println(error);
        jsonResponse(request, "{\"status\":\"error\",\"message\":\"" + error + "\"}", 500);
      } else {
        Serial.println("[OTA] Firmware update successful, restarting...");
        jsonResponse(request, "{\"status\":\"ok\",\"message\":\"Update successful, restarting...\"}");
        pendingRestart = true;
      }
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final){
      // FIX: Track progress with proper reset at start of each upload
      static size_t lastProgress = 0;
      static bool updateStarted = false;
      
      if (index == 0) {
        // Reset static accumulators at the start of a new upload
        lastProgress = 0;
        updateStarted = false;
        
        Serial.printf("[OTA] Firmware update start: %s (%u bytes expected)\n", filename.c_str(), request->contentLength());
        
        // Begin update - for firmware use U_FLASH
        if (!Update.begin(request->contentLength(), U_FLASH)) {
          Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
          return;
        }
        updateStarted = true;
        Serial.println("[OTA] Update started");
      }
      
      // Write chunk - only if update was started successfully
      if (updateStarted && Update.isRunning()) {
        if (Update.write(data, len) != len) {
          Serial.printf("[OTA] Update.write failed: %s\n", Update.errorString());
          updateStarted = false;  // Mark as failed
          return;
        }
        
        // Progress indicator
        size_t progress = index + len;
        if (progress - lastProgress > 50000) {  // Update every 50KB
          Serial.printf("[OTA] Progress: %u bytes\n", progress);
          lastProgress = progress;
        }
      }
      
      // Finalize
      if (final) {
        if (updateStarted && Update.end(true)) {
          Serial.printf("[OTA] Update complete: %u bytes\n", index + len);
        } else if (updateStarted) {
          Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        }
        // Reset state for next upload attempt
        updateStarted = false;
        lastProgress = 0;
      }
    }
  );
  
  // OTA SPIFFS update upload
  server.on("/api/ota/spiffs", HTTP_POST, 
    [](AsyncWebServerRequest* request){
      // Final response after upload completes
      if (Update.hasError()) {
        String error = "SPIFFS update failed: " + String(Update.errorString());
        Serial.println(error);
        jsonResponse(request, "{\"status\":\"error\",\"message\":\"" + error + "\"}", 500);
      } else {
        Serial.println("[OTA] SPIFFS update successful, restarting...");
        jsonResponse(request, "{\"status\":\"ok\",\"message\":\"SPIFFS update successful, restarting...\"}");
        pendingRestart = true;
      }
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final){
      // FIX: Track progress with proper reset at start of each upload
      static size_t lastProgress = 0;
      static bool updateStarted = false;
      
      if (index == 0) {
        // Reset static accumulators at the start of a new upload
        lastProgress = 0;
        updateStarted = false;
        
        Serial.printf("[OTA] SPIFFS update start: %s (%u bytes expected)\n", filename.c_str(), request->contentLength());
        
        // Begin update - for SPIFFS use U_SPIFFS
        if (!Update.begin(request->contentLength(), U_SPIFFS)) {
          Serial.printf("[OTA] SPIFFS Update.begin failed: %s\n", Update.errorString());
          return;
        }
        updateStarted = true;
        Serial.println("[OTA] SPIFFS Update started");
      }
      
      // Write chunk - only if update was started successfully
      if (updateStarted && Update.isRunning()) {
        if (Update.write(data, len) != len) {
          Serial.printf("[OTA] SPIFFS Update.write failed: %s\n", Update.errorString());
          updateStarted = false;  // Mark as failed
          return;
        }
        
        // Progress indicator
        size_t progress = index + len;
        if (progress - lastProgress > 50000) {  // Update every 50KB
          Serial.printf("[OTA] SPIFFS Progress: %u bytes\n", progress);
          lastProgress = progress;
        }
      }
      
      // Finalize
      if (final) {
        if (updateStarted && Update.end(true)) {
          Serial.printf("[OTA] SPIFFS Update complete: %u bytes\n", index + len);
        } else if (updateStarted) {
          Serial.printf("[OTA] SPIFFS Update.end failed: %s\n", Update.errorString());
        }
        // Reset state for next upload attempt
        updateStarted = false;
        lastProgress = 0;
      }
    }
  );
}

static void setupFileRoutes() {
  // Serve static assets from SPIFFS /data folder after upload via pio run -t uploadfs
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){
    AsyncWebServerResponse* res = request->beginResponse(SPIFFS, "/index.html", "text/html");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  // Explicit file routes with correct MIME and no-cache to avoid stale/partial caches
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request){
    AsyncWebServerResponse* res = request->beginResponse(SPIFFS, "/index.html", "text/html");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  server.on("/js/app.js", HTTP_GET, [](AsyncWebServerRequest* request){
    AsyncWebServerResponse* res = request->beginResponse(SPIFFS, "/js/app.js", "application/javascript");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest* request){
    AsyncWebServerResponse* res = request->beginResponse(SPIFFS, "/css/style.css", "text/css");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request){
    // Return 204 No Content to silence browser requests; add a real icon later if desired
    AsyncWebServerResponse* res = request->beginResponse(204);
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  server.serveStatic("/css/", SPIFFS, "/css/").setCacheControl("no-store").setDefaultFile("style.css");
  server.serveStatic("/js/", SPIFFS, "/js/").setCacheControl("no-store").setDefaultFile("app.js");
  server.onNotFound([](AsyncWebServerRequest* request){
    if (SPIFFS.exists(request->url())) { request->send(SPIFFS, request->url(), String()); return; }
    AsyncWebServerResponse* res = request->beginResponse(SPIFFS, "/index.html", "text/html");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });
  // Simple auth-check endpoint referenced by app.js
  server.on("/auth-check", HTTP_GET, [](AsyncWebServerRequest* request){ request->send(200); });
  // Dummy login to satisfy UI (no-op for now)
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest* request){ jsonResponse(request, "{\"status\":\"ok\"}"); });
}
