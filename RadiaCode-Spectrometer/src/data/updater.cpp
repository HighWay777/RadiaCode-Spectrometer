#include "updater.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include "ble_source.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <string.h>

// Set to 1 only to verify the update path on the bench; see begin().
#ifndef OTA_SELFTEST
#define OTA_SELFTEST 0
#endif

// Bench aid: repeatedly runs an update check while a measurement is live.
// That is the case that actually failed - WiFi coming up alongside an
// active BLE stream exhausted the heap ("BLE_INIT: Malloc failed") and
// truncated every spectrum transfer. Checking only at boot, before BLE
// connects, does not exercise it.
#ifndef WIFI_BLE_STRESS
#define WIFI_BLE_STRESS 0
#endif

namespace Updater {

static State s_state = IDLE;
static char s_status[80] = "Idle";
static char s_latest[24] = "";
static char s_assetUrl[220] = "";
static int s_progress = 0;
static volatile bool s_reqCheck = false;
static volatile bool s_reqInstall = false;
static volatile bool s_reqWifi = false;
static volatile bool s_reqRelease = false;
static bool s_started = false;
static char s_ip[20] = "";

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
const char* wifiIp() { return s_ip; }
void connectWifi() { s_reqWifi = true; }
void releaseWifiNow() { s_reqRelease = true; }

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

// Releases WiFi so its ~40-50 KB goes back to Bluedroid, and lets the BLE
// task pick the measurement back up.
static void releaseWifi() {
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true, false);
    // Without a moment between these, the driver logs
    // "wifi:timeout when WiFi un-init" and tears down untidily.
    vTaskDelay(pdMS_TO_TICKS(200));
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(100));
    s_ip[0] = '\0';
    Serial.printf("[OTA] WiFi off (heap %u)\n", (unsigned)ESP.getFreeHeap());
  }
  BleSource::resumeAfterWifi();
}

static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!Settings::hasWifi()) {
    setStatus(FAILED, "No WiFi configured - set it in Settings");
    return false;
  }
  // The two radios do not both fit in this board's heap: starting WiFi
  // while BLE was streaming gave "BLE_INIT: Malloc failed" and every
  // spectrum read then truncated mid-transfer. Drop the link first - the
  // detector keeps counting regardless, so no measurement is lost.
  BleSource::suspendForWifi();
  for (int i = 0; i < 120 && !BleSource::isSuspended(); i++) {
    vTaskDelay(pdMS_TO_TICKS(50));  // up to 6 s: a spectrum read can be 4 s
  }

  setStatus(WIFI_CONNECTING, "Connecting to %s...", Settings::wifiSsid());
  Serial.printf("[OTA] Starting WiFi (heap %u)\n", (unsigned)ESP.getFreeHeap());
  WiFi.mode(WIFI_STA);
  WiFi.begin(Settings::wifiSsid(), Settings::wifiPassword());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (WiFi.status() != WL_CONNECTED) {
    // Distinguish "wrong password / no such network" from "server problem",
    // which are fixed in completely different places.
    setStatus(FAILED, "WiFi failed - check name and password");
    s_ip[0] = '\0';
    releaseWifi();
    return false;
  }
  snprintf(s_ip, sizeof(s_ip), "%s", WiFi.localIP().toString().c_str());
  Serial.printf("[OTA] WiFi connected, IP %s\n", s_ip);
  return true;
}

static void doWifiOnly() {
  if (ensureWifi()) setStatus(IDLE, "WiFi connected - %s", s_ip);
}

