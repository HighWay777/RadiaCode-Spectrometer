#include "isotope_library.h"
#include "isotope_data.h"
#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <string.h>

namespace IsotopeLibrary {

// Bounds on an SD-loaded table. Generous versus the 21 built-ins, while
// keeping worst-case allocation (~50 KB) safely inside the free heap even if
// PSRAM turns out not to be usable on this board.
static const uint16_t MAX_ISOTOPES = ISOTOPE_LIBRARY_MAX;
static const uint16_t MAX_LINES    = 2400;
static const size_t   STRING_POOL  = 24000;

static const IsotopeDef* s_defs = BUILTIN_ISOTOPES;
static uint16_t s_count = BUILTIN_ISOTOPE_COUNT;
static bool s_fromSd = false;
static char s_source[32] = "built-in";

// Owned buffers, only allocated when an SD table is loaded.
static IsotopeDef*  s_ownedDefs = nullptr;
static IsotopeLine* s_ownedLines = nullptr;
static char*        s_ownedStrings = nullptr;

static void* bigAlloc(size_t bytes) {
  void* p = ps_malloc(bytes);          // PSRAM first if it is available
  if (p == nullptr) p = malloc(bytes); // otherwise internal heap
  return p;
}

static void releaseOwned() {
  if (s_ownedDefs) { free(s_ownedDefs); s_ownedDefs = nullptr; }
  if (s_ownedLines) { free(s_ownedLines); s_ownedLines = nullptr; }
  if (s_ownedStrings) { free(s_ownedStrings); s_ownedStrings = nullptr; }
}

void begin() {
  releaseOwned();
  s_defs = BUILTIN_ISOTOPES;
  s_count = BUILTIN_ISOTOPE_COUNT;
  s_fromSd = false;
  snprintf(s_source, sizeof(s_source), "built-in");
}

uint16_t count() { return s_count; }
bool usingSd() { return s_fromSd; }
const char* sourceName() { return s_source; }

const IsotopeDef& at(uint16_t index) {
  if (index >= s_count) index = 0;
  return s_defs[index];
}

uint16_t lineCount() {
  uint16_t n = 0;
  for (uint16_t i = 0; i < s_count; i++) n += s_defs[i].lineCount;
  return n;
}

static IsotopeCategory parseCategory(const char* s) {
  if (strcasecmp(s, "artificial") == 0)  return CAT_ARTIFICIAL;
  if (strcasecmp(s, "medical") == 0)     return CAT_MEDICAL;
  if (strcasecmp(s, "calibration") == 0) return CAT_CALIBRATION;
  return CAT_NATURAL;
}

static DecayChain parseChain(const char* s) {
  if (strcasecmp(s, "U238") == 0)  return CHAIN_U238;
  if (strcasecmp(s, "Th232") == 0) return CHAIN_TH232;
  return CHAIN_NONE;
}

static const char* categoryName(IsotopeCategory c) {
  switch (c) {
    case CAT_ARTIFICIAL:  return "artificial";
    case CAT_MEDICAL:     return "medical";
    case CAT_CALIBRATION: return "calibration";
    default:              return "natural";
  }
}

static const char* chainName(DecayChain c) {
  switch (c) {
    case CHAIN_U238:  return "U238";
    case CHAIN_TH232: return "Th232";
    default:          return "none";
  }
}

// Splits a CSV line in place. Returns the field count.
static int splitCsv(char* line, char** fields, int maxFields) {
  int n = 0;
  char* p = line;
  fields[n++] = p;
  while (*p && n < maxFields) {
    if (*p == ',') { *p = '\0'; fields[n++] = p + 1; }
    p++;
  }
  // Trim trailing CR/whitespace from the final field.
  for (int i = 0; i < n; i++) {
    char* f = fields[i];
    size_t len = strlen(f);
    while (len > 0 && (f[len - 1] == '\r' || f[len - 1] == '\n' || f[len - 1] == ' ')) {
      f[--len] = '\0';
    }
  }
  return n;
}

bool loadFromSd(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  IsotopeDef*  defs    = (IsotopeDef*)bigAlloc(sizeof(IsotopeDef) * MAX_ISOTOPES);
  IsotopeLine* lines   = (IsotopeLine*)bigAlloc(sizeof(IsotopeLine) * MAX_LINES);
  char*        strings = (char*)bigAlloc(STRING_POOL);
  if (!defs || !lines || !strings) {
    if (defs) free(defs);
    if (lines) free(lines);
    if (strings) free(strings);
    f.close();
    Serial.println("[LIB] Out of memory loading isotope library");
    return false;
  }

  uint16_t nDefs = 0, nLines = 0;
  size_t strUsed = 0;
  char lastSymbol[24] = "";
  char line[160];
  int lineNo = 0, skipped = 0;

  while (f.available() && nDefs < MAX_ISOTOPES && nLines < MAX_LINES) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    lineNo++;
    if (n == 0 || line[0] == '#') continue;

    char* fields[8];
    int nf = splitCsv(line, fields, 8);
    if (nf < 6) { skipped++; continue; }
    // Skip a header row.
    if (strcasecmp(fields[0], "symbol") == 0) continue;

    float energy = atof(fields[4]);
    float intensity = atof(fields[5]);
    if (energy <= 0.0f || intensity <= 0.0f) { skipped++; continue; }

    // New nuclide when the symbol changes.
    if (strcmp(fields[0], lastSymbol) != 0) {
      size_t symLen = strlen(fields[0]) + 1;
      size_t nameLen = strlen(fields[1]) + 1;
      if (strUsed + symLen + nameLen > STRING_POOL) break;

      char* sym = strings + strUsed;  memcpy(sym, fields[0], symLen);  strUsed += symLen;
      char* nam = strings + strUsed;  memcpy(nam, fields[1], nameLen); strUsed += nameLen;

      defs[nDefs].symbol = sym;
      defs[nDefs].name = nam;
      defs[nDefs].lines = &lines[nLines];
      defs[nDefs].lineCount = 0;
      defs[nDefs].category = parseCategory(fields[2]);
      defs[nDefs].chain = parseChain(fields[3]);
      nDefs++;
      strncpy(lastSymbol, fields[0], sizeof(lastSymbol) - 1);
      lastSymbol[sizeof(lastSymbol) - 1] = '\0';
    }

    if (defs[nDefs - 1].lineCount >= 255) continue; // lineCount is uint8_t
    lines[nLines].energyKeV = energy;
    lines[nLines].intensity = intensity;
    nLines++;
    defs[nDefs - 1].lineCount++;
  }
  f.close();

