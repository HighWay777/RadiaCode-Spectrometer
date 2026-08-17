#pragma once

#include <Arduino.h>
#include <vector>

struct BaseLayoutWidget {
  const char* id;
  const char* type;
  const char* binding;
  const char* text;
  const char* color;
  uint8_t fontSize;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

const std::vector<BaseLayoutWidget>& getBaseLayout(uint8_t baseId);
