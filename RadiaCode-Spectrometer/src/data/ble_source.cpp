#include "ble_source.h"
#include "spectrometer_data.h"
#include "../analysis/peak_search.h"

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

void requestConnect() {
  if (g_data.connection != CONN_DISCONNECTED) return;
  g_data.connection = CONN_CONNECTING;

  String address;
  if (!findDeviceAddress(address)) {
    Serial.println("[BLE] No RadiaCode device found nearby.");
    g_data.connection = CONN_DISCONNECTED;
    return;
  }

  Serial.printf("[BLE] Connecting to %s ...\n", address.c_str());
  device = new RadiaCode(address.c_str(), /* ignore_firmware_compatibility_check */ true);

  String serial = device->serialNumber();
  if (serial.length() == 0) {
    Serial.println("[BLE] Connect failed (empty serial number).");
    delete device;
    device = nullptr;
    g_data.connection = CONN_DISCONNECTED;
    return;
  }
  Serial.printf("[BLE] Connected: %s\n", serial.c_str());

  std::vector<float> calib = device->energyCalib();
  if (calib.size() >= 3) {
    g_data.calibA0 = calib[0];
    g_data.calibA1 = calib[1];
    g_data.calibA2 = calib[2];
    Serial.printf("[BLE] Energy calib: a0=%f a1=%f a2=%f\n", calib[0], calib[1], calib[2]);
  }

  device->spectrumReset();

  memset(g_data.spectrum, 0, sizeof(g_data.spectrum));
  g_data.candidateCount = 0;
  g_data.batteryPercent = -1;
  g_data.liveTimeSeconds = 0;
  connectedSinceMs = millis();
  lastRealtimePollMs = 0;
  lastSpectrumPollMs = 0;
  haveSpectrumSample = false;

  g_data.connection = CONN_CONNECTED;
}

void requestDisconnect() {
  if (device != nullptr) {
    delete device; // ~RadiaCode() disconnects the BLE client
    device = nullptr;
  }
  g_data.connection = CONN_DISCONNECTED;
  g_data.batteryPercent = -1;
  g_data.liveTimeSeconds = 0;
  g_data.doseRateUSvH = 0;
  g_data.cpm = 0;
  memset(g_data.spectrum, 0, sizeof(g_data.spectrum));
  g_data.candidateCount = 0;
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

    size_t copyCount = n < SPECTRUM_CHANNELS ? n : SPECTRUM_CHANNELS;
    uint32_t total = 0;
    for (size_t i = 0; i < copyCount; i++) {
      g_data.spectrum[i] = spec.at(i);
      total += spec.at(i);
    }

    // CPM from the delta in total accumulated counts since the last poll —
    // unambiguous units, unlike RealTimeData::count_rate (see pollRealtime()).
    unsigned long now = millis();
    if (haveSpectrumSample && total >= lastSpectrumTotalCounts) {
      float elapsedSec = (now - lastSpectrumSampleMs) / 1000.0f;
      if (elapsedSec >= 1.0f) {
        uint32_t deltaCounts = total - lastSpectrumTotalCounts;
        g_data.cpm = (deltaCounts / elapsedSec) * 60.0f;
        Serial.printf("[BLE] spectrum delta: total=%u delta=%u elapsed=%.2fs -> cpm=%.1f\n",
                      (unsigned)total, (unsigned)deltaCounts, elapsedSec, g_data.cpm);
      }
    }
    if (spec.duration_sec > 0) {
      float avgCpm = ((float)total / (float)spec.duration_sec) * 60.0f;
      Serial.printf("[BLE] spectrum cumulative: total=%u duration_sec=%u -> avg_cpm=%.1f\n",
                    (unsigned)total, (unsigned)spec.duration_sec, avgCpm);
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

void tick() {
  if (g_data.connection != CONN_CONNECTED || device == nullptr) return;

  unsigned long now = millis();
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

} // namespace BleSource