  if (nDefs == 0) {
    free(defs); free(lines); free(strings);
    Serial.println("[LIB] No usable rows in isotope library file");
    return false;
  }

  releaseOwned();
  s_ownedDefs = defs;
  s_ownedLines = lines;
  s_ownedStrings = strings;
  s_defs = defs;
  s_count = nDefs;
  s_fromSd = true;
  snprintf(s_source, sizeof(s_source), "SD (%u nuclides)", (unsigned)nDefs);

  Serial.printf("[LIB] Loaded %u nuclides / %u lines from %s (%d rows skipped)\n",
                (unsigned)nDefs, (unsigned)nLines, path, skipped);
  return true;
}

bool exportTemplate(const char* path) {
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.println("# RadiaCode-110 nuclide library");
  f.println("# Rows for one nuclide must be grouped together.");
  f.println("# category: natural|artificial|medical|calibration   chain: U238|Th232|none");
  f.println("# intensity is ABSOLUTE emission probability per decay (0..1)");
  f.println("symbol,name,category,chain,energy_keV,intensity");
  for (uint16_t i = 0; i < BUILTIN_ISOTOPE_COUNT; i++) {
    const IsotopeDef& d = BUILTIN_ISOTOPES[i];
    for (uint8_t l = 0; l < d.lineCount; l++) {
      f.printf("%s,%s,%s,%s,%.2f,%.5f\n", d.symbol, d.name,
               categoryName(d.category), chainName(d.chain),
               d.lines[l].energyKeV, d.lines[l].intensity);
    }
  }
  f.close();
  Serial.printf("[LIB] Wrote template to %s\n", path);
  return true;
}

} // namespace IsotopeLibrary
