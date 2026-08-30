#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

/**
 * Page Transition Effects
 * 
 * Provides smooth animated transitions between pages.
 * Uses hardware-accelerated methods where possible.
 */

enum TransitionType {
  TRANSITION_NONE = 0,      // Instant switch (no animation)
  TRANSITION_FADE,          // Fade out old, fade in new
  TRANSITION_SLIDE_LEFT,    // Slide from right to left (for next page)
  TRANSITION_SLIDE_RIGHT,   // Slide from left to right (for previous page)
  TRANSITION_SLIDE_UP,      // Slide up
  TRANSITION_SLIDE_DOWN     // Slide down
};

class PageTransitions {
public:
  /**
   * Initialize transitions system
   * @param gfx Display pointer
   */
  static void init(Arduino_GFX* gfx);
  
  /**
   * Start a transition animation
   * @param type Type of transition
   * @param drawNewPage Callback to draw the new page content
   */
  static void start(TransitionType type, void (*drawNewPage)());
  
  /**
   * Update the transition animation (call each frame)
   * Returns true if transition is still in progress
   */
  static bool update();
  
  /**
   * Check if a transition is currently active
   */
  static bool isActive();
  
  /**
   * Set transition speed (duration in ms)
   */
  static void setDuration(uint16_t ms);
  
  /**
   * Enable/disable transitions globally
   */
  static void setEnabled(bool enabled);
  static bool isEnabled();
  
  /**
   * Cancel current transition and skip to end state
   */
  static void skip();
  
private:
  static Arduino_GFX* display;
  static bool enabled;
  static bool active;
  static TransitionType currentType;
  static void (*newPageCallback)();
  
  // Animation state
  static unsigned long startTime;
  static uint16_t duration;
  static uint8_t progress; // 0-255
  
  // Screen dimensions
  static int16_t screenWidth;
  static int16_t screenHeight;
  
  // Internal animation functions
  static void animateFade();
  static void animateSlideLeft();
  static void animateSlideRight();
  static void animateSlideUp();
  static void animateSlideDown();
  
  // Helper to calculate eased progress
  static uint8_t easeInOut(uint8_t t);
};
