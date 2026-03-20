#ifndef CONFIG_H
#define CONFIG_H

// ==================== WIFI CONFIGURATION ====================
#define WIFI_SSID "*insert your WiFi SSID*"
#define WIFI_PASSWORD "*insert your WiFi password*"

// ==================== MQTT CONFIGURATION ====================
#define MQTT_SERVER "*insert your MQTT broker IP*"
#define MQTT_PORT 1883
#define MQTT_USERNAME "*insert your MQTT username*"
#define MQTT_PASSWORD "*insert your MQTT password*"
#define MQTT_CLIENT_ID "ESP32_Round_Clock"
#define MQTT_DISCOVERY_PREFIX "homeassistant"

// ==================== HOME ASSISTANT ====================
// Start with one MQTT light to test, then expand to multi-light selector
#define HA_PREFERRED_LIGHT_ENTITY "*insert your preferred light entity ID*"  // Example: light.bedroom_lamp
#define HA_SERVER "*insert your Home Assistant IP*"
#define HA_PORT 8123
#define HA_ACCESS_TOKEN "*insert your Home Assistant long-lived access token*"
#define HA_DISCOVERY_ENABLED true

// ==================== HARDWARE PINS ====================
// Display
#define SCREEN_BACKLIGHT_PIN 46

// Touch
#define TP_I2C_SDA_PIN 6
#define TP_I2C_SCL_PIN 7
#define TP_INT 5
#define TP_RST 13

// Rotary Encoder
#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define ENCODER_SWITCH_PIN 41

// LED Ring
#define LED_PIN 48
#define LED_COUNT 5

// ==================== UI CONFIGURATION ====================
#define IDLE_TIMEOUT_MS 15000  // 15 seconds
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

// ==================== CONTROL SETTINGS ====================
#define BRIGHTNESS_STEP 13     // Brightness change per encoder click (~5% of 255)
#define HUE_STEP 10           // Hue change per encoder click (0-360)
#define MQTT_PUBLISH_DELAY 300 // Debounce delay for MQTT publishing (ms)

// ==================== NTP CONFIGURATION ====================
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC -28800  // PST = UTC-8
#define DAYLIGHT_OFFSET_SEC 3600

// ==================== UI MODES ====================
enum UIMode {
  MODE_CLOCK,
  MODE_POWER,
  MODE_BRIGHTNESS,
  MODE_COLOR,
  MODE_DEVICE_SELECTION
};

// ==================== LIGHT STATE ====================
struct LightState {
  bool on = false;
  uint8_t brightness = 255;  // 0-255
  uint16_t hue = 0;          // 0-360
  uint8_t saturation = 100;  // 0-100
  bool available = false;
  bool supportsColor = false;
  bool supportsBrightness = false;
};

#endif // CONFIG_H
