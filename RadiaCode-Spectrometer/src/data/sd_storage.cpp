#include "sd_storage.h"
#include "background.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>

namespace SdStorage {

static SPIClass sdSPI(HSPI);
static bool s_mounted = false;
static bool s_unreadable = false;
static int s_cs = -1;
static int s_lastGoodCs = 10; // remembered so format can retry the right pin
static char s_status[48] = "not initialised";
static uint32_t s_totalMB = 0;

// CS is undocumented for this board; GPIO10 is the usual choice on Guition
// modules, the rest are plausible spares. Probing beats guessing.
static const int8_t CAND_CS[] = {10, 5, 42, 41, 2, 46, 21};

bool begin() {
  s_mounted = false;
  s_cs = -1;

  for (size_t i = 0; i < sizeof(CAND_CS) / sizeof(CAND_CS[0]); i++) {
    int8_t cs = CAND_CS[i];
    // Never probe a pin the display bus owns — driving it would corrupt the
    // panel. (21 is a QSPI data line on this board; kept in the list only so
    // the guard below documents why it is skipped.)
    if (cs == 45 || cs == 47 || cs == 21 || cs == 48 || cs == 40 || cs == 39 || cs == 17 || cs == 1) {
      continue;
    }

    sdSPI.end();
    sdSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, cs);
    // Conservative clock for probing; SD.begin renegotiates internally.
    if (SD.begin(cs, sdSPI, 4000000)) {
      uint8_t type = SD.cardType();
      if (type != CARD_NONE) {
        s_mounted = true;
        s_cs = cs;
        s_totalMB = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));
        const char* tname = (type == CARD_MMC) ? "MMC"
                          : (type == CARD_SD)  ? "SDSC"
                          : (type == CARD_SDHC) ? "SDHC" : "SD";
        snprintf(s_status, sizeof(s_status), "%s %u MB (CS=%d)", tname, (unsigned)s_totalMB, (int)cs);
        Serial.printf("[SD] Mounted: %s\n", s_status);
        if (!SD.exists("/spectra")) SD.mkdir("/spectra");
        if (!SD.exists("/background")) SD.mkdir("/background");
        return true;
      }
      SD.end();
    }
  }

  // Nothing mounted. Distinguish "no card" from "card present but its
  // filesystem is unreadable" — the latter is the common case with a 64 GB
  // card, because Windows formats anything over 32 GB as exFAT and the
  // ESP32's FATFS build only understands FAT16/FAT32. Probe the SPI layer
  // directly: if a card answers CMD0/CMD8 it is physically there.
  s_unreadable = false;
  for (size_t i = 0; i < sizeof(CAND_CS) / sizeof(CAND_CS[0]); i++) {
    int8_t cs = CAND_CS[i];
    if (cs == 45 || cs == 47 || cs == 21 || cs == 48 || cs == 40 || cs == 39 || cs == 17 || cs == 1) continue;
    sdSPI.end();
    sdSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, cs);
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    // 80 idle clocks with CS high puts an SD card into native SPI mode.
    sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int k = 0; k < 10; k++) sdSPI.transfer(0xFF);
    digitalWrite(cs, LOW);
    // CMD0 (GO_IDLE_STATE) — a present card replies 0x01.
    const uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
    for (uint8_t b : cmd0) sdSPI.transfer(b);
    uint8_t resp = 0xFF;
    for (int k = 0; k < 16 && resp == 0xFF; k++) resp = sdSPI.transfer(0xFF);
    digitalWrite(cs, HIGH);
    sdSPI.transfer(0xFF);
    sdSPI.endTransaction();
    if (resp == 0x01 || resp == 0x00) {
      s_unreadable = true;
      s_lastGoodCs = cs;
      break;
    }
  }

  sdSPI.end();
  if (s_unreadable) {
    snprintf(s_status, sizeof(s_status), "card unreadable (exFAT?)");
    Serial.printf("[SD] Card responds on CS=%d but filesystem will not mount.\n", s_lastGoodCs);
    Serial.println("[SD] Most likely exFAT. Use FORMAT on the Data screen to make it FAT32.");
  } else {
    snprintf(s_status, sizeof(s_status), "no card detected");
    Serial.println("[SD] No card found on any candidate CS pin");
  }
  return false;
}

bool cardPresentButUnreadable() { return s_unreadable; }

