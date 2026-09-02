# Offline replay harness

Runs the firmware's real analysis code on a PC against spectra captured from
the device, so a change can be judged in seconds instead of by pointing the
detector at a sample and reading the screen.

This exists because tuning identification against live hardware has no usable
feedback loop: a field session takes hours, is not repeatable, and confounds
several variables at once. Every threshold currently in `peak_search.cpp` was
set that way, which is how they came to contradict each other.

## What it is

`replay.cpp` links against the actual firmware sources —

- `src/analysis/peak_search.cpp`
- `src/data/isotope_library.cpp`
- `src/data/background.cpp`
- `src/data/spectrometer_data.cpp`

— compiled against small shims in `shim/` that stand in for `Arduino.h`,
`SD.h`, `Preferences.h` and FreeRTOS. **The analysis code is not forked or
reimplemented.** If a source starts needing more of the Arduino API, extend
the shim rather than changing the source, or the harness stops proving
anything about the firmware.

## Build

```sh
./build.sh                 # -> host/build/replay.exe
```

Needs `g++` on PATH (MinGW-w64 / WinLibs on Windows). `PEAK_SEARCH_DEBUG`
defaults to 1 here — the per-peak and per-nuclide reject log is the point.

## Capturing reference spectra

On the device: measure the sample, then **Data → EXPORT CSV**. That writes
`/spectra/spec_NNNN.csv`, which this harness reads directly — the header
already carries duration and calibration, and both are required.

Copy the files into `spectra/` with meaningful names. The three that matter:

| File | What to measure | Why |
|---|---|---|
| `background.csv` | Ambient, detector where you normally use it, **30 min or longer** | The reference every other judgement is made against. Too short and its own noise contaminates the comparison. |
| `uranium.csv` | The uranium ore | Must resolve as U-238 chain via Bi-214 and Pb-214 |
| `thorium.csv` | The lamp mantle | Must resolve as Th-232 chain via Pb-212, Tl-208, Ac-228, with uranium at zero |

Longer is better for all three; these are ground truth, captured once.

## Use

```sh
build/replay.exe spectra/uranium.csv
build/replay.exe --expect URANIUM spectra/uranium.csv
build/replay.exe --expect THORIUM spectra/thorium.csv \
                 --expect BACKGROUND spectra/background.csv
```

Exit status is non-zero if any `--expect` fails, so this drops straight into a
script or CI.

### The case this was built for

A source presented to an already-running acquisition is currently invisible
until the user taps RESET. `--mix` reconstructs that without a two-hour wait:

```sh
build/replay.exe --mix spectra/background.csv spectra/uranium.csv 7200
```

It scales the background to 7200 s, adds the ore's excess over its own
background on top, and analyses the result. Today this reports BACKGROUND.
When it reports URANIUM, that bug is fixed.
