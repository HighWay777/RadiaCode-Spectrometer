#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

/**
 * Touch Feedback Effects
 * 
 * Provides visual feedback when the user touches the LCD screen.
 * Includes ripple effects, highlight animations, and touch indicators.
 */

// Feedback type enumeration
enum TouchFeedbackType {
  FEEDBACK_NONE = 0,
  FEEDBACK_RIPPLE,      // Expanding circle ripple effect
  FEEDBACK_HIGHLIGHT,   // Brief highlight/flash effect
  FEEDBACK_DOT          // Simple touch dot indicator
};

class TouchFeedback {
public:
  /**
   * Initialize touch feedback system
   * @param gfx Display pointer
   */
  static void init(Arduino_GFX* gfx);
  
  /**
   * Trigger a touch feedback effect at specified position
   * @param x Touch X coordinate
   * @param y Touch Y coordinate
   * @param type Type of feedback effect (default: RIPPLE)
   */
  static void trigger(int16_t x, int16_t y, TouchFeedbackType type = FEEDBACK_RIPPLE);
  
  /**
   * Trigger a touch feedback on a rectangular area (button press)
   * @param x Button X position
   * @param y Button Y position
   * @param w Button width
   * @param h Button height
   */
  static void triggerButton(int16_t x, int16_t y, int16_t w, int16_t h);
  
  /**
   * Update/animate ongoing feedback effects (call in loop)
   * Returns true if an animation is active
   */
  static bool update();
  
  /**
   * Check if feedback animation is currently active
   */
  static bool isActive();
  
  /**
   * Cancel any ongoing feedback animation
   */
  static void cancel();
  
  /**
   * Set feedback color
   * @param color 16-bit RGB565 color
   */
  static void setColor(uint16_t color);
  
  /**
   * Enable/disable touch feedback globally
   */
  static void setEnabled(bool enabled);
  static bool isEnabled();
  
private:
  static Arduino_GFX* display;
  static bool enabled;
  static bool active;
  
  // Ripple effect state
  static int16_t rippleX, rippleY;
  static uint8_t rippleRadius;
  static uint8_t rippleMaxRadius;
  static uint8_t rippleAlpha;
  static unsigned long rippleStartTime;
  
  // Button highlight state
  static int16_t highlightX, highlightY;
  static int16_t highlightW, highlightH;
  static bool highlightActive;
  static unsigned long highlightStartTime;
  
  // Settings
  static uint16_t feedbackColor;
  static constexpr uint16_t RIPPLE_DURATION_MS = 200;    // Ripple animation duration
  static constexpr uint16_t HIGHLIGHT_DURATION_MS = 100; // Button highlight duration
  static constexpr uint8_t MAX_RIPPLE_RADIUS = 30;       // Maximum ripple size
  
  // Internal draw functions
  static void drawRipple();
  static void drawHighlight();
  static void drawDot();
  static void eraseRipple();
  static void eraseHighlight();
};