// Preferred check: a plain text manifest committed in the repo and fetched
// over an ordinary raw file path.
//
// Why not the API: on the reference server, Cloudflare returns an empty 403
// for every /api/* request while normal repo paths pass through. A manifest
// therefore works where the API cannot, needs no JSON parsing, and behaves
// the same on any Git host. The API is still tried as a fallback below.
//
// firmware/latest.txt:
//   version=1.0.1
//   url=https://host/owner/repo/releases/download/v1.0.1/firmware.bin
static bool doCheckManifest() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  char url[240];
  snprintf(url, sizeof(url), "https://%s%s/raw/branch/main/firmware/latest.txt",
           UPDATE_HOST, UPDATE_REPO_PATH);
  Serial.printf("[OTA] GET %s\n", url);
  const char* wantHeaders[] = {"Content-Type"};
  http.collectHeaders(wantHeaders, 1);
  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  // A sign-in-protected server answers 200 with an HTML login page.
  String ctype = http.header("Content-Type");
  String body = http.getString();
  http.end();
  if (ctype.indexOf("html") >= 0 || body.indexOf("<!DOCTYPE") >= 0) return false;

  String ver, bin;
  int start = 0;
  while (start < (int)body.length()) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = body.length();
    String line = body.substring(start, nl);
    line.trim();
    if (line.startsWith("version=")) ver = line.substring(8);
    else if (line.startsWith("url=")) bin = line.substring(4);
    start = nl + 1;
  }
  ver.trim(); bin.trim();
  if (ver.length() == 0 || bin.length() == 0) return false;

  strncpy(s_latest, ver.c_str(), sizeof(s_latest) - 1);
  s_latest[sizeof(s_latest) - 1] = '\0';
  strncpy(s_assetUrl, bin.c_str(), sizeof(s_assetUrl) - 1);
  s_assetUrl[sizeof(s_assetUrl) - 1] = '\0';

  if (versionCompare(s_latest, FIRMWARE_VERSION) <= 0) {
    setStatus(UP_TO_DATE, "Up to date (%s)", FIRMWARE_VERSION);
  } else {
    setStatus(AVAILABLE, "Version %s available", s_latest);
  }
  return true;
}

