#include "screen_analysis.h"
#include "cards.h"
#include "ui_theme.h"
#include "../data/spectrometer_data.h"
#include "../data/background.h"
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

extern Arduino_GFX* gfx;

namespace ScreenAnalysis {

static const int16_t HEADER_H = 26;
static const int16_t VERD_X = 8,  VERD_Y = 30,  VERD_W = 464, VERD_H = 62;
static const int16_t QUAL_X = 8,  QUAL_Y = 96,  QUAL_W = 228, QUAL_H = 76;
static const int16_t EVID_X = 244, EVID_Y = 96, EVID_W = 228, EVID_H = 76;
static const int16_t TIME_X = 8,  TIME_Y = 176, TIME_W = 464, TIME_H = 88;

static uint16_t verdictColor(SourceClass c) {
  switch (c) {
    case SRC_URANIUM:
    case SRC_THORIUM:
    case SRC_MIXED_NATURAL: return currentTheme->highlight;
    case SRC_ARTIFICIAL:    return currentTheme->danger;
    case SRC_POTASSIUM:
    case SRC_BACKGROUND:    return currentTheme->accent;
    default:                return currentTheme->textSecondary;
  }
}

static uint16_t gradeColor(QualityGrade g) {
  switch (g) {
    case QUAL_EXCELLENT: return currentTheme->accent;
    case QUAL_GOOD:      return currentTheme->accent;
    case QUAL_FAIR:      return currentTheme->warning;
    default:             return currentTheme->danger;
  }
}

static void formatDuration(uint32_t seconds, char* buf, size_t bufSize) {
  if (seconds >= 3600) {
    snprintf(buf, bufSize, "%uh %02um", (unsigned)(seconds / 3600), (unsigned)((seconds % 3600) / 60));
  } else if (seconds >= 60) {
    snprintf(buf, bufSize, "%um %02us", (unsigned)(seconds / 60), (unsigned)(seconds % 60));
  } else {
    snprintf(buf, bufSize, "%us", (unsigned)seconds);
  }
}

static void drawHeader() {
  gfx->fillRect(0, 0, 480, HEADER_H, currentTheme->header);
  Cards::leftText(10, 6, "Analysis & Quality", 2, currentTheme->text);
  // A verdict computed against a subtracted background means something
  // different from a raw one, so say so where the verdict is read.
  if (Background::active()) {
    const char* tag = "BKG SUBTRACTED";
    int16_t tw = Cards::textWidth(tag, 1);
    Cards::leftText(480 - 12 - tw, 10, tag, 1, currentTheme->accent);
  }
  gfx->drawFastHLine(0, HEADER_H, 480, currentTheme->cardBorder);
}

// --- verdict -------------------------------------------------------------
static SourceClass lastClass = (SourceClass)-1;
// The label is graded by confidence, so confidence has to be part of the
// cache key. Without it the "?" suffix stuck permanently: the class stayed
// SRC_URANIUM while confidence climbed through the 0.80 cut and nothing
// repainted.
static int lastVerdictConfBand = -1;

static void updateVerdict(bool force) {
  int band = (int)(g_data.verdictConfidence * 20.0f);
  if (!force && g_data.sourceClass == lastClass && band == lastVerdictConfBand) return;
  lastClass = g_data.sourceClass;
  lastVerdictConfBand = band;

  gfx->fillRect(VERD_X + 4, VERD_Y + 20, VERD_W - 8, VERD_H - 24, currentTheme->card);
  Cards::leftText(VERD_X + 12, VERD_Y + 22, sourceClassLabelGraded(g_data.sourceClass, g_data.verdictConfidence), 3,
                  verdictColor(g_data.sourceClass));
  Cards::leftText(VERD_X + 12, VERD_Y + 48, sourceClassDetail(g_data.sourceClass), 1,
                  currentTheme->textSecondary);
}

// --- quality -------------------------------------------------------------
static QualityGrade lastGrade = (QualityGrade)-1;
static int lastSigmaTenths = -1;

static void updateQuality(bool force) {
  int sigmaTenths = (int)(g_data.quality.weakestKeySigma * 10.0f);
  if (!force && g_data.quality.grade == lastGrade && sigmaTenths == lastSigmaTenths) return;
  lastGrade = g_data.quality.grade;
  lastSigmaTenths = sigmaTenths;

  gfx->fillRect(QUAL_X + 4, QUAL_Y + 20, QUAL_W - 8, QUAL_H - 24, currentTheme->card);

  // With nothing identified there is no identification to be confident in,
  // and printing "0.0 sigma" beside an empty bar just looks broken. Say what
  // is actually true instead.
  if (g_data.candidateCount == 0) {
    Cards::leftText(QUAL_X + 12, QUAL_Y + 24, "NO ID YET", 2,
                    currentTheme->textSecondary);
    Cards::leftText(QUAL_X + 12, QUAL_Y + 46, "keep measuring", 1,
                    currentTheme->textSecondary);
    return;
  }

  Cards::leftText(QUAL_X + 12, QUAL_Y + 24, qualityGradeLabel(g_data.quality.grade), 2,
                  gradeColor(g_data.quality.grade));

  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f sigma", g_data.quality.weakestKeySigma);
  Cards::leftText(QUAL_X + 12, QUAL_Y + 46, buf, 1, currentTheme->textSecondary);

  // 12 sigma is the "as good as this detector gets" target used by the
  // remaining-time extrapolation in peak_search.cpp.
  float frac = g_data.quality.weakestKeySigma / 12.0f;
  Cards::progressBar(QUAL_X + 12, QUAL_Y + 58, QUAL_W - 24, 8, frac,
                     gradeColor(g_data.quality.grade));
}

// --- evidence ------------------------------------------------------------
static int lastUraniumPct = -1, lastThoriumPct = -1;

static void updateEvidence(bool force) {
  int uPct = (int)(g_data.uranium.confidence * 100.0f);
  int tPct = (int)(g_data.thorium.confidence * 100.0f);
  if (!force && uPct == lastUraniumPct && tPct == lastThoriumPct) return;
  lastUraniumPct = uPct;
  lastThoriumPct = tPct;

  gfx->fillRect(EVID_X + 4, EVID_Y + 20, EVID_W - 8, EVID_H - 24, currentTheme->card);

  char buf[40];
  snprintf(buf, sizeof(buf), "U-238 chain  %d/%d  %d%%",
           g_data.uranium.membersDetected, g_data.uranium.membersTotal, uPct);
  Cards::leftText(EVID_X + 10, EVID_Y + 24, buf, 1,
                  uPct > 0 ? currentTheme->highlight : currentTheme->textSecondary);
  Cards::progressBar(EVID_X + 10, EVID_Y + 34, EVID_W - 20, 6,
                     g_data.uranium.confidence, currentTheme->highlight);

  snprintf(buf, sizeof(buf), "Th-232 chain %d/%d  %d%%",
           g_data.thorium.membersDetected, g_data.thorium.membersTotal, tPct);
  Cards::leftText(EVID_X + 10, EVID_Y + 48, buf, 1,
                  tPct > 0 ? currentTheme->cool : currentTheme->textSecondary);
  Cards::progressBar(EVID_X + 10, EVID_Y + 58, EVID_W - 20, 6,
                     g_data.thorium.confidence, currentTheme->cool);
}

// --- acquisition advice --------------------------------------------------
static uint32_t lastRemaining = 0xFFFFFFFF;
static uint32_t lastElapsedMin = 0xFFFFFFFF;
static ConnectionState lastConn = (ConnectionState)-1;

static void updateTimeAdvice(bool force) {
  // Only repaint when the advice actually changes materially — otherwise a
  // once-per-second countdown would repaint this card constantly.
  uint32_t elapsedSec = g_data.isWindowed ? g_data.windowDurationSec : g_data.spectrumDurationSec;
  uint32_t remainingBucket = g_data.quality.remainingSeconds / 5;
  uint32_t elapsedBucket = elapsedSec / 5;
  // Connection state is rendered by this card too. On a disconnect both
  // buckets stop changing, so without it here the card froze mid-measurement
  // and kept advising "keep measuring" against a dead link.
  ConnectionState conn = g_data.connection;
  if (!force && remainingBucket == lastRemaining && elapsedBucket == lastElapsedMin &&
      conn == lastConn) return;
  lastRemaining = remainingBucket;
  lastElapsedMin = elapsedBucket;
  lastConn = conn;

  gfx->fillRect(TIME_X + 4, TIME_Y + 20, TIME_W - 8, TIME_H - 24, currentTheme->card);

  char elapsedBuf[16], recBuf[16], remBuf[16];
  formatDuration(elapsedSec, elapsedBuf, sizeof(elapsedBuf));
  formatDuration(g_data.quality.recommendedSeconds, recBuf, sizeof(recBuf));
  formatDuration(g_data.quality.remainingSeconds, remBuf, sizeof(remBuf));

  char line[64];
  if (g_data.connection != CONN_CONNECTED) {
    Cards::leftText(TIME_X + 12, TIME_Y + 30, "Connect to start a measurement", 2,
                    currentTheme->textSecondary);
    return;
  }

  if (g_data.quality.remainingSeconds == 0 && g_data.quality.weakestKeySigma >= 12.0f) {
    Cards::leftText(TIME_X + 12, TIME_Y + 26, "Measurement complete", 2, currentTheme->accent);
    snprintf(line, sizeof(line), "Ran %s - result is statistically solid", elapsedBuf);
    Cards::leftText(TIME_X + 12, TIME_Y + 50, line, 1, currentTheme->textSecondary);
  } else {
    snprintf(line, sizeof(line), "Keep measuring ~%s", remBuf);
    Cards::leftText(TIME_X + 12, TIME_Y + 24, line, 2, currentTheme->warning);
    snprintf(line, sizeof(line), "Elapsed %s%s of rec. %s", elapsedBuf,
             g_data.isWindowed ? " (win)" : "", recBuf);
    Cards::leftText(TIME_X + 12, TIME_Y + 48, line, 1, currentTheme->textSecondary);
  }

  float progress = 0.0f;
  if (g_data.quality.recommendedSeconds > 0) {
    progress = (float)elapsedSec / (float)g_data.quality.recommendedSeconds;
    if (progress > 1.0f) progress = 1.0f;
  }
  Cards::progressBar(TIME_X + 12, TIME_Y + 64, TIME_W - 24, 10, progress, currentTheme->accent);

  char stats[48];
  snprintf(stats, sizeof(stats), "%u counts, %u peaks",
           (unsigned)g_data.quality.totalCounts, (unsigned)g_data.quality.peaksFound);
  int16_t sw = Cards::textWidth(stats, 1);
  Cards::leftText(TIME_X + TIME_W - 12 - sw, TIME_Y + 8, stats, 1, currentTheme->textSecondary);
}

void draw() {
  // candidates, the chain structs and quality are rebuilt on core 0 by
  // analyzeSpectrum(); reading them unguarded can tear a multi-field read.
  DataGuard guard;
  gfx->fillScreen(currentTheme->background);
  drawHeader();
  Cards::frame(VERD_X, VERD_Y, VERD_W, VERD_H, "SAMPLE VERDICT");
  Cards::frame(QUAL_X, QUAL_Y, QUAL_W, QUAL_H, "CONFIDENCE");
  Cards::frame(EVID_X, EVID_Y, EVID_W, EVID_H, "CHAIN EVIDENCE");
  Cards::frame(TIME_X, TIME_Y, TIME_W, TIME_H, "ACQUISITION");
  updateVerdict(true);
  updateQuality(true);
  updateEvidence(true);
  updateTimeAdvice(true);
}

void update() {
  // candidates, the chain structs and quality are rebuilt on core 0 by
  // analyzeSpectrum(); reading them unguarded can tear a multi-field read.
  DataGuard guard;
  updateVerdict(false);
  updateQuality(false);
  updateEvidence(false);
  updateTimeAdvice(false);
}

} // namespace ScreenAnalysis
