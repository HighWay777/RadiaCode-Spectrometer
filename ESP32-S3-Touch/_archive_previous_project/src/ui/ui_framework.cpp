#include <Arduino.h>
#include <HTTPClient.h>
#include <math.h>
#include <time.h>
#include <vector>
#include "ui_framework.h"
#include "boiler_credentials.h"
#include "../network/network_manager.h"
#include "../network/weather_service.h"
#include "../utils/time_manager.h"
#include "page_home.h"
#include "page_boiler.h"
#include "page_stats.h"
#include "custom_pages.h"
#include "style_overrides.h"
#include "page_transitions.h"

#include <string.h>

// Helper functions for color manipulation
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t alphaBlend(uint16_t fg, uint16_t bg, uint8_t alpha) {
  uint8_t r1 = (fg >> 11) & 0x1F, g1 = (fg >> 5) & 0x3F, b1 = fg & 0x1F;
  uint8_t r2 = (bg >> 11) & 0x1F, g2 = (bg >> 5) & 0x3F, b2 = bg & 0x1F;
  uint8_t r = ((r1 * alpha) + (r2 * (255 - alpha))) / 255;
  uint8_t g = ((g1 * alpha) + (g2 * (255 - alpha))) / 255;
  uint8_t b = ((b1 * alpha) + (b2 * (255 - alpha))) / 255;
  return (r << 11) | (g << 5) | b;
}

// Screen dimensions (updated at runtime from display)
int16_t SCREEN_WIDTH = 480;
int16_t SCREEN_HEIGHT = 272;

// Current page tracking and order
UIPage currentPage = PAGE_HOME;
uint8_t currentPageId = 0;
uint8_t UI_PAGE_ORDER[UI_MAX_TOTAL_PAGES] = {0, 1, 2, 3, 4, 5};  // Added PAGE_BOILER_CONTROL (5)
uint8_t UI_PAGE_COUNT = PAGE_COUNT;

// Theme definitions
const UITheme THEME_DARK = {
  0x0000,   // background: black
  0x001F,   // header: blue
  0x4228,   // card: dark bluish-grey
  0x8430,   // cardBorder: lighter border
  0xFFFF,   // text: white
  0xC618,   // textSecondary: light grey
  0xFDA0,   // accent: orange
  0xF800,   // warm: red
  0x051F,   // cool: blue
  0xFC00    // warning: yellow
};

const UITheme THEME_LIGHT = {
  0xFFFF,   // background: white
  0x03FF,   // header: light blue
  0xD69A,   // card: light grey
  0x8430,   // cardBorder: darker border
  0x0000,   // text: black
  0x8410,   // textSecondary: dark grey
  0xF800,   // accent: red
  0xF800,   // warm: red
  0x051F,   // cool: blue
  0xFC00    // warning: yellow
};

// Pointer to current theme
UITheme* currentTheme = (UITheme*)&THEME_DARK;

static float FONT_SCALE = 1.0f;

static inline uint8_t scaleTextInternal(uint8_t base) {
  if (base == 0) base = 1;
  float scaled = base * FONT_SCALE;
  if (scaled < 1.0f) scaled = 1.0f;
  return (uint8_t)roundf(scaled);
}

uint8_t uiScaleText(uint8_t baseSize) {
  return scaleTextInternal(baseSize);
}

// Touch handling
std::vector<UITouchHandlerCallback> touchHandlers;

// Display pointer
Arduino_GFX* display = nullptr;

// Internal accessor for display (used by custom pages)
Arduino_GFX* uiGetDisplay() { return display; }

void uiRequestRedraw() {
  if (!display) return;
  // Redraw the current page in place
  if (currentPageId < PAGE_COUNT) {
    navigateToPage((UIPage)currentPageId);
  } else if (isCustomPageId(currentPageId)) {
    navigateToPageId(currentPageId);
  }
}

// FIX #4: Request home page update bypassing navigation lock
// OPTIMIZED: Use partial updates instead of full page redraw
// ANTI-FLICKER: Only update panels that have changed data
void uiRequestHomeUpdate() {
  if (!display) return;
  
  // Only update if we're actually on the HOME page
  if (currentPageId == PAGE_HOME) {
    HomeLayoutGeometry layout = getHomeLayoutGeometry();
    int16_t width = 480 - (layout.margin * 2);
    
    // ANTI-FLICKER: Only redraw components that have ACTUALLY changed
    
    // Header - only redraw if API fail count changed
    static int lastApiFailCount = -1;
    int currentApiFailCount = NetworkManager::getApiFailureCount();
    if (lastApiFailCount != currentApiFailCount) {
      lastApiFailCount = currentApiFailCount;
      drawHomeHeader(display, layout.margin, layout.headerY, width, layout.headerHeight);
    }
    
    // Clock time - only update if minute changed
    if (hasClockChanged()) {
      drawClockPanelTime(display, layout.margin, layout.clockY, width, layout.clockHeight);
    }
    
    // Status bar - only update if status data changed
    if (hasStatusBarDataChanged()) {
      drawStatusBar(display, layout.margin, layout.statusY, width, layout.statusHeight);
    }
    
    // Boiler sensor - only update if boiler data changed
    if (hasBoilerDataChanged()) {
      drawBoilerSensorOnly(display, layout.margin, layout.weatherY, width, layout.weatherHeight);
    }
  }
}

// Navigation state variables
unsigned long lastNavigationTime = 0;
bool navLocked = false;

