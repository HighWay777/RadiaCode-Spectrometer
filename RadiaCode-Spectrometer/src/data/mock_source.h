#pragma once

// Simulated live data source, standing in for the real RC-110 BLE link
// (mkgeiger/RadiaCode) until that integration is wired up. Drives g_data
// (spectrometer_data.h) so every screen can be built/demoed without hardware.
// Swap this module out for a real BLE data source later; keep g_data and the
// screens unchanged.
namespace MockSource {
  void requestConnect();
  void requestDisconnect();
  void tick(); // call every loop() iteration; internally rate-limited by millis()
}
