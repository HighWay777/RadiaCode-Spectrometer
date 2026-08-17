#include "network_manager.h"
#include "../config/config_store.h"
#include "wifi_credentials.h"
#include <WiFi.h>
#include "weather_service.h"

// Initialize static variables
NetworkStatus NetworkManager::networkStatus = NetworkStatus::DISCONNECTED;
ApiStatus NetworkManager::apiStatus = ApiStatus::IDLE;
String NetworkManager::lastError = "";
unsigned long NetworkManager::lastApiRequest = 0;
unsigned long NetworkManager::lastWifiCheck = 0;
int NetworkManager::apiFailCount = 0;
int NetworkManager::wifiReconnectAttempts = 0;
static unsigned long lastFallbackScanAttempt = 0;

// Default API endpoint (will be overridden by config)
static const char* DEFAULT_API_URL = "https://boiler101.vhelectronics.com/api/status";

String NetworkManager::getApiUrl() {
  AppConfig* config = ConfigStore::getConfig();
  if (strlen(config->dataApiUrl) > 0) {
    return String(config->dataApiUrl);
  }
  return String(DEFAULT_API_URL);
}

void NetworkManager::init() {
  // Disconnect any existing WiFi connection
  WiFi.disconnect(true);
  delay(100);
  
  // Set WiFi mode
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable WiFi sleep mode for better responsiveness
  
  // Set hostname
  WiFi.setHostname("ESP32-S3-Touch");
  
  // Register WiFi event handlers
  WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  
  Serial.println("WiFi initialized");
}