// Auto-rotation variables
bool autoRotateEnabled = false;
unsigned long lastInteractionTime = 0;
unsigned long lastAutoRotateTime = 0;
unsigned long idleTimeoutMs = 15000;       // 15 seconds to idle
unsigned long autoRotateIntervalMs = 8000; // 8 seconds between rotations

// Home page real-time updates (FIX #1: Clock minutes update)
static unsigned long lastHomeRefreshMs = 0;
static int lastHomeMinute = -1;

// Weather API updates (every 5 minutes)
static unsigned long lastWeatherUpdateMs = 0;

void uiInit(Arduino_GFX* gfx) {
  display = gfx;

  // Get screen dimensions
  SCREEN_WIDTH = display->width();
  SCREEN_HEIGHT = display->height();

  // Load configuration for UI settings
  AppConfig* config = ConfigStore::getConfig();

  switch (config->fontSize) {
    case 1: FONT_SCALE = 0.85f; break;
    case 3: FONT_SCALE = 1.15f; break;
    case 4: FONT_SCALE = 1.30f; break;
    default: FONT_SCALE = 1.0f; break;
  }

  // Set theme based on configuration
  currentTheme = (config->theme == 0) ? (UITheme*)&THEME_DARK : (UITheme*)&THEME_LIGHT;

  // Set auto-rotation settings
  autoRotateEnabled = config->autoRotatePages;
  autoRotateIntervalMs = (config->autoRotateTime > 0 ? config->autoRotateTime : 8) * 1000UL;
  idleTimeoutMs = (config->idleTimeout > 0 ? config->idleTimeout : 15) * 1000UL;

  // Set page order (bounded) - validate stored page IDs
  for (int i = 0; i < PAGE_COUNT; i++) {
    uint8_t storedPage = config->pageOrder[i];
    // Validate: must be a valid built-in page ID (0 to PAGE_COUNT-1)
    UI_PAGE_ORDER[i] = (storedPage < PAGE_COUNT) ? storedPage : i;
  }
  // Append custom pages after built-ins
  uint8_t customCount = customPagesCount();
  Serial.printf("[UI] Init: PAGE_COUNT=%d, customCount=%d\n", PAGE_COUNT, customCount);
  for (uint8_t i = 0; i < customCount && (PAGE_COUNT + i) < UI_MAX_TOTAL_PAGES; i++) {
    UI_PAGE_ORDER[PAGE_COUNT + i] = PAGE_COUNT + i; // custom pages IDs contiguous after built-ins
  }
  UI_PAGE_COUNT = min<uint8_t>(PAGE_COUNT + customCount, UI_MAX_TOTAL_PAGES);
  Serial.printf("[UI] Total UI_PAGE_COUNT=%d\n", UI_PAGE_COUNT);

  // Initial screen draw
  display->fillScreen(currentTheme->background);
  UIPage landing = (config->landingPage < PAGE_COUNT) ? (UIPage)config->landingPage : PAGE_HOME;
  navigateToPage(landing);
}

void uiLoop() {
  // FIX #1: Synchronized per-minute refresh for all pages (no flicker)
  // All pages update together when the minute changes
  unsigned long now = millis();
  
  if (display) {
    // Check if 1 minute has passed (60000 ms)
    if (now - lastHomeRefreshMs >= 60000) {
      // Get current minute to detect changes
      time_t t = time(nullptr);
      struct tm timeinfo; localtime_r(&t, &timeinfo);
      int currentMinute = timeinfo.tm_min;
      
      // Only redraw if minute changed (avoid excessive redraws)
      if (currentMinute != lastHomeMinute) {
        lastHomeMinute = currentMinute;
        lastHomeRefreshMs = now;
        
        // Note: API data fetching is handled by NetworkManager::loop() on interval.
        // Calling fetchApiData() here would block the UI loop.
        
        // Weather updates handled by WeatherService::loop() — no forceUpdate needed here.
        
        // OPTIMIZED: Partial updates only - redraw only changed elements, not entire page
        // This eliminates flickering and improves performance
        
        if (currentPageId == PAGE_HOME) {
          // HOME page: Only update elements that have ACTUALLY changed
          HomeLayoutGeometry layout = getHomeLayoutGeometry();
          int16_t width = 480 - (layout.margin * 2);
          
          // ANTI-FLICKER: Check what changed before redrawing
          if (hasClockChanged()) {
            drawClockPanelTime(display, layout.margin, layout.clockY, width, layout.clockHeight);
          }
          
          if (hasStatusBarDataChanged()) {
            drawStatusBar(display, layout.margin, layout.statusY, width, layout.statusHeight);
          }
          
          if (hasBoilerDataChanged()) {
            drawBoilerSensorOnly(display, layout.margin, layout.weatherY, width, layout.weatherHeight);
          }
          
          // Per-5-minute updates (weather info changed) - only redraw full weather panel on weather update
          if ((lastWeatherUpdateMs == now || (lastWeatherUpdateMs > 0 && now - lastWeatherUpdateMs < 1000)) && hasWeatherDataChanged()) {
            drawWeatherPanel(display, layout.margin, layout.weatherY, width, layout.weatherHeight);
          }
        }
        else if (currentPageId == PAGE_DASHBOARD) {
          // DASHBOARD page: Redraw full page (contains cards that are harder to update partially)
          // TODO: In future, create drawPageBoilerDataOnly() for only updating metric values
          AppConfig* config = ConfigStore::getConfig();
          drawPageBoiler(display, config);
        }
        else if (currentPageId == PAGE_STATS) {
          // STATS page: Only update the metric values, not the header or layout
          AppConfig* config = ConfigStore::getConfig();
          drawPageStats();
          // TODO: In future, create drawPageStatsDataOnly() for only updating numbers
        }
        else if (currentPageId == PAGE_BOILER_CONTROL) {
          // BOILER CONTROL page: Don't redraw automatically - only update on user interaction
          // This prevents flickering. The page will refresh when user touches a button.
          // Just update the display values in-place without full redraw
        }
      }
    }
  }
  
  // Check for auto-rotation
  if (autoRotateEnabled) {
    unsigned long now = millis();

    // Check if idle
    if (now - lastInteractionTime > idleTimeoutMs) {
      // Check if time to rotate
      if (now - lastAutoRotateTime > autoRotateIntervalMs) {
        navigateNext();
        lastAutoRotateTime = now;
      }
    }
  }
}

