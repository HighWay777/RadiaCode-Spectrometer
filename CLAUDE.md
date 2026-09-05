# RadiaCode110 — Real-Time Spectroscopy / Isotope ID Project

## Vision

RadiaCode-110 (gamma spectrometer) → ESP32-S3 with an integrated 4.3" touch display →
analyzes the live gamma spectrum on-device and shows probable isotopes directly on its
own screen. Optional phone/web tier for heavier analysis or logging, but the ESP32-S3
board is now the primary UI, not just a bridge (see "On-device display hardware" below
— the plan changed once we confirmed this board has a real screen on it).

This file captures research findings and architecture decisions so far.

**This repository contains the firmware only.** The generic display+touch driver base
it was seeded from (`ESP32-S3-Touch`) was removed from the repo on 2026-09-05 and now
lives outside it, at **`../ESP32-S3-Touch-base/`** — it is reference material for
bringing up this board, not part of this build, and it accounted for 2,323 of the
repo's 2,686 tracked files. It also carries an archive of the unrelated project it came
from, including credential files, which is a second reason to keep it out. `.gitignore`
blocks the path so it cannot drift back in. Everything below that describes it is kept
because the hardware notes are still the reference for this board — just read those
paths as `../ESP32-S3-Touch-base/`.
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

## On-device display hardware (`../ESP32-S3-Touch-base/`)

`../ESP32-S3-Touch-base/` is a real PlatformIO project the user already had for unrelated
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
`../ESP32-S3-Touch-base/README.md`. **This patch is load-bearing — don't pull in an unpatched
Arduino_GFX copy.**

Pin map:
```
QSPI display: CS=45  SCK=47  D0(MOSI)=21  D1(MISO)=48  D2(WP)=40  D3(HOLD)=39  RST=17
Backlight:    GPIO1, LEDC PWM (channel 0, 12-bit, 5kHz)
GT911 touch:  SDA=8  SCL=4  INT=3  RST=38  (I2C, 400kHz)
```

