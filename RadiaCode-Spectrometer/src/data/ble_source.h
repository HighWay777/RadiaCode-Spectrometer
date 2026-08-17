#pragma once

// Real RC-110 data source over BLE, using the vendored mkgeiger/RadiaCode
// library (Libraries/RadiaCode/). Same interface as MockSource
// (data/mock_source.*) so main.cpp can swap between them; screens never
// touch this module directly, they only read g_data.
namespace BleSource {
  // Scans for a nearby device advertising a name containing "RadiaCode" and
  // connects (blocking — see CLAUDE.md "BLE integration" for why). No-op if
  // already connecting/connected.
  void requestConnect();

  void requestDisconnect();

  // Call every loop() iteration. No-op while disconnected. While connected,
  // polls real-time dose/count rate roughly every 1s and the full spectrum
  // roughly every 5s (both are blocking BLE round trips — see CLAUDE.md).
  void tick();
}
