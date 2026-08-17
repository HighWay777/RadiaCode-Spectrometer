#pragma once

/**
 * ========================================
 * WiFi Credentials Configuration
 * ========================================
 * 
 * EASY TO EDIT: Simply update the credentials below and rebuild.
 * The controller will automatically scan and connect to the first available network.
 * 
 * HOW IT WORKS:
 * 1. On first boot (or after NVRAM clear), these credentials are tried in order
 * 2. Once connected, the successful credentials are saved to NVRAM
 * 3. To force re-scan of these networks, clear NVRAM or reset to factory defaults
 * 
 * SECURITY NOTE: 
 * - These credentials are compiled into the firmware
 * - Consider using WiFi provisioning for production devices
 */

// WiFi Network Credentials (in priority order)
// Add or remove networks as needed - just update WIFI_NETWORK_COUNT accordingly

struct WiFiCredential {
  const char* ssid;
  const char* password;
};

// ========================================
// 🔧 EDIT YOUR WiFi CREDENTIALS HERE 🔧
// ========================================

const WiFiCredential WIFI_NETWORKS[] = {
  // Network 1 (Primary)
  {
    .ssid = "EVG 2.4GHz",
    .password = "***REMOVED***"
  },
  
  // Network 2 (Secondary/Backup)
  {
    .ssid = "Bubi 2.4",
    .password = "***REMOVED***"
  },
  
  // Network 3 (Optional - add more as needed)
  // {
  //   .ssid = "YourThirdNetwork",
  //   .password = "YourThirdPassword"
  // }
};

const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WiFiCredential);

// ========================================
// Optional: WiFi Scanning Configuration
// ========================================

// Maximum time to wait for each network (milliseconds)
const unsigned long WIFI_CONNECT_TIMEOUT = 10000;  // 10 seconds

// Whether to scan for strongest signal if multiple known networks are available
const bool WIFI_PREFER_STRONGEST_SIGNAL = true;
