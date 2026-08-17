#include "touch_handler.h"
#include <Wire.h>

// Initialize static members
Touch_GT911 TouchHandlerClass::touchScreen(0, 0, 0, 0, 0, 0);
bool TouchHandlerClass::initialized = false;
bool TouchHandlerClass::touching = false;
uint16_t TouchHandlerClass::touchX = 0;
uint16_t TouchHandlerClass::touchY = 0;
uint16_t TouchHandlerClass::touchStartX = 0;
uint16_t TouchHandlerClass::touchStartY = 0;
unsigned long TouchHandlerClass::touchStartTime = 0;
unsigned long TouchHandlerClass::lastInteractionTime = 0;
unsigned long TouchHandlerClass::lastTouchRead = 0;

// Gesture flags
bool TouchHandlerClass::tapDetected = false;
bool TouchHandlerClass::swipeLeftDetected = false;
bool TouchHandlerClass::swipeRightDetected = false;
bool TouchHandlerClass::swipeUpDetected = false;
bool TouchHandlerClass::swipeDownDetected = false;
uint16_t TouchHandlerClass::screenWidth = 480;
uint16_t TouchHandlerClass::screenHeight = 272;

void TouchHandlerClass::init(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin, uint16_t width, uint16_t height) {
  if (initialized) return;
  
  // Initialize I2C
  Wire.begin(sda, scl);
  Wire.setClock(400000);
  
  // Check which I2C address to use
  uint8_t gt911_addr = GT911_ADDR1;
  Wire.beginTransmission(GT911_ADDR1);
  if (Wire.endTransmission() != 0) {
    gt911_addr = GT911_ADDR2;
  }
  
  // Initialize touchscreen
  touchScreen = Touch_GT911(sda, scl, intPin, rstPin, width, height);
  touchScreen.begin(gt911_addr);
  touchScreen.setRotation(ROTATION_NORMAL);
  
  // Store screen dimensions for coordinate correction
  screenWidth = width;
  screenHeight = height;
  
  initialized = true;
  Serial.printf("Touch handler initialized (addr=0x%02X, rotation=NORMAL)\n", gt911_addr);
}

void TouchHandlerClass::update() {
  if (!initialized) return;
  
  unsigned long now = millis();
  if (now - lastTouchRead < TOUCH_POLL_MS) return;
  lastTouchRead = now;
  
  touchScreen.read();
  
  if (touchScreen.isTouched && touchScreen.touches > 0) {
    // Touch detected - GT911 ROTATION_NORMAL inverts both X and Y
    // But our display doesn't invert either, so we need to un-invert both
    // Display: (0,0) is top-left, X increases right, Y increases down
    touchX = screenWidth - touchScreen.points[0].x;   // Un-invert X
    touchY = screenHeight - touchScreen.points[0].y;  // Un-invert Y
    lastInteractionTime = now;
    
    if (!touching) {
      // Touch just started
      touching = true;
      touchStartX = touchX;
      touchStartY = touchY;
      touchStartTime = now;
      
      // Reset gesture flags
      resetGestures();
    }
  } else {
    // Not touching
    if (touching) {
      // Touch just ended - check for gestures
      touching = false;
      
      // Calculate touch duration and distance
      unsigned long touchDuration = now - touchStartTime;
      int16_t deltaX = touchX - touchStartX;
      int16_t deltaY = touchY - touchStartY;
      
      // Check if it was a tap
      if (touchDuration < TAP_TIMEOUT && abs(deltaX) < TAP_THRESHOLD && abs(deltaY) < TAP_THRESHOLD) {
        tapDetected = true;
      } 
      // Check for swipes
      else {
        // Horizontal swipes take precedence if movement is more horizontal than vertical
        if (abs(deltaX) > abs(deltaY)) {
          if (deltaX > SWIPE_THRESHOLD) {
            swipeRightDetected = true;
          } else if (deltaX < -SWIPE_THRESHOLD) {
            swipeLeftDetected = true;
          }
        } else {
          // Vertical swipes
          if (deltaY > SWIPE_THRESHOLD) {
            swipeDownDetected = true;
          } else if (deltaY < -SWIPE_THRESHOLD) {
            swipeUpDetected = true;
          }
        }
      }
    }
  }
}

bool TouchHandlerClass::isTouching() {
  return touching;
}

uint16_t TouchHandlerClass::getTouchX() {
  return touchX;
}

uint16_t TouchHandlerClass::getTouchY() {
  return touchY;
}

bool TouchHandlerClass::wasTapped() {
  return tapDetected;
}

bool TouchHandlerClass::wasSwipedLeft() {
  return swipeLeftDetected;
}

bool TouchHandlerClass::wasSwipedRight() {
  return swipeRightDetected;
}

bool TouchHandlerClass::wasSwipedUp() {
  return swipeUpDetected;
}

bool TouchHandlerClass::wasSwipedDown() {
  return swipeDownDetected;
}

void TouchHandlerClass::resetGestures() {
  tapDetected = false;
  swipeLeftDetected = false;
  swipeRightDetected = false;
  swipeUpDetected = false;
  swipeDownDetected = false;
}

unsigned long TouchHandlerClass::getLastInteractionTime() {
  return lastInteractionTime;
}