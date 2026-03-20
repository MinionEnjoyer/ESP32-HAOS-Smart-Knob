#ifndef HA_LIGHT_MANAGER_H
#define HA_LIGHT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "HARestAPIClient.h"

class HALightManager {
private:
  PubSubClient* mqttClient;
  LightState currentState;
  String selectedEntityId;
  
  unsigned long lastPublishTime = 0;
  bool pendingUpdate = false;
  uint8_t pendingBrightness = 255;
  uint16_t pendingHue = 0;
  
  // MQTT Topics
  String stateTopic;
  String commandTopic;
  String availabilityTopic;
  
  bool lightsDiscovered = false;
  HARestAPIClient* restAPIClient = nullptr;

public:
  HALightManager(PubSubClient* client) : mqttClient(client) {}
  
  // Set REST API client for fallback
  void setRestAPIClient(HARestAPIClient* client) {
    restAPIClient = client;
  }
  
  // Update state from REST API
  void updateFromRestAPI(LightState* state) {
    currentState.on = state->on;
    currentState.brightness = state->brightness;
    currentState.hue = state->hue;
    currentState.saturation = state->saturation;
    currentState.available = state->available;
    currentState.supportsColor = state->supportsColor;
    currentState.supportsBrightness = state->supportsBrightness;
    
    if (!lightsDiscovered && state->available) {
      lightsDiscovered = true;
      Serial.println("✓ Light discovered via REST API!");
    }
  }
  
  void begin() {
    selectedEntityId = HA_PREFERRED_LIGHT_ENTITY;
    
    // Build MQTT topics for the selected entity
    String entityName = selectedEntityId;
    entityName.replace("light.", "");
    
    stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/light/" + entityName + "/state";
    commandTopic = String(MQTT_DISCOVERY_PREFIX) + "/light/" + entityName + "/set";
    availabilityTopic = String(MQTT_DISCOVERY_PREFIX) + "/light/" + entityName + "/availability";
    
    Serial.println("\n=== HALightManager Initialized ===");
    Serial.print("Target Entity: ");
    Serial.println(selectedEntityId);
    Serial.print("State Topic: ");
    Serial.println(stateTopic);
    Serial.print("Command Topic: ");
    Serial.println(commandTopic);
  }
  
  void subscribeToTopics() {
    if (!mqttClient->connected()) return;
    
    // Subscribe to state updates
    mqttClient->subscribe(stateTopic.c_str());
    mqttClient->subscribe(availabilityTopic.c_str());
    
    Serial.println("Subscribed to HA light topics");
    
    // Request current state
    requestStateUpdate();
  }
  
  void requestStateUpdate() {
    // In HA MQTT, we can't directly request state, but we subscribe to state topic
    // The state will be published when the light changes
    Serial.println("Waiting for light state updates from HA...");
  }
  
  void handleMQTTMessage(String topic, byte* payload, unsigned int length) {
    // Parse JSON payload
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      return;
    }
    
    // Handle state topic
    if (topic == stateTopic) {
      Serial.println("Received light state update:");
      
      if (doc.containsKey("state")) {
        String state = doc["state"].as<String>();
        currentState.on = (state == "ON");
        Serial.print("  State: ");
        Serial.println(currentState.on ? "ON" : "OFF");
      }
      
      if (doc.containsKey("brightness")) {
        currentState.brightness = doc["brightness"];
        currentState.supportsBrightness = true;
        Serial.print("  Brightness: ");
        Serial.println(currentState.brightness);
      }
      
      if (doc.containsKey("color")) {
        if (doc["color"].containsKey("h")) {
          currentState.hue = doc["color"]["h"];
          currentState.supportsColor = true;
        }
        if (doc["color"].containsKey("s")) {
          currentState.saturation = doc["color"]["s"];
        }
        Serial.print("  Color H:");
        Serial.print(currentState.hue);
        Serial.print(" S:");
        Serial.println(currentState.saturation);
      }
      
      if (!lightsDiscovered) {
        lightsDiscovered = true;
        Serial.println("✓ Light discovered and connected!");
      }
      
      currentState.available = true;
    }
    