bool NetworkManager::connectWiFi() {
  if (networkStatus == NetworkStatus::CONNECTING) {
    return false; // Already trying to connect
  }
  
  AppConfig* config = ConfigStore::getConfig();
  
  // Check if WiFi credentials are configured
  if (strlen(config->wifiSsid) == 0) {
    lastError = "WiFi SSID not configured";
    return false;
  }
  
  Serial.printf("Connecting to WiFi SSID: %s\n", config->wifiSsid);
  networkStatus = NetworkStatus::CONNECTING;
  
  // Use static IP if configured; otherwise explicitly restore DHCP.
  // This prevents stale static settings from previous runs from breaking reconnects.
  if (config->useStaticIp) {
    IPAddress ip(config->ipAddress[0], config->ipAddress[1], 
                 config->ipAddress[2], config->ipAddress[3]);
    IPAddress gateway(config->gateway[0], config->gateway[1], 
                      config->gateway[2], config->gateway[3]);
    IPAddress subnet(config->subnet[0], config->subnet[1], 
                     config->subnet[2], config->subnet[3]);
    IPAddress dns1(config->dns1[0], config->dns1[1], 
                   config->dns1[2], config->dns1[3]);
    IPAddress dns2(config->dns2[0], config->dns2[1], 
                   config->dns2[2], config->dns2[3]);
    
    WiFi.config(ip, gateway, subnet, dns1, dns2);
  } else {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  
  // Connect to WiFi
  WiFi.begin(config->wifiSsid, config->wifiPassword);
  
  // Don't wait for connection here, it will be handled asynchronously
  return true;
}

bool NetworkManager::scanAndConnectWiFi() {
  Serial.println("[WiFi] Scanning for available networks from credential list...");
  
  // Perform WiFi scan
  int networksFound = WiFi.scanNetworks();
  Serial.printf("[WiFi] Scan complete, found %d networks\n", networksFound);
  
  if (networksFound == 0) {
    lastError = "No WiFi networks found";
    Serial.println("[WiFi] No networks detected");
    return false;
  }
  
  // Print all found networks for debugging
  Serial.println("[WiFi] Available networks:");
  for (int i = 0; i < networksFound; i++) {
    Serial.printf("  %d: %s (RSSI: %d dBm, %s)\n", 
                  i, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
  }
  
  // Find best matching network from our credential list
  int bestNetworkIndex = -1;
  int bestRSSI = -1000;
  int bestCredentialIndex = -1;
  
  for (int credIdx = 0; credIdx < WIFI_NETWORK_COUNT; credIdx++) {
    const char* targetSSID = WIFI_NETWORKS[credIdx].ssid;
    
    for (int scanIdx = 0; scanIdx < networksFound; scanIdx++) {
      String foundSSID = WiFi.SSID(scanIdx);
      int rssi = WiFi.RSSI(scanIdx);
      
      if (foundSSID.equals(targetSSID)) {
        Serial.printf("[WiFi] Found known network '%s' with RSSI %d dBm\n", targetSSID, rssi);
        
        // If preferring strongest signal, check RSSI; otherwise, use first match
        if (!WIFI_PREFER_STRONGEST_SIGNAL) {
          // Use first match (priority order)
          bestNetworkIndex = scanIdx;
          bestCredentialIndex = credIdx;
          bestRSSI = rssi;
          break;
        } else if (rssi > bestRSSI) {
          // Use strongest signal
          bestNetworkIndex = scanIdx;
          bestCredentialIndex = credIdx;
          bestRSSI = rssi;
        }
      }
    }
    
    // If not preferring strongest, stop at first match
    if (!WIFI_PREFER_STRONGEST_SIGNAL && bestNetworkIndex >= 0) {
      break;
    }
  }
  
  // Clean up scan results
  WiFi.scanDelete();
  
  if (bestNetworkIndex < 0) {
    lastError = "No known WiFi networks available";
    Serial.println("[WiFi] None of the configured networks are in range");
    return false;
  }
  
  // Found a network - update config and connect
  const char* selectedSSID = WIFI_NETWORKS[bestCredentialIndex].ssid;
  const char* selectedPassword = WIFI_NETWORKS[bestCredentialIndex].password;
  
  Serial.printf("[WiFi] Connecting to '%s' (RSSI: %d dBm)...\n", selectedSSID, bestRSSI);
  
  // Update config with selected credentials
  AppConfig* config = ConfigStore::getConfig();
  strncpy(config->wifiSsid, selectedSSID, MAX_WIFI_SSID_LEN - 1);
  config->wifiSsid[MAX_WIFI_SSID_LEN - 1] = '\0';
  strncpy(config->wifiPassword, selectedPassword, MAX_WIFI_PASS_LEN - 1);
  config->wifiPassword[MAX_WIFI_PASS_LEN - 1] = '\0';
  
  // Save to NVRAM so it persists
  ConfigStore::save();
  
  // Now connect using the standard method
  return connectWiFi();
}

NetworkStatus NetworkManager::getNetworkStatus() {
  return networkStatus;
}

ApiStatus NetworkManager::getApiStatus() {
  return apiStatus;
}

const char* NetworkManager::getLastError() {
  return lastError.c_str();
}

int NetworkManager::getApiFailureCount() {
  return apiFailCount;
}

bool NetworkManager::fetchApiData() {
  // Check if we're connected to WiFi
  if (networkStatus != NetworkStatus::CONNECTED) {
    lastError = "WiFi not connected";
    return false;
  }
  
  // Update status
  apiStatus = ApiStatus::FETCHING;
  lastApiRequest = millis();
  
  // Get API URL from config
  String apiUrl = getApiUrl();
  
  HTTPClient http;
  http.begin(apiUrl);
  
  // Set headers if needed
  // http.addHeader("Content-Type", "application/json");
  
  Serial.printf("Fetching API data from: %s\n", apiUrl.c_str());
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("API data received");
    
    // Parse the JSON response
    bool success = parseApiResponse(payload);
    
    if (success) {
      apiStatus = ApiStatus::SUCCESS;
      // CRITICAL FIX: Reset failure count when API succeeds
      // This ensures the "API error" badge disappears when connection is restored
      if (apiFailCount > 0) {
        Serial.printf("[API] Connection restored after %d failures\n", apiFailCount);
      }
      apiFailCount = 0;
      // NOTE: Don't call uiRequestHomeUpdate() here - uiLoop handles periodic updates
      // This prevents double-refresh and flickering
    } else {
      apiStatus = ApiStatus::ERROR;
      lastError = "Failed to parse API response";
      apiFailCount++;
    }
    
    http.end();
    return success;
  } else {
    apiStatus = ApiStatus::ERROR;
    lastError = "HTTP request failed: " + String(httpCode);
    Serial.printf("HTTP request failed, code: %d\n", httpCode);
    apiFailCount++;
    http.end();
    return false;
  }
}

// Flag to defer WiFi reconnection from interrupt context to main loop
static volatile bool needsWifiReconnect = false;

void NetworkManager::loop() {
  unsigned long now = millis();
  
  // FIX: Handle deferred WiFi reconnection from main loop (not interrupt context)
  if (needsWifiReconnect) {
    needsWifiReconnect = false;
    Serial.println("[WiFi] Deferred reconnection triggered from main loop");
    WiFi.reconnect();
  }
  
  // Check WiFi status periodically
  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    
    if (WiFi.status() != WL_CONNECTED && networkStatus != NetworkStatus::CONNECTING) {
      Serial.println("WiFi disconnected, attempting to reconnect...");
      connectWiFi();
    }

    // If reconnect keeps failing for a while, try auto-recover by scanning known networks.
    // This helps when saved NVS credentials are stale/invalid.
    if (WiFi.status() != WL_CONNECTED && wifiReconnectAttempts >= 6) {
      const unsigned long FALLBACK_SCAN_INTERVAL = 60000; // 60 seconds
      if (now - lastFallbackScanAttempt >= FALLBACK_SCAN_INTERVAL) {
        lastFallbackScanAttempt = now;
        Serial.println("[WiFi] Reconnect failed repeatedly; trying known-network scan fallback...");
        bool scanConnectStarted = scanAndConnectWiFi();
        if (!scanConnectStarted) {
          Serial.println("[WiFi] Fallback scan found no usable known network");
        }
      }
    }
  }
  
  // Handle API requests
  AppConfig* config = ConfigStore::getConfig();
  unsigned long apiInterval = config->weatherUpdateInterval * 60 * 1000; // Convert minutes to ms
  
  // Apply exponential backoff on failures
  if (apiFailCount > 0) {
    unsigned long backoff = min(API_ERROR_BACKOFF_MAX, 
                              API_RETRY_INTERVAL * (1 << min(apiFailCount - 1, 5)));
    apiInterval = backoff;
  }
  
  // Check if it's time to fetch API data
  // Fetch immediately on first run (lastApiRequest==0) or when interval has elapsed
  if (networkStatus == NetworkStatus::CONNECTED && 
      apiStatus != ApiStatus::FETCHING &&
      (lastApiRequest == 0 || now - lastApiRequest >= apiInterval)) {
    fetchApiData();
  }
}

