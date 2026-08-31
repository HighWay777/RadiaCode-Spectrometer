#include "settings.h"
#include <Preferences.h>
#include <math.h>

namespace Settings {

static const char* NS = "rcsettings";
// Off by default. A brand-new device connecting to a detector on its own,
// before the user has asked for anything, is surprising — it appeared to
// "reconnect by itself" during first-time setup. Opt in from Settings; the
// choice then persists in NVS until a factory reset.
static bool s_autoReconnect = false;
static bool s_connectAtStartup = false;
static uint8_t s_brightness = 200;
static bool s_wasConnected = false;
static char s_ssid[33] = "";      // 32 chars max per 802.11
static char s_pass[65] = "";      // 64 chars max for WPA2
static bool s_setupDone = false;
static bool s_autoUpdate = false;
static float s_energyGain = 1.0f;

void begin() {
  Preferences p;
  if (!p.begin(NS, true)) return;
  s_autoReconnect = p.getBool("autorec", false);
  s_connectAtStartup = p.getBool("startcon", false);
  s_brightness = p.getUChar("bright", 200);
  s_wasConnected = p.getBool("wascon", false);
  s_setupDone = p.getBool("setupdone", false);
  s_autoUpdate = p.getBool("autoupd", false);
  s_energyGain = p.getFloat("egain", 1.0f);
  if (s_energyGain < 0.90f || s_energyGain > 1.10f) s_energyGain = 1.0f;
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  strncpy(s_ssid, ssid.c_str(), sizeof(s_ssid) - 1);
  strncpy(s_pass, pass.c_str(), sizeof(s_pass) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  s_pass[sizeof(s_pass) - 1] = '\0';
  p.end();
}

const char* wifiSsid() { return s_ssid; }
const char* wifiPassword() { return s_pass; }
bool hasWifi() { return s_ssid[0] != '\0'; }

void setWifi(const char* ssid, const char* password) {
  strncpy(s_ssid, ssid ? ssid : "", sizeof(s_ssid) - 1);
  strncpy(s_pass, password ? password : "", sizeof(s_pass) - 1);
  s_ssid[sizeof(s_ssid) - 1] = '\0';
  s_pass[sizeof(s_pass) - 1] = '\0';
  Preferences p;
  if (p.begin(NS, false)) {
    p.putString("ssid", s_ssid);
    p.putString("pass", s_pass);
    p.end();
  }
}

void clearWifi() { setWifi("", ""); }

bool setupDone() { return s_setupDone; }
void setSetupDone(bool done) {
  s_setupDone = done;
  Preferences p;
  if (p.begin(NS, false)) { p.putBool("setupdone", done); p.end(); }
}

void factoryReset() {
  // Clear this namespace wholesale rather than key by key, so a preference
  // added later cannot be forgotten here and survive a "factory reset".
  Preferences p;
  if (p.begin(NS, false)) { p.clear(); p.end(); }
  // The background reference lives in its own namespace (see background.cpp).
  Preferences b;
  if (b.begin("radiacode", false)) { b.clear(); b.end(); }

  s_autoReconnect = false;   // same out-of-the-box default as a fresh device
  s_connectAtStartup = false;
  s_brightness = 200;
  s_wasConnected = false;
  s_setupDone = false;
  s_autoUpdate = false;
  s_ssid[0] = '\0';
  s_pass[0] = '\0';
  Serial.println("[SET] Factory reset - all settings and stored data cleared");
}

bool autoUpdate() { return s_autoUpdate; }
void setAutoUpdate(bool on) {
  s_autoUpdate = on;
  Preferences p;
  if (p.begin(NS, false)) { p.putBool("autoupd", on); p.end(); }
}

bool wasConnected() { return s_wasConnected; }
void setWasConnected(bool on) {
  if (s_wasConnected == on) return;  // avoid needless NVS wear
  s_wasConnected = on;
  Preferences p;
  if (p.begin(NS, false)) { p.putBool("wascon", on); p.end(); }
}

static void putBool(const char* key, bool v) {
  Preferences p;
  if (p.begin(NS, false)) { p.putBool(key, v); p.end(); }
}

bool autoReconnect() { return s_autoReconnect; }
void setAutoReconnect(bool on) { s_autoReconnect = on; putBool("autorec", on); }

bool connectAtStartup() { return s_connectAtStartup; }
void setConnectAtStartup(bool on) { s_connectAtStartup = on; putBool("startcon", on); }

float energyGain() { return s_energyGain; }

void setEnergyGain(float g) {
  if (g < 0.90f || g > 1.10f) return;          // implausible, ignore
  if (fabsf(g - s_energyGain) < 0.002f) return; // avoid needless NVS wear
  s_energyGain = g;
  Preferences p;
  if (p.begin(NS, false)) { p.putFloat("egain", g); p.end(); }
}

uint8_t brightness() { return s_brightness; }
void setBrightness(uint8_t v) {
  if (v < 20) v = 20;
  s_brightness = v;
  Preferences p;
  if (p.begin(NS, false)) { p.putUChar("bright", v); p.end(); }
}

} // namespace Settings