void navigateToPageId(uint8_t pageId) {
  // built-ins
  if (pageId < PAGE_COUNT) {
    navigateToPage((UIPage)pageId);
    return;
  }

  // custom pages
  if (!isCustomPageId(pageId)) return;

  unsigned long now = millis();
  if (navLocked && (now - lastNavigationTime < UI_NAV_COOLDOWN)) {
    return;
  }
  lastNavigationTime = now;
  navLocked = true;
  lastInteractionTime = now;
  currentPageId = pageId;

  clearTouchHandlers();
  display->fillScreen(currentTheme->background);
  drawCustomPageById(pageId);
  delay(50);
  navLocked = false;
}

void navigateToPage(UIPage page) {
  if (page >= PAGE_COUNT) return;

  // Prevent rapid page flipping
  unsigned long now = millis();
  if (navLocked && (now - lastNavigationTime < UI_NAV_COOLDOWN)) {
    return;
  }

  // Update navigation state
  lastNavigationTime = now;
  navLocked = true;
  currentPage = page;
  currentPageId = (uint8_t)page;

  // Record interaction
  lastInteractionTime = now;

  // Clear old touch handlers
  clearTouchHandlers();

  // Clear screen
  display->fillScreen(currentTheme->background);

  // Draw the new page
  switch (page) {
    case PAGE_HOME:
      drawPageHome();
      // Apply overlay widgets for this built-in page
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_HOME));
      break;
    case PAGE_DASHBOARD:
      drawPageDashboard();
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_DASHBOARD));
      break;
    case PAGE_STATS:
      drawPageStats();
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_STATS));
      break;
    case PAGE_CONTROLS:
      drawPageControls();
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_CONTROLS));
      break;
    case PAGE_SETTINGS:
      drawPageSettings();
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_SETTINGS));
      break;
    case PAGE_BOILER_CONTROL:
      drawPageBoilerControl();
      drawBuiltinOverlay(static_cast<uint8_t>(PAGE_BOILER_CONTROL));
      // Register touch handler for boiler control buttons
      registerTouchHandler(handleBoilerControlTouch);
      break;
    default: {
      uint8_t pid = static_cast<uint8_t>(page);
      if (isCustomPageId(pid)) {
        drawCustomPageById(pid);
      }
      break;
    }
  }

  // Reset navigation lock after a short delay
  delay(50);
  navLocked = false;
}

void navigateNext() {
  // Find current index in order
  int idx = 0;
  for (; idx < UI_PAGE_COUNT; idx++) if (UI_PAGE_ORDER[idx] == (uint8_t)currentPage) break;
  int nextIdx = (idx + 1) % UI_PAGE_COUNT;
  navigateToPageId(UI_PAGE_ORDER[nextIdx]);
}

void navigatePrev() {
  int idx = 0;
  for (; idx < UI_PAGE_COUNT; idx++) if (UI_PAGE_ORDER[idx] == (uint8_t)currentPage) break;
  int prevIdx = (idx + UI_PAGE_COUNT - 1) % UI_PAGE_COUNT;
  navigateToPageId(UI_PAGE_ORDER[prevIdx]);
}

// Transition-enabled navigation: slide right when going to next page
void navigateNextWithTransition() {
  // Find next page ID
  int idx = 0;
  for (; idx < UI_PAGE_COUNT; idx++) if (UI_PAGE_ORDER[idx] == currentPageId) break;
  int nextIdx = (idx + 1) % UI_PAGE_COUNT;
  uint8_t nextPageId = UI_PAGE_ORDER[nextIdx];
  
  // Store the target page ID for the callback
  static uint8_t pendingPageId = 0;
  pendingPageId = nextPageId;
  
  // Create callback that draws the new page
  auto drawCallback = []() {
    navigateToPageId(pendingPageId);
  };
  
  // Start slide transition (slides left to reveal next page)
  PageTransitions::start(TRANSITION_SLIDE_LEFT, drawCallback);
}

// Transition-enabled navigation: slide left when going to previous page
void navigatePrevWithTransition() {
  // Find previous page ID
  int idx = 0;
  for (; idx < UI_PAGE_COUNT; idx++) if (UI_PAGE_ORDER[idx] == currentPageId) break;
  int prevIdx = (idx + UI_PAGE_COUNT - 1) % UI_PAGE_COUNT;
  uint8_t prevPageId = UI_PAGE_ORDER[prevIdx];
  
  // Store the target page ID for the callback
  static uint8_t pendingPageId = 0;
  pendingPageId = prevPageId;
  
  // Create callback that draws the new page
  auto drawCallback = []() {
    navigateToPageId(pendingPageId);
  };
  
  // Start slide transition (slides right to reveal previous page)
  PageTransitions::start(TRANSITION_SLIDE_RIGHT, drawCallback);
}

