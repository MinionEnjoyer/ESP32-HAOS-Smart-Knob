#ifndef LED_RING_CONTROLLER_H
#define LED_RING_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

class LEDRingController {
private:
  Adafruit_NeoPixel strip;
  uint16_t currentHue = 0;
  uint8_t currentBrightness = 128;
  UIMode currentMode = MODE_CLOCK;

public:
  LEDRingController() : strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800) {}
  
  void begin() {
    strip.begin();
    strip.setBrightness(25);  // Global brightness (10%)
    strip.clear();
    strip.show();
    Serial.println("LED Ring initialized");
  }
  
  // Convert HSV to RGB
  uint32_t hsvToRgb(uint16_t hue, uint8_t sat, uint8_t val) {
    uint8_t r, g, b;
    
    // Normalize hue to 0-255 range
    uint8_t h = (hue * 255) / 360;
    uint8_t s = (sat * 255) / 100;
    uint8_t v = val;
    
    if (s == 0) {
      r = g = b = v;
    } else {
      uint8_t region = h / 43;
      uint8_t remainder = (h - (region * 43)) * 6;
      
      uint8_t p = (v * (255 - s)) >> 8;
      uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
      uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
      
      switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
      }
    }
    
    return strip.Color(r, g, b);
  }
  
  void setMode(UIMode mode) {
    currentMode = mode;
    update(currentHue, currentBrightness);
  }
  
  void update(uint16_t hue, uint8_t brightness) {
    currentHue = hue;
    currentBrightness = brightness;
    
    switch (currentMode) {
      case MODE_CLOCK:
        showClockMode();
        break;
      case MODE_BRIGHTNESS:
        showBrightnessMode(brightness);
        break;
      case MODE_COLOR:
        showColorMode(hue);
        break;
    }
  }
  
  void showClockMode() {
    // Subtle pulsing white for clock mode
    static unsigned long lastPulse = 0;
    static uint8_t pulseValue = 0;
    static int8_t pulseDirection = 1;
    
    if (millis() - lastPulse > 50) {
      lastPulse = millis();
      pulseValue += pulseDirection * 5;
      if (pulseValue >= 50 || pulseValue <= 10) {
        pulseDirection *= -1;
      }
    }
    
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(pulseValue, pulseValue, pulseValue));
    }
    strip.show();
  }
  
  void showBrightnessMode(uint8_t brightness) {
    // Show brightness as white LED bar
    // Map 0-255 brightness to 0-5 LEDs
    int activeLEDs = map(brightness, 0, 255, 0, LED_COUNT);
    uint8_t whiteLevel = map(brightness, 0, 255, 0, 200);
    
    for (int i = 0; i < LED_COUNT; i++) {
      if (i < activeLEDs) {
        strip.setPixelColor(i, strip.Color(whiteLevel, whiteLevel, whiteLevel));
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
  }
  
  void showColorMode(uint16_t hue) {
    // Show current hue color on all LEDs
    uint32_t color = hsvToRgb(hue, 100, 200);
    
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }
  
  void clear() {
    strip.clear();
    strip.show();
  }
  
  void setBrightness(uint8_t brightness) {
    strip.setBrightness(brightness);
    strip.show();
  }
  
  // Animation for mode transition
  void animateTransition() {
    for (int brightness = 0; brightness <= 50; brightness += 10) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, brightness, brightness));
      }
      strip.show();
      delay(20);
    }
    for (int brightness = 50; brightness >= 0; brightness -= 10) {
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, brightness, brightness));
      }
      strip.show();
      delay(20);
    }
  }
};

#endif // LED_RING_CONTROLLER_H
