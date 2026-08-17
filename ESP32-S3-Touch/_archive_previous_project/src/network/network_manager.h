#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../ui/page_home.h"

// Network status
enum class NetworkStatus {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  ERROR
};

// API status
enum class ApiStatus {
  IDLE,
  FETCHING,
  SUCCESS,
  ERROR
};

// Network and API management class
class NetworkManager {
public:
  // Initialize the network manager
  static void init();
  
  // Connect to WiFi using config settings
  static bool connectWiFi();
  
  // Scan and connect to best available WiFi from hardcoded list
  static bool scanAndConnectWiFi();
  
  // Get network status
  static NetworkStatus getNetworkStatus();
  
  // Get API status
  static ApiStatus getApiStatus();
  
  // Get last error message
  static const char* getLastError();
  
  // Get API failure count (for checking if API is offline)
  static int getApiFailureCount();
  
  // Fetch API data
  static bool fetchApiData();
  
  // Process in main loop
  static void loop();
  
  // WiFi event handlers
  static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
  static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
  static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
  
  // Get WiFi signal strength
  static int getWiFiRSSI();
  // Get current SSID (empty if not connected)
  static String getSSID();
  // Get local IP as string (empty if not connected)
  static String getLocalIPString();
  
  // Get/set data API URL
  static String getApiUrl();
  
private:
  static NetworkStatus networkStatus;
  static ApiStatus apiStatus;
  static String lastError;
  static unsigned long lastApiRequest;
  static unsigned long lastWifiCheck;
  static int apiFailCount;
  static int wifiReconnectAttempts;
  
  // Connection check intervals
  static const unsigned long WIFI_CHECK_INTERVAL = 10000;  // 10 seconds
  static const unsigned long API_RETRY_INTERVAL = 60000;   // 1 minute
  static const unsigned long API_ERROR_BACKOFF_MAX = 600000; // 10 minutes
  
  // Parse API response
  static bool parseApiResponse(const String& jsonString);
};