void drawHeader(const char* title) {
  // Draw header background
  display->fillRect(0, 0, SCREEN_WIDTH, 24, currentTheme->header);

  // Draw title text
  display->setFont(nullptr);
  display->setTextColor(currentTheme->text);
  display->setCursor(8, 6);
  display->setTextSize(uiScaleText(2));
  display->print(title);

  // Draw page dots
  int dotSpacing = 10;
  int dotRadius = 3;
  int dotsWidth = UI_PAGE_COUNT * dotSpacing;
  int startX = SCREEN_WIDTH - dotsWidth - 10;

  for (int i = 0; i < UI_PAGE_COUNT; i++) {
    uint8_t pageId = UI_PAGE_ORDER[i];
    uint16_t color = (pageId == currentPageId) ? currentTheme->text : currentTheme->textSecondary;
    display->fillCircle(startX + i * dotSpacing, 12, dotRadius, color);
  }
}

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title) {
  // Draw card background with border
  display->fillRoundRect(x, y, w, h, 8, currentTheme->card);
  display->drawRoundRect(x, y, w, h, 8, currentTheme->cardBorder);

  // Draw title bar
  display->fillRect(x + 1, y + 1, w - 2, 24, currentTheme->header);

  // Draw title text
  display->setFont(nullptr);
  display->setTextColor(currentTheme->text);
  display->setCursor(x + 8, y + 6);
  display->setTextSize(uiScaleText(1));
  display->print(title);
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool pressed) {
  // Choose colors based on state
  uint16_t bgColor = pressed ? currentTheme->accent : currentTheme->card;
  uint16_t borderColor = currentTheme->cardBorder;

  // Draw button
  display->fillRoundRect(x, y, w, h, 8, bgColor);
  display->drawRoundRect(x, y, w, h, 8, borderColor);

  // Draw label
  display->setFont(nullptr);
  uint8_t btnTextSize = uiScaleText(2);
  int16_t textWidth = strlen(label) * 6 * btnTextSize;
  int16_t textHeight = 8 * btnTextSize;
  display->setTextColor(currentTheme->text);
  display->setCursor(x + (w - textWidth) / 2, y + (h - textHeight) / 2);
  display->setTextSize(btnTextSize);
  display->print(label);
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float progress, uint16_t color) {
  // Ensure progress is between 0.0 and 1.0
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;

  // Draw background
  display->drawRoundRect(x, y, w, h, h / 2, currentTheme->cardBorder);

  // Calculate fill width
  int16_t fillWidth = (int16_t)((w - 4) * progress);
  if (fillWidth > 0) {
    display->fillRoundRect(x + 2, y + 2, fillWidth, h - 4, (h - 4) / 2, color);
  }
}

void drawToggle(int16_t x, int16_t y, bool state) {
  int16_t width = 40;
  int16_t height = 20;
  int16_t knobSize = height - 4;

  // Draw background
  uint16_t bgColor = state ? currentTheme->accent : currentTheme->card;
  display->fillRoundRect(x, y, width, height, height / 2, bgColor);
  display->drawRoundRect(x, y, width, height, height / 2, currentTheme->cardBorder);

  // Draw knob
  int16_t knobX = state ? (x + width - knobSize - 2) : (x + 2);
  display->fillCircle(knobX + knobSize / 2, y + height / 2, knobSize / 2, currentTheme->text);
}

void drawClock(int16_t x, int16_t y, int16_t size) {
  // Draw clock face
  display->drawCircle(x, y, size, currentTheme->text);

  // Draw hour markers
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180.0f;
    int16_t x1 = x + (int16_t)((size - 4) * cosf(angle));
    int16_t y1 = y + (int16_t)((size - 4) * sinf(angle));
    int16_t x2 = x + (int16_t)(size * cosf(angle));
    int16_t y2 = y + (int16_t)(size * sinf(angle));
    display->drawLine(x1, y1, x2, y2, currentTheme->text);
  }

  // Draw hands (placeholder)
  display->drawLine(x, y, x + (int16_t)(size * 0.6f), y, currentTheme->accent); // Hour
  display->drawLine(x, y, x, y - (int16_t)(size * 0.8f), currentTheme->text);   // Minute
}

void drawWifiIcon(int16_t x, int16_t y, int16_t size, int strength) {
  // Map strength (usually in dBm) to bars (0-4)
  int bars = 0;

  if (strength > -70) bars = 4;
  else if (strength > -80) bars = 3;
  else if (strength > -90) bars = 2;
  else if (strength > -100) bars = 1;

  // Draw the bars
  int barWidth = size / 6;
  int spacing = barWidth / 2;
  int maxHeight = size;

  for (int i = 0; i < 4; i++) {
    int barHeight = maxHeight * (i + 1) / 4;
    uint16_t color = (i < bars) ? currentTheme->accent : currentTheme->textSecondary;

    display->fillRect(
      x + i * (barWidth + spacing),
      y - barHeight,
      barWidth,
      barHeight,
      color
    );
  }
}

void drawThermoIcon(int16_t x, int16_t y, int16_t size, uint16_t color) {
  // Draw thermometer bulb
  display->fillCircle(x, y + size / 2, size / 3, color);

  // Draw thermometer stem
  display->fillRect(x - size / 8, y - size / 2, size / 4, size, color);

  // Draw the mercury line
  display->fillRect(x - size / 16, y, size / 8, size / 2, 0xF800);
}

