#include "page_transitions.h"
#include "ui_theme.h"

// Static member initialization
Arduino_GFX* PageTransitions::display = nullptr;
bool PageTransitions::enabled = true;
bool PageTransitions::active = false;
TransitionType PageTransitions::currentType = TRANSITION_NONE;
void (*PageTransitions::newPageCallback)() = nullptr;

unsigned long PageTransitions::startTime = 0;
uint16_t PageTransitions::duration = 150; // 150ms default
uint8_t PageTransitions::progress = 0;

int16_t PageTransitions::screenWidth = 480;
int16_t PageTransitions::screenHeight = 272;

void PageTransitions::init(Arduino_GFX* gfx) {
  display = gfx;
  if (display) {
    screenWidth = display->width();
    screenHeight = display->height();
  }
  enabled = true;
  active = false;
}

void PageTransitions::start(TransitionType type, void (*drawNewPage)()) {
  if (!display || !drawNewPage) return;
  
  // If transitions disabled or type is NONE, just draw directly
  if (!enabled || type == TRANSITION_NONE) {
    drawNewPage();
    return;
  }
  
  currentType = type;
  newPageCallback = drawNewPage;
  startTime = millis();
  progress = 0;
  active = true;
  
  // For slide transitions, we need to execute the transition frames
  // Since we can't double-buffer easily, we'll use a simplified approach
  
  // Start the animation
  update();
}

bool PageTransitions::update() {
  if (!active || !display || !newPageCallback) return false;
  
  unsigned long now = millis();
  unsigned long elapsed = now - startTime;
  
  if (elapsed >= duration) {
    // Transition complete
    active = false;
    progress = 255;
    
    // Draw final state
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
    
    return false;
  }
  
  // Calculate linear progress (0-255)
  progress = (uint8_t)((elapsed * 255) / duration);
  
  // Apply easing
  uint8_t easedProgress = easeInOut(progress);
  
  // Execute animation based on type
  switch (currentType) {
    case TRANSITION_FADE:
      animateFade();
      break;
      
    case TRANSITION_SLIDE_LEFT:
      animateSlideLeft();
      break;
      
    case TRANSITION_SLIDE_RIGHT:
      animateSlideRight();
      break;
      
    case TRANSITION_SLIDE_UP:
      animateSlideUp();
      break;
      
    case TRANSITION_SLIDE_DOWN:
      animateSlideDown();
      break;
      
    default:
      break;
  }
  
  return true;
}

bool PageTransitions::isActive() {
  return active;
}

void PageTransitions::setDuration(uint16_t ms) {
  duration = ms;
}

void PageTransitions::setEnabled(bool en) {
  enabled = en;
  if (!enabled && active) {
    skip();
  }
}

bool PageTransitions::isEnabled() {
  return enabled;
}

void PageTransitions::skip() {
  if (!active) return;
  
  active = false;
  progress = 255;
  
  // Draw final state immediately
  if (newPageCallback && display) {
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
  }
}

// Easing function: ease-in-out (smooth acceleration and deceleration)
uint8_t PageTransitions::easeInOut(uint8_t t) {
  // Quadratic ease-in-out
  // For t in 0-255 range
  if (t < 128) {
    // Ease in: 2*t^2 (scaled)
    return (uint8_t)((2UL * t * t) / 255);
  } else {
    // Ease out: 1 - 2*(1-t)^2 (scaled)
    uint16_t inv = 255 - t;
    return (uint8_t)(255 - ((2UL * inv * inv) / 255));
  }
}

// Animation implementations
// Note: These are simplified versions since we can't easily store old frame

void PageTransitions::animateFade() {
  // Simplified fade: quick fade to black then draw new content on completion
  uint16_t fadeColor = 0x0000; // Black
  
  if (progress < 180) {
    // Phase 1: Draw semi-transparent overlay (50% dither)
    for (int16_t y = 0; y < screenHeight; y += 8) {
      for (int16_t x = 0; x < screenWidth; x += 8) {
        if (((x + y) / 8) % 2 == 0) {
          display->fillRect(x, y, 4, 4, fadeColor);
        }
      }
    }
  } else {
    // Phase 2: Fade completely to black
    display->fillScreen(fadeColor);
  }
}

void PageTransitions::animateSlideLeft() {
  // Simplified slide: wipe effect from right to left
  // Draw new content progressively from right side
  
  int16_t revealX = screenWidth - (progress * screenWidth / 255);
  
  if (revealX <= 0) {
    // Fully revealed
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
    active = false;
    return;
  }
  
  // Draw reveal line (visual indicator of transition)
  uint16_t lineColor = currentTheme ? currentTheme->accent : 0xFFFF;
  display->drawFastVLine(revealX, 0, screenHeight, lineColor);
  display->drawFastVLine(revealX + 1, 0, screenHeight, lineColor);
  
  // Clear area behind the line (where new content will appear)
  uint16_t bgColor = currentTheme ? currentTheme->background : 0x0000;
  display->fillRect(revealX + 2, 0, screenWidth - revealX - 2, screenHeight, bgColor);
}

void PageTransitions::animateSlideRight() {
  // Simplified slide: wipe effect from left to right
  
  int16_t revealX = (progress * screenWidth / 255);
  
  if (revealX >= screenWidth) {
    // Fully revealed
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
    active = false;
    return;
  }
  
  // Draw reveal line
  uint16_t lineColor = currentTheme ? currentTheme->accent : 0xFFFF;
  display->drawFastVLine(revealX, 0, screenHeight, lineColor);
  display->drawFastVLine(revealX - 1, 0, screenHeight, lineColor);
  
  // Clear area behind the line
  uint16_t bgColor = currentTheme ? currentTheme->background : 0x0000;
  display->fillRect(0, 0, revealX - 1, screenHeight, bgColor);
}

void PageTransitions::animateSlideUp() {
  // Simplified slide: wipe effect from bottom to top
  
  int16_t revealY = screenHeight - (progress * screenHeight / 255);
  
  if (revealY <= 0) {
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
    active = false;
    return;
  }
  
  uint16_t lineColor = currentTheme ? currentTheme->accent : 0xFFFF;
  display->drawFastHLine(0, revealY, screenWidth, lineColor);
  display->drawFastHLine(0, revealY + 1, screenWidth, lineColor);
  
  uint16_t bgColor = currentTheme ? currentTheme->background : 0x0000;
  display->fillRect(0, revealY + 2, screenWidth, screenHeight - revealY - 2, bgColor);
}

void PageTransitions::animateSlideDown() {
  // Simplified slide: wipe effect from top to bottom
  
  int16_t revealY = (progress * screenHeight / 255);
  
  if (revealY >= screenHeight) {
    display->fillScreen(currentTheme ? currentTheme->background : 0x0000);
    newPageCallback();
    active = false;
    return;
  }
  
  uint16_t lineColor = currentTheme ? currentTheme->accent : 0xFFFF;
  display->drawFastHLine(0, revealY, screenWidth, lineColor);
  display->drawFastHLine(0, revealY - 1, screenWidth, lineColor);
  
  uint16_t bgColor = currentTheme ? currentTheme->background : 0x0000;
  display->fillRect(0, 0, screenWidth, revealY - 1, bgColor);
}
