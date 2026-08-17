#pragma once

#include <Arduino.h>
#include <Touch_GT911.h>

// Forward declaration to avoid conflict with the class name
class TouchHandlerClass {
public:
  // Initialize the touch handler
  static void init(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin, uint16_t width, uint16_t height);
  
  // Update touch state in the main loop
  static void update();
  
  // Get touch status
  static bool isTouching();
  
  // Get touch coordinates
  static uint16_t getTouchX();
  static uint16_t getTouchY();
  
  // Get gesture information
  static bool wasTapped();
  static bool wasSwipedLeft();
  static bool wasSwipedRight();
  static bool wasSwipedUp();
  static bool wasSwipedDown();
  
  // Reset gesture flags (call after handling)
  static void resetGestures();
  
  // Last interaction time
  static unsigned long getLastInteractionTime();
  
private:
  static Touch_GT911 touchScreen;
  static bool initialized;
  static bool touching;
  static uint16_t touchX;
  static uint16_t touchY;
  static uint16_t touchStartX;
  static uint16_t touchStartY;
  static unsigned long touchStartTime;
  static unsigned long lastInteractionTime;
  static unsigned long lastTouchRead;
  
  // Gesture flags
  static bool tapDetected;
  static bool swipeLeftDetected;
  static bool swipeRightDetected;
  static bool swipeUpDetected;
  static bool swipeDownDetected;
  
  // Screen dimensions for coordinate transformation
  static uint16_t screenWidth;
  static uint16_t screenHeight;
  
  // Constants
  static constexpr uint32_t TOUCH_POLL_MS = 15;      // ~66 Hz polling rate
  static constexpr uint16_t SWIPE_THRESHOLD = 40;    // Minimum distance for swipe
  static constexpr uint16_t TAP_THRESHOLD = 12;      // Maximum movement for tap
  static constexpr uint16_t TAP_TIMEOUT = 300;       // Maximum tap duration in ms
};

// Create a type alias for backward compatibility
typedef TouchHandlerClass TouchHandler;