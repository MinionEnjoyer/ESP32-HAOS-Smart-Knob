#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class InputManager {
private:
  // Encoder state
  volatile int encoderPosition = 0;
  volatile bool encoderPressed = false;
  volatile unsigned long lastEncoderTime = 0;
  volatile unsigned long lastPressTime = 0;
  
  int lastProcessedPosition = 0;
  unsigned long lastActivityTime = 0;
  
  // Touch state
  int16_t touchStartX = -1;
  int16_t touchStartY = -1;
  int16_t lastTouchX = -1;
  int16_t lastTouchY = -1;
  bool touchActive = false;
  bool swipeProcessed = false;
  
  static InputManager* instance;
  
  // ISR handlers - removed IRAM_ATTR to avoid linker issues
  static void handleEncoderISR() {
    if (instance) {
      unsigned long now = millis();
      // Debounce: ignore encoder changes within 10ms of last change
      if (now - instance->lastEncoderTime < 10) {
        return;
      }
      instance->lastEncoderTime = now;
      
      // Simplified encoder reading - just increment/decrement based on A pin state
      int aState = digitalRead(ENCODER_A_PIN);
      int bState = digitalRead(ENCODER_B_PIN);
      
      if (aState != bState) {
        instance->encoderPosition++;
      } else {
        instance->encoderPosition--;
      }
    }
  }
  
  static void handleButtonISR() {
    if (instance) {
      unsigned long now = millis();
      // Debounce button and only trigger on press (LOW), not release
      if (digitalRead(ENCODER_SWITCH_PIN) == LOW) {
        // Only register if enough time has passed since last press (250ms debounce)
        if (now - instance->lastPressTime > 250) {
          instance->encoderPressed = true;
          instance->lastPressTime = now;
        }
      }
    }
  }

public:
  InputManager() {
    instance = this;
  }
  
  void begin() {
    // Setup encoder pins
    pinMode(ENCODER_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_B_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SWITCH_PIN, INPUT_PULLUP);
    
    // Attach interrupts
    attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_SWITCH_PIN), handleButtonISR, CHANGE);
    
    lastActivityTime = millis();
    
    Serial.println("Input Manager initialized");
  }
  
  // Returns encoder delta since last call
  int getEncoderDelta() {
    int delta = encoderPosition - lastProcessedPosition;
    lastProcessedPosition = encoderPosition;
    
    if (delta != 0) {
      lastActivityTime = millis();
    }
    
    return delta;
  }
  
  bool wasEncoderPressed() {
    if (encoderPressed) {
      encoderPressed = false;
      lastActivityTime = millis();
      return true;
    }
    return false;
  }
  
  // Touch handling
  void updateTouch(bool touched, int16_t x, int16_t y) {
    if (touched && !touchActive) {
      // Touch started - record start position
      touchStartX = x;
      touchStartY = y;
      lastTouchX = x;
      lastTouchY = y;
      touchActive = true;
      swipeProcessed = false;
      lastActivityTime = millis();
    } else if (!touched && touchActive) {
      // Touch ended
      touchActive = false;
    } else if (touched && touchActive) {
      // Touch drag - update current position
      lastTouchX = x;
      lastTouchY = y;
      lastActivityTime = millis();
    }
  }
  
  bool isTouchActive() {
    return touchActive;
  }
  
  // Get horizontal swipe (left/right for device switching)
  // Returns: -1 = left, 0 = none, 1 = right
  int getHorizontalSwipe() {
    if (!touchActive && touchStartX != -1 && !swipeProcessed) {
      int16_t deltaX = lastTouchX - touchStartX;
      int16_t deltaY = abs(lastTouchY - touchStartY);
      
      // Horizontal swipe: X movement > 50 pixels AND more horizontal than vertical
      if (abs(deltaX) > 50 && abs(deltaX) > deltaY) {
        touchStartX = -1;
        touchStartY = -1;
        swipeProcessed = true;
        return (deltaX > 0) ? 1 : -1;
      }
    }
    return 0;
  }
  
  // Get vertical swipe (up/down for settings switching)
  // Returns: -1 = up, 0 = none, 1 = down
  int getVerticalSwipe() {
    if (!touchActive && touchStartY != -1 && !swipeProcessed) {
      int16_t deltaX = abs(lastTouchX - touchStartX);
      int16_t deltaY = lastTouchY - touchStartY;
      
      // Vertical swipe: Y movement > 50 pixels AND more vertical than horizontal
      if (abs(deltaY) > 50 && abs(deltaY) > deltaX) {
        touchStartX = -1;
        touchStartY = -1;
        swipeProcessed = true;
        return (deltaY > 0) ? 1 : -1;
      }
    }
    return 0;
  }
  
  // Idle timeout check
  bool isIdle() {
    return (millis() - lastActivityTime) > IDLE_TIMEOUT_MS;
  }
  
  void resetActivityTimer() {
    lastActivityTime = millis();
  }
  
  unsigned long getTimeSinceActivity() {
    return millis() - lastActivityTime;
  }
};

// Static instance pointer
InputManager* InputManager::instance = nullptr;

#endif // INPUT_MANAGER_H