void NetworkManager::onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi event: CONNECTED to AP, waiting for IP...");
}

void NetworkManager::onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  networkStatus = NetworkStatus::CONNECTED;
  wifiReconnectAttempts = 0;
  lastFallbackScanAttempt = 0;
  
  Serial.println("WiFi fully connected with IP");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
  
  // Immediately trigger boiler API fetch and allow weather service to fetch on first run
  lastApiRequest = 0; // force immediate fetch in next loop
}

void NetworkManager::onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  networkStatus = NetworkStatus::DISCONNECTED;
  Serial.println("WiFi disconnected");
  
  // Increment reconnect attempts
  wifiReconnectAttempts++;
  
  // Always schedule reconnect from main loop (retry forever as required)
  needsWifiReconnect = true;
}

int NetworkManager::getWiFiRSSI() {
  if (networkStatus == NetworkStatus::CONNECTED) {
    return WiFi.RSSI();
  }
  return -100; // Default low value when not connected
}

String NetworkManager::getSSID() {
  if (networkStatus == NetworkStatus::CONNECTED) {
    return WiFi.SSID();
  }
  return String("");
}

String NetworkManager::getLocalIPString() {
  if (networkStatus == NetworkStatus::CONNECTED) {
    return WiFi.localIP().toString();
  }
  return String("");
}