**Touch**: GT911 over I2C via `Libraries/Touch_GT911/` (a different, unused GT911
library — `Gt911-arduino-main`/`TAMC_GT911` — was archived; don't reintroduce both).

**What's in `../ESP32-S3-Touch-base/` now** (post-cleanup):
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

**What got archived** (moved, not deleted, to `../ESP32-S3-Touch-base/_archive_previous_project/`
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

`RadiaCode-Spectrometer/` is the real firmware project (seeded from the ESP32-S3-Touch
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
- `src/data/isotope_db.h` — 21 nuclides with published (NNDC/ENSDF-style) line
  energies and **absolute** intensities, plus a `category`
  (natural/artificial/medical/calibration) and `chain` (U-238/Th-232/none) per entry.
  **Rewritten 2026-08-22 after the uranium-ore field test** (below): the original table
  listed "U-238 chain" and "Th-232 chain" as single composite pseudo-isotopes, which is
  physically wrong. U-238 and Th-232 are effectively gamma-silent — you never see the
  parent. Real samples are identified through their daughters, so the table now carries
  the actual chain members: **Bi-214, Pb-214, Ra-226, Th-234, Pa-234m, U-235** (U-238
  chain) and **Tl-208, Ac-228, Pb-212, Bi-212** (Th-232 chain). Their absence was the
  direct cause of real ore being reported as Ba-133/I-131/Eu-152.
- `src/analysis/peak_search.*` — the classical algorithm from "Isotope-ID algorithm
  options" option 1. Rewritten 2026-08-22; the pipeline is now:
  resolution-aware boxcar smoothing → **side-band** continuum estimate → local maxima
  of the *continuum-subtracted* spectrum → area-based significance
  (`netArea / √bkgArea`) → parabolic sub-channel centroid → energy conversion → match
  against `isotope_db` within a ±0.75·FWHM window → per-nuclide scoring → decay-chain
  aggregation → plain-language `SourceClass` → counting-statistics quality/time advice.
  All window sizes (smoothing, continuum side-bands, local-max, integration) scale with
  the local FWHM in channels, because a peak is ~9 channels wide at 100 keV but ~46 at
  2614 keV — the original fixed ±3/±20 channel windows were badly wrong at both ends.
  Two gates suppress false IDs: the nuclide's **strongest** line must be matched, and a
  multi-line nuclide needs **≥2 matched lines** for corroboration.
  `lastPeaks()`/`lastPeakCount()` expose the peak list so the spectrum chart draws
  exactly what the identification used instead of recomputing it.
- `src/ui/cards.*` — the small drawing toolkit every screen uses: rounded-rect card
  frames (with a highlighted-border state for "this is the current top match"), a
  partial-redraw-safe big-number stat area, buttons, progress bars, text helpers.
- `src/ui/ui_theme.*` — dark theme (`APP_THEME`) with accent/warning/danger/highlight
  colors; `currentTheme` also feeds the inherited `touch_feedback`/`page_transitions`
  modules from the base.
- `src/ui/screen_home.*`, `screen_isotopes.*`, `screen_spectrum.*`,
  `screen_analysis.*`, `screen_isotope_detail.*` — the five screens (below). Each
  exposes `draw()` (full redraw, called on screen entry) and `update()` (partial
  redraw of just the live values, called on a 500ms timer). The cycle screens
  **diff-cache** what they last drew and skip the redraw when nothing changed (added
  2026-08-04 — the original unconditional redraw-every-tick was visibly flashy on the
  QSPI panel).
- `src/main.cpp` — screen state machine (`SCR_HOME`/`SCR_ISOTOPES`/`SCR_SPECTRUM`/
  `SCR_ANALYSIS` cycle via swipe, plus `SCR_ISOTOPE_DETAIL` as a tap-only drill-down
  outside that cycle), tap routing, and the `BleSource::tick()` / periodic `update()`
  calls.

**Screen 1 — Home** (`screen_home.*`): header with title + connection status
dot/label. Four cards: **Dose Rate** (µSv/h, color-coded accent→warning→danger as it
rises), **Count Rate** (**CPS**, switched from CPM 2026-08-22 — CPS is what the official
app shows, so the two are directly comparable, and CPM produced needlessly huge numbers
on this very sensitive detector), **Likely Source** (the sample-level `SourceClass`
verdict such as "URANIUM ORE", *not* a single daughter nuclide — for a natural sample
the individual nuclides are evidence, not the answer), **RC-110 Status** (battery %,
`LIVE HH:MM` session timer). Full-width **Connect/Disconnect** button at the bottom.

**Screen 2 — Isotope Candidates** (`screen_isotopes.*`): 2×3 grid of candidate cards
(symbol, confidence % + bar, name, matched energy, matched/total line count, and an
origin tag — "U chain"/"Th chain"/"lab source"/"medical"/"artificial" — so an
implausible nuclide is obvious at a glance). Top match gets the gold highlighted
border. **Tapping a populated card** opens Screen 5.

**Screen 3 — Live Spectrum** (`screen_spectrum.*`): 1024 channels rebinned into a
464px bar chart, x-axis labelled in keV from the live calibration, gold triangle
markers on the peaks the identification actually used, and energy labels on the three
strongest.

  **Shows the net spectrum when a background reference is active** (added 2026-08-22).
  Until then the chart always plotted `g_data.spectrum` — the *raw* counts — while
  subtraction happened on internal buffers inside `peak_search.cpp`. The chart
  therefore looked identical whether subtraction was on or off, and the whole feature
  appeared to do nothing; that was the user's "I don't see the benefit" report, and it
  was a real gap rather than a misunderstanding. `correctedSpectrum()` /
  `backgroundCounts()` / `backgroundApplied()` now expose the analysis buffers, the
  header reads **Spectrum - NET** vs **Spectrum - RAW**, the background reference is
  drawn as a dim grey trace behind the net bars for direct comparison, and the footer
  names the reference in use. Net columns carry two series, so a changed column is
  repainted whole instead of delta-patched — still per-column, so no plot-wide flicker. **Defaults to a logarithmic y-axis** (added 2026-08-22) with decade
gridlines — on uranium ore the low-energy region is ~100× the 1764 keV Bi-214 line, so
on a linear axis the diagnostic high-energy peaks are invisible. **Tap toggles
log/linear.**

  *Chart rendering is deliberately incremental* (2026-08-22, fixing visible flicker):
  clearing and repainting the whole 464×150 plot every 500 ms was the flicker source,
  so each column caches its last height and only the changed segment is drawn — grown
  bars paint just the new tip, shrunk bars clear just the vacated part. That only works
  if the y-scale is stable, so the scale snaps to whole decades (log) or 1/2/5·10ⁿ
  (linear) and only moves when the data genuinely outgrows it; a live-max rescale would
  change every column every tick and force a full repaint anyway. Peak markers/labels
  live in their own strip above the plot so they never repaint the chart, and are
  redrawn only when the peak set changes. Same lesson as the card screens: on this QSPI
  panel, *never* clear-then-redraw an area on a timer.

**Screen 4 — Analysis & Quality** (`screen_analysis.*`, new 2026-08-22): the "can I
trust this, and how long do I need to wait" screen. Four cards: **Sample Verdict**
(`SourceClass` label + detail), **Confidence** (quality grade + σ + progress toward the
12σ target), **Chain Evidence** (U-238 and Th-232 chains with members-detected counts
and confidence bars), and **Acquisition** — elapsed vs recommended time, a progress
bar, total counts/peaks, and a plain instruction: *"Keep measuring ~4m 30s"* or
*"Measurement complete"*. The time estimate is a real extrapolation, not a guess:
peak significance grows as √t, so `t_needed = t_elapsed · (12 / σ_current)²`, capped
at 2 h.

**Screen 5 — Isotope Detail** (`screen_isotope_detail.*`): reached only by tapping a
candidate card on Screen 2. Snapshots the tapped `IsotopeCandidate` at tap time (so it
stays stable if the live ranking reorders), and shows symbol/name, **origin context**
("Natural - U-238 chain", "Lab calibration source", "Medical tracer - short lived"),
confidence/lines-matched/σ, and every known gamma line in two columns with the matched
one in gold. BACK button returns to Screen 2.

Navigation: swipe left/right cycles Home → Isotopes → Spectrum → Analysis → Home
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
   the display base and the original copy in `RadiaCode-Spectrometer/` had a leftover
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

## Uranium-ore field test (2026-08-22) — the most informative session so far

Measured a rock from an abandoned uranium mine: **27.67 µSv/h, ~2,308 CPS**
(~150× background). Cross-check: the RC-110's published sensitivity is ~77 CPS per
µSv/h for Cs-137, so 2308/77 ≈ 30 µSv/h vs the 27.67 displayed — the two independent
readings agree within ~8%, confirming both.

The UI reported **Ba-133 100%, I-131 83%, Ra-226 65%, Eu-152 43%** — i.e. two lab
calibration sources and a medical tracer on a rock, with no uranium. All four
confidences were reproducible by hand from the code, which pinned down three distinct
root causes:

1. **Peak list truncated by scan order, not significance.** `findPeaks()` scanned
   channels upward and stopped at `MAX_DETECTED_PEAKS`. On an ore spectrum the dense
   low-energy forest consumed all 12 slots below ~650 keV, so Bi-214's 1120/1764 keV,
   Pa-234m's 1001 keV, K-40's 1460 keV and Tl-208's 2614 keV were **never examined**.
   This alone explains why Ra-226 capped at 65% (its 1120/1764 lines unseen) and
   Eu-152 at 43% (1408 unseen). Fixed: collect all candidates, rank by significance,
   keep the top 24.
2. **Peaks located on the raw spectrum instead of the continuum-subtracted one**, with
   a centred boxcar continuum that averaged each peak into its own background. On a
   steeply falling spectrum this biases the apparent maximum low — measured energies
   were reading several keV under the true lines (344 vs Pb-214's 351.9; 184 vs
   Ra-226's 186.2). Fixed: side-band continuum + local maxima on the net spectrum +
   parabolic sub-channel centroid.
3. **The decay-chain daughters were missing from the isotope table** (root cause of the
   misidentification). One real Pb-214 peak at 351.9 keV was being claimed by Ba-133
   (356), Eu-152 (344.3) and I-131 (364.5) — all within one FWHM at this resolution.
   With Pb-214 and Bi-214 in the table, the real nuclides win that peak.

**Key physics to remember:** U-238 emits essentially no detectable gammas. Uranium is
identified through daughters in secular equilibrium — chiefly **Bi-214 (609.3, 1120.3,
1764.5 keV)** and **Pb-214 (295.2, 351.9 keV)**, with Ra-226's 186.2 keV blended
inseparably with U-235's 185.7 keV. The 609 keV Bi-214 line is the single most
diagnostic uranium marker at 8.4% FWHM. Similarly, thorium is identified via
**Tl-208 (2614.5 keV)** and **Ac-228 (911, 969 keV)**.

**Not yet re-verified against the uranium rock** — the rebuilt detection is flashed but
the ore sample has not been re-measured since. (It *was* verified against a thorium
source, below.)

## Thorium-mantle field test (2026-08-22) — four more real bugs

Measured a Petromax lamp mantle (thorium dioxide). Initially reported "NATURAL MIX",
i.e. thorium *plus* uranium, which is wrong — mantles are thorium. Serial peak logging
(`PEAK_SEARCH_DEBUG` in `peak_search.cpp`) made each cause visible:

1. **One peak was counted as evidence for every nuclide with a line nearby.** At the
   then-current ±0.75 FWHM window the two natural chains overlap badly: Ac-228's real
   338.3 keV peak satisfied Pb-214's *strongest* line (351.9, 13.6 keV away) and
   Tl-208's real 583.2 keV peak satisfied Bi-214's *strongest* line (609.3, 26.1 keV
   away). Pure thorium therefore scored a fully corroborated uranium chain. **Fixed
   with competitive assignment**: `computeBestClaims()` records, for each peak, the
   closest line of any nuclide; a nuclide only gets credit if its line is within
   `SHARE_MARGIN_FWHM` of that best claim. Tolerance also tightened to 0.6 FWHM.
2. **"Requires ≥2 matched lines" rejected nuclides whose signature really is one
   line.** Pb-212 — the clearest thorium indicator, 238.6 keV at 43.6% and the
   strongest peak in the whole spectrum — was discarded because its only other line
   (300 keV) is 13× weaker and will essentially never appear. Fixed by requiring
   corroboration only from lines that are *plausibly detectable*.
3. **Gating on emission intensity instead of detectability.** Tl-208's strongest
   emission is 2614 keV (99.8%), but a small CsI crystal detects it ~5× less
   efficiently than its 583 keV line (85%), so the 2614 keV peak was absent while 583
   was obvious — and the "strongest line must match" gate threw Tl-208 away. Fixed
   with `relEfficiency()`: lines are now ranked by **emission × detector efficiency**,
   so a nuclide's "defining" line is the one you can actually expect to see.
4. **Flat-topped peaks reported once per channel** (duplicate 1543/1544 keV entries)
   because the local-maximum test used `>` with no tie-break. Fixed.

**Result on hardware**: Pb-212 detected at 5 s, THORIUM verdict at 10 s with 3 of 4
chain members, uranium chain steady at 0%. Before these fixes the same sample took far
longer and reported the wrong answer.

**Also added — ROI (region-of-interest) testing for weak sources** (`testRoi()`):
blind peak-finding needs a line to rise into a clean local maximum, which sources near
background never manage. Since the library says exactly where each line should be, a
line that fails peak-matching is now tested directly at its own energy: integrate the
window, subtract the fitted continuum, and accept a statistically significant excess
even with no visible peak. Guarded by a centroid check so the window can't simply catch
the flank of a neighbouring strong peak, and by a higher σ bar for a nuclide's defining
line (4σ) than for corroborating lines (2.5σ).

**Known limitation — measured peak energies still read low**, e.g. the Pb-212 line
centroids around 233 keV against a true 238.6. Partly residual continuum slope, but
partly real physics: at 8.4% FWHM the Ac-228 209 keV line is only ~30 keV from Pb-212's
238.6 keV with a ~33 keV FWHM, so they genuinely blend into one asymmetric bump whose
centroid sits between them. The matching tolerance absorbs this; don't read the
displayed peak energies as precise line energies.

## Counting statistics correction (2026-08-22)

Peak significance was `netArea / √backgroundArea`, which ignores both the signal's own
Poisson noise and the uncertainty of the background estimate. Correct form is
`net / √(sample + background estimate variance + continuum)` — implemented as
`significanceOver()` and shared by both the peak search and the ROI test so the two
kinds of evidence are on the same scale.

The old formula **overstated weak peaks by ~1.5–2×**, exactly where it matters. Re-scored
against the thorium-mantle log: Pb-212 238 keV 34σ → **17σ**, Tl-208 583 keV 7.8σ →
**4.7σ**, Ac-228 338 keV 2.8σ → **1.9σ** (i.e. that Ac-228 line was genuinely marginal
while being displayed as solid). Peak-listing threshold lowered 2.5 → 2.0 to keep
similar sensitivity now that the reported numbers are honest. Quality grades and the
recommended-time advisor now read pessimistically compared to before — that is the
correction, not a regression.

Also added **Gate 3: `MIN_CANDIDATE_SIGMA` (3σ)**. On plain background a 2.0σ noise bump
at 110 keV fell within tolerance of Co-57's 122 keV line and produced an "89% Co-57"
candidate from nothing. The `SourceClass` guard caught it (verdict stayed
"MEASURING…"), but no nuclide should be *listed* on sub-3σ evidence.

## Background subtraction (2026-08-22)

`src/data/background.*` + Screen 5 (`screen_background.*`). Ambient background is not
noise — it is real signal that is not your sample: room air carries radon daughters
(Bi-214/Pb-214, genuine U-238 chain members) and building materials carry K-40. Near
background level no amount of extra time separates a faint sample from those, because
both grow at the same rate. A stored reference is the only way to do trace work.

- Stored in **NVS** as counts/second per channel, `uint16` milli-counts/s → a 2 KB blob
  that fits comfortably in the default 20 KB `nvs` partition, and scales to any later
  acquisition time. Enabled flag and reference duration persist alongside it.
- `record()` refuses acquisitions shorter than `MIN_RECORD_SECONDS` (30 s) — a noisy
  reference would inject its own noise into every subsequent measurement.
- Subtraction happens in `prepareSpectra()` before smoothing, so the continuum, peak
  search and ROI tests all operate on the sample's own excess.
- **Variance is propagated, not ignored**: `bgVarScale = acquisition / reference
  duration`, so a reference measured far longer than the sample contributes almost no
  uncertainty while a short one is correctly penalised. Removing counts also removes
  certainty, and pretending otherwise would inflate apparent significance.
- The Analysis screen shows a `BKG SUBTRACTED` tag in its header, because a verdict
  computed against a subtracted background means something different from a raw one.

**Background flow, second rework (2026-08-22).** Importing a file wrote
`/background/<SLOT>.csv` but left the *active* reference untouched — serial confirmed
`/background/HOME.csv` present with `[BKG] stored=0 enabled=0`. Activating it needed two
further, undiscoverable steps on another screen. Three changes closed that:

- **Import completes the job**: it now loads the imported slot as the active reference
  and turns subtraction on, reporting `ACTIVE: HOME - 24h22m, 24.9 cps, subtraction ON`.
- **The active reference has a name** (`Background::label()`, persisted in NVS). The
  headline reads `USING: HOME` rather than a bare "SUBTRACTION ACTIVE" — knowing
  subtraction is on is useless without knowing *what* is being subtracted.
- **The library marks the live slot** with a gold border and an `IN USE` / `LOADED` tag,
  and is reachable from the Background screen (previously only from Data, which nobody
  would guess). BACK returns to whichever screen opened it.

**Screen 5 UI is a recording state machine, not a snapshot button** (redesigned
2026-08-22 after the first attempt tested badly). The original exposed "RECORD BKG",
which snapshotted whatever had already accumulated — but the label reads as *start
recording*, so pressing it produced no visible change and the page was rightly called
useless. It now models an explicit session with three states, and the controls always
reflect what the device is doing:

| State | Panel headline | Primary button | Secondary buttons |
|---|---|---|---|
| No reference | NO REFERENCE STORED | START RECORDING | Subtract (disabled), Erase (disabled) |
| Recording | RECORDING BACKGROUND + `mm:ss / 05:00` + progress bar + live counts | `WAIT 12s…` (disabled) → **STOP & SAVE** | CANCEL |
| Reference held | SUBTRACTION ACTIVE / REFERENCE STORED (OFF) | START RECORDING | SUBTRACT: ON/OFF, ERASE REF |

START clears the device spectrum (`BleSource::resetSpectrum()`) and begins a fresh
acquisition; STOP & SAVE stays disabled with a live countdown until
`MIN_RECORD_SECONDS` has elapsed, so the "too short" error is prevented rather than
reported. Recording auto-cancels if the BLE link drops. A header chip
(RECORDING / SUBTRACTING / REF STORED / NO REF) shows state from any glance.

When a reference is held, the panel headline is the **live excess over background**
(`+2.8 cps over background`), colour-coded by whether the sample is actually above
background — that single number is what answers "is there anything here?" for a weak
specimen, and it belongs in the largest text on the page rather than buried in a
detail line.

## ⚠️ TouchFeedback (tap ripple) is disabled — do not re-enable

`src/ui/touch_feedback.*` is inherited from the board base and is **switched off** in
`main.cpp` (`setEnabled(false)`, never `init()`ed, never `update()`d, never
`trigger()`ed). Its ripple erases itself by painting `currentTheme->background` over
the affected area. That is fine on an empty screen, but on any card, button or chart it
punches a **permanent dark blob** that never repairs itself — seen on hardware as a
black circle stuck on the RECORD button, then on every screen once taps were routed
more widely. Any redraw-on-a-timer would eventually clear it, but the diff-caching
everywhere means nothing repaints unless its value changed, so the blob persists.

Controls provide their own feedback instead: `Cards::button()` takes `enabled` (dims
the label) and `pressed` (darkens the fill, gold double border), and
`ScreenBackground::actOn()` paints the pressed state, waits ~110 ms so it is visible,
performs the action, then re-renders only what changed. If a tap ripple is ever wanted
again it must repaint what it covered, not assume a flat background.

## microSD storage (2026-08-22)

`src/data/sd_storage.*` + Screen 6 (`screen_data.*`). **Confirmed working on hardware:
`SDHC 61120 MB (CS=10)`.**

**Pinout — determined empirically, not documented anywhere findable.**
`SCK=12, MISO=13, MOSI=11`, and `begin()` probes a candidate CS list; GPIO10 answered
first. This works because ours is the **capacitive-touch (GT911, I2C)** variant — the
*resistive* variant of this board puts an XPT2046 on SPI 11/12/13, which is where the
"SD access breaks after a display flush" warning on atomic14's board page comes from.
Our display is on its own QSPI bus (45/47/21/48/40/39), so there is no shared-bus
hazard. `begin()` explicitly refuses to probe any display/backlight pin.

**Deliberately optional.** The live background reference stays in NVS, *not* on SD: it
is only 2 KB and subtraction must not break because a card was removed. SD carries
things NVS cannot — currently CSV spectrum export (`/spectra/spec_NNNN.csv`,
auto-numbered since there is no RTC) with calibration coefficients in the header so the
energy axis can be rebuilt offline in Gamma-MCA, plus archival copies of the background
reference. `loadBackgroundRef()` is stubbed — reading a reference back needs a
`Background::` setter that takes raw rates, and saving is the useful half today.

## Background recording length — what actually helps

Recording was never capped; the old 5-minute progress bar just made it *look* finished.
The panel now shows elapsed as `h:mm:ss` and, more usefully, **what sample length the
reference supports**: a reference contributes negligible extra uncertainty once it is
roughly 10× the sample run (`bgVarScale = T_sample / T_bg`), so a 10-minute reference
is reported as "good for 1m samples".

**Longer is not indefinitely better.** Statistically the gain is ~1/√t, so past ~10× the
intended sample length there is almost nothing left to win. Against that, indoor radon
concentration genuinely swings by factors of several over hours with ventilation and
weather — so a 24–48 h average can match *current* room conditions worse than a fresh
30-minute reference. Practical sweet spot: **10–60 minutes, re-recorded when the room
or detector position changes.** Multi-hour is fine in a stable space but is not the
upgrade it looks like.

## Named background references (2026-08-22)

`Background::setFromRates()` + `SdStorage::saveBackgroundRef/loadBackgroundRef/refInfo`
+ `screen_bkg_library.*`. Six fixed slots — HOME, CAR, BASEMENT, GARDEN, FIELD, SPARE —
stored as `/background/<SLOT>.csv` (counts/s per channel, duration and totals in header
comments). **Fixed names deliberately**: there is no keyboard on this device, and a
handful of location presets covers the real use case without inventing a text-entry UI.
Reached by tapping LIBRARY on the Data screen; a MODE button flips between SAVE and
LOAD, then a slot tap acts. Loading installs the reference into NVS as the active one,
so it survives with the card removed.

## Importing backgrounds from the official RadiaCode software

Two routes, both producing `/background/<SLOT>.csv`:

**On-device (preferred)** — `src/data/rc_import.*` + Screen `screen_import.*`.
Drop RadiaCode `.xml` exports into `/import` (or the card root), then
Data → BKG LIBRARY → **IMPORT XML**. The screen lists what it found, a `TO: <SLOT>`
button picks the destination, and tapping a file converts it. Afterwards load it
normally from the library (MODE: LOAD).

An initial instinct was to keep XML parsing off the ESP32 as "heavy and brittle" —
that was overcautious. A full DOM parser would be, but the export has a known shape and
only three tags matter, so `rc_import.cpp` is a **streaming tag scanner**: read through
a 512-byte buffer, capture text after `<MeasurementTime>`, `<Coefficient>` and
`<DataPoint>`, ignore everything else. A few hundred bytes of state, no allocation
beyond a temporary 4 KB counts array, and it stops at the first `</Spectrum>` so a
paired `BackgroundEnergySpectrum` block is ignored.

**On a PC** — `tools/radiacode_to_background.py` does the same conversion in bulk and
stays useful for batches.

Both apply the same two guards:

- **Channel count must be 1024**; anything else is refused rather than rebinned,
  because silently resampling would shift the energy mapping.
- **Calibration is compared against the device's** `a1` (which dominates the mapping)
  and flagged if it differs by more than ~1%. A reference recorded under a different
  calibration maps channels to different energies, so subtracting it removes counts
  from the wrong places — the main risk when importing from another unit.

⚠️ **`SpectrometerData::calibValid`** exists because of this feature. Before the RC-110
is connected, `calibA1` is a placeholder (3.0), so the mismatch check fired on *every*
import — the on-device self-test caught it reporting `mismatch=1` for a file whose
calibration was identical. The comparison now runs only once `energyCalib()` has really
been read, and the UI says "calibration unchecked (not connected)" rather than
implying it verified something.

Verified on hardware with a synthetic export written to the card at boot
(`RC_IMPORT_SELFTEST` in `main.cpp`, left in place but set to 0): parsed 1024 channels,
600 s, 3067 counts, `a1=2.3754`, `mismatch=0`. Set it to 1 to re-run after touching the
scanner.

## Runtime-loadable nuclide library (2026-08-22)

The isotope table is no longer a compile-time constant. Split into:
- `isotope_db.h` — types only (`IsotopeLine`, `IsotopeDef`, category/chain enums)
- `isotope_data.h` — the ~21 built-in nuclides, included **only** by the library module
- `isotope_library.*` — the active table, `count()`/`at(i)`, and the CSV loader

At boot, `/isotopes.csv` on the card supersedes the built-ins; if absent, a template of
the built-in table is written to `/isotopes_template.csv` so the card is immediately
editable. Bounded at `ISOTOPE_LIBRARY_MAX` (300) nuclides / 2400 lines / 24 KB of
strings, allocated via `ps_malloc` with a heap fallback (~50 KB worst case, which fits
even if PSRAM turns out unusable on this board).

CSV format is one row per gamma line, rows for a nuclide grouped together:
`symbol,name,category,chain,energy_keV,intensity` where category is
`natural|artificial|medical|calibration`, chain is `U238|Th232|none`, and intensity is
**absolute** emission probability per decay (0..1), not relative.

**Verified by round-trip on hardware**: the firmware parses back the template it just
wrote and reports `21 nuclides / 60 lines, 0 rows skipped`, matching the built-in table
exactly. That self-check is left in place (it only runs when `/isotopes.csv` is absent)
as a regression guard — if writer and reader ever drift apart, the boot log says so
instead of a user-supplied library silently misbehaving.

Consumers (`peak_search.cpp`, `screen_isotopes.cpp`, `screen_isotope_detail.cpp`) now
iterate `IsotopeLibrary::count()`/`at(i)`. **Loop counters must be `uint16_t`** — with a
300-nuclide ceiling the old `uint8_t` would wrap — and per-nuclide working arrays are
sized `ISOTOPE_LIBRARY_MAX`, not the live count.

⚠️ **Editing these files from PowerShell**: a `Get-Content`/`Set-Content` round-trip
mangled the UTF-8 em-dashes in comments into mojibake (read as ANSI, written as UTF-8).
Source is now pure ASCII. Prefer the Edit tool, or read/write with an explicit
`UTF8Encoding` if scripting a bulk change.

## Continuous acquisition / auto-reconnect (2026-08-22)

**The key fact: the RC-110 accumulates its spectrum internally whether or not BLE is
connected.** The original code called `device->spectrumReset()` on *every* connect, so
a dropped link silently destroyed a long run and restarted from zero. It was our reset
doing the damage, not the dropout.

New rule: **no connect path ever resets.** `connectInternal(freshStart)` is only called
with `freshStart=true` from nowhere at present — manual connect, auto-reconnect and
connect-at-startup all resume. Data is discarded solely by an explicit RESET
(`BleSource::resetSpectrum()`, the two-tap RESET button on Home). This means an
acquisition survives a BLE dropout, a deliberate disconnect, *and* an ESP32 reboot,
because the counts live on the RC-110 the whole time.

- `requestDisconnect()` no longer clears the spectrum either — the run is paused and
  still on screen, not thrown away.
- **Link-loss detection**: `radiacodeBleLinkUp()` (patched into `BluetoothTransport`)
  exposes `BLEClient::isConnected()`. `tick()` notices a drop within one tick instead
  of every poll silently returning nothing.
- **Retry with backoff**: `userWantsConnection` stays true from an explicit CONNECT
  until an explicit DISCONNECT, so drops are retried (4 s, backing off to 30 s) while a
  deliberate disconnect is respected. Home shows `RECONNECTING...`.

⚠️ **Two more `BluetoothTransport` patches were required** for this to be usable:
1. `execute()` now returns immediately if `isConnected()` is false. Without it, every
   poll on a dead link waited the full response timeout.
2. That timeout was **30 s** — one stalled poll froze the entire UI. Cut to 4 s; real
   responses arrive in well under a second.

`Settings` (NVS, `src/data/settings.*`) holds **auto-reconnect** (default on),
**connect-at-startup** (default off) and display brightness, edited on the Settings
screen reached from the `SET` button on Home.

**SD autosave**: `SdStorage::autosaveSpectrum()` writes `/spectra/_autosave.csv` every
5 minutes while connected. Since the RC-110 already covers link drops and ESP32
reboots, this exists for the one case that genuinely loses data — the RC-110 itself
being switched off — and as a record of a long run.

## Threading: BLE on core 0, UI on core 1 (2026-08-22)

Adding auto-reconnect made the UI visibly sluggish, and the cause was
architectural rather than incidental: **all BLE I/O ran inside `loop()`**. A retry
performs a 5-second scan, and each spectrum poll chunks 4 KB through 18-byte BLE writes
with `delay(5)` between them plus a `delay(50)` response wait — hundreds of ms every
5 s, and 5 whole seconds per reconnect attempt, all blocking the display.

`BleSource::begin()` now starts a worker pinned to **core 0** (12 KB stack — the
Bluedroid call chain is deep and `analyzeSpectrum()` runs there too, though its large
buffers are file-scope statics rather than stack). `loop()` on core 1 drives only
display and touch.

- Every public entry point (`requestConnect`, `requestDisconnect`, `resetSpectrum`) is
  now a **non-blocking flag** consumed by the task; `requestConnect()` sets
  `CONN_CONNECTING` itself so the UI reacts instantly.
- `BleSource::tick()` is retained but empty, so `loop()` keeps its original shape.
- **`dataLock()`/`dataUnlock()` + `DataGuard`** (in `spectrometer_data.*`, a recursive
  mutex) guard the bulk arrays. The BLE task holds it while copying the spectrum and
  analysing; `ScreenSpectrum::rebin()` holds it while snapshotting for the chart.
  Anything else that walks `g_data.spectrum` or the analysis buffers must too.

## Working without an SD card

Nothing about measuring depends on the card: the spectrum arrives over BLE and the
active background reference lives in NVS. `SdStorage::pollPresence()` (called from
`loop()`, rate-limited to 3 s) re-validates via `SD.cardType()` so a card **pulled after
boot** is noticed — otherwise writes would fail silently against a cached "mounted"
flag — and remounts automatically when one is inserted, retrying only the CS pin that
answered before.

## The self-reconnect ~40 s after power-on (2026-09-05)

Reported twice, and twice answered with a fix that addressed something else.
The earlier WiFi/BLE work stopped the two radios from *corrupting* each other;
it never removed the disconnect, because the disconnect is deliberate. Root
cause:

- `Updater::begin()` scheduled the automatic update check at **boot + 45 s**.
- Any WiFi work calls `BleSource::suspendForWifi()`, which drops the link and
  runs `BLEDevice::deinit(false)` - the two radios cannot both hold their
  buffers on this board.
- So the check reliably evicted an *established* link and the device re-scanned
  and reconnected, roughly 30-40 s after the user saw it connect.

It is conditional, which is why it does not always reproduce: the check is only
scheduled when **auto-update is on AND WiFi credentials are saved**, and it
fires **once per boot**. Out of WiFi range, or watching for under 45 s, and
nothing happens. The user observed exactly this ("I just powered it on, but it
does not show the same issue").

**Fix - do the check before anything is connected, not after.** There is then
no link to evict:
- `STARTUP_CHECK_DELAY_MS` 45000 -> **1500**, and `Updater::begin()` calls
  `suspendForWifi()` itself. That only sets flags, so it is safe to call before
  the BLE task exists; the task acknowledges on its first iteration with
  nothing to tear down.
- `Updater::begin()` moved **ahead of** `BleSource::begin()` in `setup()`.
  Started after it, the check could only ever interrupt a link already coming
  up - the ordering is the fix, not the delay value.
- **Watchdog** in the BLE task: a suspension held over `SUSPEND_WATCHDOG_MS`
  (120 s) while `Updater::busy()` is false is taken back, with a log line.
  Suspension is now raised at boot before any WiFi work, so a job that never
  reaches `releaseWifi()` would otherwise leave the detector permanently
  disconnected with nothing on screen explaining why. Gated on `busy()` so a
  firmware download, which takes minutes, is never cut off.

Audited the whole class rather than the instance: the only remaining callers of
`ensureWifi()` are the three Settings buttons (test credentials, check,
install). **Nothing automatic can drop the link after boot any more.**

### Verified on hardware

Boot log, twice:

```
 3.2  [OTA] Auto-update enabled, checking before the detector connects
 3.2  [BLE] Suspending for WiFi (heap 187592)     <- nothing connected yet
 5.0  [OTA] WiFi connected, IP ...
 6.0  [OTA] Up to date (1.0.6)
 6.3  [BLE] Resuming after WiFi
 6.4  [BLE] Reconnect attempt 1...                <- first and only connect
14.7  [BLE] Connected: RC-110-000983
15.5  [WIN] resumed accumulation: 804s, 20662 counts
```

Then **10 minutes with a thorium source**: one connect, zero `[BLE] Link lost`,
zero further scans, zero `[WIN]` rebases after the initial adopt.

⚠️ The log still says `Reconnect attempt 1` for what is now the *first*
connect. Cosmetic only, but misleading when reading a boot capture.

### Identification re-verified at the same time

Checked before changing anything, because "identification is broken" was the
other half of the report. It is not: replaying the three real captures through
`host/` (which compiles the firmware's own analysis sources) gives
**THORIUM 120 s, URANIUM 300 s, BACKGROUND stable**, all monotonic, plus
URANIUM 300 s after presentation onto a 2 h accumulation. No regression.

Live confirmation, thorium source already on the detector and diluted into
804 s of adopted accumulation:

```
 18.3 [ID] U 0.6 sigma (0.7% amb 0.5% -)  Th 4.3 sigma (4.5% amb 2.0% ELEV) -> MEASURING...
188.9 [ID] U 0.7 sigma (0.7% amb 0.5% -)  Th 5.0 sigma (4.9% amb 2.1% ELEV) -> THORIUM
616.3 [ID] U 0.6 sigma (0.5% amb 0.5% -)  Th 6.0 sigma (4.8% amb 2.1% ELEV) -> THORIUM
```

**86 consecutive THORIUM verdicts, no flicker.** The uranium chain sat on its
learned floor (0.5-0.7% against 0.5%) and was never elevated, so the
cross-talk that used to produce NATURAL MIX stayed suppressed for the whole
run. The ambient floor rose 2.0 -> 2.1% during MEASURING and then stopped dead
once THORIUM was asserted - the asymmetric-learning block, observed working.

The known `Buffer overflow prevented in readFloat` from the vendored library
appeared once in 10 minutes; the poll was skipped and the verdict did not
change across it, as previously assumed but not until now actually observed.

**Still cosmetic, unchanged:** `Eu-152` lists at 58% on pure background and 73%
on the real ore. It is the 344.3/351.9 keV degeneracy documented above, the
verdict is unaffected, and per the 2026-09-02 measurement a likelihood-ratio
test would confirm it rather than remove it. Do not chase it with a threshold.

## Ideas not yet built
- **User-facing energy calibration.** Not needed today: the RC-110 reports its own
  `a0/a1/a2` and they check out. Would only matter for deliberately re-calibrating
  against a known source; risky to expose since a bad edit silently breaks every ID.
- Escape peaks (single/double at 2103/1592 keV from Tl-208's 2614 keV) are not modelled
  and can masquerade as real lines.
- Template/shape correlation (option 3 above) remains the more robust approach at this
  resolution than discrete peak-picking.

## Known issues found on hardware testing (2026-08-04)
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

## OTA updates — the install failure and its cause (2026-08-30)

Checking for updates worked; installing always failed. Root cause was ordering,
not networking: `Update.begin()` (`esp_ota_begin()`) **erases the entire ~1.6 MB
OTA slot up front**, which takes on the order of ten seconds. The original
`doInstall()` opened the HTTPS connection first and erased second, so the TLS
socket sat idle through the whole erase and was dropped; the read loop then saw
no data, hit its stall timeout, and reported "Update aborted at 0%".

Fix in `src/data/updater.cpp`: **erase before connecting.** Alongside it —
`setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` (release assets are commonly
served via a redirect), the read loop now runs while `http.connected()` rather
than only against a known `Content-Length` so a chunked response still
completes, and each failure names its own stage with byte counts and free heap
instead of one generic message. Note `WiFiClientSecure` on this core has **no
`setBufferSizes()`** — the mbedTLS buffers are fixed.

Verified end to end on hardware: 1,654,368 bytes downloaded (heap 125 KB before,
78 KB during), installed, rebooted into the new version, then reported
"Up to date". `OTA_SELFTEST` in `updater.cpp` (default 0, same pattern as
`RC_IMPORT_SELFTEST`) drives check-and-install from boot so the path can be
re-tested without anyone tapping the screen. `FIRMWARE_VERSION` is now
`#ifndef`-guarded so a bench build can claim to be older:
`PLATFORMIO_BUILD_FLAGS='-DOTA_SELFTEST=1 -DFIRMWARE_VERSION=\"1.0.0\"'`.

**Repo renamed** `highway/Radiacode110` -> **`highway/RadiaCode-Spectrometer`**:
the firmware supports the RC-102/103/103G/110, so a model-specific name was
misleading. `UPDATE_REPO_PATH` and the READMEs follow. Gitea keeps a 307 from
the old path, so nothing already deployed breaks. Released as **v1.0.2**.

## WiFi and BLE cannot both hold their buffers (2026-08-30)

The most damaging bug found so far, and one that only appears when both radios
are wanted at once. Symptom: the device would not connect at all, with
`E BLE_INIT: Malloc failed`, then every spectrum transfer truncating
(`Received 2292 of 3272 bytes`), a flood of `BT_BTC: btc_gattc_cback transfer
failed`, and eventually `search service failed`.

**It was not the analysis changes.** Comparing three captures made the cause
exact: the two healthy runs contained zero `[OTA]` lines, the broken one had
three. Bringing WiFi up is what breaks BLE.

`ensureWifi()` started the WiFi station and **never stopped it**, so ~40-50 KB
stayed gone for the rest of the session, and the startup update check fires
precisely while the BLE task is connecting.

Three separate defects, each needing its own fix:

1. **Nothing serialised the two radios.** `Updater` now calls
   `BleSource::suspendForWifi()` before any WiFi work and `resumeAfterWifi()`
   after, and `releaseWifi()` powers the station down when each job finishes
   (also on leaving the Settings screen, which keeps WiFi up to display the
   IP). This is lossless: the detector accumulates internally regardless -
   the same property that already survives dropouts and reboots.
2. **A fixed delay was not enough.** The first attempt waited 300 ms, but a
   spectrum read is a blocking round trip of up to 4 s, so WiFi still started
   mid-transfer. The BLE task now sets `suspendAcked` only once the radio is
   genuinely released and `ensureWifi()` waits for that acknowledgement.
3. **Deleting the client freed almost nothing.** Bluedroid stays initialised
   and keeps its buffers, so only ~4 KB came back: heap at WiFi start was
   ~92 KB against ~178 KB at boot, and TLS (~40-50 KB) failed with
   `SSL - Memory allocation failed`. The task now calls
   `BLEDevice::deinit(false)`, which returns **~51 KB**. `deinit(true)` must
   never be used - it calls `esp_bt_controller_mem_release()` and BLE could
   then never be started again that boot. `BLEDevice::init()` is idempotent
   and both call sites invoke it, so the stack returns on the next connect.

⚠️ **`userWantsConnection` must not be touched while suspended.** The first
version cleared it every iteration of the suspended branch "to block the retry
path" - but that branch already `continue`s past `tickInternal()`, so the write
did nothing except destroy a connect requested *during* the WiFi window
(connect-at-boot, or a CONNECT tap). A deliberate DISCONNECT is still honoured
because `reqDisconnect` is handled above the block.

**`BLEClient` was leaked on every connect** (patch 4 in `BluetoothTransport.cpp`).
Arduino-ESP32's `BLEDevice::createClient()` is a bare `new BLEClient()`, and
neither the library nor `BLEDevice::deinit()` ever deletes it; our destructor
called `disconnect()` and dropped the pointer, leaking the client plus its whole
discovered service/characteristic tree. Measured at **-4.15 KB per cycle**,
which progressively starved the TLS handshake. The destructor now waits 100 ms
for the async disconnect callback and deletes it. `BLEDevice::m_pClient` is left
dangling, which is safe - only the next `createClient()` ever reassigns it.

**`WIFI_BLE_STRESS`** (in `updater.cpp`, default 0) runs four update checks at
90 s intervals *while a measurement is live*, logging free heap around each.
Checking only at boot - before BLE connects - does not exercise this at all,
which is why the first fix looked complete and was not. Verified: heap flat at
88.3 KB / 143.6 KB / 155.7 KB across all four rounds (±60 bytes of noise,
against -4150 before), all four checks succeeded, zero malloc/SSL/BT/link
errors, 66 analyses.

## Identification fixes from the uranium-stone test (2026-08-30)

Real ore reported `Bi-214 92% 4/7 lines` yet `U-238 chain 0% (1/6)` and a
verdict of BACKGROUND. Three more gates were wrong:

1. **`scoreChain()` hard-required two members.** Bi-214 with four matched lines
   is uranium whatever else is resolved, but with Pb-214 below the 30% bar the
   chain scored zero and `classify()` fell through to BACKGROUND. A member that
   is conclusive alone (`CHAIN_SOLO_CONFIDENCE` 0.75 with `CHAIN_SOLO_LINES` 3
   independently matched lines) now carries the chain. Same mistake as the old
   Pb-212 rejection: corroboration demanded where the evidence already
   corroborates itself.
2. **The sub-110 keV region was allowed to identify a nuclide.** Below
   `XRAY_REGION_KEV` the spectrum is lead/bismuth/thorium K X-ray fluorescence,
   backscatter and the Compton edge - none of it modelled - so any dense, high-Z
   sample grows a large bump there, and Ba-133's 81 keV line claimed it for 76%.
   A nuclide with lines above that region must now be identified by one of them;
   the low line may still corroborate. Nuclides that genuinely emit only low
   (Am-241 at 59.5) keep their own strongest line.
3. **The ROI fallback had no competitive assignment.** Peak matching refuses to
   credit a nuclide whose line sits further from a peak than a rival's, but the
   ROI path did not - so a nuclide denied a peak by competition simply won the
   same counts through ROI instead. That is how Ba-133 kept claiming the real
   Ac-228/Pb-214 blend near 340 keV. `roiBetterExplainedElsewhere()` applies the
   same test to ROI centroids.

**`doReset()` silently did nothing while disconnected**, so a RESET tapped
before the link came up was discarded. It now returns a bool and the request
latches until it can be honoured.

Verified on hardware: uranium stones -> `URANIUM ORE` in 17 s, `U-238 chain
100% (3/6)`, Bi-214 86% / Ra-226 100% / U-235 73%. A later thorium source ->
`THORIUM`, `Th-232 chain 98% (2/4)`, Pb-212 95%, uranium chain 0%.

**`RESET_SPECTRUM_ON_BOOT`** (in `main.cpp`, default 0) clears the detector's
accumulation at startup so a bench test begins from zero without anyone tapping
RESET. Never set it in a release build - counts surviving a reboot is the whole
point of the normal path.

⚠️ **Serial debug output was being corrupted**, merging lines and dropping
characters, which made diagnosis unreliable. The analysis burst from core 0
overran the stock 256-byte TX buffer. `Serial.setTxBufferSize(1024)` fixes it -
deliberately not larger, because that heap is contended with Bluedroid.

## Home screen: two display bugs found on hardware (2026-08-30)

Both spotted from a photo of the Home screen during a uranium-then-thorium
session, and they looked like one problem while being unrelated.

**The dose gradient strip accumulated marker ticks.** `drawDoseScale()` paints
the marker two pixels taller than the strip (`fillRect(mx - 1, y - 2, 3, h + 4)`)
but redrew only the gradient band, `y` to `y + h`. Every marker's top and bottom
stubs therefore survived, and each new dose reading left another pair behind
until the strip carried a row of ticks across its whole width. RESET appeared
not to clear it because the ticks come from the dose rate moving, not from the
accumulation at all. Fixed by clearing the full band the marker can touch
before redrawing. Same rule as the chart and the tap ripple: on this QSPI panel,
erase exactly what you are about to draw over.

**`LIVE` showed wall-clock since connect, not the acquisition.** It read
`g_data.liveTimeSeconds`, which restarts at every connect - so it displayed
00:00 while the verdict on screen was computed from hours of accumulated
counts. The spectrum deliberately survives dropouts, disconnects and reboots
(the counts live on the RC-110), so the elapsed time beside it has to mean the
same thing. It now shows `spectrumDurationSec`, labelled **`ACQ`**: the
detector's own accumulation clock, which RESET clears and which the Analysis
screen was already using for its statistics. Confirmed on hardware - 163 s
after a reflash the detector reported 901 s accumulated, where the old field
would have said 00:02.

⚠️ Orphaned serial-capture processes hold COM9 and make `pio run -t upload`
fail with `PermissionError(13)`. They are `python.exe ... -u -` and outlive a
killed background task; kill them before flashing.

## Identification rebuilt on template fitting (2026-09-01)

The peak-search-and-gate pipeline was replaced. It had reached the point where
each field test added another veto, and vetoes only ever subtract sensitivity:
~28 tuned constants and 6 boolean gates, each correct for the sample that
motivated it and wrong for some sample not yet tried. Measured symptom, on the
real uranium capture: correct at 300 s and 600 s, **wrong at 900 s and 1200 s**
(NATURAL MIX, then THORIUM), correct again at 1800 s. More data made it worse.
That is the "works worse than before" report, and it is not a tuning problem.

**`host/` - offline replay harness. Build this before changing analysis.**
`host/build.sh` compiles the *real* analysis sources against shims in
`host/shim/` (Arduino, SD, Preferences, FreeRTOS), so what runs on the PC is
the firmware's own code, not a copy. Needs g++ (WinLibs MinGW; build.sh finds
it at the winget path automatically). Modes:
- `replay <csv>` - analyse a capture (the format `SdStorage::exportSpectrum()`
  already writes; duration and calibration come from its header)
- `--expect VERDICT` - assert, non-zero exit on failure
- `--sweep` - Poisson-thins a capture to shorter durations and reports the
  verdict at each, i.e. **time to identification**. This is what exposed the
  non-monotonic failure above; a single full-length run hides it.
- `--present <bg> <src> <seconds>` - simulates background accumulating, then a
  sample being placed on the detector. The only way to test the window logic,
  which needs a time series rather than one blended spectrum.
- `--bkg <csv>` - installs a capture as the active background reference.

Real captures live in `host/spectra/` (`background.csv` 3808 s,
`thorium.csv` 1048 s, `uranium.csv` 1829 s). **These are the authority.**
`host/make_synthetic.py` generates spectra from published NNDC data with an
independently-written detector response - deliberately NOT sharing physics with
the firmware, unlike `analysis_selftest.cpp`, whose `efficiency()` mirrors the
analyser's own `relEfficiency()` and therefore cannot fail on an assumption the
two share.

**The new pipeline** (`src/analysis/templates.*`, `fit.*`, `identify.*`):
1. **Templates** - per-nuclide expected spectrum: photopeaks at `fwhmKeVAt()`
   width scaled by efficiency, each with its Compton continuum. At 8.4% FWHM
   most counts are *not* in photopeaks, so a peak finder discards most of the
   information and then needs tie-breakers to allocate what survives.
2. **16 broad continuum bumps**, logarithmically spaced, 40% width (~5x the
   resolution, so they cannot imitate a line). Four global exponentials were
   far too rigid: chi2 62, and the fit recruited nuclide templates as
   continuum filler - every nuclide with a low-energy line scored >30 sigma by
   absorbing the sub-110 keV X-ray forest. With the bump basis, chi2 3.3.
   This also retires `XRAY_REGION_KEV`: the forest gets modelled instead of
   being banned by a hardcoded cutoff.
3. **Poisson ML-EM fit**. Non-negative by construction, ~40 lines, no
   active-set bookkeeping.
4. **Full Fisher covariance, not the diagonal.** The diagonal pretends
   overlapping templates are measured independently and reported `Co-60
   12.8 sigma` on background containing none. Inverting the matrix (Cholesky,
   over an active set, ridge term for genuinely degenerate pairs like U-235
   185.7 against Ra-226 186.2) is what makes 3 sigma mean *this* nuclide.
5. **Chain-coupled columns.** Chain members share one activity in secular
   equilibrium. With six free U-238 amplitudes, Tl-208's 583 keV template ate
   Bi-214's 609 keV line and produced thorium on pure uranium - the MIX bug.
   Composite columns fit the whole pattern or nothing.
   > Members get **no individual column**. Keeping both makes them exactly
   > degenerate; every chain nuclide came back at sigma ~1e5, the fit correctly
   > saying it could not tell them apart.

**Result on the real captures** (`--sweep`, and `--present` after 2 h of
background):

| | old | new |
|---|---|---|
| thorium | NATURAL MIX | THORIUM from **60 s** |
| uranium | flips MIX/THORIUM at 900-1200 s | URANIUM from **600 s**, monotonic |
| background | saved only by the dose floor | BACKGROUND, stable to 3808 s |
| source after 2 h accumulation | BACKGROUND (needed RESET) | uranium 300 s, thorium 60 s |

**`src/data/spectrum_window.*` - the RESET bug.** The detector integrates
forever, so a sample presented mid-run adds counts linearly while the banked
background grows as sqrt(t): on a 2 h accumulation real ore is 2.6% of the
spectrum. Analysis now runs on counts accumulated since a baseline snapshot,
and the baseline rolls forward automatically when the count rate steps up by
more than `STEP_SIGMA` of its own Poisson noise. The full accumulation is
untouched and still drives the chart and the export. `setCumulative(true)`
restores whole-run analysis for a genuinely weak sample needing hours.

⚠️ **`CHAIN_MIN_FRACTION` is the weak point.** Without a stored reference there
is nothing to establish that a chain belongs to the sample rather than the
room, so the chain must account for >=4% of counts (measured: ambient gives
Th 1.9% / U 0.7%; the mantle 18%, the ore 7.4%). This replaced the
dose+cps floors but shares their flaw - it was what still failed the 2 h case
before the window existed. **With a background reference active the test is
skipped entirely**, which is the right way to run for weak samples.

⚠️ **RAM went 40.1% -> 47.4%** (+24 KB static). The Fisher matrices (64 KB),
the window snapshots (8 KB) and the template matrix are all `ps_malloc`'d to
PSRAM for this reason. **Not yet verified against `WIFI_BLE_STRESS`** - given
the WiFi/BLE heap history above, run that before trusting an OTA check during
a live measurement.

**`USE_TEMPLATE_FIT`** (`peak_search.cpp`, default 1) switches back to the old
path for comparison. `findPeaks()` is still run, but only to mark the chart -
it no longer decides anything, and its top-24 truncation is out of the
identification path.

**Still open**: residuals are 10-16% (the response model is approximate);
`Th-232` reads ~7 sigma on the uranium capture (real cross-talk); strong
*synthetic* sources still show false positives above 90% where the real
captures do not, which is partly the generator and partly the response model.
`estimateGain()` remains disabled - under fitting, gain drift belongs in the
likelihood as a nuisance parameter, not in a separate voting estimator.

## PSRAM was never actually enabled (2026-09-01) - and it failed silently

`-D BOARD_HAS_PSRAM` in `build_flags` does **not** enable PSRAM. The Arduino
framework selects a prebuilt sdkconfig by `board_build.arduino.memory_type`,
and the default (`qio_qspi`) has SPIRAM compiled out. On hardware
`ESP.getPsramSize()` returned **0**, every `ps_malloc()` fell through to its
heap fallback, and the 156 KB template matrix could not fit in the ~72 KB of
free heap.

The damaging part was the silence: `Templates::build()` returned false,
`analyzeSpectrum()` took its documented fallback to the old peak-search path,
and the device ran perfectly - as the *old* code. Serial showed a healthy
connect, a clean OTA check and no errors. Nothing on screen or in the log
distinguished it from the new analysis working. Two flash-and-observe cycles
were spent before adding the `[TPL]` allocation log that made it visible in
one line.

Fix: `board_build.arduino.memory_type = qio_opi` in `platformio.ini`.
Measured after: **psram free 8,191,099**, and heap free went *up* 72,884 ->
106,656 because the matrix moved off it. WiFi/BLE coexistence measured across
a full OTA check with the fit resident: 106 KB at BLE suspend, 161 KB with the
stack down, 174 KB during TLS, back to 157 KB - no malloc or SSL failures.

Rules this leaves behind:
- **A failed allocation must say so.** `Templates::allocate()` and
  `SpectrumWindow::ensureBuffers()` both log now. A silent fallback that keeps
  the device apparently working is worse than a crash.
- **Verify PSRAM before relying on it**, per the warning in the board section
  above - which was correct and was ignored here.
- `qio_opi` is octal PSRAM. If a future board revision uses quad, this becomes
  `qio_qspi` and PSRAM silently disappears again; the `[TPL]` line is the
  check.

## Audit fixes (2026-09-01)

Six defects found by an external review of the fitting code, all reproduced in
`host/` before fixing and verified after:

1. **`Fit::buildActiveSet()` selected by index, not amplitude.** It kept the
   first 64 columns above the floor, so with an SD library approaching
   `ISOTOPE_LIBRARY_MAX` (300) any nuclide past column 64 got no covariance
   entry, held sigma 0 and could never be identified. Now displaces the
   weakest member.
2. **The candidate list bypassed the fraction test the verdict applied**, so
   `Co-60 76%` and `Na-22 60%` were listed on clean background - rejected by
   the headline but shown on the card the user actually reads.
3. **A stored reference bypassed the elevation gate entirely**
   (`uElevated = haveReference || ...`), and background fitted against itself
   reported **THORIUM 70%**. Fixed by comparing the chain against the fitted
   reference amplitude rather than against total counts
   (`CHAIN_MIN_VS_REFERENCE`): measured 0.05 for background-on-itself, 0.87
   for the mantle, 0.35 uranium / 0.15 thorium on the ore - which also
   suppresses the U/Th cross-talk.
4. **The step detector was one-sided.** Removing a sample left its counts
   banked in the open window and the device went on naming an isotope that was
   no longer there. Now rebases on a drop too.
5. **`STEP_SIGMA` 6.0 -> 4.0.** At ~20 cps over a 5 s poll, 6 sigma demanded
   roughly a 60% rate jump, so weak or shielded samples never opened a window.
6. **`ScreenHome`/`ScreenAnalysis` read `g_data` unguarded** on core 1 while
   core 0 rebuilds it. `DataGuard` added to both `draw()` and `update()`.

**Still wrong**: `Ba-133` reads ~70% on the real ore capture. Its 81 keV line
absorbs the unmodelled low-energy X-ray fluorescence that any dense high-Z
sample produces - the same region the old code handled with `XRAY_REGION_KEV`.
The verdict is unaffected (URANIUM 94%) but the candidate card is wrong. The
principled fix is to require support from lines above ~110 keV for nuclides
that have them, which needs a second fit restricted to that range.

## Hardware verification on the real uranium ore (2026-09-01)

Watched end to end with `IDENTIFY_DEBUG=1`, ore in place, window opened fresh:

```
U 1.5 sigma (7.4%)  Th 0.3 sigma  -> MEASURING...
U 2.8 sigma (9.0%)  Th 0.7 sigma  -> MEASURING...
U 3.9 sigma (7.1%)  Th 1.5 sigma  -> MEASURING...
U 5.0 sigma (6.8%)  Th 2.0 sigma  -> URANIUM      (crosses)
U 5.1 sigma (6.7%)  Th 2.0 sigma  -> URANIUM      (holds, no flicker)
```

**Roughly 7-8 minutes** to identify this ore, which reads 0.28 uSv/h - under
2x background. That is counting statistics, not a delay: significance grows as
sqrt(t), and the chain has to reach `VERDICT_SIGMA`. The thorium chain stayed
flat at ~2 sigma / 2.3% throughout while uranium climbed to 5 sigma / 6.8%, so
the cross-talk that used to produce NATURAL MIX is genuinely suppressed.

⚠️ **`pio device monitor` resets the ESP32 when it opens the port** (it asserts
DTR/RTS), which rebases `SpectrumWindow` and restarts the measurement. Every
observation therefore began from zero, and a source part-way to identification
looked like it was stuck. To watch a run in progress without disturbing it:

```python
# .platformio/penv/Scripts/python.exe - the system python has no pyserial
import serial
s = serial.Serial(); s.port='COM9'; s.baudrate=115200
s.dtr = False; s.rts = False        # <-- the point
s.timeout = 1; s.open()
```

⚠️ This board enumerates as **USB-Serial/JTAG**, not a USB-UART bridge, so the
classic "pulse DTR/RTS to reset" trick does not apply: driving both lines high
drops it into the ROM downloader (`waiting for download`) and it then sits
silent until rescued. To force a clean reboot and capture the boot log, use
esptool instead - `python .platformio/packages/tool-esptoolpy/esptool.py --chip
esp32s3 --port COM9 --after hard_reset read_mac` - then open the port with
dtr/rts false, retrying for a few seconds because the reset re-enumerates the
USB device and the port briefly disappears.

## Two bugs introduced by the fitting work, and fixed (2026-09-01)

Both were reported from the device, and both were mine.

**Display lag, swipes needing 2-4 attempts.** `pollSpectrum()` held `DataGuard`
across its whole body, and the ML-EM fit had been placed inside it - 60
iterations over ~39 templates x 1024 channels, one to three seconds with the
mutex held. Every draw and touch handler on core 1 blocked behind it. The fit
works on `s_analysis`, a buffer private to the BLE task, so it never needed the
lock: it now runs outside, and the lock is taken only for the spectrum copy and
a short results publish.

**Real uranium reported BACKGROUND.** Not a measurement failure - a false
claim. `SRC_BACKGROUND` was asserted after only 120 s / 4000 counts, but this
ore needs ~600 s to reach the verdict threshold, so for eight minutes the
device announced "background" with a genuine source in front of it. Saying
nothing would have been better. Now governed by `BACKGROUND_MIN_SECONDS` (600)
and `BACKGROUND_MIN_COUNTS` (20000): until there has been enough counting to
have seen something, the answer is MEASURING.

Found while fixing that: making the step detector bidirectional at 4 sigma
doubled its false-trigger rate, and **every false trigger resets the evidence
window to zero**, which alone could keep the device permanently unable to
identify anything. Steps now need confirmation across two consecutive polls
(`STEP_CONFIRMATIONS`) and a 90 s minimum window (`MIN_WINDOW_SECONDS`).

## Chain members are listed as evidence, and the fit range (2026-09-01)

**Only one nuclide per chain was being listed.** The composite chain column
means members have no individual amplitude, and the candidate list showed a
single representative - so a thorium mantle read "Pb-212" alone. That hides the
corroboration the identification actually rests on, and reads exactly like the
single-line evidence this pipeline replaced. Reported from the device: "for
this identification 2 candidates needed... I think one is missing."

`Identify::run()` now lists up to `CHAIN_MEMBERS_SHOWN` (3) members ranked by
branch x intensity x efficiency, skipping any whose only lines are below the
X-ray region. They share one significance because they share one fitted
amplitude - under secular equilibrium there is one measurement, not three - and
the screens' "Th chain"/"U chain" tag makes that read correctly. Real captures
now show **Pb-212 / Tl-208 / Ac-228** and **Pb-214 / Bi-214 / Ra-226**.

**`FIT_MIN_KEV` (150 keV) excludes the X-ray region from the likelihood.**
Below it the spectrum is lead/bismuth K fluorescence, backscatter and the
Compton edge, none of it modelled, and a nuclide with a low line could absorb
it: Ba-133 scored 5.1 sigma / 70% on real uranium ore largely on its 81 keV
line. Excluding that region removed it and raised the uranium chain from
11.8 to 12.6 sigma.

The bound was chosen by measurement, not taste - 110/130/150/170/200 were all
tried against the three real captures. 110 gave the highest chain significance
but let Co-57 in at the boundary; 150 is the cleanest on background. **Cost:
Am-241 (59.5 keV only) can no longer be identified.** It was never reliable
there for the same reason, but it is a real loss.

⚠️ **`Eu-152` still reads ~5.4 sigma / 73% on the real ore**, and it is NOT a
fluorescence artefact - it persists at every cut from 110 to 200 keV. Its
support is its 344.3 keV line sitting 7.6 keV from Pb-214's 351.9 under a
~29 keV FWHM: genuine template degeneracy with the uranium chain. The verdict
is unaffected (URANIUM 99%). Chasing it with another threshold is the trap this
whole rewrite exists to escape; the real fix is a likelihood-ratio test
(refit without the nuclide and compare) rather than another constant.

`Fit::minEnergyKeV()` exists because `dominantLine()` was displaying Eu-152 as
"best 121.8 keV" - a line below the fit range that contributed nothing. The UI
must not name a line the fit never used.

Time to verdict after these changes (`--sweep`, real captures): thorium 120 s,
uranium **300 s** (was 600 s), background settles at 900 s.

## The 4%-of-counts cliff, and the learned ambient baseline (2026-09-02)

Reported from the device: identification appeared to need **0.20 uSv/h**, a
source hovering at 0.18-0.22 never settled, the headline never reached a stable
BACKGROUND *or* a verdict, and two candidates showed on screen 2 the whole time.

There is no dose threshold in the fitting path, but the report was accurate.
`CHAIN_MIN_FRACTION` (4% of total counts) **lands almost exactly at 0.20 uSv/h
for this ore**, so it behaved as a hardcoded dose gate. Reproduced offline by
diluting the real ore capture into the real background:

| dose | U chain | old verdict |
|---|---|---|
| 0.176 | **8.2 sigma, 3.7%** | MEASURING (vetoed at 3.7 < 4.0) |
| 0.201 | 11.4 sigma, 5.2% | URANIUM |

An 8-sigma detection was being discarded for missing a fixed cutoff by 0.3
percentage points. The leaked evidence resurfaced as the two candidates the
user saw - K-40, and **Eu-152 at 7.3 sigma**, which is the known 344.3/351.9 keV
degeneracy with Pb-214 (see the fit-range note above). The candidate screen was
showing the uranium the headline had just thrown away.

**`src/analysis/ambient.*` - the room's chain level, measured not assumed.**
The fraction test itself is right; comparing against a *constant* is what was
wrong. Ambient U-238/Th-232 varies by room (radon, ventilation, stone), so the
floor of what this detector sees here IS ambient. The gate is now a ratio to a
learned floor (`ELEVATION_RATIO` 2.0, plus `MIN_ABSOLUTE_EXCESS` so a
pathologically low floor cannot make everything elevated).

Learning is asymmetric, and that asymmetry is the whole safety argument: the
estimate falls fast (`ADAPT_DOWN` 0.25 - a lower reading is direct evidence of
where the floor is) but rises glacially (`ADAPT_UP` 0.002/poll, ~1 h) **and only
while nothing is identified**, so a sample left on the detector for hours cannot
teach the device to ignore it. Only windows past 600 s / 20k counts are used;
the level persists in NVS (`rcambient`, ppm as uint32 - Preferences has no float
on the host shim). Skipped entirely when a background reference is active, where
the chain amplitude is already an excess.

**Why a ratio beats raw significance here.** Sigma alone cannot do this job: the
response model leaks ~2.5% of the ore into the thorium chain, and that leak is a
*bias*, so its significance grows without limit as counting continues (Th reads
4.6 sigma on pure uranium) while its *fraction* stays put. Testing the fraction
keeps working on long runs where a sigma cutoff would eventually call the
cross-talk real.

Measured separation against the learned floor - background 1.0x both chains,
ore 11x its own and 1.6x the cross-talk, mantle 11x and 0.1x - so 2.0 sits in a
wide empty gap rather than being tuned to a sample.

**Result.** The binding constraint is now counting statistics, which is
something the user can act on by measuring longer:

| capture | before | after |
|---|---|---|
| background 3808 s | BACKGROUND | BACKGROUND (unchanged, stable) |
| real ore | URANIUM 300 s | URANIUM 300 s (unchanged) |
| thorium mantle | THORIUM 120 s | THORIUM 120 s (unchanged) |
| **0.176 uSv/h ore** | **never** | **URANIUM at 1800 s** |

Detection floor at 80 minutes moved **0.20 -> ~0.153 uSv/h** against 0.131
ambient, and it now degrades gracefully (0.146 MEASURING, 0.139 BACKGROUND)
instead of cliff-edging.

⚠️ **`IDENTIFY_DEBUG` did not compile in the replay harness** - identify.cpp
guarded `<Arduino.h>` on `ifdef ARDUINO`, so the flag was a build error in the
one place it is most useful. The shim supplies `Serial`; the include is now
unconditional, matching peak_search.cpp. Build with
`IDENTIFY_DEBUG=1 bash host/build.sh`, which now also prints the ambient level
and the ELEV flag per chain.

⚠️ `sampleIsElevated()` still carries a literal 0.20 uSv/h, but only as the
pre-first-analysis placeholder for the Home card's SOURCE FOUND / MEASURING
label. It decides nothing once any candidate exists.

## A reboot silently discarded the whole accumulation (2026-09-02)

Found by watching serial through the flash that deployed the ambient fix above,
and it is the other half of the "it keeps restarting the measurement" report.

```
14:11:18 [ID] U 6.4 sigma (2.8% ELEV) ... -> URANIUM        <- 1h20m of counts
14:11:21 [ID] U 0.1 sigma (1.1%)      ... -> MEASURING...   <- next poll: gone
```

`SpectrumWindow::update()` opened with `if (!s_haveBase) rebase(full)`, and
`rebase()` means *baseline = the accumulation as it stands now*. On the first
poll after a restart that subtracts the accumulation from itself and hands the
analyser an empty spectrum. So the device threw away every count the RC-110 had
banked while the ESP32 was off - the exact data the whole no-connect-path-ever-
resets design exists to preserve - and began rediscovering a source it had just
correctly identified one poll earlier.

Cost is worst where it hurts most: a weak sample needs ~30 minutes of window, so
any reboot, reflash, or `pio device monitor` (which asserts DTR/RTS) restarted
that half hour. It also made the bug hard to see, because the single correct
verdict scrolled past before the reset.

Fixed with `s_adoptOnFirstSight`: the first poll after `begin()` adopts the
accumulation instead - baseline zeroed, so the window IS the run so far. Every
later path is unchanged, and in particular:

- `rebase()` still means discard, and clears the flag, so an explicit RESET
  before the first poll is not undone by the adopt path.
- The step detector still opens a fresh window when a sample is presented, and
  its bidirectional test still rebases if one was removed while we were off.
  Verified with `--present` after 2 h of background: window opens at 25 s,
  URANIUM at 300 s, unchanged.

⚠️ `SpectrumWindow::windowSeconds()` returns `s_baseDuration` - where the window
*starts* - while its comment claims it is "acquisition time the analyser is
seeing", which would be `full - base`. Nothing calls it, so nothing is broken;
fix the comment or the function before the first caller trusts it.

## Eu-152 on the ore: the likelihood-ratio fix would NOT work (2026-09-02)

The note above proposes fixing the spurious `Eu-152 73% / 5.5 sigma` on real
uranium ore with a likelihood-ratio test - refit without the nuclide and compare.
Measured before building it, by deleting Eu-152 from the table and re-running
the real ore capture:

| | residual | reduced chi2 | U-238 chain |
|---|---|---|---|
| with Eu-152 | 10.9% | 1.96 | 12.6 sigma |
| without | 11.2% | **2.07** | **13.8 sigma** |

Over ~920 degrees of freedom (channels above `FIT_MIN_KEV` less the columns),
that 0.11 of reduced chi2 is a **chi2 change near 100**. An LRT would therefore
report Eu-152 at ~9 sigma and *confirm* it - the opposite of the intent. Do not
build it for this purpose.

Both halves of the paradox are true at once: Eu-152 really does improve the
likelihood, and the counts really do belong to uranium - the chain climbs from
12.6 to 13.8 sigma the moment Eu-152 cannot take them. Its six lines act as
flexible filler for **response-model error** in the chain templates (peak shape,
low-energy tail, the 338.3/344.3/351.9 keV blend at ~29 keV FWHM), and a
likelihood test cannot distinguish filler-for-model-error from signal, because
by construction both improve the likelihood.

So this is not a statistics problem and no further test will fix it. It is the
response model, which is the same root cause as the 10-16% residuals already
noted. Until that improves, the honest options are to leave it (the verdict is
unaffected - URANIUM 96%) or to stop *listing* a `CAT_CALIBRATION` nuclide that
is degenerate with an already-identified chain. The second is presentation, not
identification, and should be labelled as such if it is ever done.

### Hardware verification of both fixes (2026-09-02)

One hour of continuous serial capture against the user's real ore at
~0.18-0.22 uSv/h - the sample that could never be identified before.

```
[WIN] resumed accumulation: 7104s, 174581 counts
[TPL] matrix 159744 bytes for 39 columns (psram free 8191099, heap free 106684)
[ID] U 6.5 sigma (2.8% amb 1.2% ELEV)  Th 6.5 sigma (2.4% amb 2.2% -)  -> URANIUM
```

URANIUM **three seconds after boot**, from accumulation carried across a
reflash. Then **718 analyses, every one URANIUM** - no flicker, no spurious
window rebase (`[WIN]` appears once, at boot), no BLE malloc/SSL failures.

| | 14:15 | 15:15 |
|---|---|---|
| U sigma | 6.5 | 8.0 |
| U fraction | 2.8% | 2.7% |
| Th sigma | 6.5 | 7.8 |
| Th fraction | 2.4% | 2.4% |
| learned floor U / Th | 1.2% / 2.2% | 1.2% / 2.2% |

Three things this run settles, none of which a single-shot test would show:

1. **The sample sits at 2.8% of counts.** Under `CHAIN_MIN_FRACTION` 0.04 it
   could never have been identified, at any acquisition length.
2. **Sigma cannot separate these chains; the fraction can.** Thorium tracked
   uranium almost exactly for the whole hour (7.8 against 8.0 at the end) and
   was correctly rejected throughout, because 2.4% never approached 2x the
   2.2% floor. Both sigmas grew as sqrt(t) while both fractions stayed flat -
   which is the entire argument for the ratio test, observed rather than
   asserted.
3. **Asymmetric learning holds.** An hour with a source present did not raise
   either floor by a single tenth of a percent, because upward adaptation is
   blocked while a source is identified.

Also exercised incidentally on the release build: a full OTA check during a
live measurement - BLE suspend at 106 KB heap, stack down 161 KB, WiFi up
174 KB, "Up to date (1.0.6)", WiFi off, BLE reconnected and resumed - with the
template matrix resident in PSRAM. That is the `WIFI_BLE_STRESS` case the
fitting work had left unverified, and it passes.

⚠️ `Error: Buffer overflow prevented in readFloat` / `readUint16` appeared
**twice in an hour**, from the vendored RadiaCode library's decoder. Harmless
as seen - the library guards the read, the poll is skipped and the verdict is
unaffected - but it means a short or misaligned notification packet is
arriving occasionally. Not diagnosed; noted so it is not mistaken for new
damage later.