void drawUpDownIcon(int16_t x, int16_t y, int16_t size, bool up) {
  int16_t arrowSize = size / 2;
  uint16_t color = up ? currentTheme->warm : currentTheme->cool;

  if (up) {
    // Draw up arrow
    display->fillTriangle(
      x, y - arrowSize,
      x - arrowSize, y + arrowSize,
      x + arrowSize, y + arrowSize,
      color
    );
  } else {
    // Draw down arrow
    display->fillTriangle(
      x, y + arrowSize,
      x - arrowSize, y - arrowSize,
      x + arrowSize, y - arrowSize,
      color
    );
  }
}

// Register touch handlers
void registerTouchHandler(UITouchHandlerCallback handler) {
  touchHandlers.push_back(handler);
}

void clearTouchHandlers() {
  touchHandlers.clear();
}

bool invokeTouchHandlers(int16_t x, int16_t y) {
  for (auto& handler : touchHandlers) {
    if (handler && handler(x, y)) {
      return true;  // Touch was consumed
    }
  }
  return false;  // No handler consumed the touch
}

// Page implementations (wrappers)
void drawPageHome() {
  AppConfig* cfg = ConfigStore::getConfig();
  ::drawPageHome(display, cfg);
}

void drawPageDashboard() {
  AppConfig* cfg = ConfigStore::getConfig();
  drawPageBoiler(display, cfg);
}

void drawPageStats() {
  AppConfig* cfg = ConfigStore::getConfig();
  drawPageStatsEnergy(display, cfg);
}

// Helper to draw a styled card with StyleOverride support
static void drawStyledCard(int16_t x, int16_t y, int16_t w, int16_t h, 
                           const char* title, const char* elementId) {
  if (StyleOverrides::isHidden(elementId)) return;
  
  uint16_t bgColor = StyleOverrides::getColor(elementId, currentTheme->card);
  uint16_t textColor = StyleOverrides::getTextColor(elementId, currentTheme->text);
  
  display->fillRoundRect(x, y, w, h, 8, bgColor);
  display->drawRoundRect(x, y, w, h, 8, currentTheme->cardBorder);
  
  // Draw title bar
  display->fillRect(x + 1, y + 1, w - 2, 24, currentTheme->header);
  
  if (title && title[0]) {
    display->setFont(nullptr);
    display->setTextColor(textColor);
    display->setCursor(x + 8, y + 6);
    display->setTextSize(uiScaleText(1));
    display->print(title);
  }
}

void drawPageControls() {
  display->setTextWrap(false);
  drawHeader("SysInfo");
  display->setFont(nullptr);
  int m = 10; int y = 24 + m; int h = 80; int w = (SCREEN_WIDTH - m*3)/2; int x = m;
  
  // Draw styled cards with StyleOverrides support
  drawStyledCard(x, y, w, h, "WiFi", "controls.wifi");
  drawStyledCard(x + w + m, y, w, h, "IP", "controls.ip");
  drawStyledCard(x, y + h + m, w, h, "Uptime", "controls.uptime");
  drawStyledCard(x + w + m, y + h + m, w, h, "Memory", "controls.memory");

  // Populate
  char buf[64];
  uint8_t labelSize = uiScaleText(1);
  uint8_t valueSize = uiScaleText(2);
  
  // WiFi
  if (!StyleOverrides::isHidden("controls.wifi")) {
    snprintf(buf, sizeof(buf), "%s (%d dBm)", NetworkManager::getSSID().c_str(), NetworkManager::getWiFiRSSI());
    uint16_t labelCol = StyleOverrides::getTextColor("controls.wifi", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("controls.wifi", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + 12, y + 28);
    display->print("SSID / RSSI");
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + 12, y + 42);
    display->print(buf);
  }
  
  // IP
  if (!StyleOverrides::isHidden("controls.ip")) {
    uint16_t labelCol = StyleOverrides::getTextColor("controls.ip", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("controls.ip", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + w + m + 12, y + 28);
    display->print("IPv4");
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + w + m + 12, y + 42);
    display->print(NetworkManager::getLocalIPString());
  }
  
  // Uptime
  if (!StyleOverrides::isHidden("controls.uptime")) {
    uint32_t up = millis()/1000; uint32_t hh = (up/3600)%24; uint32_t mm = (up/60)%60; uint32_t ss = up%60; uint32_t dd = up/86400;
    uint16_t labelCol = StyleOverrides::getTextColor("controls.uptime", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("controls.uptime", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + 12, y + h + m + 28);
    display->print("Uptime");
    snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u", dd, hh, mm, ss);
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + 12, y + h + m + 42);
    display->print(buf);
  }
  
  // Memory
  if (!StyleOverrides::isHidden("controls.memory")) {
    uint16_t labelCol = StyleOverrides::getTextColor("controls.memory", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("controls.memory", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + w + m + 12, y + h + m + 28);
    display->print("Free Heap");
    unsigned freeHeap = (unsigned)ESP.getFreeHeap();
    snprintf(buf, sizeof(buf), "%u KB", freeHeap / 1024);
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + w + m + 12, y + h + m + 42);
    display->print(buf);
  }
}

