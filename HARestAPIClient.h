#ifndef HA_REST_API_CLIENT_H
#define HA_REST_API_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"

class HARestAPIClient {
private:
  String accessToken;
  String serverURL;
  HTTPClient http;

public:
  HARestAPIClient() {
    serverURL = String("http://") + HA_SERVER + ":" + String(HA_PORT);
    accessToken = HA_ACCESS_TOKEN;
  }
  
  // Get light state from Home Assistant
  bool getLightState(String entityId, LightState* state) {
    String url = serverURL + "/api/states/" + entityId;
    
    Serial.print("REST API GET: ");
    Serial.println(url);
    
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      http.end();
      
      // Parse JSON response
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
      }
      
      // Extract state
      String stateStr = doc["state"];
      state->on = (stateStr == "on");
      state->available = true;
      
      // Extract attributes
      if (doc["attributes"].containsKey("brightness")) {
        state->brightness = doc["attributes"]["brightness"];
        state->supportsBrightness = true;
      }
      
      if (doc["attributes"]["color_mode"].as<String>() == "hs" || 
          doc["attributes"].containsKey("hs_color")) {
        state->supportsColor = true;
        if (doc["attributes"].containsKey("hs_color")) {
          state->hue = doc["attributes"]["hs_color"][0];
          state->saturation = doc["attributes"]["hs_color"][1];
        }
      }
      
      Serial.println("✓ Got light state from HA REST API");
      Serial.print("  State: ");
      Serial.println(state->on ? "ON" : "OFF");
      Serial.print("  Brightness: ");
      Serial.println(state->brightness);
      Serial.print("  Hue: ");
      Serial.println(state->hue);
      
      return true;
    } else {
      Serial.print("HTTP GET failed: ");
      Serial.println(httpCode);
      http.end();
      return false;
    }
  }
  
  // Set light state via Home Assistant service call
  bool setLightState(String entityId, bool on, uint8_t brightness, uint16_t hue) {
    String url = serverURL + "/api/services/light/turn_" + (on ? String("on") : String("off"));
    
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");
    
    // Build JSON payload
    StaticJsonDocument<256> doc;
    doc["entity_id"] = entityId;
    
    if (on) {
      doc["brightness"] = brightness;
      JsonArray hs_color = doc.createNestedArray("hs_color");
      hs_color.add(hue);
      hs_color.add(100); // Full saturation
    }
    
    String payload;
    serializeJson(doc, payload);
    
    Serial.print("REST API call: ");
    Serial.println(payload);
    
    int httpCode = http.POST(payload);
    http.end();
    
    if (httpCode == 200) {
      Serial.println("✓ Light command sent successfully");
      return true;
    } else {
      Serial.print("✗ HTTP POST failed: ");
      Serial.println(httpCode);
      return false;
    }
  }
  
  // Fetch all light entities from Home Assistant (limit to maxLights)
  int getAllLights(String* entityIds, String* friendlyNames, int maxLights) {
    String url = serverURL + "/api/states";
    
    Serial.println("Fetching all light entities...");
    
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      http.end();
      
      // Parse JSON response - using larger buffer
      DynamicJsonDocument doc(16384);  // 16KB for large response
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return 0;
      }
      
      int lightCount = 0;
      JsonArray states = doc.as<JsonArray>();
      
      // Filter for light entities only
      for (JsonObject entity : states) {
        String entityId = entity["entity_id"].as<String>();
        
        // Check if it's a light entity
        if (entityId.startsWith("light.") && lightCount < maxLights) {
          entityIds[lightCount] = entityId;
          
          // Get friendly name if available
          if (entity["attributes"].containsKey("friendly_name")) {
            friendlyNames[lightCount] = entity["attributes"]["friendly_name"].as<String>();
          } else {
            // Use entity ID without "light." prefix
            friendlyNames[lightCount] = entityId.substring(6);
          }
          
          lightCount++;
          Serial.print("  Found: ");
          Serial.print(entityId);
          Serial.print(" (");
          Serial.print(friendlyNames[lightCount - 1]);
          Serial.println(")");
        }
      }
      
      Serial.print("✓ Found ");
      Serial.print(lightCount);
      Serial.println(" light entities");
      return lightCount;
      
    } else {
      Serial.print("HTTP GET failed: ");
      Serial.println(httpCode);
      http.end();
      return 0;
    }
  }
};

#endif // HA_REST_API_CLIENT_H
