# WiFi Multi-Network Configuration Guide

## 🎯 Overview

Your ESP32 controller now supports **multiple WiFi networks with automatic fallback**. The system will scan for available networks and connect to the best one from your configured list.

## 📝 How to Configure WiFi Networks

### 1. Edit the Credentials File

Open [`src/network/wifi_credentials.h`](src/network/wifi_credentials.h) and edit the `WIFI_NETWORKS` array:

```cpp
const WiFiCredential WIFI_NETWORKS[] = {
  // Network 1 (Primary)
  {
    .ssid = "YourPrimaryNetwork",
    .password = "YourPrimaryPassword"
  },
  
  // Network 2 (Secondary/Backup)
  {
    .ssid = "YourBackupNetwork",
    .password = "YourBackupPassword"
  },
  
  // Add more networks as needed...
};
```

### 2. Rebuild and Upload

After editing `wifi_credentials.h`:

```bash
# Build and upload firmware
pio run -t upload

# Or use VS Code task: "Build+Upload FW"
```

### 3. Clear NVRAM (if needed)

If you've changed credentials and the device is still using old ones:

**Option A - Factory Reset via Settings:**
- Go to Settings page on device
- Select "Reset to Factory Defaults"
- Device will reboot and scan for new networks

**Option B - Clear NVRAM via Serial:**
```cpp
// Add this temporarily to setup() in main.cpp
ConfigStore::resetToDefaults();
ConfigStore::save();
```

**Option C - Full Flash Erase:**
```bash
pio run -t erase
pio run -t upload
```

---

## 🔧 How It Works

### First Boot (No Saved Credentials)
1. Device scans for WiFi networks
2. Compares scan results with your configured list in `wifi_credentials.h`
3. Connects to first available network (or strongest signal if configured)
4. **Saves successful credentials to NVRAM**

### Subsequent Boots (Credentials Saved)
1. Device loads WiFi credentials from NVRAM
2. Attempts to connect to saved network
3. If connection fails repeatedly, you can trigger a re-scan via Settings

### Priority Mode
- **Priority Order** (default): Connects to first network in the list that's available
- **Strongest Signal**: Set `WIFI_PREFER_STRONGEST_SIGNAL = true` to connect to the network with best RSSI

---

## 🛠️ Configuration Options

In [`wifi_credentials.h`](src/network/wifi_credentials.h):

```cpp
// Maximum time to wait for each network
const unsigned long WIFI_CONNECT_TIMEOUT = 10000;  // 10 seconds

// Whether to prefer strongest signal over priority order
const bool WIFI_PREFER_STRONGEST_SIGNAL = true;
```

### Adding More Networks

Simply add more entries to the array:

```cpp
const WiFiCredential WIFI_NETWORKS[] = {
  {"Home_5GHz", "password1"},
  {"Home_2.4GHz", "password1"},
  {"Office_WiFi", "password2"},
  {"Mobile_Hotspot", "password3"},
  // ... up to any number of networks
};
```

The `WIFI_NETWORK_COUNT` is calculated automatically.

---

## 🔍 Troubleshooting

### "Device won't connect to new WiFi"
**Problem:** Changed credentials in `wifi_credentials.h`, but device still tries old network.

**Solution:** Old credentials are saved in NVRAM. Clear them:
1. Reset to factory defaults (Settings page)
2. Or flash with `pio run -t erase` then `pio run -t upload`

### "Device connects to wrong network"
**Problem:** Multiple configured networks are available, but device picks the wrong one.

**Solution:** 
- Reorder networks in `WIFI_NETWORKS` array (higher priority = earlier in list)
- Or set `WIFI_PREFER_STRONGEST_SIGNAL = true` to always use strongest

### "Scan finds networks but won't connect"
**Problem:** Device scans and finds your network but fails to connect.

**Possible causes:**
- Wrong password in `wifi_credentials.h` (check for typos)
- Router is blocking the device (check MAC address filter)
- Network is full (some routers limit connected devices)
- Signal is too weak (device may see it but can't maintain connection)

### "Want to add WPA3 or enterprise WiFi"
**Current limitation:** This implementation supports WPA/WPA2-PSK networks only.

For enterprise WiFi (WPA2-Enterprise), you'll need to modify `NetworkManager::scanAndConnectWiFi()` to use `WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, username, password)`.

---

## 📊 Network Status Monitoring

Monitor WiFi connection via serial output:

```
[WiFi] Scanning for available networks from credential list...
[WiFi] Scan complete, found 8 networks
[WiFi] Available networks:
  0: Home_5GHz (RSSI: -45 dBm, Encrypted)
  1: Home_2.4GHz (RSSI: -38 dBm, Encrypted)
  ...
[WiFi] Found known network 'Home_2.4GHz' with RSSI -38 dBm
[WiFi] Connecting to 'Home_2.4GHz' (RSSI: -38 dBm)...
[WiFi] Connected! IP: 192.168.1.150
```

---

## 🔐 Security Notes

### ⚠️ Credentials in Source Code
- WiFi passwords are **compiled into the firmware binary**
- Anyone with access to the firmware can extract credentials
- Consider this when deploying to production devices

### 🛡️ Better Alternatives for Production
1. **WiFi Provisioning** - Let users enter credentials via Bluetooth or AP mode
2. **Encrypted Storage** - Store credentials in encrypted NVS partition
3. **Environment Variables** - Use build flags to inject secrets at compile time

Example with build flags (platformio.ini):
```ini
[env:esp32-s3-devkitc-1]
build_flags = 
    -DWIFI_SSID=\"YourNetwork\"
    -DWIFI_PASS=\"YourPassword\"
```

---

## 📱 Runtime WiFi Management

Users can also change WiFi settings via the device UI:

1. **Settings Page** → Network section
2. Enter new SSID and password
3. Save settings
4. Device reconnects automatically

Changes made via UI are saved to NVRAM and take precedence over `wifi_credentials.h`.

To revert to hardcoded credentials, do a factory reset.

---

## ✅ Testing Checklist

After configuring WiFi:

- [ ] Edit `wifi_credentials.h` with at least 2 networks
- [ ] Build and upload firmware
- [ ] Clear NVRAM (factory reset or erase)
- [ ] Verify device scans and connects to primary network
- [ ] Turn off primary network, verify fallback to secondary
- [ ] Check serial monitor for connection logs
- [ ] Verify RSSI and IP address display correctly
- [ ] Test device functionality (API calls, NTP sync, etc.)

---

## 🚀 Quick Start Commands

```bash
# Edit credentials
code src/network/wifi_credentials.h

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor

# Full clean rebuild (if needed)
pio run -t clean
pio run -t upload
```

---

## 📚 Related Files

- **WiFi Credentials:** [`src/network/wifi_credentials.h`](src/network/wifi_credentials.h) - Edit this file
- **Network Manager:** [`src/network/network_manager.cpp`](src/network/network_manager.cpp) - Connection logic
- **Main Setup:** [`src/main.cpp`](src/main.cpp#L115-L140) - Initialization code
- **Config Store:** [`src/config/config_store.cpp`](src/config/config_store.cpp) - NVRAM persistence

---

## 💡 Pro Tips

1. **Test both networks** before deploying - make sure both SSID/password combinations work
2. **Use descriptive names** in comments to remember which network is which
3. **Keep 2.4GHz as backup** - better range than 5GHz for distant devices
4. **Monitor RSSI values** - networks with RSSI < -75 dBm may be unreliable
5. **Document your changes** - note which credentials are for which location

---

Need help? Check the serial monitor output for detailed connection logs.
