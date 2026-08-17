# RadiaCode110 — Real-Time Spectroscopy / Isotope ID Project

## Vision

RadiaCode-110 (gamma spectrometer) → ESP32-S3 with an integrated 4.3" touch display →
analyzes the live gamma spectrum on-device and shows probable isotopes directly on its
own screen. Optional phone/web tier for heavier analysis or logging, but the ESP32-S3
board is now the primary UI, not just a bridge (see "On-device display hardware" below
— the plan changed once we confirmed this board has a real screen on it).

This file captures research findings and architecture decisions so far. Two folders now:
- `ESP32-S3-Touch/` — the cleaned, generic display+touch driver base (see "On-device
  display hardware" below). Kept pristine/reusable — future non-RadiaCode ESP32-S3
  projects on this board can start from a copy of it too. Not meant to accumulate
  spectrometer-specific code.
- `RadiaCode-Spectrometer/` (2026-08-04) — the actual implementation, seeded from that
  base. Has a working card-based 3-screen UI, running today on simulated data. See
  "RadiaCode-Spectrometer implementation" below for what's built and what's next.

## Hardware reality check (RC-110)

- Detector: CsI(Tl) scintillator + SiPM.
- Spectrum: 1024 channels, quadratic energy calibration `a0, a1, a2` (channel → keV),
  readable from the device.
- **Measured resolution: ~8.4% ± 0.3% FWHM at 662 keV (Cs-137).** This is the single
  biggest constraint on what "isotope ID" can realistically mean here.
- No official API — every BLE integration that exists is reverse-engineered.

### Why 8.4% FWHM matters

- Lab-grade HPGe detectors: <1% FWHM. Good NaI(Tl) handhelds: ~7%. RC-110 at 8.4% is
  on the wide end even for scintillators.
- Practical effect: gamma lines within roughly 60–100 keV of each other blur into one
  bump instead of resolving as two peaks. Strong, isolated sources (Cs-137, Co-60,
  Am-241, K-40, natural Ra-226/Th-232 chains) are identifiable. Mixed/weak sources or
  isotopes with closely-spaced lines are not reliably distinguishable.
- Conclusion: target "probable candidate isotope, ranked by confidence" — the same
  class of result RadiaCode's own app gives via its manual tap-to-identify feature —
  not lab-grade definitive ID.

## On-device display hardware (ESP32-S3-Touch)

`ESP32-S3-Touch/` is a real PlatformIO project the user already had for unrelated
purposes (a WiFi weather/boiler dashboard with a web-based "page builder"). It was
cleaned out (2026-08-03) to keep only the display/touch driver layer and repurposed as
the base for the spectrometer UI. Confirmed via `pio run`: builds clean, RAM 6.1%
(19,948 / 327,680 B), Flash 25.4% (333,553 / 1,310,720 B) used by the minimal bring-up.

**Board**: ESP32-S3 (JC4827W543 board), 4 MB flash, 8 MB PSRAM (`BOARD_HAS_PSRAM` build
flag set; no octal/quad PSRAM mode explicitly configured yet — check before relying on
PSRAM-backed buffers).

**Display**: NV3041A controller, 480×272 IPS, driven over QSPI (4-line quad SPI, *not*
RGB parallel, *not* SPI TFT in the usual sense). 480×272 is the standard resolution for
this panel size/controller class (matches the JC4827W543 4.3" module), so the "4.3 inch"
description checks out.
Driven via a **patched** Arduino_GFX 1.4.4 (`Libraries/Arduino_GFX-1.4.4/`) — the stock
driver's `writeAddrWindow()` causes flicker/tearing on partial-screen updates over QSPI;
the patch (contiguous `writeC8D16D16` calls) is applied and documented in
`ESP32-S3-Touch/README.md`. **This patch is load-bearing — don't pull in an unpatched
Arduino_GFX copy.**

Pin map:
```
QSPI display: CS=45  SCK=47  D0(MOSI)=21  D1(MISO)=48  D2(WP)=40  D3(HOLD)=39  RST=17
Backlight:    GPIO1, LEDC PWM (channel 0, 12-bit, 5kHz)
GT911 touch:  SDA=8  SCL=4  INT=3  RST=38  (I2C, 400kHz)
```

**Touch**: GT911 over I2C via `Libraries/Touch_GT911/` (a different, unused GT911
library — `Gt911-arduino-main`/`TAMC_GT911` — was archived; don't reintroduce both).

**What's in `ESP32-S3-Touch/` now** (post-cleanup):
- `src/main.cpp` — minimal bring-up: QSPI bus/display init, backlight PWM, GT911 touch
  init, placeholder screen, Serial-logged tap coordinates. This is the starting point
  for the spectrometer screens.
- `src/ui/touch_handler.*` — GT911 driver wrapper (tap/swipe gesture detection). Generic, kept.
- `src/ui/touch_feedback.*` — tap ripple animation. Generic, kept (needed `ui_theme.h`
  split out of it, see below).
- `src/ui/page_transitions.*` — slide/fade animations between screens. Generic, kept.
- `src/ui/ui_theme.h` — new, minimal: just the `UITheme` color-struct + `extern
  UITheme* currentTheme;`. Split out of the old app's `ui_framework.h` so
  touch_feedback/page_transitions didn't have to pull in the whole discarded app UI
  framework (which was tangled up with the old app's config store). Populate a real
  theme for the spectrometer UI here.
- `Libraries/Arduino_GFX-1.4.4/`, `Libraries/Touch_GT911/` — the two libraries
  actually used; everything else under the old `Libraries/` was dead/unreferenced.
- `README.md` — kept as-is; it's a genuinely good, self-contained hardware bring-up
  guide (wiring, the driver patch, a copyable minimal example). Worth reading before
  touching the display code.

**What got archived** (moved, not deleted, to `ESP32-S3-Touch/_archive_previous_project/`
— recoverable if ever needed, but not meant to be built against):
- The old app: WiFi/weather/boiler dashboard pages, the web-based "LCD page builder"
  (`AsyncWebServer` routes + SPIFFS-served JS/HTML frontend in old `data/`), config
  store, NTP time manager.
- ~113 top-level `*.md` files — AI-agent session debug logs from that prior project
  (bug reports, fix summaries), not reference docs. `README.md` was the one exception
  worth keeping.
- Dead vendored libraries that were never actually wired up: a full **LVGL v8.3-dev
  source tree** + two `lv_conf.h` variants (present but `lv_init`/`lv_disp_drv` never
  called anywhere — the old app was hand-rolled directly on Arduino_GFX calls, not
  LVGL), `TFT_eSPI_original` (unused), `Gt911-arduino-main`/`TAMC_GT911` (unused
  duplicate touch lib), and three fully unrelated libraries (`blinker-library-master`,
  `ESP32-audioI2S-master`, `Regexp-master`).
- **⚠️ Two files with real plaintext credentials got archived, not deleted**:
  `_archive_previous_project/src/network/wifi_credentials.h` (WiFi SSID/password) and
  `_archive_previous_project/src/ui/boiler_credentials.h` (HTTP Basic Auth user/pass
  for a remote boiler API). They're only on this local machine (not a git repo, nothing
  pushed), but don't copy them into the new project, and rotate those credentials if
  this folder is ever backed up, shared, or turned into a git repo.

**LVGL decision point**: the spectrometer UI will want a live-updating line/bar chart
(the 1024-channel spectrum) plus a scrollable list of candidate isotopes with
confidence — LVGL's `lv_chart`/`lv_list` widgets are a much better fit for that than
hand-rolling immediate-mode draws (which is what the old app did). LVGL source is
sitting in the archive folder if we decide to wire it in; doing so means writing the
`lv_disp_drv` flush callback (→ Arduino_GFX) and `lv_indev_drv` read callback (→ GT911)
glue, which doesn't exist yet in this codebase. Recommend deciding this before writing
the first real spectrometer screen rather than starting hand-rolled and migrating later.

**Toolchain note**: this machine's global PlatformIO package cache had no
`toolchain-xtensa-esp32s3` installed yet (only plain ESP32); first build had to fetch
it. Package-mirror SSL errors were transient — a retry succeeded.

## RadiaCode-Spectrometer implementation

`RadiaCode-Spectrometer/` is the real firmware project (seeded from the `ESP32-S3-Touch`
base). Builds clean via `pio run`: RAM ~22% (72,640 / 327,680 B), Flash ~77%
(1,009,753 / 1,310,720 B — the Bluedroid BLE stack is the big line item). **Live on
real hardware as of 2026-08-04**: connects to the user's actual RC-110 (serial
`RC-110-000983`) over BLE, reads real energy calibration/spectrum/dose-rate, and runs
the isotope matcher against real data. `MockSource` (`src/data/mock_source.*`) is still
in the tree and still compiles, just unused by default — see the module layout below
to swap back for offline UI testing without hardware.

**UI approach: hand-rolled Arduino_GFX, not LVGL.** The base project has LVGL v8.3-dev
vendored but unwired (see the archived-project note above). Went with hand-rolled cards
on the proven, QSPI-patched Arduino_GFX stack instead — lower integration risk than
writing untested `lv_disp_drv`/`lv_indev_drv` glue from scratch, and this hardware's
partial-update patch is already validated for exactly this style of drawing. Revisit
LVGL if the card UI becomes limiting (e.g. wanting scrollable lists, smooth chart
zoom/pan) — nothing here blocks migrating later.

**Module layout**:
- `src/data/spectrometer_data.h/.cpp` — `SpectrometerData` (the `g_data` global,
  defined once in the `.cpp`): connection state, battery, dose rate, CPM, the
  1024-channel spectrum + calibration coefficients, and up to `MAX_CANDIDATES` (6)
  ranked isotope matches. This struct is the seam between data source and UI — every
  screen reads only from it.
- `src/data/ble_source.*` — **the live data source.** Scans for a BLE device
  advertising a name containing "RadiaCode" (no hardcoded MAC — see "BLE integration"
  below), connects via the vendored `mkgeiger/RadiaCode` library, reads
  `energyCalib()`/`spectrumReset()` once at connect time, then polls `dataBuf()`
  (~1s, dose rate) and `spectrum()` (~5s, full histogram → feeds `analyzeSpectrum()`)
  on a timer from `tick()`. `main.cpp` calls this, not `MockSource`.
- `src/data/mock_source.*` — simulated data producer (Cs-137 peak ramping in over a
  K-40 background), kept for offline UI testing without hardware. To switch back:
  in `main.cpp`, swap the `#include "data/ble_source.h"` for `mock_source.h` and
  `BleSource::tick()` for `MockSource::tick()` (and `ScreenHome::handleTap` calls
  `BleSource::requestConnect/Disconnect` directly — swap those too).
- `src/data/isotope_db.h` — starter table of ~13 isotopes (Cs-137, Co-60, Am-241, K-40,
  Na-22, I-131, Ba-133, Eu-152, Ra-226 chain, Th-232 chain, U-238 chain, Co-57, Mn-54)
  with public NNDC-style line energies/intensities. Per the earlier open item, this is a
  starting point, not validated against a proper reference — expand/correct before
  trusting results beyond demo purposes.
- `src/analysis/peak_search.*` — the classical algorithm from "Isotope-ID algorithm
  options" option 1: boxcar smoothing → wide-boxcar continuum estimate → local-maxima
  peaks above a 3σ Poisson-significance threshold → energy conversion via calibration →
  match against `isotope_db` within a tolerance window derived from the
  8.4%-FWHM-at-662-keV resolution model (scaled ∝ 1/√E). **A candidate's strongest line
  must be matched, not just any line** (added 2026-08-04 after a real false positive —
  see "Known issues found on hardware" below) — matching only a minor secondary line
  isn't real evidence. Produces `g_data.candidates`, sorted by confidence.
- `src/ui/cards.*` — the small drawing toolkit every screen uses: rounded-rect card
  frames (with a highlighted-border state for "this is the current top match"), a
  partial-redraw-safe big-number stat area, buttons, progress bars, text helpers.
- `src/ui/ui_theme.*` — dark theme (`APP_THEME`) with accent/warning/danger/highlight
  colors; `currentTheme` also feeds the inherited `touch_feedback`/`page_transitions`
  modules from the base.
- `src/ui/screen_home.*`, `screen_isotopes.*`, `screen_spectrum.*`,
  `screen_isotope_detail.*` — the four screens (below). Each exposes `draw()` (full
  redraw, called on screen entry) and `update()` (partial redraw of just the live
  values, called on a 500ms timer). The three main screens now **diff-cache** what
  they last drew and skip the redraw when nothing changed (added 2026-08-04 — the
  original unconditional redraw-every-tick was visibly flashy on the QSPI panel).
- `src/main.cpp` — screen state machine (`SCR_HOME`/`SCR_ISOTOPES`/`SCR_SPECTRUM`
  cycle via swipe, plus `SCR_ISOTOPE_DETAIL` as a tap-only drill-down outside that
  cycle), tap routing, and the `BleSource::tick()` / periodic `update()` calls.

**Screen 1 — Home** (`screen_home.*`): header with title + connection status
dot/label. Four cards: **Dose Rate** (µSv/h, color-coded accent→warning→danger as it
rises), **Count Rate** (CPM), **Likely Source** (top isotope candidate, symbol in
highlight gold, confidence %, name), **RC-110 Status** (battery %, `LIVE HH:MM` session
timer). Full-width **Connect/Disconnect** button at the bottom.

**Screen 2 — Isotope Candidates** (`screen_isotopes.*`): 2×3 grid of candidate cards
(symbol, confidence % + bar, name, matched energy). The top match (`candidates[0]`) gets
the gold highlighted border + gold symbol color. **Tapping a populated card** opens
Screen 4 (added 2026-08-04, per request: "when candidate appear, and when user click on
it, to show more details").

**Screen 3 — Live Spectrum** (`screen_spectrum.*`): the 1024-channel spectrum rebinned
into a 464px-wide bar chart, y-axis auto-scaled to the current max bin, x-axis labeled
in keV (from the live calibration coefficients), with small gold triangle markers over
every channel `peak_search` currently flags as statistically significant.

**Screen 4 — Isotope Detail** (`screen_isotope_detail.*`, new 2026-08-04): reached only
by tapping a candidate card on Screen 2, not via swipe. Snapshots the tapped
`IsotopeCandidate` at tap time (so it stays stable even if the live ranking reorders
while viewing it), shows the full symbol/name, confidence bar, matched peak energy, and
every known gamma line for that isotope from `isotope_db` (matched line highlighted
gold). BACK button returns to Screen 2.

Navigation: swipe left/right cycles Home → Isotopes → Spectrum → Home
(`TRANSITION_SLIDE_LEFT`/`RIGHT`). Isotopes↔Detail is tap-only, also using
`TRANSITION_SLIDE_LEFT`/`RIGHT` — **not** `TRANSITION_FADE`, see "Known issues" below.

## BLE integration (mkgeiger/RadiaCode)

Vendored into `Libraries/RadiaCode/` (same `lib_extra_dirs = Libraries` pattern as
Arduino_GFX/Touch_GT911) from the upstream repo's `main` branch. Uses the classic
ESP32 Arduino BLE stack (`<BLEDevice.h>`, Bluedroid) — needs `CONFIG_BLUEDROID_ENABLED`
from the framework's real `sdkconfig.h`, see the landmine below.

**Connection is by BLE scan + name match, not a hardcoded MAC.** `BleSource::
findDeviceAddress()` does a 5s active scan, logs every discovered device to Serial, and
connects to the first one whose advertised name contains "RadiaCode" (confirmed
working: `RadiaCode-110#RC-110-000983`). This avoids hardcoding a MAC that would break
if the device is swapped or re-paired.

**Two patches were required to get this actually connecting on our hardware:**

1. **`include/sdkconfig.h` landmine (framework-level, not a library patch).** Both
   `ESP32-S3-Touch/` and the original copy in `RadiaCode-Spectrometer/` had a leftover
   `include/sdkconfig.h` from the old boiler-app project — a hand-written stub
   defining a handful of WiFi/FreeRTOS tuning macros, including
   `CONFIG_ESP32S3_SPIRAM_SUPPORT 0` (disabling PSRAM) and no Bluetooth macros at all.
   Because PlatformIO searches the project's own `include/` before framework library
   dirs, this stub **silently shadowed** the real ~1000-macro framework
   `sdkconfig.h` (at `tools/sdk/esp32s3/qio_qspi/include/`), so
   `CONFIG_BLUEDROID_ENABLED` was never defined and `<BLEDevice.h>`'s entire class
   body compiled out — `'BLEDevice' has not been declared`. **Fix: delete the stub**
   (done in both projects). Watch for this exact file reappearing if either project
   is ever re-seeded from an older copy — it's an easy one to miss since the filename
   looks like a legitimate framework artifact.
2. **`Libraries/RadiaCode/src/BluetoothTransport.cpp` — BLE address type.** Confirmed
   on hardware that `BLE_ADDR_TYPE_PUBLIC` is correct for our RC-110; the patch tries
   public first, random as fallback (see the patch comment in that file dated
   2026-08-04 for the full story — an initial guess to try random-first, based on an
   early connect failure, turned out to be backwards; the real cause of that first
   failure was almost certainly something else already holding the BLE connection,
   most likely the official phone app still connected).

**CPM is derived from the spectrum's own accumulating counts** (`pollSpectrum()` in
`ble_source.cpp`: delta of total counts between polls ÷ elapsed seconds × 60), not from
`RealTimeData::count_rate` directly. Investigated as a possible bug (initial hardware
reading was ~1700 CPM, which looked wrong) but **cross-validated three independent
ways** — the device's own real-time telemetry (steady ~28.7 CPS), the spectrum-delta
calc, and a cumulative total/duration check all agreed closely, and the user separately
confirmed against the official RadiaCode phone app (~26 CPS, matching). Conclusion: not
a bug — this detector is simply far more sensitive than a classic Geiger tube, so a
background CPM in the ~1700 range alongside a normal, safe ~0.18 µSv/h dose rate is
expected, not alarming. `dose_rate * 10000` (undocumented library scale) also checked
out against the phone app.

## Known issues found on hardware testing (2026-08-04)

- **Ba-133 false positive**: appeared as a "likely source" at ~33% confidence with no
  actual Ba-133 present, because only its minor 81 keV line matched some noise/peak,
  not its dominant 356 keV line. Fixed in `peak_search.cpp` — a candidate now requires
  its strongest line specifically to be matched. Worth retesting whether other
  multi-line isotopes in the table produce similar low-confidence noise; consider also
  adding a minimum-confidence floor before a "Likely Source" is shown at all (e.g. hide
  below ~40%) if weak partial matches keep surfacing.
- **Reboot on tapping into the isotope detail screen**: happened once, cause not fully
  confirmed (no serial monitor was attached at the exact moment, so no crash
  backtrace was captured). The one clearly-implicated suspect: that tap path used
  `TRANSITION_FADE`, whose `animateFade()` (`page_transitions.cpp`) redraws the full
  screen via ~1000 individual 4×4 `fillRect` calls *per animation frame*, repeated
  across many frames during the transition — dramatically more QSPI traffic than the
  slide transitions (a couple of line draws per frame) that had already been
  hardware-proven via swipe navigation all session. Mitigated by switching that
  navigation to `TRANSITION_SLIDE_LEFT`/`RIGHT` instead (main.cpp) — same visual
  effect family, but the already-proven code path. **Not yet re-verified on hardware**
  — if it reboots again, capture `pio device monitor` output at the moment of the
  crash; ESP32 Arduino prints a "Guru Meditation Error" + backtrace on panic, which
  would pinpoint the real cause immediately instead of guessing.

## Existing building blocks (reuse, don't reinvent)

| Project | What it gives us |
|---|---|
| [cdump/radiacode](https://github.com/cdump/radiacode) (Python) | Reference USB/BLE protocol implementation. `device.spectrum()`, `device.energy_calib()`. Everything else below was reverse-engineered from this. |
| [mkgeiger/RadiaCode](https://github.com/mkgeiger/RadiaCode) (Arduino/C++) | ESP32 BLE library, **explicitly lists RC-110 as supported**. Exposes `spectrum()` / `spectrumAccum()` (per-channel counts), calibration coefficients `a0/a1/a2`, and `dose_rate`/`count_rate` via `dataBuf()`. Uses static shared buffers to avoid heap fragmentation on ESP32. Author states the protocol is reverse-engineered and the library is **not guaranteed 100% complete/correct** — validate against our actual RC-110 firmware before relying on it. |
| [darkmatter2222/Open-RadiaCode-Android](https://github.com/darkmatter2222/Open-RadiaCode-Android) | Existing Android app, already does real-time isotope ID — via a **CNN backend** ("VegaModel", 34.5M params, 82 isotopes) running as a separate GPU microservice the phone calls. Not an on-device model. |
| [Gamma-MCA](https://github.com/OpenGammaProject/Gamma-MCA) (OpenGammaProject) | Browser-based PWA (Web Serial/USB), does classical peak search via Gaussian-correlation filtering + isotope-library energy matching. Works offline, no backend needed. Good reference for the classical algorithm. |
| RadiaCode's own app | Already does semi-manual isotope ID (tap-hold on a peak → match against its ~100-isotope library). Useful as a ground-truth baseline to compare our results against. |

## Isotope-ID algorithm options, and where each one can actually run

### 1. Classical peak search + energy-library matching — recommended baseline
Background/continuum subtraction → smoothing → peak search (2nd-derivative or
Gaussian-correlation, à la Gamma-MCA) → compare peak energies against a nuclide line
table → score candidates.

- **Memory**: trivial. Spectrum is 1024 × 4 bytes ≈ 4 KB; a ~100-isotope line table is a
  few KB more.
- **CPU**: trivial — runs fine on ESP32 in real time, or in JS in a browser, or on a phone.
- **Accuracy**: bounded by the 8.4% FWHM as above — fine for strong isolated sources,
  weak for mixtures.
- This is effectively what Gamma-MCA and RadiaCode's own app already do.

### 2. CNN / ML classifier — what Open-RadiaCode-Android uses
- **Not ESP32-feasible.** Practical TFLite-Micro budgets on ESP32-S3 top out around
  200–500 KB of INT8-quantized weights; a 34.5M-parameter model is ~2 orders of
  magnitude too large even after aggressive quantization.
- Needs a phone/PC/cloud backend — which is exactly why Open-RadiaCode-Android calls
  out to a separate GPU microservice instead of running on-device.
- Could in principle squeeze more signal out of the noisy continuum shape than raw
  peak-picking, but is far heavier to build/train/maintain than option 1, and requires
  a labeled RC-110-specific training set to be accurate (a model trained on other
  detectors' spectra won't transfer cleanly given RC-110's specific resolution/response).

### 3. Template/shape correlation (middle ground)
Correlate the whole measured spectrum against precomputed per-isotope response
templates (templates already convolved with the ~8.4% resolution) instead of picking
discrete peaks. More robust to poor resolution than raw peak search, still light
enough for a phone or even ESP32 with a modest template set. Closer to what dedicated
RIID hardware does. Reasonable stretch goal after option 1 works.

## Recommended architecture (revised now that the ESP32-S3 has its own screen)

The earlier assumption was "ESP32 = bridge only, UI = phone/web" because a usable
isotope-ID display needs a real screen. That objection no longer applies — the
ESP32-S3-Touch board *is* a real screen. Revised plan:

- **ESP32-S3-Touch does it all for the baseline case**: BLE-connect to the RC-110 via
  `mkgeiger/RadiaCode`, pull `spectrum()` + `energy_calib()` + dose/count rate, run
  option 1 (classical peak-search + energy-library matching) locally — it's genuinely
  trivial CPU/memory (a few KB, sub-millisecond), no reason to ship it off-device — and
  render the live spectrum + candidate isotopes directly on the NV3041A display via the
  driver stack described above. No phone required for normal use.
- **Phone/web tier becomes optional, not required**: useful for option 3 (template
  correlation, heavier but still light) or option 2 (CNN, needs a backend regardless),
  and for the logging/sharing ideas from the earlier research (Safecast/GMCMap
  upload, Home Assistant, GPS-tagged survey logging) if the user wants that later. The
  ESP32-S3 can still relay over WiFi to any of those when wanted — it's additive, not
  the primary path anymore.
- **Practical build order**: ~~(1) get RC-110 BLE spectrum data flowing~~ (2) ~~decide
  LVGL-vs-hand-rolled and build the live spectrum chart screen~~ (3) ~~add classical
  peak-search + isotope-table matching and a candidate-list screen~~ — (2) and (3) are
  done, on simulated data, in `RadiaCode-Spectrometer/` (see that section below). Went
  hand-rolled, not LVGL. Remaining: (1) actually get RC-110 BLE spectrum data flowing
  (`mkgeiger/RadiaCode`'s BLE stack alongside the display/touch stack, watch for
  BLE+display RAM/stack pressure) — the one piece the UI was built to not depend on;
  (4) optional: WiFi relay/logging tier.

## Open items to verify before building

- ~~Validate `mkgeiger/RadiaCode` against our actual RC-110 firmware version~~ — done,
  works against our real RC-110 (serial `RC-110-000983`) after the two patches
  documented in "BLE integration" above.
- ~~Confirm BLE connection concurrency~~ — circumstantial evidence it's one-BLE-client-
  at-a-time: an early connect attempt failed repeatedly with a low-level GATT error,
  then succeeded right after the user confirmed the official phone app was closed.
  Not a controlled test (address-type patching happened around the same time), so
  treat as likely-but-not-proven. If reconnects start failing again, check whether the
  phone app (or anything else) is holding a BLE connection to the device first.
- Measure actual spectrum-streaming cadence/throughput over BLE — "real-time" update
  rate should be measured on hardware, not assumed.
- Source a nuclide line-energy table properly (e.g. NNDC data, or reuse Gamma-MCA's
  isotope list, which is separately licensed) rather than copying RadiaCode's own
  in-app isotope library, which is their proprietary content.

## Sources

- [mkgeiger/RadiaCode](https://github.com/mkgeiger/RadiaCode) — ESP32 Arduino BLE library
- [cdump/radiacode](https://github.com/cdump/radiacode) — reference Python BLE/USB library
- [darkmatter2222/Open-RadiaCode-Android](https://github.com/darkmatter2222/Open-RadiaCode-Android) — CNN-backed Android isotope ID
- [OpenGammaProject/Gamma-MCA](https://github.com/OpenGammaProject/Gamma-MCA) — browser MCA + classical isotope ID
- [Gamma-MCA RadiaCode-103 spectra discussion](https://github.com/OpenGammaProject/Gamma-MCA/discussions/231)
- [RadiaCode software page](https://www.radiacode.com/software)
- [RadiaCode spectrum isotopes library](https://www.radiacode.com/spectrum-isotopes-library?lang=en)
- [RadiaCode-110 on Hackster.io](https://www.hackster.io/radiacode/radiacode-110-06f4e7) — resolution spec (8.4% FWHM)
- [Radionuclide identification device — Wikipedia](https://en.wikipedia.org/wiki/Radionuclide_identification_device)