bool formatCard() {
  Serial.println("[SD] Formatting card to FAT32 - this erases everything...");
  if (s_mounted) { SD.end(); s_mounted = false; }
  sdSPI.end();

  int cs = (s_cs >= 0) ? s_cs : s_lastGoodCs;
  sdSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, cs);
  // format_if_empty=true makes the mount helper run mkfs when the existing
  // filesystem cannot be mounted — which is exactly the exFAT case.
  if (SD.begin(cs, sdSPI, 4000000, "/sd", 5, /* format_if_empty */ true)) {
    if (SD.cardType() != CARD_NONE) {
      s_mounted = true;
      s_unreadable = false;
      s_cs = cs;
      s_totalMB = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));
      snprintf(s_status, sizeof(s_status), "FAT32 %u MB (CS=%d)", (unsigned)s_totalMB, cs);
      if (!SD.exists("/spectra")) SD.mkdir("/spectra");
      if (!SD.exists("/background")) SD.mkdir("/background");
      Serial.printf("[SD] Format OK: %s\n", s_status);
      return true;
    }
  }
  sdSPI.end();
  snprintf(s_status, sizeof(s_status), "format failed");
  Serial.println("[SD] Format failed");
  return false;
}

static void removeDirContents(const char* dirPath) {
  File dir = SD.open(dirPath);
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    char path[80];
    snprintf(path, sizeof(path), "%s", f.path());
    bool isDir = f.isDirectory();
    f.close();
    if (!isDir) SD.remove(path);
    f = dir.openNextFile();
  }
  dir.close();
}

bool eraseAllData() {
  if (!s_mounted) return false;
  removeDirContents("/spectra");
  removeDirContents("/background");
  Serial.println("[SD] Erased /spectra and /background");
  return true;
}

bool available() { return s_mounted; }
int csPin() { return s_cs; }
const char* status() { return s_status; }
uint32_t totalMB() { return s_totalMB; }

uint32_t freeMB() {
  if (!s_mounted) return 0;
  uint64_t used = SD.usedBytes();
  uint64_t total = SD.totalBytes();
  if (total <= used) return 0;
  return (uint32_t)((total - used) / (1024ULL * 1024ULL));
}

int countSpectra() {
  if (!s_mounted) return 0;
  File dir = SD.open("/spectra");
  if (!dir) return 0;
  int n = 0;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) n++;
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  return n;
}

bool exportSpectrum(const SpectrometerData& d, char* outPath, size_t outLen) {
  if (!s_mounted) return false;

  // Auto-number rather than overwrite; no RTC on this board so a timestamp
  // is not available.
  char path[40];
  int idx = 1;
  for (; idx < 10000; idx++) {
    snprintf(path, sizeof(path), "/spectra/spec_%04d.csv", idx);
    if (!SD.exists(path)) break;
  }
  if (idx >= 10000) return false;

  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;

  // Header comments carry everything needed to re-derive the energy axis
  // offline (Gamma-MCA and friends read plain CSV).
  f.println("# RadiaCode-110 spectrum export");
  f.printf("# duration_s,%u\n", (unsigned)d.spectrumDurationSec);
  f.printf("# total_counts,%u\n", (unsigned)d.quality.totalCounts);
  f.printf("# cps,%.3f\n", d.cps);
  f.printf("# dose_uSv_h,%.4f\n", d.doseRateUSvH);
  f.printf("# calib_a0,%.6f\n", d.calibA0);
  f.printf("# calib_a1,%.6f\n", d.calibA1);
  f.printf("# calib_a2,%.8f\n", d.calibA2);
  f.printf("# background_subtracted,%d\n", Background::active() ? 1 : 0);
  f.println("channel,energy_keV,counts");

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    f.printf("%d,%.2f,%u\n", i, channelToKeV(d, i), (unsigned)d.spectrum[i]);
  }
  f.close();

  if (outPath && outLen) {
    strncpy(outPath, path, outLen - 1);
    outPath[outLen - 1] = '\0';
  }
  Serial.printf("[SD] Wrote %s\n", path);
  return true;
}

void pollPresence() {
  static unsigned long lastCheckMs = 0;
  unsigned long now = millis();
  if (now - lastCheckMs < 3000) return;
  lastCheckMs = now;

  if (s_mounted) {
    // cardType() goes to the card, so a pulled card reports CARD_NONE.
    if (SD.cardType() == CARD_NONE) {
      Serial.println("[SD] Card removed");
      SD.end();
      s_mounted = false;
      s_unreadable = false;
      s_totalMB = 0;
      snprintf(s_status, sizeof(s_status), "card removed");
    }
    return;
  }

  // Not mounted: try to pick up a newly inserted card, but only on the pin
  // that answered before, so this stays cheap.
  int cs = (s_lastGoodCs >= 0) ? s_lastGoodCs : 10;
  sdSPI.end();
  sdSPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, cs);
  if (SD.begin(cs, sdSPI, 4000000) && SD.cardType() != CARD_NONE) {
    s_mounted = true;
    s_cs = cs;
    s_unreadable = false;
    s_totalMB = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));
    snprintf(s_status, sizeof(s_status), "SD %u MB (CS=%d)", (unsigned)s_totalMB, cs);
    if (!SD.exists("/spectra")) SD.mkdir("/spectra");
    if (!SD.exists("/background")) SD.mkdir("/background");
    if (!SD.exists("/import")) SD.mkdir("/import");
    Serial.printf("[SD] Card inserted: %s\n", s_status);
  } else {
    sdSPI.end();
  }
}

