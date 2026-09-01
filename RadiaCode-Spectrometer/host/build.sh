#!/usr/bin/env bash
# Builds the offline replay harness.
#
# Compiles the real analysis sources against the shims in host/shim/, so what
# runs here is the same code the firmware runs - not a reimplementation. If a
# source below starts needing more of the Arduino API, extend the shim rather
# than forking the source.
set -euo pipefail

cd "$(dirname "$0")/.."

OUT=host/build
mkdir -p "$OUT"

# WinLibs installs via winget but does not put itself on PATH for a bash
# shell, so fall back to the known install location before giving up.
CXX=${CXX:-g++}
if ! command -v "$CXX" >/dev/null 2>&1; then
  WINLIBS="$LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
  if [ -x "$WINLIBS/g++.exe" ]; then
    export PATH="$WINLIBS:$PATH"
  else
    echo "error: no C++ compiler found. Install one with:" >&2
    echo "  winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT" >&2
    exit 1
  fi
fi

# -DPEAK_SEARCH_DEBUG=1 is the point of the harness: the per-peak and
# per-nuclide reject log is what makes a wrong answer diagnosable.
"$CXX" -std=c++17 -O2 -g \
  -DPEAK_SEARCH_DEBUG="${PEAK_SEARCH_DEBUG:-1}" \
  -I host/shim \
  -o "$OUT/replay.exe" \
  host/replay.cpp \
  src/analysis/peak_search.cpp \
  src/analysis/templates.cpp \
  src/analysis/fit.cpp \
  src/analysis/identify.cpp \
  src/data/isotope_library.cpp \
  src/data/background.cpp \
  src/data/spectrometer_data.cpp \
  src/data/spectrum_window.cpp \
  -lm

echo "built $OUT/replay.exe"
