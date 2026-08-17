#pragma once

#include <Arduino_GFX_Library.h>
#include "../config/config_store.h"

// Forward declarations
void uiInit(Arduino_GFX* gfx);
void uiLoop();

// Color definitions for UI themes
struct UITheme {
  uint16_t background;      // Background color
  uint16_t header;          // Header background
  uint16_t card;            // Card background
  uint16_t cardBorder;      // Card border
  uint16_t text;            // Normal text
  uint16_t textSecondary;   // Secondary text (labels, etc)
  uint16_t accent;          // Accent color (highlights)
  uint16_t warm;            // Warm color (heating, up indicators)
  uint16_t cool;            // Cool color (cooling, down indicators)
  uint16_t warning;         // Warning color
};

// Global UI themes
extern const UITheme THEME_DARK;
extern const UITheme THEME_LIGHT;
extern UITheme* currentTheme;

// Screen dimensions
extern int16_t SCREEN_WIDTH;
extern int16_t SCREEN_HEIGHT;

// UI Page IDs (built-ins)
enum UIPage {
  PAGE_HOME = 0,
  PAGE_DASHBOARD = 1,
  PAGE_STATS = 2,
  PAGE_CONTROLS = 3,
  PAGE_SETTINGS = 4,
  PAGE_BOILER_CONTROL = 5,  // Remote boiler control page
  PAGE_COUNT = 6
};

// Maximum total pages including custom pages
static constexpr uint8_t UI_MAX_TOTAL_PAGES = 12;

// Current page tracking
extern UIPage currentPage;
extern uint8_t currentPageId; // supports both built-in and custom page IDs
// Ordered page sequence (indexes of built-ins 0..PAGE_COUNT-1 and custom >= PAGE_COUNT)
extern uint8_t UI_PAGE_ORDER[UI_MAX_TOTAL_PAGES];
extern uint8_t UI_PAGE_COUNT;

// Touch handling constants (UI navigation)
static constexpr uint16_t UI_NAV_COOLDOWN = 500; // Cooldown period after page navigation

// Functions for page navigation
void navigateToPage(UIPage page);
void navigateToPageId(uint8_t pageId);
void navigateNext();
void navigatePrev();
void navigateNextWithTransition();
void navigatePrevWithTransition();

// Drawing helpers
void drawHeader(const char* title);
void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* title);
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool pressed = false);
void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float progress, uint16_t color);
void drawToggle(int16_t x, int16_t y, bool state);
void drawClock(int16_t x, int16_t y, int16_t size);

// Font scaling helper based on user preferences
uint8_t uiScaleText(uint8_t baseSize);

// Icon drawing helpers
void drawWifiIcon(int16_t x, int16_t y, int16_t size, int strength);
void drawThermoIcon(int16_t x, int16_t y, int16_t size, uint16_t color);
void drawUpDownIcon(int16_t x, int16_t y, int16_t size, bool up);

// Page rendering functions
void drawPageHome();
void drawPageDashboard();
void drawPageStats();
void drawPageControls();
void drawPageSettings();
void drawPageBoilerControl();

// Boiler control touch handler
bool handleBoilerControlTouch(int16_t x, int16_t y);

// Custom pages integration (implemented in custom_pages.cpp)
bool isCustomPageId(uint8_t pageId);
void drawCustomPageById(uint8_t pageId);

// Display accessor for advanced modules
Arduino_GFX* uiGetDisplay();

// Request a UI redraw from data producers
void uiRequestRedraw();

// FIX #4: Request home page update without navigation lock (for real-time data)
void uiRequestHomeUpdate();

// Touch handler type
typedef bool (*UITouchHandlerCallback)(int16_t x, int16_t y);

// Register touch handlers for the current page
void registerTouchHandler(UITouchHandlerCallback handler);

// Invoke all registered touch handlers with current touch position
// Returns true if any handler consumed the touch event
bool invokeTouchHandlers(int16_t x, int16_t y);
void clearTouchHandlers();