#include "ble_source.h"
#include "spectrometer_data.h"
#include "settings.h"
#include "../analysis/peak_search.h"
#include <BluetoothTransport.h>

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <RadiaCode.h>
#include <string.h>

// g_data is defined once in spectrometer_data.cpp.

namespace BleSource {

static RadiaCode* device = nullptr;
static unsigned long connectedSinceMs = 0;
static unsigned long lastRealtimePollMs = 0;
static unsigned long lastSpectrumPollMs = 0;

// For deriving CPM ourselves from the spectrum's own accumulating counts
// (see pollSpectrum() below) rather than trusting RealTimeData::count_rate's
// undocumented scale.
static uint32_t lastSpectrumTotalCounts = 0;
static unsigned long lastSpectrumSampleMs = 0;
static bool haveSpectrumSample = false;

// Auto-reconnect state. userWantsConnection stays true from an explicit
// CONNECT until an explicit DISCONNECT, so a dropped link is retried but a
// deliberate disconnect is respected.
static bool userWantsConnection = false;
static unsigned long nextRetryMs = 0;
static int reconnectAttempts = 0;
static const unsigned long RETRY_BASE_MS = 4000;
static const unsigned long RETRY_MAX_MS = 30000;

static const uint32_t SCAN_SECONDS = 5;
static const unsigned long REALTIME_POLL_MS = 1000;
static const unsigned long SPECTRUM_POLL_MS = 5000;

// The RC-110's exact advertised local name isn't documented anywhere we
// could verify ahead of time, so this scans and matches by substring rather
// than a hardcoded name/MAC — resilient to firmware/model-name variations.
// Every discovered device is logged to Serial so a mismatch is diagnosable.
static bool findDeviceAddress(String& outAddress) {
  BLEDevice::init("RadiaCodeSpectrometer");
  BLEScan* scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);

  Serial.println("[BLE] Scanning for a RadiaCode device...");
  BLEScanResults results = scanner->start(SCAN_SECONDS, false);

  bool found = false;
  int count = results.getCount();
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    String name = dev.getName().c_str();
    String addr = dev.getAddress().toString().c_str();
    Serial.printf("[BLE]   found \"%s\" [%s] RSSI=%d\n", name.c_str(), addr.c_str(), dev.getRSSI());

    if (!found && name.indexOf("RadiaCode") >= 0) {
      outAddress = addr;
      found = true;
    }
  }

  scanner->clearResults();
  return found;
}

