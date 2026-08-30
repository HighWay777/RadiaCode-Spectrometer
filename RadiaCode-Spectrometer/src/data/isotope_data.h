#pragma once

#include "isotope_db.h"

// Built-in nuclide table — the fallback when no library is present on SD.
// Include only from isotope_library.cpp; everything else goes through
// IsotopeLibrary so the SD-loaded table is picked up transparently.
//
// Lines are limited to roughly the strongest 3-7 per nuclide; anything much
// below ~3% intensity is not realistically resolvable at 8.4% FWHM and only
// adds spurious match opportunities.

// ---- U-238 chain members (what "uranium ore" actually looks like) ----
static const IsotopeLine TH234_LINES[]  = {{92.6f, 0.0433f}, {63.29f, 0.0375f}};
static const IsotopeLine PA234M_LINES[] = {{1001.03f, 0.00842f}, {766.36f, 0.00294f}};
static const IsotopeLine RA226_LINES[]  = {{186.21f, 0.0364f}};
static const IsotopeLine PB214_LINES[]  = {{351.93f, 0.3560f}, {295.22f, 0.1841f}, {242.00f, 0.0725f}};
static const IsotopeLine BI214_LINES[]  = {{609.31f, 0.4549f}, {1764.49f, 0.1531f}, {1120.29f, 0.1491f},
                                           {1238.11f, 0.0583f}, {2204.21f, 0.0491f}, {768.36f, 0.0489f},
                                           {934.06f, 0.0311f}};
static const IsotopeLine U235_LINES[]   = {{185.72f, 0.570f}, {143.76f, 0.1096f},
                                           {163.33f, 0.0508f}, {205.31f, 0.0501f}};

// ---- Th-232 chain members ----
static const IsotopeLine AC228_LINES[]  = {{911.20f, 0.258f}, {968.97f, 0.158f}, {338.32f, 0.1127f},
                                           {463.00f, 0.0440f}, {794.95f, 0.0425f}, {209.25f, 0.0389f}};
static const IsotopeLine PB212_LINES[]  = {{238.63f, 0.436f}, {300.09f, 0.0318f}};
static const IsotopeLine BI212_LINES[]  = {{727.33f, 0.0667f}, {1620.50f, 0.0151f}};
static const IsotopeLine TL208_LINES[]  = {{2614.51f, 0.9976f}, {583.19f, 0.850f}, {510.77f, 0.226f},
                                           {860.56f, 0.125f}, {277.37f, 0.066f}};

// ---- Other natural ----
static const IsotopeLine K40_LINES[]    = {{1460.82f, 0.1055f}};

// ---- Artificial / calibration / medical ----
static const IsotopeLine CS137_LINES[]  = {{661.66f, 0.851f}};
static const IsotopeLine CO60_LINES[]   = {{1173.23f, 0.9985f}, {1332.49f, 0.9998f}};
static const IsotopeLine MN54_LINES[]   = {{834.85f, 0.9998f}};
static const IsotopeLine AM241_LINES[]  = {{59.54f, 0.359f}};
static const IsotopeLine NA22_LINES[]   = {{511.00f, 1.798f}, {1274.54f, 0.9994f}};
static const IsotopeLine CO57_LINES[]   = {{122.06f, 0.856f}, {136.47f, 0.1068f}};
static const IsotopeLine BA133_LINES[]  = {{356.01f, 0.6205f}, {81.00f, 0.329f}, {302.85f, 0.1834f},
                                           {383.85f, 0.0894f}, {276.40f, 0.0716f}};
static const IsotopeLine EU152_LINES[]  = {{121.78f, 0.2853f}, {344.28f, 0.2659f}, {1408.01f, 0.2087f},
                                           {964.06f, 0.1451f}, {1112.08f, 0.1367f}, {778.90f, 0.1293f}};
static const IsotopeLine I131_LINES[]   = {{364.49f, 0.815f}, {636.99f, 0.0716f}, {284.31f, 0.0614f},
                                           {80.19f, 0.0262f}};
static const IsotopeLine TC99M_LINES[]  = {{140.51f, 0.8906f}};

static const IsotopeDef BUILTIN_ISOTOPES[] = {
  // U-238 chain
  {"Bi-214",  "Bismuth-214",       BI214_LINES,  7, CAT_NATURAL, CHAIN_U238},
  {"Pb-214",  "Lead-214",          PB214_LINES,  3, CAT_NATURAL, CHAIN_U238},
  {"Ra-226",  "Radium-226",        RA226_LINES,  1, CAT_NATURAL, CHAIN_U238},
  {"Th-234",  "Thorium-234",       TH234_LINES,  2, CAT_NATURAL, CHAIN_U238},
  {"Pa-234m", "Protactinium-234m", PA234M_LINES, 2, CAT_NATURAL, CHAIN_U238},
  {"U-235",   "Uranium-235",       U235_LINES,   4, CAT_NATURAL, CHAIN_U238},
  // Th-232 chain
  {"Tl-208",  "Thallium-208",      TL208_LINES,  5, CAT_NATURAL, CHAIN_TH232},
  {"Ac-228",  "Actinium-228",      AC228_LINES,  6, CAT_NATURAL, CHAIN_TH232},
  {"Pb-212",  "Lead-212",          PB212_LINES,  2, CAT_NATURAL, CHAIN_TH232},
  {"Bi-212",  "Bismuth-212",       BI212_LINES,  2, CAT_NATURAL, CHAIN_TH232},
  // Other natural
  {"K-40",    "Potassium-40",      K40_LINES,    1, CAT_NATURAL, CHAIN_NONE},
  // Artificial
  {"Cs-137",  "Caesium-137",       CS137_LINES,  1, CAT_ARTIFICIAL, CHAIN_NONE},
  {"Co-60",   "Cobalt-60",         CO60_LINES,   2, CAT_ARTIFICIAL, CHAIN_NONE},
  {"Mn-54",   "Manganese-54",      MN54_LINES,   1, CAT_ARTIFICIAL, CHAIN_NONE},
  // Calibration sources
  {"Am-241",  "Americium-241",     AM241_LINES,  1, CAT_CALIBRATION, CHAIN_NONE},
  {"Na-22",   "Sodium-22",         NA22_LINES,   2, CAT_CALIBRATION, CHAIN_NONE},
  {"Co-57",   "Cobalt-57",         CO57_LINES,   2, CAT_CALIBRATION, CHAIN_NONE},
  {"Ba-133",  "Barium-133",        BA133_LINES,  5, CAT_CALIBRATION, CHAIN_NONE},
  {"Eu-152",  "Europium-152",      EU152_LINES,  6, CAT_CALIBRATION, CHAIN_NONE},
  // Medical (short half-life — implausible in geological samples)
  {"I-131",   "Iodine-131",        I131_LINES,   4, CAT_MEDICAL, CHAIN_NONE},
  {"Tc-99m",  "Technetium-99m",    TC99M_LINES,  1, CAT_MEDICAL, CHAIN_NONE},
};

static const uint16_t BUILTIN_ISOTOPE_COUNT =
    sizeof(BUILTIN_ISOTOPES) / sizeof(BUILTIN_ISOTOPES[0]);
