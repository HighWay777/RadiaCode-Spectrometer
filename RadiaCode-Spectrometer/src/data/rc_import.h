#pragma once

#include <stdint.h>
#include "spectrometer_data.h"

// On-device import of spectra exported by the official RadiaCode software.
//
// The export is XML, but a full parser is unnecessary: the file is a known
// shape and we only need three tags — <MeasurementTime>, <Coefficient> and a
// long run of <DataPoint>. A streaming tag scanner handles that in a few
// hundred bytes of state, so the conversion runs on the device and no PC step
// is needed. (tools/radiacode_to_background.py does the same job on a PC and
// stays available for bulk work.)
namespace RcImport {

  static const int MAX_FILES = 12;
  static const int NAME_LEN = 48;

  struct FileEntry {
    char path[NAME_LEN];   // full path, e.g. /import/bkg1.xml
    char label[NAME_LEN];  // display name
    uint32_t size;
  };

  struct Result {
    bool ok;
    uint32_t durationSec;
    uint32_t totalCounts;
    int channels;
    float a0, a1, a2;
    bool calibMismatch;    // vs the live device calibration
    char error[56];
  };

  // Creates /import if missing. Lists *.xml found in /import and in the card
  // root, so files can simply be dropped anywhere obvious.
  int scan(FileEntry* out, int maxFiles);

  // Parses `path` and, on success, writes /background/<slot>.csv.
  // `live` supplies the device's current calibration for the mismatch check.
  Result importToSlot(const char* path, const char* slot, const SpectrometerData& live);
}