    // Handle availability topic
    if (topic == availabilityTopic) {
      String availability = String((char*)payload).substring(0, length);
      currentState.available = (availability == "online");
      Serial.print("Light availability: ");
      Serial.println(currentState.available ? "online" : "offline");
    }
  }
  
  void setBrightness(uint8_t brightness) {
    pendingBrightness = constrain(brightness, 0, 255);
    pendingUpdate = true;
    Serial.print("Brightness queued: ");
    Serial.println(pendingBrightness);
  }
  
  void setHue(uint16_t hue) {
    pendingHue = hue % 360;
    pendingUpdate = true;
    Serial.print("Hue queued: ");
    Serial.println(pendingHue);
  }
  
  void setColor(uint16_t hue, uint8_t saturation) {
    pendingHue = hue % 360;
    currentState.saturation = constrain(saturation, 0, 100);
    pendingUpdate = true;
  }
  
  void turnOn() {
    // Restore previous brightness if it was 0 (turned off)
    uint8_t brightnessToUse = currentState.brightness;
    if (brightnessToUse == 0) {
      brightnessToUse = 255;  // Default to full brightness if none stored
    }
    sendCommand(true, brightnessToUse, currentState.hue);
  }
  
  void turnOff() {
    // Keep current brightness/hue in state, just turn off
    sendCommand(false, currentState.brightness, currentState.hue);
  }
  
  void loop() {
    // Debounced MQTT publishing
    if (pendingUpdate && (millis() - lastPublishTime > MQTT_PUBLISH_DELAY)) {
      sendCommand(true, pendingBrightness, pendingHue);
      pendingUpdate = false;
      lastPublishTime = millis();
    }
  }
  
  void sendCommand(bool on, uint8_t brightness, uint16_t hue) {
    // Use REST API if available
    if (restAPIClient != nullptr) {
      Serial.println("Using REST API to send command");
      bool success = restAPIClient->setLightState(selectedEntityId, on, brightness, hue);
      
      if (success) {
        // Optimistically update local state
        currentState.on = on;
        currentState.brightness = brightness;
        currentState.hue = hue;
      }
      return;
    }
    
    // Fallback to MQTT (for MQTT lights)
    if (!mqttClient->connected()) {
      Serial.println("✗ Neither REST API nor MQTT available!");
      return;
    }
    
    StaticJsonDocument<256> doc;
    
    doc["state"] = on ? "ON" : "OFF";
    
    if (on && currentState.supportsBrightness) {
      doc["brightness"] = brightness;
    }
    
    if (on && currentState.supportsColor) {
      JsonObject color = doc.createNestedObject("color");
      color["h"] = hue;
      color["s"] = currentState.saturation;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    bool success = mqttClient->publish(commandTopic.c_str(), payload.c_str());
    
    if (success) {
      Serial.print("✓ Sent to HA via MQTT: ");
      Serial.println(payload);
      
      // Optimistically update local state
      currentState.on = on;
      currentState.brightness = brightness;
      currentState.hue = hue;
    } else {
      Serial.println("✗ Failed to publish to MQTT");
    }
  }
  
  // Set which entity to control
  void setEntityId(String entityId) {
    selectedEntityId = entityId;
    Serial.print("Switched to controlling: ");
    Serial.println(selectedEntityId);
  }
  
  // Getters
  bool isLightDiscovered() { return lightsDiscovered; }
  bool isLightAvailable() { return currentState.available; }
  bool isLightOn() { return currentState.on; }
  uint8_t getBrightness() { return currentState.brightness; }
  uint16_t getHue() { return currentState.hue; }
  uint8_t getSaturation() { return currentState.saturation; }
  bool supportsColor() { return currentState.supportsColor; }
  bool supportsBrightness() { return currentState.supportsBrightness; }
  String getEntityId() { return selectedEntityId; }
};

#endif // HA_LIGHT_MANAGER_H