bool autosaveSpectrum(const SpectrometerData& d) {
  if (!s_mounted || d.spectrumDurationSec == 0) return false;
  const char* path = "/spectra/_autosave.csv";
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.println("# RadiaCode-110 autosave checkpoint");
  f.printf("# duration_s,%u\n", (unsigned)d.spectrumDurationSec);
  f.printf("# total_counts,%u\n", (unsigned)d.quality.totalCounts);
  f.printf("# calib_a0,%.6f\n", d.calibA0);
  f.printf("# calib_a1,%.6f\n", d.calibA1);
  f.printf("# calib_a2,%.8f\n", d.calibA2);
  f.println("channel,counts");
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    f.printf("%d,%u\n", i, (unsigned)d.spectrum[i]);
  }
  f.close();
  return true;
}

bool saveBackgroundRef(const char* name) {
  if (!s_mounted || !Background::available()) return false;
  char path[48];
  snprintf(path, sizeof(path), "/background/%s.csv", name);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.printf("# background reference\n");
  f.printf("# duration_s,%u\n", (unsigned)Background::durationSec());
  f.printf("# total_counts,%u\n", (unsigned)Background::totalCounts());
  f.println("channel,cps");
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    f.printf("%d,%.4f\n", i, Background::rateAt(i));
  }
  f.close();
  Serial.printf("[SD] Saved background to %s\n", path);
  return true;
}

static void logDir(const char* path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[SD]   %s : (missing)\n", path);
    if (dir) dir.close();
    return;
  }
  int n = 0;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      Serial.printf("[SD]   %s/%s  %u bytes\n", path, f.name(), (unsigned)f.size());
      n++;
    }
    f.close();
    f = dir.openNextFile();
  }
  if (n == 0) Serial.printf("[SD]   %s : (empty)\n", path);
  dir.close();
}

void logInventory() {
  if (!s_mounted) { Serial.println("[SD] inventory: no card"); return; }
  Serial.println("[SD] --- card inventory ---");
  logDir("/import");
  logDir("/background");
  Serial.printf("[SD]   /spectra : %d file(s)\n", countSpectra());
}

bool refExists(const char* name) {
  if (!s_mounted) return false;
  char path[48];
  snprintf(path, sizeof(path), "/background/%s.csv", name);
  return SD.exists(path);
}

bool refInfo(const char* name, uint32_t* durationSec, uint32_t* totalCounts) {
  if (!s_mounted) return false;
  char path[48];
  snprintf(path, sizeof(path), "/background/%s.csv", name);
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  uint32_t dur = 0, tot = 0;
  char line[64];
  // Only the header comments are needed, so stop as soon as data begins.
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    if (line[0] != '#') break;
    unsigned v = 0;
    if (sscanf(line, "# duration_s,%u", &v) == 1) dur = v;
    else if (sscanf(line, "# total_counts,%u", &v) == 1) tot = v;
  }
  f.close();
  if (durationSec) *durationSec = dur;
  if (totalCounts) *totalCounts = tot;
  return dur > 0;
}

bool loadBackgroundRef(const char* name) {
  if (!s_mounted) return false;
  char path[48];
  snprintf(path, sizeof(path), "/background/%s.csv", name);
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  // 4 KB of floats — allocated rather than static so it is only held while
  // actually loading.
  float* rates = (float*)calloc(SPECTRUM_CHANNELS, sizeof(float));
  if (rates == nullptr) { f.close(); return false; }

  uint32_t dur = 0, tot = 0;
  char line[64];
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    if (n == 0) continue;
    if (line[0] == '#') {
      unsigned v = 0;
      if (sscanf(line, "# duration_s,%u", &v) == 1) dur = v;
      else if (sscanf(line, "# total_counts,%u", &v) == 1) tot = v;
      continue;
    }
    int ch = 0; float cps = 0.0f;
    if (sscanf(line, "%d,%f", &ch, &cps) == 2) {
      if (ch >= 0 && ch < SPECTRUM_CHANNELS) rates[ch] = cps;
    }
  }
  f.close();

  bool ok = (dur > 0) && Background::setFromRates(rates, dur, tot);
  free(rates);
  if (ok) {
    // Name the active reference so the UI can show what is being subtracted.
    Background::setLabel(name);
    Serial.printf("[SD] Loaded background reference '%s' (%us) - now ACTIVE\n",
                  name, (unsigned)dur);
  }
  return ok;
}

} // namespace SdStorage