static void doCheck() {
  if (!ensureWifi()) return;
  setStatus(CHECKING, "Checking for updates...");

  // Manifest first: it survives an API-blocking proxy.
  if (doCheckManifest()) return;
  Serial.println("[OTA] No manifest, falling back to the Gitea API");

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
  Serial.printf("[OTA] GET %s\n", url);
  // Needed to tell a real API answer from a login page served as 200.
  const char* wantHeaders[] = {"Content-Type"};
  http.collectHeaders(wantHeaders, 1);
  if (!http.begin(client, url)) {
    setStatus(FAILED, "Cannot reach %s", UPDATE_HOST);
    return;
  }
  http.setTimeout(10000);
  int code = http.GET();

  if (code < 0) {
    http.end();
    setStatus(FAILED, "Cannot reach %s", UPDATE_HOST);
    return;
  }
  if (code == 403 || code == 401) {
    // Seen on a real server: Gitea with REQUIRE_SIGNIN_VIEW enabled, or a
    // proxy blocking /api. Anonymous clients cannot read anything.
    http.end();
    setStatus(FAILED, "Server needs login - make the repo public");
    return;
  }
  if (code == 404) {
    http.end();
    setStatus(FAILED, "No release published yet for this repo");
    return;
  }
  if (code != 200) {
    http.end();
    setStatus(FAILED, "Update server returned %d", code);
    return;
  }

  // A sign-in-protected Gitea answers 200 with an HTML login page, which
  // would otherwise be parsed as JSON and reported as a vague parse error.
  String ctype = http.header("Content-Type");
  if (ctype.indexOf("json") < 0) {
    http.end();
    setStatus(FAILED, "Got a web page, not release data - repo not public");
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

  s_progress = 0;

  // Erase BEFORE opening the connection. esp_ota_begin() erases the whole
  // target region up front, which takes many seconds on a 1.6 MB image; doing
  // it with an HTTPS socket already open left that socket idle long enough
  // for the far end to drop it, and the download then stalled and failed.
  setStatus(DOWNLOADING, "Preparing flash...");
  size_t space = ESP.getFreeSketchSpace();
  Serial.printf("[OTA] Free OTA space %u bytes, heap %u\n",
                (unsigned)space, (unsigned)ESP.getFreeHeap());
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    setStatus(FAILED, "No OTA partition (err %d)", (int)Update.getError());
    return;
  }

  setStatus(DOWNLOADING, "Downloading %s...", s_latest);

  WiFiClientSecure client;
  client.setInsecure();
  // Note: this core's WiFiClientSecure has no setBufferSizes(); the mbedTLS
  // buffers are fixed. Heap is logged above so an allocation failure during a
  // 1.6 MB TLS transfer alongside BLE is visible rather than mysterious.
  HTTPClient http;
  if (!http.begin(client, s_assetUrl)) {
    Update.abort();
    setStatus(FAILED, "Download failed to start");
    return;
  }
  http.setTimeout(20000);
  http.setConnectTimeout(15000);
  // Release assets are commonly served via a redirect.
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  if (code != 200) {
    http.end();
    Update.abort();
    setStatus(FAILED, "Download HTTP %d", code);
    return;
  }

  int total = http.getSize();
  if (total > 0 && (size_t)total > space) {
    http.end();
    Update.abort();
    setStatus(FAILED, "Image %d B > %u B free", total, (unsigned)space);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  static uint8_t chunk[2048];
  int written = 0;
  unsigned long lastData = millis();

  // Loop on the connection rather than only on a known length, so a chunked
  // or redirected response still completes.
  while ((total <= 0 || written < total) && (http.connected() || stream->available())) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(chunk, avail > sizeof(chunk) ? sizeof(chunk) : avail);
      if (n <= 0) break;
      if (Update.write(chunk, n) != (size_t)n) {
        Serial.printf("[OTA] flash write failed at %d bytes, err %d\n", written, (int)Update.getError());
        break;
      }
      written += n;
      if (total > 0) s_progress = (int)((int64_t)written * 100 / total);
      lastData = millis();
    } else {
      if (millis() - lastData > 20000) { Serial.println("[OTA] stream stalled"); break; }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  http.end();
  Serial.printf("[OTA] Downloaded %d of %d bytes, heap %u\n",
                written, total, (unsigned)ESP.getFreeHeap());

  if (total > 0 && written != total) {
    Update.abort();
    setStatus(FAILED, "Incomplete: %d of %d B", written, total);
    return;
  }
  if (!Update.end(true)) {
    int err = (int)Update.getError();
    Update.abort();
    setStatus(FAILED, "Verify failed (err %d)", err);
    return;
  }

  setStatus(SUCCESS, "Installed %s - restarting", s_latest);
  vTaskDelay(pdMS_TO_TICKS(1500));
  ESP.restart();
}

static void updaterTask(void*) {
  for (;;) {
    // Each job owns WiFi for its duration only. Holding the station up
    // between operations would keep BLE starved indefinitely, which is
    // the exact failure this ordering exists to prevent. A successful
    // install reboots and never reaches the release.
    if (s_reqCheck)   { s_reqCheck = false;   doCheck();   releaseWifi(); }
    if (s_reqInstall) { s_reqInstall = false; doInstall(); releaseWifi(); }
    // "Test my credentials" keeps the link up so Settings can show the IP;
    // leaving that screen releases it.
    if (s_reqWifi)    { s_reqWifi = false;    doWifiOnly(); }
    if (s_reqRelease) { s_reqRelease = false; releaseWifi(); }
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

#if WIFI_BLE_STRESS
  xTaskCreatePinnedToCore([](void*) {
    for (int round = 1; round <= 4; round++) {
      vTaskDelay(pdMS_TO_TICKS(90000));   // let BLE connect and stream
      Serial.printf("[STRESS] round %d: check while measuring (heap %u)\n",
                    round, (unsigned)ESP.getFreeHeap());
      checkForUpdate();
      for (int i = 0; i < 120 && busy(); i++) vTaskDelay(pdMS_TO_TICKS(500));
      Serial.printf("[STRESS] round %d done: %s (heap %u)\n",
                    round, statusText(), (unsigned)ESP.getFreeHeap());
    }
    Serial.println("[STRESS] complete");
    vTaskDelete(nullptr);
  }, "stress", 4096, nullptr, 1, nullptr, 1);
#endif

#if OTA_SELFTEST
  // Development aid: drive a full check-and-install at boot so the update
  // path can be exercised end to end without anyone tapping the screen.
  // Never enabled in a released build.
  if (Settings::hasWifi()) {
    Serial.println("[OTA] SELFTEST: checking and installing automatically");
    xTaskCreatePinnedToCore([](void*) {
      vTaskDelay(pdMS_TO_TICKS(4000));
      checkForUpdate();
      // Wait for the check to settle, then install if something is offered.
      for (int i = 0; i < 60 && busy(); i++) vTaskDelay(pdMS_TO_TICKS(500));
      Serial.printf("[OTA] SELFTEST: state=%d status=%s\n", (int)state(), statusText());
      if (state() == AVAILABLE) installUpdate();
      vTaskDelete(nullptr);
    }, "otatest", 4096, nullptr, 1, nullptr, 1);
  }
#endif
}

void checkForUpdate() { if (!busy()) s_reqCheck = true; }
void installUpdate()  { if (!busy()) s_reqInstall = true; }

} // namespace Updater