bool NetworkManager::parseApiResponse(const String& jsonString) {
  // Use ArduinoJson to parse the response
  // Increased buffer size to handle full API response (typically ~1.5KB)
  DynamicJsonDocument doc(4096);
  
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    Serial.printf("Response length: %d bytes\n", jsonString.length());
    return false;
  }
  
  // Debug: Log successful parse
  Serial.printf("[API] Parsed response successfully (%d bytes)\n", jsonString.length());
  
  // Extract data and update the boiler/energy portion of the data structure
  WeatherData data;
  
  // Helper lambdas for flexible key lookup
  auto getFloat = [&](std::initializer_list<const char*> keys) -> float {
    for (auto k : keys) { if (doc.containsKey(k)) return doc[k].as<float>(); }
    return NAN;
  };
  auto getInt = [&](std::initializer_list<const char*> keys, int def=-1) -> int {
    for (auto k : keys) { if (doc.containsKey(k)) return doc[k].as<int>(); }
    return def;
  };
  auto getBool = [&](std::initializer_list<const char*> keys, bool def=false) -> bool {
    for (auto k : keys) { if (doc.containsKey(k)) return doc[k].as<bool>(); }
    return def;
  };
  auto getString = [&](std::initializer_list<const char*> keys) -> String {
    for (auto k : keys) { if (doc.containsKey(k)) return doc[k].as<String>(); }
    return String();
  };

  // ===== TEMPERATURE DATA =====
  // Primary keys first, then fallbacks
  data.currentTemp = getFloat({"currentTemp", "temp", "current", "temperature"});
  data.targetTemp  = getFloat({"targetTemp", "target", "setpoint", "aiTargetTemp"});
  data.dayTemp     = getFloat({"dayTemp", "day_setpoint"});
  data.nightTemp   = getFloat({"nightTemp", "night_setpoint"});
  data.boostTemp   = getFloat({"boostTemp", "boost_setpoint"});
  
  // ===== STATE DATA =====
  data.isNightTime       = getBool({"isNightTime", "nightTime"});
  data.isBoostTime       = getBool({"isBoostTime", "boostTime"});
  data.targetSource      = getString({"targetSource", "source", "mode"});
  
  // Heater state - handle both bool and string "heater" field
  data.heaterOn = getBool({"heaterOn", "heating", "isHeating"});
  // Also check string "heater" field (API returns "on"/"off")
  if (doc.containsKey("heater") && doc["heater"].is<const char*>()) {
    String heaterStr = doc["heater"].as<String>();
    heaterStr.toLowerCase();
    data.heaterOn = (heaterStr == "on" || heaterStr == "true" || heaterStr == "1");
  }
  
  data.manualOverride    = getBool({"manualOverride", "override"});
  
  // Heater control mode: 0=off, 1=manual, 2=auto, 3=ai
  data.heaterControlMode = getInt({"heaterControlMode"}, 0);
  
  // AI enabled state
  data.aiEnabled = getBool({"aiEnabled", "ai", "autoAI"});
  
  // Storage mode - shows where boiler settings are being read from (MQTT or NVRAM)
  data.storageMode = getString({"activeStorageSource", "storageSource", "storage"});
  Serial.printf("[API] Storage mode parsed: '%s'\n", data.storageMode.c_str());
  if (data.storageMode.isEmpty()) data.storageMode = "???";
  
  // ===== REMOTE DEVICE STATUS =====
  data.remoteFreeMemory = getInt({"freeMemory", "freeHeap"}, 0);
  data.remoteRSSI = getInt({"rssi", "wifiRSSI"}, -100);
  data.remoteUptime = getString({"uptime"});
  data.remoteServerTime = getString({"serverTime"});
  
  // ===== NETWORK DATA =====
  // Use remote API's RSSI if provided, otherwise use local WiFi RSSI
  data.wifiRSSI = getInt({"wifiRSSI", "rssi"}, WiFi.RSSI());
  
  // ===== ENERGY/EFFICIENCY METRICS =====
  data.efficiency   = getFloat({"efficiency"});
  data.heatLossRate = getFloat({"heatLossRate", "heat_loss_rate"});
  data.kwhTotal     = getFloat({"kwhTotal", "kWhTotal", "energyTotal"});
  data.costTotal    = getFloat({"costTotal", "totalCost"});
  data.dailyKWh     = getFloat({"dailyKWh", "kWhToday", "energyToday"});
  data.costToday    = getFloat({"costToday", "todayCost"});
  data.dailyCost    = getFloat({"dailyCost", "costDay"});
  
  // ===== OPTIONAL WEATHER CODE =====
  // Pass through weatherCode if provided by local API
  if (doc.containsKey("weatherCode")) {
    data.weatherCode = doc["weatherCode"].as<int>();
  }

  // Debug: Log key values for verification
  Serial.printf("[API] Boiler: %.1f°C -> %.1f°C, Heater: %s, Mode: %d, AI: %s\n",
                data.currentTemp, data.targetTemp,
                data.heaterOn ? "ON" : "OFF",
                data.heaterControlMode,
                data.aiEnabled ? "enabled" : "disabled");
  Serial.printf("[API] Energy: %.2f kWh today, €%.2f cost, %.1f%% efficiency\n",
                data.dailyKWh, data.costToday, data.efficiency);

  updateBoilerData(data);
  
  return true;
}