#pragma once

#include <stdint.h>

// Over-the-air firmware updates from the project's public Gitea repo.
//
// How it works: Gitea exposes releases at
//   https://<host>/api/v1/repos/<owner>/<repo>/releases/latest
// which returns JSON carrying a tag_name and the release's assets. We compare
// tag_name against FIRMWARE_VERSION and, if it is newer, stream the
// firmware.bin asset straight into the inactive OTA partition. The running
// image is never touched, so a failed download simply leaves the device on
// the current version.
//
// The repo is public, so no token is stored on the device.
namespace Updater {
  enum State {
    IDLE,
    WIFI_CONNECTING,
    CHECKING,
    UP_TO_DATE,
    AVAILABLE,
    DOWNLOADING,
    SUCCESS,
    FAILED
  };

  void begin();

  // Non-blocking; work happens on a background task so the UI stays live.
  void checkForUpdate();
  void installUpdate();     // only meaningful after AVAILABLE

  State state();
  const char* statusText();
  const char* latestVersion();
  int progressPercent();    // during DOWNLOADING
  bool busy();
}