// `freshStart` decides whether the device's accumulating spectrum is cleared.
//
// The RC-110 keeps integrating internally whether or not BLE is connected, so
// a reconnect can simply resume the same measurement — the counts collected
// during the dropout are still there. Clearing on every connect (the original
// behaviour) threw those away and restarted from zero, which meant an
// unstable link silently destroyed long runs.
static bool connectInternal(bool freshStart) {
  g_data.connection = CONN_CONNECTING;

  String address;
  if (!findDeviceAddress(address)) {
    Serial.println("[BLE] No RadiaCode device found nearby.");
    g_data.connection = CONN_DISCONNECTED;
    return false;
  }

  Serial.printf("[BLE] Connecting to %s ...\n", address.c_str());
  device = new RadiaCode(address.c_str(), /* ignore_firmware_compatibility_check */ true);

  String serial = device->serialNumber();
  if (serial.length() == 0) {
    Serial.println("[BLE] Connect failed (empty serial number).");
    delete device;
    device = nullptr;
    g_data.connection = CONN_DISCONNECTED;
    return false;
  }
  Serial.printf("[BLE] Connected: %s\n", serial.c_str());

  // Derive the model from the serial number the device reports, e.g.
  // "RC-110-000983" -> "RadiaCode-110". Works for every model the library
  // supports (RC-102/103/103G/110) rather than assuming one.
  strncpy(g_data.deviceSerial, serial.c_str(), sizeof(g_data.deviceSerial) - 1);
  g_data.deviceSerial[sizeof(g_data.deviceSerial) - 1] = '\0';
  {
    const char* s = g_data.deviceSerial;
    char model[12] = "";
    if (strncasecmp(s, "RC-", 3) == 0) {
      const char* start = s + 3;
      size_t i = 0;
      while (start[i] && start[i] != '-' && i < sizeof(model) - 1) { model[i] = start[i]; i++; }
      model[i] = '\0';
    }
    if (model[0]) snprintf(g_data.deviceModel, sizeof(g_data.deviceModel), "RadiaCode-%s", model);
    else          snprintf(g_data.deviceModel, sizeof(g_data.deviceModel), "RadiaCode");
  }
  Serial.printf("[BLE] Model: %s\n", g_data.deviceModel);

  std::vector<float> calib = device->energyCalib();
  if (calib.size() >= 3) {
    g_data.calibA0 = calib[0];
    g_data.calibA1 = calib[1];
    g_data.calibA2 = calib[2];
    g_data.calibValid = true;
    Serial.printf("[BLE] Energy calib: a0=%f a1=%f a2=%f\n", calib[0], calib[1], calib[2]);
  }

  if (freshStart) {
    device->spectrumReset();
    memset(g_data.spectrum, 0, sizeof(g_data.spectrum));
    g_data.candidateCount = 0;
    g_data.liveTimeSeconds = 0;
    g_data.spectrumDurationSec = 0;
    g_data.sourceClass = SRC_INSUFFICIENT;
    g_data.uranium = {0.0f, 0, 0};
    g_data.thorium = {0.0f, 0, 0};
    g_data.quality = {0, 0.0f, 0.0f, QUAL_POOR, 0, 0, 0};
    connectedSinceMs = millis();
  } else {
    Serial.println("[BLE] Resumed - device kept accumulating, measurement continues");
  }

  g_data.batteryPercent = -1;
  lastRealtimePollMs = 0;
  lastSpectrumPollMs = 0;
  haveSpectrumSample = false;   // CPS delta needs a fresh baseline either way
  reconnectAttempts = 0;

  g_data.connection = CONN_CONNECTED;
  Settings::setWasConnected(true); // so a power cycle resumes this session
  return true;
}

// --- request queue --------------------------------------------------------
// The UI must never block on BLE. Screens set these flags and return
// immediately; the worker task on core 0 performs the actual work, which can
// take seconds (a scan alone is 5 s).
static volatile bool reqConnect = false;
static volatile bool reqDisconnect = false;
static volatile bool reqReset = false;

void requestConnect() {
  if (g_data.connection != CONN_DISCONNECTED) return;
  g_data.connection = CONN_CONNECTING; // immediate UI feedback
  reqConnect = true;
}

void requestDisconnect() { reqDisconnect = true; }
void resetSpectrum()     { reqReset = true; }

static void doConnect() {
  if (g_data.connection == CONN_CONNECTED) return;
  userWantsConnection = true;
  // Deliberately NOT a fresh start. The device has been integrating the whole
  // time — through a dropped link, a disconnect, even an ESP32 reboot — so
  // connecting resumes that run. Data is only ever discarded by an explicit
  // RESET, which means no accidental path can destroy a long acquisition.
  connectInternal(/* freshStart */ false);
}

// Connect on power-up without anyone pressing anything.
void beginAutoStart() {
  // Two independent reasons to connect at boot:
  //  - connect-at-startup: the user always wants a link.
  //  - auto-reconnect + a session that was live when power was lost. Without
  //    this second case a power cycle mid-measurement left the device idle
  //    even with auto-reconnect on, because "wants a connection" only ever
  //    lived in RAM.
  bool resume = Settings::autoReconnect() && Settings::wasConnected();
  if (!Settings::connectAtStartup() && !resume) return;

  Serial.printf("[BLE] Auto-start (%s)\n",
                Settings::connectAtStartup() ? "connect-at-startup" : "resuming session");
  userWantsConnection = true;
  nextRetryMs = 0; // performed by the worker task, so setup() is not blocked
}

