#pragma once

#include <stdint.h>
#include "isotope_db.h"

// The active nuclide table. Defaults to the ~21 built-in nuclides compiled
// into flash; if /isotopes.csv is found on the SD card at boot it is parsed
// and replaces them, allowing a far larger library than is comfortable to
// keep in firmware.
//
// CSV format — one row per gamma line, rows for the same nuclide grouped
// together (order within a nuclide does not matter):
//
//   symbol,name,category,chain,energy_keV,intensity
//   Bi-214,Bismuth-214,natural,U238,609.31,0.4549
//   Bi-214,Bismuth-214,natural,U238,1764.49,0.1531
//   Cs-137,Caesium-137,artificial,none,661.66,0.851
//
//   category: natural | artificial | medical | calibration
//   chain:    U238 | Th232 | none
//   intensity: absolute emission probability per decay (0..1), NOT relative
//
// exportTemplate() writes the built-in table in exactly this format, so the
// card can be seeded and then extended by hand.
// Hard ceiling on table size. Scoring code sizes its per-nuclide working
// arrays by this, so it must be a compile-time constant.
#define ISOTOPE_LIBRARY_MAX 300

namespace IsotopeLibrary {
  void begin();                            // install the built-in table
  bool loadFromSd(const char* path);       // replace it from CSV
  bool exportTemplate(const char* path);   // write built-in table as CSV

  uint16_t count();
  const IsotopeDef& at(uint16_t index);
  bool usingSd();
  const char* sourceName();
  uint16_t lineCount();                    // total gamma lines in the table
}
