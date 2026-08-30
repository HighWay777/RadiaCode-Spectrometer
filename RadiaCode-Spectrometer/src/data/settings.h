#pragma once

#include <stdint.h>

// Where the device looks for new firmware. The repo is public, so releases
// are fetched anonymously over HTTPS and no token has to live on the device.
#define UPDATE_HOST      "git.vhelectronics.com"
#define UPDATE_REPO_PATH "/highway/RadiaCode-Spectrometer"
// Overridable from platformio.ini / build flags so a bench build can pretend
// to be older and exercise the update path.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.4"
#endif

// Small persisted preferences, kept in NVS next to the background reference.
//
// Nothing secret is ever compiled into the image: WiFi credentials are typed
// in on the device and stored here, so the firmware and its source can be
// published without carrying anyone's network details.
namespace Settings {
  void begin();

  // Reconnect automatically after the BLE link drops. The RC-110 keeps
  // accumulating its spectrum internally while disconnected, so a recovered
  // link resumes the same measurement rather than starting a new one.
  bool autoReconnect();
  void setAutoReconnect(bool on);

  // Attempt to connect as soon as the board powers up.
  bool connectAtStartup();
  void setConnectAtStartup(bool on);

  uint8_t brightness();          // 20..255
  void setBrightness(uint8_t v);

  // Remembers that a measurement session was live, so a power cycle can pick
  // it up again. Set on a successful connect, cleared only by a deliberate
  // disconnect — a crash or power loss therefore resumes, while "I pressed
  // DISCONNECT" stays disconnected.
  bool wasConnected();
  void setWasConnected(bool on);

  // --- WiFi (entered on-device, stored here) -----------------------------
  const char* wifiSsid();
  const char* wifiPassword();
  void setWifi(const char* ssid, const char* password);
  bool hasWifi();
  void clearWifi();

  // False until first-run setup has been completed or skipped.
  bool setupDone();
  void setSetupDone(bool done);

  // --- Firmware updates ---------------------------------------------------
  bool autoUpdate();             // check for a new release at startup
  void setAutoUpdate(bool on);

  // Wipes every stored preference, the WiFi credentials and the background
  // reference, returning the device to an out-of-the-box state. The caller is
  // expected to reboot straight afterwards so first-run setup appears again.
  void factoryReset();
}
