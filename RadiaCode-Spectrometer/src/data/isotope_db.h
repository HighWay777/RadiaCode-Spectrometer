#pragma once

#include <stdint.h>

// Types only. The built-in nuclide table lives in isotope_data.h (included
// solely by isotope_library.cpp) and is reached through IsotopeLibrary, which
// can swap in a much larger table loaded from SD at boot.
//
// Energies in keV, intensities as ABSOLUTE emission probability per decay of
// THAT nuclide (not per parent-chain decay).
//
// IMPORTANT (learned on hardware): decay-chain members are the whole point of
// this table for natural samples. U-238 and Th-232 are effectively
// gamma-silent — you never see the parent. Uranium ore is identified by its
// daughters (Pb-214, Bi-214, Ra-226, Th-234) in secular equilibrium with it.

struct IsotopeLine {
  float energyKeV;
  float intensity; // absolute emission probability per decay, 0..1
};

enum IsotopeCategory {
  CAT_NATURAL,      // primordial / natural decay chains
  CAT_ARTIFICIAL,   // fission or activation products
  CAT_MEDICAL,      // short-lived medical tracers
  CAT_CALIBRATION   // typical lab check-source nuclides
};

enum DecayChain {
  CHAIN_NONE = 0,
  CHAIN_U238,   // U-238 -> ... -> Pb-206 (includes the Ra-226 sub-chain)
  CHAIN_TH232   // Th-232 -> ... -> Pb-208
};

struct IsotopeDef {
  const char* symbol;
  const char* name;
  const IsotopeLine* lines;
  uint8_t lineCount;
  IsotopeCategory category;
  DecayChain chain;
};
