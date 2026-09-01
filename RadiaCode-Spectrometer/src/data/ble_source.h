#pragma once

// Real RC-110 data source over BLE, using the vendored mkgeiger/RadiaCode
// library (Libraries/RadiaCode/). Same interface as MockSource
// (data/mock_source.*) so main.cpp can swap between them; screens never
// touch this module directly, they only read g_data.
namespace BleSource {
  // Starts the worker task on core 0. All BLE I/O and spectrum analysis
  // happen there so the UI thread never blocks on a scan or a stalled read.
  // Call once from setup(); every request below is non-blocking.
  void begin();

  // Scans for a nearby device advertising a name containing "RadiaCode" and
  // connects (blocking — see CLAUDE.md "BLE integration" for why). No-op if
  // already connecting/connected.
  void requestConnect();

  void requestDisconnect();

  // Switches the detector itself off, then drops the link. The protocol
  // exposes a PWR bit in DEVICE_CTRL; this is reverse-engineered and not
  // documented by the manufacturer, so the UI asks for confirmation before
  // calling it. The accumulated spectrum is NOT cleared - it lives on the
  // detector and is still there when it is switched back on.
  void requestPowerOff();

  // Clears the device's accumulating spectrum and restarts the acquisition
  // clock, so a new sample can be measured without disconnecting. No-op
  // while disconnected.
  void resetSpectrum();

  // Arms connect-at-startup (per Settings). tick() performs the actual
  // connection so setup() is not blocked by a BLE scan.
  void beginAutoStart();

  // Bluedroid and the WiFi stack together do not fit in this board's heap:
  // bringing WiFi up while BLE was streaming produced "BLE_INIT: Malloc
  // failed" and every spectrum transfer then truncated mid-read. So WiFi work
  // suspends the link first and resumes it afterwards. This costs nothing -
  // the detector accumulates internally the whole time, exactly as it does
  // across a dropout or a reboot.
  //
  // suspend() is safe to nest-call and remembers whether a link was up, so
  // resume() only reconnects if it actually interrupted one.
  void suspendForWifi();
  void resumeAfterWifi();
  bool isSuspended();

  // True while an auto-reconnect is pending, so the UI can say "reconnecting"
  // rather than plain "offline".
  bool reconnectPending();

  // Call every loop() iteration. No-op while disconnected. While connected,
  // polls real-time dose rate roughly every 1s and the full spectrum roughly
  // every 5s (both are blocking BLE round trips — see CLAUDE.md). CPS is
  // derived from the spectrum's own accumulating counts.
  void tick();
}
