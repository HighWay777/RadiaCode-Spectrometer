#pragma once

#include <stdint.h>
#include <stddef.h>
#include "spectrometer_data.h"

// microSD storage on the JC4827W543's TF slot.
//
// Pinout note: this board's *resistive* variant puts an XPT2046 touch
// controller on SPI pins 11/12/13. Ours is the capacitive (GT911, I2C)
// variant, so that SPI bus is free and the card has it to itself — which also
// avoids the documented "SD access breaks after a display flush" quirk that
// affects boards sharing a bus with the panel. The display is on its own QSPI
// bus (45/47/21/48/40/39) either way.
//
// The CS pin is not documented anywhere we could find, so begin() probes a
// list of candidates and reports which one answered. Everything is optional:
// if no card is present the rest of the firmware behaves exactly as before.
namespace SdStorage {
  static const int8_t PIN_SCK  = 12;
  static const int8_t PIN_MISO = 13;
  static const int8_t PIN_MOSI = 11;

  bool begin();
  bool available();
  int  csPin();               // -1 if not mounted
  const char* status();       // human-readable state for the UI
  uint32_t totalMB();
  uint32_t freeMB();

  // True when a card answered on SPI but its filesystem could not be mounted
  // — overwhelmingly this means exFAT, which Windows picks by default for
  // cards over 32 GB and which the ESP32 FATFS build cannot read.
  bool cardPresentButUnreadable();

  // Reformats the card to FAT32. DESTRUCTIVE — erases everything on it.
  // Only meaningful when the card cannot currently be mounted; a healthy
  // FAT32 card is left alone (use eraseAllData() to clear one of those).
  bool formatCard();

  // Deletes everything this firmware wrote (/spectra, /background) without
  // touching the filesystem itself.
  bool eraseAllData();

  // Writes the live spectrum as CSV under /spectra/, auto-numbered.
  // outPath receives the filename written. Returns false if unavailable.
  bool exportSpectrum(const SpectrometerData& d, char* outPath, size_t outLen);

  // Background reference library — named slots holding counts-per-second per
  // channel, so a reference can be kept per location (home, car, basement...)
  // and reloaded instead of re-measured.
  bool saveBackgroundRef(const char* name);
  bool loadBackgroundRef(const char* name);
  bool refExists(const char* name);
  bool refInfo(const char* name, uint32_t* durationSec, uint32_t* totalCounts);

  int  countSpectra();

  // Dumps /import and /background to Serial. Answers "where did my file go?"
  // without needing to pull the card.
  void logInventory();

  // Periodic checkpoint of the live spectrum to /spectra/_autosave.csv
  // (overwritten each time). The RC-110 already preserves an accumulating run
  // across a link drop or an ESP32 reboot, so this exists for the case that
  // does lose data: the RC-110 itself being switched off or reset.
  bool autosaveSpectrum(const SpectrometerData& d);

  // Re-validates the card periodically: notices removal after boot (writes
  // would otherwise fail silently against a cached "mounted" flag) and
  // remounts automatically when a card is inserted. Cheap; call from loop().
  // Nothing here is required for measuring — the spectrum comes over BLE and
  // the active background reference lives in internal flash, so the device
  // keeps accumulating and identifying with no card at all.
  void pollPresence();
}
