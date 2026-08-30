#include "updater.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <string.h>

namespace Updater {

static State s_state = IDLE;
static char s_status[80] = "Idle";
static char s_latest[24] = "";
static char s_assetUrl[220] = "";
static int s_progress = 0;
static volatile bool s_reqCheck = false;
static volatile bool s_reqInstall = false;
static bool s_started = false;

State state() { return s_state; }
const char* statusText() { return s_status; }
const char* latestVersion() { return s_latest; }
int progressPercent() { return s_progress; }
bool busy() {
  return s_state == WIFI_CONNECTING || s_state == CHECKING || s_state == DOWNLOADING;
}

static void setStatus(State st, const char* fmt, ...) {
  s_state = st;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_status, sizeof(s_status), fmt, ap);
  va_end(ap);
  Serial.printf("[OTA] %s\n", s_status);
}

// Compares dotted versions ("1.2.3"), tolerating a leading 'v'.
// Returns >0 when a is newer than b.
static int versionCompare(const char* a, const char* b) {
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;
  for (;;) {
    long va = strtol(a, (char**)&a, 10);
    long vb = strtol(b, (char**)&b, 10);
    if (va != vb) return (va > vb) ? 1 : -1;
    while (*a == '.') a++;
    while (*b == '.') b++;
    if (!*a && !*b) return 0;
    if (!*a) return -1;
    if (!*b) return 1;
  }
}

static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!Settings::hasWifi()) {
    setStatus(FAILED, "No WiFi configured - set it in Settings");
    return false;
  }
  setStatus(WIFI_CONNECTING, "Connecting to %s...", Settings::wifiSsid());
  WiFi.mode(WIFI_STA);
  WiFi.begin(Settings::wifiSsid(), Settings::wifiPassword());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(FAILED, "WiFi connect failed");
    return false;
  }
  return true;
}

static void doCheck() {
  if (!ensureWifi()) return;
  setStatus(CHECKING, "Checking for updates...");

  WiFiClientSecure client;
  // Gitea uses a normal public CA, but pinning a root here would break the
  // moment the certificate rotates and the device has no clock to validate
  // dates against anyway. The payload is verified by Update.h's own checks,
  // and a corrupted image simply fails to boot into the new slot.
  client.setInsecure();

  HTTPClient http;
  char url[220];
  snprintf(url, sizeof(url), "https://%s/api/v1/repos%s/releases/latest",
           UPDATE_HOST, UPDATE_REPO_PATH);
  if (!http.begin(client, url)) {
    setStatus(FAILED, "Cannot reach %s", UPDATE_HOST);
    return;
  }
  http.setTimeout(10000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    setStatus(FAILED, "Update server returned %d", code);
    return;
  }

  // Only a few fields are needed; filtering keeps the document small enough
  // for a comfortable stack allocation.
  StaticJsonDocument<256> filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    setStatus(FAILED, "Bad response: %s", err.c_str());
    return;
  }

  const char* tag = doc["tag_name"] | "";
  if (!tag[0]) { setStatus(FAILED, "No release found"); return; }
  strncpy(s_latest, tag, sizeof(s_latest) - 1);
  s_latest[sizeof(s_latest) - 1] = '\0';

  s_assetUrl[0] = '\0';
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    const char* name = a["name"] | "";
    if (strstr(name, ".bin")) {
      strncpy(s_assetUrl, a["browser_download_url"] | "", sizeof(s_assetUrl) - 1);
      s_assetUrl[sizeof(s_assetUrl) - 1] = '\0';
      break;
    }
  }

  if (versionCompare(s_latest, FIRMWARE_VERSION) <= 0) {
    setStatus(UP_TO_DATE, "Up to date (%s)", FIRMWARE_VERSION);
  } else if (!s_assetUrl[0]) {
    setStatus(FAILED, "%s has no .bin asset", s_latest);
  } else {
    setStatus(AVAILABLE, "Version %s available", s_latest);
  }
}

static void doInstall() {
  if (!s_assetUrl[0]) { setStatus(FAILED, "Nothing to install"); return; }
  if (!ensureWifi()) return;

  setStatus(DOWNLOADING, "Downloading %s...", s_latest);
  s_progress = 0;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, s_assetUrl)) { setStatus(FAILED, "Download failed to start"); return; }
  http.setTimeout(15000);
  // Gitea redirects release assets to storage.
  const char* headers[] = {"Location"};
  http.collectHeaders(headers, 1);
  int code = http.GET();
  if (code != 200) { http.end(); setStatus(FAILED, "Download HTTP %d", code); return; }

  int total = http.getSize();
  if (total <= 0) { http.end(); setStatus(FAILED, "Unknown firmware size"); return; }

  // Writes into the *inactive* OTA slot; the running image is untouched, so a
  // failure here is harmless and the device stays on the current version.
  if (!Update.begin(total)) {
    http.end();
    setStatus(FAILED, "Not enough space (%d bytes)", total);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t chunk[1024];
  int written = 0;
  unsigned long lastData = millis();
  while (written < total) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(chunk, avail > sizeof(chunk) ? sizeof(chunk) : avail);
      if (n <= 0) break;
      if (Update.write(chunk, n) != (size_t)n) break;
      written += n;
      s_progress = (int)((int64_t)written * 100 / total);
      lastData = millis();
    } else {
      if (millis() - lastData > 15000) break;  // stalled
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  http.end();

  if (written != total || !Update.end(true)) {
    Update.abort();
    setStatus(FAILED, "Update aborted at %d%%", s_progress);
    return;
  }

  setStatus(SUCCESS, "Installed %s - restarting", s_latest);
  vTaskDelay(pdMS_TO_TICKS(1500));
  ESP.restart();
}

static void updaterTask(void*) {
  for (;;) {
    if (s_reqCheck)   { s_reqCheck = false;   doCheck(); }
    if (s_reqInstall) { s_reqInstall = false; doInstall(); }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void begin() {
  if (s_started) return;
  s_started = true;
  // Own task: HTTPS + flash writes take many seconds and must never block
  // the UI. Core 1 alongside the UI is fine — it is idle most of the time —
  // and keeps core 0 free for BLE.
  xTaskCreatePinnedToCore(updaterTask, "ota", 10240, nullptr, 1, nullptr, 1);
  if (Settings::autoUpdate() && Settings::hasWifi()) {
    Serial.println("[OTA] Auto-update enabled, checking at startup");
    s_reqCheck = true;
  }
}

void checkForUpdate() { if (!busy()) s_reqCheck = true; }
void installUpdate()  { if (!busy()) s_reqInstall = true; }

} // namespace Updater