void drawPageSettings() {
  drawHeader("Settings");
  display->setTextWrap(false);
  display->setFont(nullptr);
  int m = 10; int y = 24 + m; int w = (SCREEN_WIDTH - m*3)/2; int h = 80; int x = m;
  
  // Draw styled cards with StyleOverrides support
  drawStyledCard(x, y, w, h, "Theme", "settings.theme");
  drawStyledCard(x + w + m, y, w, h, "Brightness", "settings.brightness");
  drawStyledCard(x, y + h + m, w, h, "Clock", "settings.clock");
  drawStyledCard(x + w + m, y + h + m, w, h, "Auto-Rotate", "settings.rotate");

  AppConfig* cfg = ConfigStore::getConfig();
  uint8_t labelSize = uiScaleText(1);
  uint8_t valueSize = uiScaleText(2);
  
  // Theme
  if (!StyleOverrides::isHidden("settings.theme")) {
    uint16_t labelCol = StyleOverrides::getTextColor("settings.theme", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("settings.theme", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + 12, y + 28);
    display->print("Current");
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + 12, y + 42);
    display->print(cfg->theme == 0 ? "Dark" : "Light");
  }
  
  // Brightness
  if (!StyleOverrides::isHidden("settings.brightness")) {
    uint16_t labelCol = StyleOverrides::getTextColor("settings.brightness", currentTheme->textSecondary);
    uint16_t valueCol = StyleOverrides::getTextColor("settings.brightness", currentTheme->text);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + w + m + 12, y + 28);
    display->print("Value");
    char buf[16]; snprintf(buf, sizeof(buf), "%u", cfg->brightness);
    display->setTextColor(valueCol);
    display->setTextSize(valueSize);
    display->setCursor(x + w + m + 12, y + 42);
    display->print(buf);
  }
  
  // Clock
  if (!StyleOverrides::isHidden("settings.clock")) {
    uint16_t labelCol = StyleOverrides::getTextColor("settings.clock", currentTheme->textSecondary);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + 12, y + h + m + 28);
    display->print("Show Clock");
    drawToggle(x + w - 40, y + h + m + 20, cfg->showClock);
  }
  
  // Auto-rotate
  if (!StyleOverrides::isHidden("settings.rotate")) {
    uint16_t labelCol = StyleOverrides::getTextColor("settings.rotate", currentTheme->textSecondary);
    display->setTextColor(labelCol);
    display->setTextSize(labelSize);
    display->setCursor(x + w + m + 12, y + h + m + 28);
    display->print("Enabled");
    drawToggle(x + w + m + w - 40, y + h + m + 20, cfg->autoRotatePages);
  }
}

// ========== BOILER CONTROL PAGE ==========
// Provides touch controls for remote boiler management

// Button definitions for boiler control page
struct BoilerButton {
  int16_t x, y, w, h;
  const char* label;
  int action;  // 0=OFF, 1=MAN, 2=AUTO, 3=AI, 10=temp--, 11=temp++
};

// Global button layout (populated in draw function)
static BoilerButton boilerButtons[6];
static int boilerButtonCount = 0;

