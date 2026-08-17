#pragma once

#include <stdint.h>

// Common gamma-ray lines for a small set of isotopes a handheld scintillator
// is realistically able to flag (see CLAUDE.md "Why 8.4% FWHM matters").
// Energies/relative intensities are widely published nuclear-data values
// (NNDC-style), not copied from RadiaCode's own in-app library. This table
// is a starting point — expand/correct against a real reference before
// trusting results for anything but demo purposes.

struct IsotopeLine {
  float energyKeV;
  float intensity; // relative emission probability, 0..1, strongest line = 1.0
};

struct IsotopeDef {
  const char* symbol;
  const char* name;
  const IsotopeLine* lines;
  uint8_t lineCount;
};

static const IsotopeLine CS137_LINES[]  = {{661.7f, 1.00f}};
static const IsotopeLine CO60_LINES[]   = {{1173.2f, 1.00f}, {1332.5f, 1.00f}};
static const IsotopeLine AM241_LINES[]  = {{59.5f, 1.00f}};
static const IsotopeLine K40_LINES[]    = {{1460.8f, 1.00f}};
static const IsotopeLine NA22_LINES[]   = {{511.0f, 1.00f}, {1274.5f, 0.80f}};
static const IsotopeLine I131_LINES[]   = {{364.5f, 1.00f}, {637.0f, 0.20f}};
static const IsotopeLine BA133_LINES[]  = {{356.0f, 1.00f}, {81.0f, 0.50f}};
static const IsotopeLine EU152_LINES[]  = {{344.3f, 1.00f}, {121.8f, 0.70f}, {1408.0f, 0.60f}};
static const IsotopeLine RA226_LINES[]  = {{609.3f, 1.00f}, {186.2f, 0.50f}, {1120.3f, 0.40f}, {1764.5f, 0.40f}};
static const IsotopeLine TH232_LINES[]  = {{238.6f, 1.00f}, {583.2f, 0.70f}, {2614.5f, 0.90f}};
static const IsotopeLine U238_LINES[]   = {{1001.0f, 0.30f}, {766.4f, 0.20f}, {63.3f, 0.30f}};
static const IsotopeLine CO57_LINES[]   = {{122.1f, 1.00f}};
static const IsotopeLine MN54_LINES[]   = {{834.8f, 1.00f}};

static const IsotopeDef ISOTOPE_DB[] = {
  {"Cs-137", "Caesium-137",         CS137_LINES,  1},
  {"Co-60",  "Cobalt-60",           CO60_LINES,   2},
  {"Am-241", "Americium-241",       AM241_LINES,  1},
  {"K-40",   "Potassium-40 (bkg)",  K40_LINES,    1},
  {"Na-22",  "Sodium-22",           NA22_LINES,   2},
  {"I-131",  "Iodine-131",          I131_LINES,   2},
  {"Ba-133", "Barium-133",          BA133_LINES,  2},
  {"Eu-152", "Europium-152",        EU152_LINES,  3},
  {"Ra-226", "Radium-226 chain",    RA226_LINES,  4},
  {"Th-232", "Thorium-232 chain",   TH232_LINES,  3},
  {"U-238",  "Uranium-238 chain",   U238_LINES,   3},
  {"Co-57",  "Cobalt-57",           CO57_LINES,   1},
  {"Mn-54",  "Manganese-54",        MN54_LINES,   1},
};

static const uint8_t ISOTOPE_DB_COUNT = sizeof(ISOTOPE_DB) / sizeof(ISOTOPE_DB[0]);
