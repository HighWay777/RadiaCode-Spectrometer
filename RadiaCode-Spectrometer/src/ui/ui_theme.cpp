#include "ui_theme.h"

UITheme APP_THEME = {
  /* background    */ rgb565(8, 10, 16),
  /* header        */ rgb565(14, 18, 28),
  /* card          */ rgb565(22, 26, 38),
  /* cardBorder    */ rgb565(46, 54, 70),
  /* text          */ rgb565(235, 238, 242),
  /* textSecondary */ rgb565(150, 158, 172),
  /* accent        */ rgb565(0, 200, 180),
  /* warm          */ rgb565(255, 150, 60),
  /* cool          */ rgb565(90, 160, 255),
  /* warning       */ rgb565(255, 170, 0),
  /* danger        */ rgb565(230, 60, 60),
  /* highlight     */ rgb565(255, 210, 60),
};

UITheme* currentTheme = &APP_THEME;