void drawPageBoilerControl() {
  display->setTextWrap(false);
  drawHeader("Boiler Control");
  display->setFont(nullptr);
  
  // Get current boiler state
  WeatherData* wd = getCurrentWeatherData();
  
  int m = 10;  // margin
  int headerHeight = 24;
  int y = headerHeight + m;
  int x = m;
  int fullW = SCREEN_WIDTH - m * 2;
  
  // === TOP SECTION: Current status display ===
  int statusH = 60;
  
  // Background panel for status
  uint16_t statusBg = alphaBlend(currentTheme->card, currentTheme->background, 220);
  display->fillRoundRect(x, y, fullW, statusH, 8, statusBg);
  display->drawRoundRect(x, y, fullW, statusH, 8, alphaBlend(statusBg, currentTheme->text, 100));
  
  // Current temperature (large, left side) - use ASCII only
  char tempStr[24];
  snprintf(tempStr, sizeof(tempStr), "%.1fC", wd ? wd->currentTemp : 0.0);
  display->setTextColor(currentTheme->text);
  display->setTextSize(3);
  display->setCursor(x + 15, y + 15);
  display->print(tempStr);
  
  // Arrow symbol (use simple ASCII)
  display->setTextSize(2);
  display->setTextColor(currentTheme->textSecondary);
  display->setCursor(x + 120, y + 20);
  display->print(">>");
  
  // Target temp
  snprintf(tempStr, sizeof(tempStr), "%.0fC", wd ? wd->targetTemp : 0.0);
  display->setTextColor(currentTheme->cool);
  display->setTextSize(3);
  display->setCursor(x + 160, y + 15);
  display->print(tempStr);
  
  // Mode display (right side)
  const char* modeStr = "---";
  uint16_t modeColor = currentTheme->textSecondary;
  if (wd) {
    switch (wd->heaterControlMode) {
      case 0: modeStr = "OFF"; modeColor = currentTheme->textSecondary; break;
      case 1: modeStr = "MANUAL"; modeColor = currentTheme->warm; break;
      case 2: modeStr = "AUTO"; modeColor = currentTheme->accent; break;
      case 3: modeStr = "AI"; modeColor = rgb565(0, 200, 0); break;
    }
  }
  
  display->setTextSize(1);
  display->setTextColor(currentTheme->textSecondary);
  int16_t modeX = x + fullW - 70;
  display->setCursor(modeX, y + 8);
  display->print("Mode:");
  display->setTextSize(2);
  display->setTextColor(modeColor);
  display->setCursor(modeX, y + 22);
  display->print(modeStr);
  
  // Storage mode indicator (below mode)
  bool isMqtt = (wd && wd->storageMode.equalsIgnoreCase("MQTT"));
  display->setTextSize(1);
  display->setTextColor(isMqtt ? rgb565(0, 180, 0) : rgb565(200, 150, 0));
  display->setCursor(modeX, y + 45);
  display->print(isMqtt ? "[MQTT]" : "[NVR]");
  
  y += statusH + m;
  
  // === MODE BUTTONS (3 buttons in a row: OFF, MANUAL, AI) ===
  int btnH = 45;
  int btnW = (fullW - m * 2) / 3;  // 3 buttons with 2 gaps
  int btnY = y;
  
  // Button definitions: label, color, API mode string, heaterControlMode value for "active" check
  struct ModeButtonDef {
    const char* label;
    uint16_t color;
    const char* apiMode;
    int activeMode;  // heaterControlMode value that makes this button "active"
  };
  
  ModeButtonDef modeButtons[] = {
    {"OFF",    currentTheme->textSecondary, "off",         0},
    {"MANUAL", currentTheme->warm,          "auto",        1},
    {"AI",     rgb565(0, 200, 0),           "advanced-ai", 3}
  };
  
  boilerButtonCount = 0;
  
  for (int i = 0; i < 3; i++) {
    int bx = x + i * (btnW + m);
    bool isActive = (wd && wd->heaterControlMode == modeButtons[i].activeMode);
    
    // Button background
    uint16_t btnBg = isActive ? alphaBlend(modeButtons[i].color, currentTheme->card, 150) 
                              : alphaBlend(currentTheme->card, currentTheme->background, 200);
    uint16_t btnBorder = isActive ? modeButtons[i].color : currentTheme->cardBorder;
    
    display->fillRoundRect(bx, btnY, btnW, btnH, 6, btnBg);
    display->drawRoundRect(bx, btnY, btnW, btnH, 6, btnBorder);
    if (isActive) {
      display->drawRoundRect(bx + 1, btnY + 1, btnW - 2, btnH - 2, 5, btnBorder);  // Double border for active
    }
    
    // Button text
    display->setTextSize(1);
    display->setTextColor(isActive ? currentTheme->text : currentTheme->textSecondary);
    int16_t textW = strlen(modeButtons[i].label) * 6;  // Approximate width
    display->setCursor(bx + (btnW - textW) / 2, btnY + (btnH - 8) / 2);
    display->print(modeButtons[i].label);
    
    // Store button info for touch handling - action stores index into modeButtons array
    boilerButtons[boilerButtonCount++] = { (int16_t)bx, (int16_t)btnY, (int16_t)btnW, (int16_t)btnH, modeButtons[i].label, i };
  }
  
  y += btnH + m * 2;
  
  // === TEMPERATURE ADJUSTMENT SECTION ===
  int tempSectionH = SCREEN_HEIGHT - y - m;
  
  // Background panel
  display->fillRoundRect(x, y, fullW, tempSectionH, 8, statusBg);
  display->drawRoundRect(x, y, fullW, tempSectionH, 8, alphaBlend(statusBg, currentTheme->text, 100));
  
  // Label
  display->setTextSize(1);
  display->setTextColor(currentTheme->textSecondary);
  display->setCursor(x + 15, y + 8);
  display->print("Target Temperature Adjustment");
  
  // Big temperature display in center (use snprintf for clean output)
  float targetTemp = wd ? wd->targetTemp : 20.0;
  char tempDisplay[16];
  snprintf(tempDisplay, sizeof(tempDisplay), "%.0fC", targetTemp);
  display->setTextSize(4);
  display->setTextColor(currentTheme->cool);
  int16_t tempDispW = strlen(tempDisplay) * 24;  // Approximate width for size 4
  display->setCursor(x + (fullW - tempDispW) / 2, y + 30);
  display->print(tempDisplay);
  
  // Minus button (left)
  int tempBtnW = 80;
  int tempBtnH = tempSectionH - 35;
  int tempBtnY = y + 25;
  int minusBtnX = x + 15;
  int plusBtnX = x + fullW - tempBtnW - 15;
  
  // Minus button
  display->fillRoundRect(minusBtnX, tempBtnY, tempBtnW, tempBtnH, 8, currentTheme->cool);
  display->drawRoundRect(minusBtnX, tempBtnY, tempBtnW, tempBtnH, 8, alphaBlend(currentTheme->cool, currentTheme->text, 150));
  display->setTextSize(4);
  display->setTextColor(currentTheme->text);
  display->setCursor(minusBtnX + tempBtnW/2 - 12, tempBtnY + tempBtnH/2 - 14);
  display->print("-");
  
  // Plus button
  display->fillRoundRect(plusBtnX, tempBtnY, tempBtnW, tempBtnH, 8, currentTheme->warm);
  display->drawRoundRect(plusBtnX, tempBtnY, tempBtnW, tempBtnH, 8, alphaBlend(currentTheme->warm, currentTheme->text, 150));
  display->setTextSize(4);
  display->setTextColor(currentTheme->text);
  display->setCursor(plusBtnX + tempBtnW/2 - 12, tempBtnY + tempBtnH/2 - 14);
  display->print("+");
  
  // Store temperature buttons
  boilerButtons[boilerButtonCount++] = { (int16_t)minusBtnX, (int16_t)tempBtnY, (int16_t)tempBtnW, (int16_t)tempBtnH, "-", 10 };
  boilerButtons[boilerButtonCount++] = { (int16_t)plusBtnX, (int16_t)tempBtnY, (int16_t)tempBtnW, (int16_t)tempBtnH, "+", 11 };
  
  // Note: Touch handler is registered by navigateToPage -> drawPageBoilerControl flow.
  // Since we already have page check in handler, we don't need to re-register on every redraw.
  // Handler registration happens once when page is first navigated to.
}