static void doDisconnect() {
  userWantsConnection = false; // stop auto-reconnect: this was deliberate
  Settings::setWasConnected(false); // and do not resume it after a reboot
  if (device != nullptr) {
    delete device; // ~RadiaCode() disconnects the BLE client
    device = nullptr;
  }
  g_data.connection = CONN_DISCONNECTED;
  g_data.batteryPercent = -1;
  // Live rates are meaningless without a link, but the accumulated spectrum
  // and its analysis stay on screen: the run is paused, not thrown away, and
  // reconnecting picks it straight back up. RESET is the only way to clear it.
  g_data.doseRateUSvH = 0;
  g_data.cps = 0;
}

bool reconnectPending() {
  return userWantsConnection && Settings::autoReconnect() &&
         g_data.connection == CONN_DISCONNECTED;
}

static void doReset() {
  if (g_data.connection != CONN_CONNECTED || device == nullptr) return;
  device->spectrumReset();
  memset(g_data.spectrum, 0, sizeof(g_data.spectrum));
  g_data.spectrumDurationSec = 0;
  g_data.candidateCount = 0;
  g_data.sourceClass = SRC_INSUFFICIENT;
  g_data.uranium = {0.0f, 0, 0};
  g_data.thorium = {0.0f, 0, 0};
  g_data.quality = {0, 0.0f, 0.0f, QUAL_POOR, 0, 0, 0};
  haveSpectrumSample = false;
  lastSpectrumPollMs = 0;
  connectedSinceMs = millis();
  Serial.println("[BLE] Spectrum reset - new measurement started");
}

static void pollRealtime() {
  try {
    std::vector<DataItem*> items = device->dataBuf();
    for (DataItem* item : items) {
      if (item->type == TYPE_REAL_TIME_DATA) {
        // Note: count_rate is deliberately NOT used for CPM here — its scale
        // relative to the device's actual count rate didn't check out on our
        // hardware (read ~30x too high). CPM is instead derived from the
        // spectrum's own accumulating counts in pollSpectrum() below, which
        // is unambiguous (counts / elapsed seconds). dose_rate's *10000
        // scale did check out against the on-device reading, so that one's
        // left as-is.
        // Cross-checked on hardware (2026-08-04): count_rate*60 and the
        // independent spectrum-delta CPM in pollSpectrum() below agree
        // closely (~1650-1730 CPM at ~0.18 uSv/h background). Both are kept:
        // spectrum-delta is used for g_data.cpm since it doesn't depend on
        // this field's undocumented meaning, but they're consistent, so
        // count_rate really is CPS, not a scaling bug.
        RealTimeData* rt = static_cast<RealTimeData*>(item);
        g_data.doseRateUSvH = rt->dose_rate * 10000.0f;
      } else if (item->type == TYPE_RAW_DATA) {
        RawData* raw = static_cast<RawData*>(item);
        if (g_data.doseRateUSvH == 0.0f && raw->dose_rate > 0.0f) {
          g_data.doseRateUSvH = raw->dose_rate * 10000.0f;
        }
      } else if (item->type == TYPE_RARE_DATA) {
        RareData* rare = static_cast<RareData*>(item);
        g_data.batteryPercent = (int)rare->charge_level;
      }
    }
    for (DataItem* item : items) delete item;
    items.clear();
  } catch (...) {
    Serial.println("[BLE] Error reading real-time data");
  }
}

