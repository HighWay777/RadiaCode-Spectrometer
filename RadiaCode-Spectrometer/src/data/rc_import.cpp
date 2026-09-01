#include "rc_import.h"
#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <string.h>

namespace RcImport {

static bool endsWithXml(const char* name) {
  size_t n = strlen(name);
  if (n < 4) return false;
  return strcasecmp(name + n - 4, ".xml") == 0;
}

static void addFrom(const char* dirPath, FileEntry* out, int maxFiles, int* count) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  File f = dir.openNextFile();
  while (f && *count < maxFiles) {
    if (!f.isDirectory()) {
      const char* nm = f.name();
      // Some cores hand back a full path here and a bare name elsewhere.
      const char* base = strrchr(nm, '/');
      base = base ? base + 1 : nm;
      if (endsWithXml(base)) {
        snprintf(out[*count].path, NAME_LEN, "%s/%s",
                 (strcmp(dirPath, "/") == 0) ? "" : dirPath, base);
        strncpy(out[*count].label, base, NAME_LEN - 1);
        out[*count].label[NAME_LEN - 1] = '\0';
        out[*count].size = f.size();
        (*count)++;
      }
    }
    f.close();
    f = dir.openNextFile();
  }
  if (f) f.close();
  dir.close();
}

int scan(FileEntry* out, int maxFiles) {
  if (!SD.exists("/import")) SD.mkdir("/import");
  int count = 0;
  addFrom("/import", out, maxFiles, &count);
  addFrom("/", out, maxFiles, &count);
  return count;
}

// --- streaming tag scanner ------------------------------------------------
// Reads through a buffer rather than byte-at-a-time File::read(), which is
// painfully slow on SD. Only the three tags we care about are captured; the
// rest of the document is skipped without being understood.

enum Capture { CAP_NONE, CAP_TIME, CAP_COEF, CAP_POINT };

struct Reader {
  File f;
  uint8_t buf[512];
  size_t len = 0, pos = 0;

  int next() {
    if (pos >= len) {
      len = f.read(buf, sizeof(buf));
      pos = 0;
      if (len == 0) return -1;
    }
    return buf[pos++];
  }
};

Result importToSlot(const char* path, const char* slot, const SpectrometerData& live) {
  Result r = {};
  r.ok = false;
  r.channels = 0;

  Reader rd;
  rd.f = SD.open(path, FILE_READ);
  if (!rd.f) { snprintf(r.error, sizeof(r.error), "cannot open file"); return r; }

  uint32_t* counts = (uint32_t*)calloc(SPECTRUM_CHANNELS, sizeof(uint32_t));
  if (!counts) {
    rd.f.close();
    snprintf(r.error, sizeof(r.error), "out of memory");
    return r;
  }

  char tag[40];
  char text[40];
  int textLen = 0;
  Capture cap = CAP_NONE;
  int nCoef = 0;
  int nPoints = 0;
  bool sawSpectrumEnd = false;
  float coeffs[3] = {0, 0, 0};
  double duration = 0;

  int c;
  while ((c = rd.next()) >= 0 && !sawSpectrumEnd) {
    if (c == '<') {
      // A tag begins: first flush any text captured since the last tag.
      if (cap != CAP_NONE && textLen > 0) {
        text[textLen] = '\0';
        if (cap == CAP_TIME) {
          duration = atof(text);
        } else if (cap == CAP_COEF) {
          if (nCoef < 3) coeffs[nCoef++] = (float)atof(text);
        } else if (cap == CAP_POINT) {
          if (nPoints < SPECTRUM_CHANNELS) {
            long v = atol(text);
            counts[nPoints++] = (v > 0) ? (uint32_t)v : 0;
          }
        }
      }
      cap = CAP_NONE;
      textLen = 0;

      // Read the tag name.
      int t = 0;
      while ((c = rd.next()) >= 0 && c != '>') {
        if (t < (int)sizeof(tag) - 1) tag[t++] = (char)c;
      }
      tag[t] = '\0';
      if (c < 0) break;

      // Strip any namespace prefix and attributes.
      char* name = tag;
      for (char* p = tag; *p; p++) {
        if (*p == ':') name = p + 1;
        if (*p == ' ') { *p = '\0'; break; }
      }

      if (name[0] == '/') {
        // Closing tag: the first complete spectrum is all we want, so a
        // second block (e.g. a paired BackgroundEnergySpectrum) is ignored.
        if (strcasecmp(name + 1, "Spectrum") == 0 && nPoints > 0) sawSpectrumEnd = true;
        continue;
      }
      if (strcasecmp(name, "MeasurementTime") == 0)   cap = CAP_TIME;
      else if (strcasecmp(name, "Coefficient") == 0)  cap = CAP_COEF;
      else if (strcasecmp(name, "DataPoint") == 0)    cap = CAP_POINT;
    } else if (cap != CAP_NONE) {
      if (c != '\r' && c != '\n' && c != '\t' && textLen < (int)sizeof(text) - 1) {
        text[textLen++] = (char)c;
      }
    }
  }
  rd.f.close();

  r.channels = nPoints;
  r.durationSec = (uint32_t)(duration + 0.5);
  r.a0 = coeffs[0]; r.a1 = coeffs[1]; r.a2 = coeffs[2];

  if (nPoints == 0) {
    free(counts);
    snprintf(r.error, sizeof(r.error), "no <DataPoint> data found");
    return r;
  }
  if (nPoints != SPECTRUM_CHANNELS) {
    // Rebinning would silently shift the energy mapping, so refuse instead.
    free(counts);
    snprintf(r.error, sizeof(r.error), "%d channels, need %d", nPoints, SPECTRUM_CHANNELS);
    return r;
  }
  if (duration < 1.0) {
    free(counts);
    snprintf(r.error, sizeof(r.error), "no valid MeasurementTime");
    return r;
  }

  uint32_t total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) total += counts[i];
  r.totalCounts = total;

  // A reference only subtracts correctly if its channels map to the same
  // energies as the live spectrum. a1 dominates that mapping. Only meaningful
  // once the real calibration has been read from the device — before that
  // calibA1 is a placeholder and every import would look mismatched.
  if (nCoef >= 2 && live.calibValid && live.calibA1 > 0.0f) {
    r.calibMismatch = fabsf(coeffs[1] - live.calibA1) > live.calibA1 * 0.01f;
  }

  char outPath[64];
  snprintf(outPath, sizeof(outPath), "/background/%s.csv", slot);
  if (!SD.exists("/background")) SD.mkdir("/background");
  SD.remove(outPath);
  File out = SD.open(outPath, FILE_WRITE);
  if (!out) {
    free(counts);
    snprintf(r.error, sizeof(r.error), "cannot write %s", outPath);
    return r;
  }
  out.println("# background reference");
  out.printf("# duration_s,%u\n", (unsigned)r.durationSec);
  out.printf("# total_counts,%u\n", (unsigned)total);
  out.printf("# imported_from,%s\n", path);
  out.println("channel,cps");
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    out.printf("%d,%.4f\n", i, (float)counts[i] / (float)r.durationSec);
  }
  out.close();
  free(counts);

  Serial.printf("[IMP] %s -> %s (%u ch, %us, %u counts)\n",
                path, outPath, (unsigned)nPoints, (unsigned)r.durationSec, (unsigned)total);
  r.ok = true;
  return r;
}

} // namespace RcImport
