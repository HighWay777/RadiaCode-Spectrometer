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

  // Clears the device's accumulating spectrum and restarts the acquisition
  // clock, so a new sample can be measured without disconnecting. No-op
  // while disconnected.
  void resetSpectrum();

  // Arms connect-at-startup (per Settings). tick() performs the actual
  // connection so setup() is not blocked by a BLE scan.
  void beginAutoStart();

  // True while an auto-reconnect is pending, so the UI can say "reconnecting"
  // rather than plain "offline".
  bool reconnectPending();

  // Call every loop() iteration. No-op while disconnected. While connected,
  // polls real-time dose rate roughly every 1s and the full spectrum roughly
  // every 5s (both are blocking BLE round trips — see CLAUDE.md). CPS is
  // derived from the spectrum's own accumulating counts.
  void tick();
}