// Touch handler for boiler control page
bool handleBoilerControlTouch(int16_t x, int16_t y) {
  // Only handle touches if we're on the boiler control page
  if (currentPageId != PAGE_BOILER_CONTROL) return false;
  
  static unsigned long lastTouchTime = 0;
  unsigned long now = millis();
  
  // Debounce - 300ms between touches to prevent double-triggers
  if (now - lastTouchTime < 300) return false;
  
  Serial.printf("[BoilerCtrl] Touch at (%d, %d), checking %d buttons\n", x, y, boilerButtonCount);
  
  // Check each button
  for (int i = 0; i < boilerButtonCount; i++) {
    BoilerButton& btn = boilerButtons[i];
    Serial.printf("[BoilerCtrl] Button %d: x=%d y=%d w=%d h=%d label=%s\n", i, btn.x, btn.y, btn.w, btn.h, btn.label);
    if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
      lastTouchTime = now;
      
      // Remote boiler API credentials from boiler_credentials.h
      
      if (btn.action >= 0 && btn.action <= 2) {
        // Mode change button (action is index: 0=OFF, 1=MANUAL, 2=AI)
        // API expects string: off, auto, advanced-ai
        const char* modeStrings[] = {"off", "auto", "advanced-ai"};
        const char* modeValue = modeStrings[btn.action];
        Serial.printf("[BoilerCtrl] Mode button pressed: %d (%s) -> sending '%s'\n", btn.action, btn.label, modeValue);
        
        // Send mode change directly to remote boiler API
        HTTPClient http;
        AppConfig* cfg = ConfigStore::getConfig();
        String baseUrl = String(BOILER_API_BASE);
        if (cfg && strlen(cfg->dataApiUrl) > 0) {
          baseUrl = String(cfg->dataApiUrl);
          int apiIdx = baseUrl.indexOf("/api/");
          if (apiIdx > 0) {
            baseUrl = baseUrl.substring(0, apiIdx);
          }
        }
        String url = baseUrl + "/api/mode";
        http.begin(url);
        http.setAuthorization(BOILER_USER, BOILER_PASS);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.setTimeout(5000);
        
        String payload = "mode=" + String(modeValue);
        Serial.printf("[BoilerCtrl] POST %s: %s\n", url.c_str(), payload.c_str());
        
        int httpCode = http.POST(payload);
        
        if (httpCode > 0) {
          String response = http.getString();
          Serial.printf("[BoilerCtrl] Mode change response: %d - %s\n", httpCode, response.c_str());
        } else {
          Serial.printf("[BoilerCtrl] Mode change failed: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
        
        // Refresh data (don't redraw entire page - just update will happen on next fetch)
        delay(100);
        NetworkManager::fetchApiData();
        // Redraw just the mode section instead of full page to reduce flicker
        drawPageBoilerControl();
        return true;
      }
      else if (btn.action == 10 || btn.action == 11) {
        // Temperature adjustment
        WeatherData* wd = getCurrentWeatherData();
        float currentTarget = wd ? wd->targetTemp : 20.0;
        float newTarget = (btn.action == 10) ? currentTarget - 1.0 : currentTarget + 1.0;
        
        // Clamp temperature range (15-60C for boiler)
        if (newTarget < 15.0) newTarget = 15.0;
        if (newTarget > 60.0) newTarget = 60.0;
        
        Serial.printf("[BoilerCtrl] Temp button pressed: %s, new target: %.0f\n", btn.label, newTarget);
        
        // Send temperature change directly to remote boiler API
        HTTPClient http;
        AppConfig* cfg = ConfigStore::getConfig();
        String baseUrl = String(BOILER_API_BASE);
        if (cfg && strlen(cfg->dataApiUrl) > 0) {
          baseUrl = String(cfg->dataApiUrl);
          int apiIdx = baseUrl.indexOf("/api/");
          if (apiIdx > 0) {
            baseUrl = baseUrl.substring(0, apiIdx);
          }
        }
        String url = baseUrl + "/api/temperature";
        http.begin(url);
        http.setAuthorization(BOILER_USER, BOILER_PASS);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.setTimeout(5000);
        
        String payload = "temperature=" + String((int)newTarget);
        Serial.printf("[BoilerCtrl] POST %s: %s\n", url.c_str(), payload.c_str());
        
        int httpCode = http.POST(payload);
        
        if (httpCode > 0) {
          String response = http.getString();
          Serial.printf("[BoilerCtrl] Temp change response: %d - %s\n", httpCode, response.c_str());
        } else {
          Serial.printf("[BoilerCtrl] Temp change failed: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
        
        // Refresh data (don't redraw entire page - just update will happen on next fetch)
        delay(100);
        NetworkManager::fetchApiData();
        // Redraw just the page content instead of full navigate to reduce flicker
        drawPageBoilerControl();
        return true;
      }
    }
  }
  
  Serial.println("[BoilerCtrl] Touch not on any button");
  return false;  // Touch not handled
}