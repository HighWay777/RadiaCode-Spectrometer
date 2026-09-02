#include "ambient.h"
#include <Preferences.h>
#include <math.h>

namespace Ambient {

static const char* NS = "rcambient";

// Seeds, measured on this detector from host/spectra/background.csv (3808 s
// of ordinary indoor ambient): the U-238 chain takes 0.8% of counts and the
// Th-232 chain 1.6%. Seeded slightly high so a fresh device errs toward
// silence rather than toward announcing the room as a sample, then learns
// down to the truth within the first few well-counted windows.
#define SEED_FRACTION_U238  0.012f
#define SEED_FRACTION_TH232 0.022f

// How far above the learned floor a chain must sit to count as the sample's
// rather than the room's.
//
// 2.0 is chosen from the measured separation, not from taste. Against the
// learned floor the real captures give: background 1.0x (both chains), the
// uranium ore 11x for its own chain and 1.6x for the thorium it cross-talks
// into, the thorium mantle 11x and 0.1x. So 2.0 sits in a wide empty gap - the
// nearest thing on either side is 1.6x, and that one is a known artefact we
// want rejected.
#define ELEVATION_RATIO 2.0f

// A floor learned pathologically low must not make everything elevated. The
// excess also has to be a visible share of the spectrum in absolute terms.
#define MIN_ABSOLUTE_EXCESS 0.008f

// An observation is only usable as evidence about the floor once it has been
// counted long enough for its fraction to be stable. Below this the fit's
// chain amplitude swings by more than the effect being measured.
#define LEARN_MIN_SECONDS 600u
#define LEARN_MIN_COUNTS  20000.0

// Adaptation rates, per observation (one per spectrum poll, ~5 s).
//
// Downward is quick: a lower reading is direct evidence that the floor is
// lower, and there is no failure mode in believing it - the worst case is a
// slightly more sensitive device.
//
// Upward is deliberately glacial. At 0.002 per poll the estimate takes on the
// order of an hour to follow a genuine rise, which is fast enough for weather
// and ventilation moving indoor radon and far too slow for a measurement
// session to drag the floor up behind itself.
#define ADAPT_DOWN 0.25f
#define ADAPT_UP   0.002f

struct Level {
  float frac;
  bool  learned;
};

static Level s_u = { SEED_FRACTION_U238,  false };
static Level s_th = { SEED_FRACTION_TH232, false };
// Written back to NVS sparingly: this runs every poll and NVS has a finite
// erase life. Only a change big enough to matter is worth a write.
static float s_savedU = SEED_FRACTION_U238;
static float s_savedTh = SEED_FRACTION_TH232;
#define SAVE_DELTA 0.0015f

static Level* levelFor(DecayChain c) {
  if (c == CHAIN_U238) return &s_u;
  if (c == CHAIN_TH232) return &s_th;
  return nullptr;
}

// Stored as parts per million in a uint32: Preferences has no float on the
// host shim, and ppm is finer than anything this estimate can resolve.
static float fromPpm(uint32_t ppm, float fallback) {
  if (ppm == 0 || ppm > 500000u) return fallback;
  return (float)ppm * 1e-6f;
}

void begin() {
  Preferences p;
  if (!p.begin(NS, true)) return;
  uint32_t u = p.getULong("u238", 0);
  uint32_t t = p.getULong("th232", 0);
  p.end();
  s_u.frac = fromPpm(u, SEED_FRACTION_U238);
  s_u.learned = (u != 0);
  s_th.frac = fromPpm(t, SEED_FRACTION_TH232);
  s_th.learned = (t != 0);
  s_savedU = s_u.frac;
  s_savedTh = s_th.frac;
}

static void persist() {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.putULong("u238", (uint32_t)(s_u.frac * 1e6f));
  p.putULong("th232", (uint32_t)(s_th.frac * 1e6f));
  p.end();
  s_savedU = s_u.frac;
  s_savedTh = s_th.frac;
}

void reset() {
  s_u = { SEED_FRACTION_U238, false };
  s_th = { SEED_FRACTION_TH232, false };
  Preferences p;
  if (p.begin(NS, false)) {
    p.remove("u238");
    p.remove("th232");
    p.end();
  }
  s_savedU = s_u.frac;
  s_savedTh = s_th.frac;
}

float fractionOf(DecayChain c) {
  const Level* l = levelFor(c);
  return l ? l->frac : 0.0f;
}

bool learned(DecayChain c) {
  const Level* l = levelFor(c);
  return l ? l->learned : false;
}

bool elevated(DecayChain c, float frac) {
  const Level* l = levelFor(c);
  if (!l) return false;
  float byRatio = l->frac * ELEVATION_RATIO;
  float byExcess = l->frac + MIN_ABSOLUTE_EXCESS;
  float need = (byRatio > byExcess) ? byRatio : byExcess;
  return frac >= need;
}

void observe(DecayChain c, float frac, uint32_t windowSeconds,
             double totalCounts, bool identified) {
  Level* l = levelFor(c);
  if (!l) return;
  if (windowSeconds < LEARN_MIN_SECONDS || totalCounts < LEARN_MIN_COUNTS) return;
  if (frac < 0.0f || frac > 1.0f) return;

  if (frac < l->frac) {
    l->frac += (frac - l->frac) * ADAPT_DOWN;
    l->learned = true;
  } else if (!identified) {
    l->frac += (frac - l->frac) * ADAPT_UP;
    l->learned = true;
  }

  if (fabsf(s_u.frac - s_savedU) > SAVE_DELTA ||
      fabsf(s_th.frac - s_savedTh) > SAVE_DELTA) {
    persist();
  }
}

} // namespace Ambient
