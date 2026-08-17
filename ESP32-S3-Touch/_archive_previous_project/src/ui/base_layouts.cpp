#include "base_layouts.h"
#include "ui_framework.h"
#include "page_home.h"
#include <algorithm>

static void addBox(std::vector<BaseLayoutWidget>& target, const char* id, const char* type, const char* label,
                   int16_t x, int16_t y, int16_t w, int16_t h,
                   const char* binding = nullptr, const char* color = nullptr, uint8_t fontSize = 2) {
  target.push_back({ id, type, binding, label, color, fontSize, x, y, w, h });
}

const std::vector<BaseLayoutWidget>& getBaseLayout(uint8_t baseId) {
  static std::vector<BaseLayoutWidget> cache[PAGE_COUNT];
  static bool populated[PAGE_COUNT] = { false };
  static std::vector<BaseLayoutWidget> empty;

  if (baseId >= PAGE_COUNT) {
    empty.clear();
    return empty;
  }

  if (!populated[baseId]) {
    auto& vec = cache[baseId];
    vec.clear();
    const int16_t margin = 10;
    const int16_t headerHeight = 24;
    switch (baseId) {
      case PAGE_HOME: {
        HomeLayoutGeometry geo = getHomeLayoutGeometry();
        HomeWeatherLayout weather = computeHomeWeatherLayout(true);
        int16_t headerX = geo.margin;
        int16_t headerW = SCREEN_WIDTH - geo.margin * 2;
        addBox(vec, "home.header", "label", "Header", headerX, geo.headerY, headerW, geo.headerHeight);
        addBox(vec, "home.weather.panel", "panel", "Weather Panel", weather.panelX, weather.panelY, weather.panelW, weather.panelH);
        addBox(vec, "home.weather.icon", "weatherIcon", "Icon", weather.iconX, weather.iconY, weather.iconW, weather.iconH);
        addBox(vec, "home.weather.summary", "value", "Temperature Summary", weather.summaryX, weather.summaryY, weather.summaryW, 48);
        int16_t badgeY = weather.badgesY;
        int16_t badgeW = std::max<int16_t>(180, weather.summaryW / 2 - 6);
        addBox(vec, "home.badge.extTemp", "badge", "Outside Temp", weather.summaryX, badgeY, badgeW, weather.badgeHeight, "extTemp");
        addBox(vec, "home.badge.humidity", "badge", "Humidity", weather.summaryX + badgeW + weather.badgeGap, badgeY, badgeW, weather.badgeHeight, "humidity");
        badgeY += weather.badgeHeight + weather.badgeGap;
        addBox(vec, "home.badge.pressure", "badge", "Pressure", weather.summaryX, badgeY, badgeW, weather.badgeHeight, "pressure");
        addBox(vec, "home.badge.wind", "badge", "Wind", weather.summaryX + badgeW + weather.badgeGap, badgeY, badgeW, weather.badgeHeight, "windKph");
        if (weather.showSensor) {
          addBox(vec, "home.badge.boiler", "badge", "Boiler", weather.sensorX, weather.sensorY, weather.sensorW, weather.sensorH, "currentTemp");
        }
        addBox(vec, "home.clock.panel", "value", "Clock Panel", geo.margin, geo.clockY, weather.panelW, geo.clockHeight);
        addBox(vec, "home.status", "label", "Status Bar", geo.margin, geo.statusY, weather.panelW, geo.statusHeight);
        break;
      }
      case PAGE_DASHBOARD: {
        const int16_t cardW = (SCREEN_WIDTH - margin * 3) / 2;
        const int16_t y0 = headerHeight + margin;
        const int16_t h1 = 100;
        addBox(vec, "boiler.current", "value", "Current", margin, y0, cardW, h1, "currentTemp");
        addBox(vec, "boiler.target", "value", "Target", margin + cardW + margin, y0, cardW, h1, "targetTemp");
        const int16_t y1 = y0 + h1 + margin;
        const int16_t h2 = 70;
        addBox(vec, "boiler.delta", "value", "Delta", margin, y1, cardW, h2);
        addBox(vec, "boiler.status", "label", "Status", margin + cardW + margin, y1, cardW, h2);
  const int16_t y2 = y1 + h2 + margin;
  const int16_t scheduleH = max((int16_t)40, (int16_t)(SCREEN_HEIGHT - y2 - margin - 8));
        addBox(vec, "boiler.schedule", "panel", "Schedule", margin, y2, cardW * 2 + margin, scheduleH);
        break;
      }
      case PAGE_STATS: {
        const int16_t cardW = (SCREEN_WIDTH - margin * 3) / 2;
        const int16_t h = 80;
        const int16_t y = headerHeight + margin;
        addBox(vec, "stats.efficiency", "value", "Efficiency", margin, y, cardW, h, "efficiency");
        addBox(vec, "stats.loss", "value", "Heat Loss", margin + cardW + margin, y, cardW, h);
        const int16_t y2 = y + h + margin;
        addBox(vec, "stats.kwh", "value", "Total kWh", margin, y2, cardW, h, "kwhTotal");
        addBox(vec, "stats.cost", "value", "Total Cost", margin + cardW + margin, y2, cardW, h, "costTotal");
        const int16_t y3 = y2 + h + margin;
        addBox(vec, "stats.dailyKwh", "value", "Daily kWh", margin, y3, cardW, h, "dailyKWh");
        addBox(vec, "stats.dailyCost", "value", "Daily Cost", margin + cardW + margin, y3, cardW, h, "dailyCost");
        break;
      }
      case PAGE_CONTROLS: {
        const int16_t cardW = (SCREEN_WIDTH - margin * 3) / 2;
        const int16_t h = 80;
        const int16_t y = headerHeight + margin;
        addBox(vec, "controls.wifi", "label", "WiFi", margin, y, cardW, h);
        addBox(vec, "controls.ip", "label", "IP", margin + cardW + margin, y, cardW, h);
        const int16_t y2 = y + h + margin;
        addBox(vec, "controls.uptime", "label", "Uptime", margin, y2, cardW, h);
        addBox(vec, "controls.memory", "label", "Memory", margin + cardW + margin, y2, cardW, h);
        break;
      }
      case PAGE_SETTINGS: {
        const int16_t cardW = (SCREEN_WIDTH - margin * 3) / 2;
        const int16_t h = 80;
        const int16_t y = headerHeight + margin;
        addBox(vec, "settings.theme", "label", "Theme", margin, y, cardW, h);
        addBox(vec, "settings.brightness", "label", "Brightness", margin + cardW + margin, y, cardW, h);
        const int16_t y2 = y + h + margin;
        addBox(vec, "settings.clock", "label", "Clock", margin, y2, cardW, h);
        addBox(vec, "settings.rotate", "label", "Auto Rotate", margin + cardW + margin, y2, cardW, h);
        break;
      }
      case PAGE_BOILER_CONTROL: {
        // Boiler control page layout for builder
        const int16_t fullW = SCREEN_WIDTH - margin * 2;
        int16_t y = headerHeight + margin;
        
        // Status section
        addBox(vec, "boilerctrl.status", "panel", "Status", margin, y, fullW, 60);
        y += 60 + margin;
        
        // Mode buttons row
        const int16_t btnW = (fullW - margin * 3) / 4;
        addBox(vec, "boilerctrl.btn.off", "button", "OFF", margin, y, btnW, 45);
        addBox(vec, "boilerctrl.btn.manual", "button", "MANUAL", margin + btnW + margin, y, btnW, 45);
        addBox(vec, "boilerctrl.btn.auto", "button", "AUTO", margin + 2*(btnW + margin), y, btnW, 45);
        addBox(vec, "boilerctrl.btn.ai", "button", "AI", margin + 3*(btnW + margin), y, btnW, 45);
        y += 45 + margin * 2;
        
        // Temperature adjustment section
        const int16_t tempSectionH = SCREEN_HEIGHT - y - margin;
        addBox(vec, "boilerctrl.temp", "panel", "Temperature", margin, y, fullW, tempSectionH);
        break;
      }
      default:
        break;
    }
    populated[baseId] = true;
  }

  return cache[baseId];
}
