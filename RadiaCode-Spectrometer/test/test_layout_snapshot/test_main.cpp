#include <Arduino.h>
#include <unity.h>
#include "ui/base_layouts.h"
#include "ui/ui_framework.h"

static const BaseLayoutWidget* findWidget(const std::vector<BaseLayoutWidget>& layout, const char* id) {
  for (const auto& widget : layout) {
    if (widget.id && strcmp(widget.id, id) == 0) {
      return &widget;
    }
  }
  return nullptr;
}

void test_home_layout_snapshot(void) {
  const auto& layout = getBaseLayout(PAGE_HOME);
  TEST_ASSERT_EQUAL_UINT8(11, layout.size());

  const BaseLayoutWidget* header = findWidget(layout, "home.header");
  TEST_ASSERT_NOT_NULL(header);
  TEST_ASSERT_EQUAL_INT16(0, header->x);
  TEST_ASSERT_EQUAL_INT16(0, header->y);
  TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH, header->w);
  TEST_ASSERT_EQUAL_INT16(24, header->h);

  const BaseLayoutWidget* weatherPanel = findWidget(layout, "home.weather.panel");
  TEST_ASSERT_NOT_NULL(weatherPanel);
  TEST_ASSERT_EQUAL_STRING("panel", weatherPanel->type);
  TEST_ASSERT_INT_WITHIN(2, 10, weatherPanel->x);
  TEST_ASSERT_INT_WITHIN(2, 34, weatherPanel->y);
  TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH - 20, weatherPanel->w);

  const BaseLayoutWidget* icon = findWidget(layout, "home.weather.icon");
  TEST_ASSERT_NOT_NULL(icon);
  TEST_ASSERT_EQUAL_STRING("weatherIcon", icon->type);
  TEST_ASSERT_TRUE(icon->w >= 80);
  TEST_ASSERT_TRUE(icon->h >= 80);

  const BaseLayoutWidget* clock = findWidget(layout, "home.clock.panel");
  TEST_ASSERT_NOT_NULL(clock);
  TEST_ASSERT_TRUE(clock->y > weatherPanel->y);
  TEST_ASSERT_TRUE(clock->h >= weatherPanel->h - 4);

  const BaseLayoutWidget* status = findWidget(layout, "home.status");
  TEST_ASSERT_NOT_NULL(status);
  TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT - 20, status->y);
  TEST_ASSERT_EQUAL_INT16(20, status->h);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_home_layout_snapshot);
  UNITY_END();
}

void loop() {
  // not used
}
