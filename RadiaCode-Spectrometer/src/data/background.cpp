#include "background.h"
#include "../analysis/templates.h"
#include <Preferences.h>
#include <string.h>

namespace Background {

static const char* NS      = "radiacode";
static const char* K_DATA  = "bgdata";
static const char* K_DUR   = "bgdur";
static const char* K_TOTAL = "bgtot";
static const char* K_EN    = "bgen";
static const char* K_LABEL = "bglbl";

// counts/second x 1000, per channel. uint16 keeps the NVS blob at 2 KB
// (comfortable inside the default 20 KB nvs partition) while resolving down
// to 0.001 c/s — far below anything measurable in a sane acquisition time.
static uint16_t s_milliCps[SPECTRUM_CHANNELS];
static bool s_available = false;
static bool s_enabled = false;
static uint32_t s_duration = 0;
static uint32_t s_total = 0;
static char s_label[24] = "";

void begin() {
  memset(s_milliCps, 0, sizeof(s_milliCps));
  Preferences prefs;
  if (!prefs.begin(NS, /* readOnly */ true)) {
    s_available = false;
    s_enabled = false;
    return;
  }
  size_t len = prefs.getBytesLength(K_DATA);
  if (len == sizeof(s_milliCps)) {
    prefs.getBytes(K_DATA, s_milliCps, sizeof(s_milliCps));
    s_duration = prefs.getULong(K_DUR, 0);
    s_total = prefs.getULong(K_TOTAL, 0);
    s_available = (s_duration > 0);
  }
  s_enabled = prefs.getBool(K_EN, false);
  String lbl = prefs.getString(K_LABEL, "");
  strncpy(s_label, lbl.c_str(), sizeof(s_label) - 1);
  s_label[sizeof(s_label) - 1] = '\0';
  prefs.end();
}

const char* label() { return s_label[0] ? s_label : "unnamed"; }

void setLabel(const char* text) {
  strncpy(s_label, text ? text : "", sizeof(s_label) - 1);
  s_label[sizeof(s_label) - 1] = '\0';
  Preferences prefs;
  if (prefs.begin(NS, false)) {
    prefs.putString(K_LABEL, s_label);
    prefs.end();
  }
}

bool available() { return s_available; }
bool enabled()   { return s_enabled; }
bool active()    { return s_available && s_enabled; }

void setEnabled(bool on) {
  s_enabled = on;
  Templates::invalidate();
  Preferences prefs;
  if (prefs.begin(NS, false)) {
    prefs.putBool(K_EN, on);
    prefs.end();
  }
}

bool record(const SpectrometerData& d) {
  if (d.spectrumDurationSec < MIN_RECORD_SECONDS) return false;

  float dur = (float)d.spectrumDurationSec;
  uint32_t total = 0;
  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    total += d.spectrum[i];
    float rate = (float)d.spectrum[i] / dur;      // counts/second
    float milli = rate * 1000.0f;
    if (milli < 0.0f) milli = 0.0f;
    if (milli > 65535.0f) milli = 65535.0f;
    s_milliCps[i] = (uint16_t)(milli + 0.5f);
  }

  s_duration = d.spectrumDurationSec;
  s_total = total;
  s_available = true;
  s_enabled = true; // recording one implies wanting to use it
  snprintf(s_label, sizeof(s_label), "recorded here");

  Preferences prefs;
  if (!prefs.begin(NS, false)) return false;
  bool ok = prefs.putBytes(K_DATA, s_milliCps, sizeof(s_milliCps)) == sizeof(s_milliCps);
  prefs.putULong(K_DUR, s_duration);
  prefs.putULong(K_TOTAL, s_total);
  prefs.putBool(K_EN, s_enabled);
  prefs.putString(K_LABEL, s_label);
  prefs.end();
  Templates::invalidate();
  return ok;
}

bool setFromRates(const float* cps, uint32_t durationSec, uint32_t totalCounts) {
  if (cps == nullptr || durationSec == 0) return false;

  for (int i = 0; i < SPECTRUM_CHANNELS; i++) {
    float milli = cps[i] * 1000.0f;
    if (milli < 0.0f) milli = 0.0f;
    if (milli > 65535.0f) milli = 65535.0f;
    s_milliCps[i] = (uint16_t)(milli + 0.5f);
  }
  s_duration = durationSec;
  s_total = totalCounts;
  s_available = true;
  s_enabled = true;

  Preferences prefs;
  if (!prefs.begin(NS, false)) return false;
  bool ok = prefs.putBytes(K_DATA, s_milliCps, sizeof(s_milliCps)) == sizeof(s_milliCps);
  prefs.putULong(K_DUR, s_duration);
  prefs.putULong(K_TOTAL, s_total);
  prefs.putBool(K_EN, s_enabled);
  prefs.putString(K_LABEL, s_label);
  prefs.end();
  Templates::invalidate();
  return ok;
}

void erase() {
  memset(s_milliCps, 0, sizeof(s_milliCps));
  s_available = false;
  s_enabled = false;
  s_duration = 0;
  s_total = 0;
  s_label[0] = 0; // a stale name must not outlive its data
  Templates::invalidate();
  Preferences prefs;
  if (prefs.begin(NS, false)) {
    prefs.remove(K_DATA);
    prefs.remove(K_DUR);
    prefs.remove(K_TOTAL);
    prefs.remove(K_LABEL);
    prefs.putBool(K_EN, false);
    prefs.end();
  }
}

float rateAt(int channel) {
  if (!s_available || channel < 0 || channel >= SPECTRUM_CHANNELS) return 0.0f;
  return (float)s_milliCps[channel] * 0.001f;
}

uint32_t durationSec() { return s_duration; }
uint32_t totalCounts() { return s_total; }

} // namespace Background