static void pollSpectrum() {
  try {
    Spectrum spec = device->spectrum();
    size_t n = spec.size();
    if (n == 0) return;

    // The UI reads these arrays from the other core.
    DataGuard guard;

    size_t copyCount = n < SPECTRUM_CHANNELS ? n : SPECTRUM_CHANNELS;
    uint32_t total = 0;
    for (size_t i = 0; i < copyCount; i++) {
      g_data.spectrum[i] = spec.at(i);
      total += spec.at(i);
    }

    // Device-reported acquisition time drives the counting statistics on the
    // Analysis screen — more trustworthy than our own wall clock, since the
    // spectrum accumulates on the device from spectrumReset() onward.
    g_data.spectrumDurationSec = spec.duration_sec;

    // CPS from the delta in total accumulated counts since the last poll —
    // unambiguous units, unlike RealTimeData::count_rate (see pollRealtime()).
    unsigned long now = millis();
    if (haveSpectrumSample && total >= lastSpectrumTotalCounts) {
      float elapsedSec = (now - lastSpectrumSampleMs) / 1000.0f;
      if (elapsedSec >= 1.0f) {
        uint32_t deltaCounts = total - lastSpectrumTotalCounts;
        g_data.cps = deltaCounts / elapsedSec;
      }
    }
    lastSpectrumTotalCounts = total;
    lastSpectrumSampleMs = now;
    haveSpectrumSample = true;

    if (spec.a1 != 0.0f) {
      g_data.calibA0 = spec.a0;
      g_data.calibA1 = spec.a1;
      g_data.calibA2 = spec.a2;
    }

    analyzeSpectrum(g_data);
  } catch (...) {
    Serial.println("[BLE] Error reading spectrum");
  }
}

static void tickInternal() {
  unsigned long now = millis();

  // Drop detection. The transport now reports link state directly, so a lost
  // connection is noticed within a tick instead of every poll silently
  // returning nothing.
  if (g_data.connection == CONN_CONNECTED && device != nullptr && !radiacodeBleLinkUp()) {
    Serial.println("[BLE] Link lost");
    delete device;
    device = nullptr;
    g_data.connection = CONN_DISCONNECTED;
    g_data.cps = 0;
    g_data.doseRateUSvH = 0;
    // Spectrum and analysis are deliberately left intact: the device is still
    // counting, and this run resumes as soon as the link comes back.
    reconnectAttempts = 0;
    nextRetryMs = now + RETRY_BASE_MS;
  }

  // Retry while the user still wants a connection and hasn't disconnected.
  if (g_data.connection == CONN_DISCONNECTED && userWantsConnection &&
      Settings::autoReconnect()) {
    if (now >= nextRetryMs) {
      reconnectAttempts++;
      Serial.printf("[BLE] Reconnect attempt %d...\n", reconnectAttempts);
      if (!connectInternal(/* freshStart */ false)) {
        // Back off so a device that is off or out of range does not pin the
        // CPU scanning, but keep trying indefinitely.
        unsigned long delayMs = RETRY_BASE_MS * (unsigned long)reconnectAttempts;
        if (delayMs > RETRY_MAX_MS) delayMs = RETRY_MAX_MS;
        nextRetryMs = millis() + delayMs;
      }
    }
    return;
  }

  if (g_data.connection != CONN_CONNECTED || device == nullptr) return;

  g_data.liveTimeSeconds = (now - connectedSinceMs) / 1000;

  if (now - lastRealtimePollMs >= REALTIME_POLL_MS) {
    lastRealtimePollMs = now;
    pollRealtime();
  }

  if (now - lastSpectrumPollMs >= SPECTRUM_POLL_MS) {
    lastSpectrumPollMs = now;
    pollSpectrum();
  }
}

// --- worker task ----------------------------------------------------------
// Everything above runs here, on core 0. The Arduino loop() (core 1) drives
// only the display and touch, so a 5-second BLE scan or a stalled poll no
// longer stalls the UI — which is exactly what made the device feel "lazy"
// once auto-reconnect started scanning in the main loop.
static void bleTask(void*) {
  for (;;) {
    if (reqDisconnect) { reqDisconnect = false; doDisconnect(); }
    if (reqReset)      { reqReset = false;      doReset(); }
    if (reqConnect)    { reqConnect = false;    doConnect(); }
    tickInternal();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void begin() {
  // 12 KB: the Bluedroid call chain is deep and analyzeSpectrum() runs here
  // too (its big working buffers are file-scope statics, not stack).
  xTaskCreatePinnedToCore(bleTask, "ble", 12288, nullptr, 1, nullptr, 0);
}

// Retained so main.cpp's loop stays unchanged in shape; the real work is on
// the task now.
void tick() {}

} // namespace BleSource
