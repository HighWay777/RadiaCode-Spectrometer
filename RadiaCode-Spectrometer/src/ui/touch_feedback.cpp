#include "touch_feedback.h"
#include "ui_theme.h"

// Static member initialization
Arduino_GFX* TouchFeedback::display = nullptr;
bool TouchFeedback::enabled = true;
bool TouchFeedback::active = false;

// Ripple effect state
int16_t TouchFeedback::rippleX = 0;
int16_t TouchFeedback::rippleY = 0;
uint8_t TouchFeedback::rippleRadius = 0;
uint8_t TouchFeedback::rippleMaxRadius = MAX_RIPPLE_RADIUS;
uint8_t TouchFeedback::rippleAlpha = 255;
unsigned long TouchFeedback::rippleStartTime = 0;

// Button highlight state
int16_t TouchFeedback::highlightX = 0;
int16_t TouchFeedback::highlightY = 0;
int16_t TouchFeedback::highlightW = 0;
int16_t TouchFeedback::highlightH = 0;
bool TouchFeedback::highlightActive = false;
unsigned long TouchFeedback::highlightStartTime = 0;

// Settings
uint16_t TouchFeedback::feedbackColor = 0xFFFF; // White by default

void TouchFeedback::init(Arduino_GFX* gfx) {
  display = gfx;
  enabled = true;
  active = false;
  
  // Get accent color from theme if available
  if (currentTheme) {
    feedbackColor = currentTheme->accent;
  }
}

void TouchFeedback::trigger(int16_t x, int16_t y, TouchFeedbackType type) {
  if (!enabled || !display) return;
  
  // Cancel any previous animation
  cancel();
  
  switch (type) {
    case FEEDBACK_RIPPLE:
      rippleX = x;
      rippleY = y;
      rippleRadius = 2;
      rippleMaxRadius = MAX_RIPPLE_RADIUS;
      rippleAlpha = 200;
      rippleStartTime = millis();
      active = true;
      break;
      
    case FEEDBACK_DOT:
      // Simple dot - immediate draw, no animation
      display->fillCircle(x, y, 4, feedbackColor);
      // Schedule erase after short delay
      rippleX = x;
      rippleY = y;
      rippleStartTime = millis();
      active = true;
      break;
      
    case FEEDBACK_HIGHLIGHT:
    case FEEDBACK_NONE:
    default:
      break;
  }
}

void TouchFeedback::triggerButton(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!enabled || !display) return;
  
  cancel();
  
  highlightX = x;
  highlightY = y;
  highlightW = w;
  highlightH = h;
  highlightActive = true;
  highlightStartTime = millis();
  active = true;
  
  // Draw immediate highlight overlay
  drawHighlight();
}

bool TouchFeedback::update() {
  if (!active || !display) return false;
  
  unsigned long now = millis();
  
  // Update ripple animation
  if (rippleStartTime > 0) {
    unsigned long elapsed = now - rippleStartTime;
    
    if (elapsed >= RIPPLE_DURATION_MS) {
      // Animation complete - cleanup
      rippleStartTime = 0;
      active = false;
      return false;
    }
    
    // Calculate animation progress (0.0 to 1.0)
    float progress = (float)elapsed / (float)RIPPLE_DURATION_MS;
    
    // Expand radius
    rippleRadius = (uint8_t)(2 + (rippleMaxRadius - 2) * progress);
    
    // Fade out alpha
    rippleAlpha = (uint8_t)(200 * (1.0f - progress));
    
    // Draw ripple frame
    drawRipple();
    
    return true;
  }
  
  // Update button highlight
  if (highlightActive) {
    unsigned long elapsed = now - highlightStartTime;
    
    if (elapsed >= HIGHLIGHT_DURATION_MS) {
      // Highlight complete - erase and cleanup
      eraseHighlight();
      highlightActive = false;
      active = false;
      return false;
    }
    
    return true;
  }
  
  return false;
}

bool TouchFeedback::isActive() {
  return active;
}

void TouchFeedback::cancel() {
  if (highlightActive) {
    eraseHighlight();
  }
  active = false;
  rippleStartTime = 0;
  highlightActive = false;
}

void TouchFeedback::setColor(uint16_t color) {
  feedbackColor = color;
}

void TouchFeedback::setEnabled(bool en) {
  enabled = en;
  if (!enabled) {
    cancel();
  }
}

bool TouchFeedback::isEnabled() {
  return enabled;
}

// Internal draw functions

void TouchFeedback::drawRipple() {
  if (!display) return;
  
  // Calculate faded color based on alpha
  // Simple approach: blend with background
  uint16_t bgColor = currentTheme ? currentTheme->background : 0x0000;
  
  // Extract RGB components
  uint8_t r1 = (feedbackColor >> 11) & 0x1F;
  uint8_t g1 = (feedbackColor >> 5) & 0x3F;
  uint8_t b1 = feedbackColor & 0x1F;
  
  uint8_t r2 = (bgColor >> 11) & 0x1F;
  uint8_t g2 = (bgColor >> 5) & 0x3F;
  uint8_t b2 = bgColor & 0x1F;
  
  // Blend based on alpha
  uint8_t blend = rippleAlpha;
  uint8_t r = ((r1 * blend) + (r2 * (255 - blend))) / 255;
  uint8_t g = ((g1 * blend) + (g2 * (255 - blend))) / 255;
  uint8_t b = ((b1 * blend) + (b2 * (255 - blend))) / 255;
  
  uint16_t blendedColor = (r << 11) | (g << 5) | b;
  
  // Draw expanding circle outline (not filled to minimize overdraw)
  display->drawCircle(rippleX, rippleY, rippleRadius, blendedColor);
  
  // Draw inner ring for more visible effect
  if (rippleRadius > 3) {
    display->drawCircle(rippleX, rippleY, rippleRadius - 2, blendedColor);
  }
}

void TouchFeedback::drawHighlight() {
  if (!display) return;
  
  // Draw semi-transparent overlay on the button area
  // Using XOR-style highlight for visibility without storing background
  
  // Draw highlight border
  display->drawRoundRect(highlightX, highlightY, highlightW, highlightH, 4, feedbackColor);
  display->drawRoundRect(highlightX + 1, highlightY + 1, highlightW - 2, highlightH - 2, 3, feedbackColor);
}

void TouchFeedback::drawDot() {
  if (!display) return;
  display->fillCircle(rippleX, rippleY, 4, feedbackColor);
}

void TouchFeedback::eraseRipple() {
  // Ripple effect is transient and blends with background
  // No explicit erase needed - the next frame will overwrite
}

void TouchFeedback::eraseHighlight() {
  if (!display) return;
  
  // Erase the highlight border by redrawing with cardBorder and card background colors
  uint16_t borderColor = currentTheme ? currentTheme->cardBorder : 0x0000;
  uint16_t bgColor = currentTheme ? currentTheme->card : 0x0000;
  
  display->drawRoundRect(highlightX, highlightY, highlightW, highlightH, 4, borderColor);
  display->drawRoundRect(highlightX + 1, highlightY + 1, highlightW - 2, highlightH - 2, 3, bgColor);
}
