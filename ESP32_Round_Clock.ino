#define LGFX_USE_V1
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <time.h>
#include "CST816D.h"
#include "Config.h"
#include "HALightManager.h"
#include "HARestAPIClient.h"
#include "LEDRingController.h"
#include "InputManager.h"

// Global instances
WiFiClient espClient;
PubSubClient mqttClient(espClient);
HALightManager lightManager(&mqttClient);
HARestAPIClient restAPI;
LEDRingController ledRing;
InputManager inputManager;

// Device management
#define MAX_LIGHTS 10
String lightEntityIds[MAX_LIGHTS];
String lightFriendlyNames[MAX_LIGHTS];
int totalLights = 0;
int currentLightIndex = 0;

// UI State
UIMode currentMode = MODE_CLOCK;
unsigned long modeStartTime = 0;

// LovyanGFX Display Configuration
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 10;
      cfg.pin_mosi = 11;
      cfg.pin_miso = -1;
      cfg.pin_dc = 3;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 9;
      cfg.pin_rst = 14;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX gfx;
CST816D touch(TP_I2C_SDA_PIN, TP_I2C_SCL_PIN, TP_RST, TP_INT);

// LVGL Setup
static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
static lv_color_t *buf1 = NULL;

// LVGL UI Elements - Clock Screen
lv_obj_t *clockScreen = NULL;
lv_obj_t *timeLabel;
lv_obj_t *dateLabel;
lv_obj_t *statusLabel;

// LVGL UI Elements - Control Screens
lv_obj_t *controlScreen = NULL;
lv_obj_t *modeLabel;
lv_obj_t *valueLabel;
lv_obj_t *infoLabel;

// LVGL UI Elements - Device Selection Screen
lv_obj_t *deviceScreen = NULL;
lv_obj_t *deviceTitleLabel;
lv_obj_t *deviceNameLabel;
lv_obj_t *deviceIndexLabel;

// Display flushing for LVGL
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() > 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

// Touch input for LVGL
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY, &gesture);

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
  }
  
  // Update input manager
  inputManager.updateTouch(touched, touchX, touchY);
}

// Initialize backlight
void initBacklight() {
  ledcAttach(SCREEN_BACKLIGHT_PIN, 5000, 8);
  ledcWrite(SCREEN_BACKLIGHT_PIN, 200); // 78% brightness
}

// WiFi connection
void setupWiFi() {
  Serial.println("\n=== WiFi Setup ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Connecting");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi CONNECTED!");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi FAILED!");
    Serial.print("  Status code: ");
    Serial.println(WiFi.status());
  }
}

// MQTT Callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message on topic: ");
  Serial.println(topic);
  
  lightManager.handleMQTTMessage(String(topic), payload, length);
}

// MQTT reconnect
void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.println("\n=== MQTT Connection Attempt ===");
    Serial.print("  Broker: ");
    Serial.print(MQTT_SERVER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    Serial.print("  Client ID: ");
    Serial.println(MQTT_CLIENT_ID);
    Serial.print("  Username: ");
    Serial.println(MQTT_USERNAME);
    Serial.print("  Connecting...");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println(" ✓ SUCCESS!");
      
      // Subscribe to light topics
      lightManager.subscribeToTopics();
      
      // Publish online status
      mqttClient.publish((String(MQTT_DISCOVERY_PREFIX) + "/status").c_str(), "online");
    } else {
      int rc = mqttClient.state();
      Serial.print(" ✗ FAILED! rc=");
      Serial.println(rc);
      
      // Decode error
      switch(rc) {
        case -4: Serial.println("  Error: MQTT_CONNECTION_TIMEOUT - server didn't respond"); break;
        case -3: Serial.println("  Error: MQTT_CONNECTION_LOST"); break;
        case -2: Serial.println("  Error: MQTT_CONNECT_FAILED - network problem, can't reach broker"); break;
        case -1: Serial.println("  Error: MQTT_DISCONNECTED"); break;
        case 1: Serial.println("  Error: MQTT_CONNECT_BAD_PROTOCOL"); break;
        case 2: Serial.println("  Error: MQTT_CONNECT_BAD_CLIENT_ID"); break;
        case 3: Serial.println("  Error: MQTT_CONNECT_UNAVAILABLE - broker unavailable"); break;
        case 4: Serial.println("  Error: MQTT_CONNECT_BAD_CREDENTIALS"); break;
        case 5: Serial.println("  Error: MQTT_CONNECT_UNAUTHORIZED"); break;
        default: Serial.println("  Error: Unknown error code"); break;
      }
      
      // Check WiFi
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("  ⚠ WiFi is disconnected!");
      }
    }
  }
}

// Create Clock UI
void createClockUI() {
  clockScreen = lv_scr_act();  // Save reference to clock screen
  lv_obj_set_style_bg_color(clockScreen, lv_color_hex(0x000000), 0);
  
  // Time label (large, centered)
  timeLabel = lv_label_create(clockScreen);
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(timeLabel, "00:00");
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, -20);
  
  // Date label (smaller, below time)
  dateLabel = lv_label_create(clockScreen);
  lv_obj_set_style_text_font(dateLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(dateLabel, lv_color_hex(0xAAAAAA), 0);
  lv_label_set_text(dateLabel, "Loading...");
  lv_obj_align(dateLabel, LV_ALIGN_CENTER, 0, 40);
  
  // Status label (small, at bottom)
  statusLabel = lv_label_create(clockScreen);
  lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x00FF00), 0);
  lv_label_set_text(statusLabel, "Starting...");
  lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create Control UI
void createControlUI() {
  if (controlScreen == NULL) {
    controlScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(controlScreen, lv_color_hex(0x000000), 0);
    
    // Mode label (top) - matching device selection style
    modeLabel = lv_label_create(controlScreen);
    lv_obj_set_style_text_font(modeLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(modeLabel, lv_color_hex(0x00AAFF), 0);
    lv_label_set_text(modeLabel, "BRIGHTNESS");
    lv_obj_align(modeLabel, LV_ALIGN_TOP_MID, 0, 20);
    
    // Value label (center, large)
    valueLabel = lv_label_create(controlScreen);
    lv_obj_set_style_text_font(valueLabel, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(valueLabel, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(valueLabel, "50%");
    lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, -10);
    
    // Info label (bottom) - matching device selection style
    infoLabel = lv_label_create(controlScreen);
    lv_obj_set_style_text_font(infoLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(infoLabel, lv_color_hex(0x888888), 0);
    lv_label_set_text(infoLabel, "Rotate to adjust\nSwipe to navigate");
    lv_obj_align(infoLabel, LV_ALIGN_BOTTOM_MID, 0, -20);
  }
}

// Create Device Selection UI
void createDeviceUI() {
  if (deviceScreen == NULL) {
    deviceScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(deviceScreen, lv_color_hex(0x1a1a1a), 0);
    
    // Title
    deviceTitleLabel = lv_label_create(deviceScreen);
    lv_obj_set_style_text_font(deviceTitleLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(deviceTitleLabel, lv_color_hex(0x00AAFF), 0);
    lv_label_set_text(deviceTitleLabel, "SELECT DEVICE");
    lv_obj_align(deviceTitleLabel, LV_ALIGN_TOP_MID, 0, 20);
    
    // Device name (center, large, with wrapping)
    deviceNameLabel = lv_label_create(deviceScreen);
    lv_obj_set_style_text_font(deviceNameLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(deviceNameLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(deviceNameLabel, 200);  // Prevent overflow
    lv_label_set_long_mode(deviceNameLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(deviceNameLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(deviceNameLabel, "Device");
    lv_obj_align(deviceNameLabel, LV_ALIGN_CENTER, 0, -10);
    
    // Index indicator
    deviceIndexLabel = lv_label_create(deviceScreen);
    lv_obj_set_style_text_font(deviceIndexLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(deviceIndexLabel, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(deviceIndexLabel, "1/1");
    lv_obj_align(deviceIndexLabel, LV_ALIGN_CENTER, 0, 40);
    
    // Info label
    lv_obj_t *deviceInfo = lv_label_create(deviceScreen);
    lv_obj_set_style_text_font(deviceInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(deviceInfo, lv_color_hex(0x888888), 0);
    lv_label_set_text(deviceInfo, "Rotate to browse\nSwipe to exit");
    lv_obj_align(deviceInfo, LV_ALIGN_BOTTOM_MID, 0, -20);
  }
}

// Update device selection screen
void updateDeviceScreen() {
  if (totalLights > 0) {
    lv_label_set_text(deviceNameLabel, lightFriendlyNames[currentLightIndex].c_str());
    
    char idx[20];
    snprintf(idx, sizeof(idx), "%d / %d", currentLightIndex + 1, totalLights);
    lv_label_set_text(deviceIndexLabel, idx);
  }
}

// Update clock display
void updateClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lv_label_set_text(timeLabel, "--:--");
    lv_label_set_text(dateLabel, "No time");
    return;
  }
  
  // Format time (HH:MM)
  char timeStr[6];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  lv_label_set_text(timeLabel, timeStr);
  
  // Format date (Day, Mon DD)
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %b %d", &timeinfo);
  lv_label_set_text(dateLabel, dateStr);
  
  // Update status
  if (WiFi.status() == WL_CONNECTED) {
    if (mqttClient.connected()) {
      if (lightManager.isLightDiscovered()) {
        lv_label_set_text(statusLabel, "HA Connected");
        lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x00FF00), 0);
      } else {
        lv_label_set_text(statusLabel, "Finding lights...");
        lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFFAA00), 0);
      }
    } else {
      lv_label_set_text(statusLabel, "MQTT Offline");
      lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFFAA00), 0);
    }
  } else {
    lv_label_set_text(statusLabel, "WiFi Disconnected");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFF0000), 0);
  }
}

// Helper function to get color name from hue
const char* getColorName(uint16_t hue) {
  if (hue < 15 || hue >= 345) return "Red";
  else if (hue < 45) return "Orange";
  else if (hue < 75) return "Yellow";
  else if (hue < 165) return "Green";
  else if (hue < 195) return "Cyan";
  else if (hue < 255) return "Blue";
  else if (hue < 285) return "Purple";
  else return "Magenta";
}

// Update control screen
void updateControlScreen() {
  if (currentMode == MODE_POWER) {
    lv_label_set_text(modeLabel, "POWER");
    
    bool isOn = lightManager.isLightOn();
    lv_label_set_text(valueLabel, isOn ? "ON" : "OFF");
    lv_label_set_text(infoLabel, "Press to toggle");
    lv_obj_set_style_bg_color(controlScreen, lv_color_hex(0x000000), 0);
    
  } else if (currentMode == MODE_BRIGHTNESS) {
    lv_label_set_text(modeLabel, "BRIGHTNESS");
    
    uint8_t brightness = lightManager.getBrightness();
    int percent = map(brightness, 0, 255, 0, 100);
    char text[10];
    sprintf(text, "%d%%", percent);
    lv_label_set_text(valueLabel, text);
    lv_label_set_text(infoLabel, "Rotate to adjust");
    
    // Reset background to black for brightness mode
    lv_obj_set_style_bg_color(controlScreen, lv_color_hex(0x000000), 0);
    
  } else if (currentMode == MODE_COLOR) {
    lv_label_set_text(modeLabel, "COLOR");
    
    // Sync with actual light state before displaying
    uint16_t hue = lightManager.getHue();
    
    // Convert HSV to RGB for display (corrected formula)
    float h = hue;
    float s = 1.0;  // Full saturation
    float v = 1.0;  // Full brightness for vivid colors
    
    float c = v * s;
    float x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    float m = v - c;
    
    float r, g, b;
    if (h < 60) {
      r = c; g = x; b = 0;
    } else if (h < 120) {
      r = x; g = c; b = 0;
    } else if (h < 180) {
      r = 0; g = c; b = x;
    } else if (h < 240) {
      r = 0; g = x; b = c;
    } else if (h < 300) {
      r = x; g = 0; b = c;
    } else {
      r = c; g = 0; b = x;
    }
    
    // Add m and scale to 0-255
    int red = (int)((r + m) * 255);
    int green = (int)((g + m) * 255);
    int blue = (int)((b + m) * 255);
    
    lv_color_t color = lv_color_make(red, green, blue);
    lv_obj_set_style_bg_color(controlScreen, color, 0);
    
    // Show color name
    lv_label_set_text(valueLabel, getColorName(hue));
    lv_label_set_text(infoLabel, "Rotate to adjust");
  }
}

// Switch UI mode
void switchToMode(UIMode newMode) {
  if (currentMode == newMode) return;
  
  Serial.print("Switching to mode: ");
  Serial.println(newMode);
  
  currentMode = newMode;
  modeStartTime = millis();
  
  if (newMode == MODE_CLOCK) {
    lv_scr_load(clockScreen);  // Load the saved clock screen
    ledRing.setMode(MODE_CLOCK);
  } else if (newMode == MODE_DEVICE_SELECTION) {
    createDeviceUI();
    updateDeviceScreen();
    lv_scr_load(deviceScreen);
    ledRing.setMode(MODE_BRIGHTNESS);  // Keep LED ring showing light state
  } else {
    createControlUI();
    lv_scr_load(controlScreen);
    updateControlScreen();
    ledRing.setMode(newMode);
  }
}

// Handle encoder input
void handleEncoderInput() {
  int delta = inputManager.getEncoderDelta();
  
  // Check button press for power toggle - check BEFORE processing encoder delta
  if (inputManager.wasEncoderPressed()) {
    if (currentMode == MODE_POWER) {
      // Toggle light on/off with button press
      bool currentState = lightManager.isLightOn();
      Serial.print("Button pressed! Current state: ");
      Serial.println(currentState ? "ON" : "OFF");
      
      if (currentState) {
        lightManager.turnOff();
        Serial.println("→ Turning OFF");
      } else {
        lightManager.turnOn();
        Serial.println("→ Turning ON");
      }
      updateControlScreen();
      return;  // Don't process encoder rotation on same cycle
    }
  }
  
  if (delta != 0) {
    // Wake up from clock if idle
    if (currentMode == MODE_CLOCK && lightManager.isLightDiscovered()) {
      switchToMode(MODE_POWER);
      return;
    }
    
    if (currentMode == MODE_DEVICE_SELECTION && totalLights > 0) {
      // Browse devices with encoder - normalize to one device per click
      int step = (delta > 0) ? 1 : -1;  // Normalize to +1 or -1
      currentLightIndex = (currentLightIndex + step + totalLights) % totalLights;
      updateDeviceScreen();
      Serial.print("Browsing device: ");
      Serial.println(lightFriendlyNames[currentLightIndex]);
    } else if (currentMode == MODE_BRIGHTNESS) {
      // Limit to single step per encoder click
      if (abs(delta) > 0) {
        int step = (delta > 0) ? 1 : -1;  // Normalize to +1 or -1
        uint8_t brightness = lightManager.getBrightness();
        int change = step * BRIGHTNESS_STEP;
        brightness = constrain(brightness + change, 0, 255);
        lightManager.setBrightness(brightness);
        ledRing.update(lightManager.getHue(), brightness);
        updateControlScreen();
        
        Serial.print("Brightness adjusted: ");
        Serial.println(brightness);
      }
    } else if (currentMode == MODE_COLOR) {
      // Limit to single step per encoder click
      if (abs(delta) > 0) {
        int step = (delta > 0) ? 1 : -1;  // Normalize to +1 or -1
        uint16_t hue = lightManager.getHue();
        int change = step * HUE_STEP;
        hue = (hue + change + 360) % 360;
        lightManager.setHue(hue);
        ledRing.update(hue, lightManager.getBrightness());
        updateControlScreen();
        
        Serial.print("Hue adjusted: ");
        Serial.println(hue);
      }
    }
  }
}

// Handle touch swipe
void handleTouchSwipe() {
  // Vertical swipe: cycle through Power → Brightness → Color
  int verticalSwipe = inputManager.getVerticalSwipe();
  if (verticalSwipe != 0) {
    if (currentMode == MODE_POWER) {
      if (verticalSwipe == -1) {  // Swipe up = go to brightness
        switchToMode(MODE_BRIGHTNESS);
      }
    } else if (currentMode == MODE_BRIGHTNESS) {
      if (verticalSwipe == -1) {  // Swipe up = go to color
        switchToMode(MODE_COLOR);
      } else if (verticalSwipe == 1) {  // Swipe down = go to power
        switchToMode(MODE_POWER);
      }
    } else if (currentMode == MODE_COLOR) {
      if (verticalSwipe == 1) {  // Swipe down = go to brightness
        switchToMode(MODE_BRIGHTNESS);
      }
    }
  }
  
  // Horizontal swipe: Carousel navigation Clock ↔ Device Selection ↔ Light Settings
  int horizontalSwipe = inputManager.getHorizontalSwipe();
  if (horizontalSwipe != 0) {
    Serial.print("Horizontal swipe detected: ");
    Serial.println(horizontalSwipe == 1 ? "RIGHT" : "LEFT");
    Serial.print("Current mode: ");
    Serial.println(currentMode);
    
    // Linear navigation: Clock ←→ Device Selection ←→ Settings
    if (currentMode == MODE_CLOCK && lightManager.isLightDiscovered()) {
      // From Clock: swipe LEFT → Device Selection
      if (horizontalSwipe == -1) {
        Serial.println("Clock → Device Selection (LEFT swipe)");
        switchToMode(MODE_DEVICE_SELECTION);
      }
    } else if (currentMode == MODE_DEVICE_SELECTION) {
      if (horizontalSwipe == -1) {
        // From Device Selection: swipe LEFT → Settings
        Serial.println("Device Selection → Settings (LEFT swipe)");
        
        // Switch to controlling the selected device
        lightManager.setEntityId(lightEntityIds[currentLightIndex]);
        
        // Load the selected device state
        LightState newDeviceState;
        if (restAPI.getLightState(lightEntityIds[currentLightIndex], &newDeviceState)) {
          lightManager.updateFromRestAPI(&newDeviceState);
        }
        switchToMode(MODE_POWER);
      } else if (horizontalSwipe == 1) {
        // From Device Selection: swipe RIGHT → Clock
        Serial.println("Device Selection → Clock (RIGHT swipe)");
        switchToMode(MODE_CLOCK);
      }
    } else if (currentMode == MODE_POWER || currentMode == MODE_BRIGHTNESS || currentMode == MODE_COLOR) {
      if (horizontalSwipe == 1) {
        // From Settings: swipe RIGHT → Device Selection
        Serial.println("Settings → Device Selection (RIGHT swipe)");
        switchToMode(MODE_DEVICE_SELECTION);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32 Round Clock with HA Light Control ===");
  
  // CRITICAL: Enable power pins for display
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  
  // Initialize display
  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.setColorDepth(16);
  gfx.fillScreen(TFT_BLACK);
  gfx.endWrite();
  
  // Initialize backlight
  initBacklight();
  
  // Initialize touch
  touch.begin();
  
  // Initialize LVGL
  lv_init();
  
  // Allocate LVGL buffers in PSRAM
  size_t buffer_size = sizeof(lv_color_t) * screenWidth * screenHeight;
  buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  
  if (!buf || !buf1) {
    Serial.println("Failed to allocate LVGL buffers!");
    while (1) delay(100);
  }
  
  lv_disp_draw_buf_init(&draw_buf, buf, buf1, screenWidth * screenHeight);
  
  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  
  // Initialize touch driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
  
  // Create Clock UI
  createClockUI();
  
  // Initialize LED Ring
  ledRing.begin();
  ledRing.setMode(MODE_CLOCK);
  
  // Initialize Input Manager
  inputManager.begin();
  
  // Connect to WiFi
  setupWiFi();
  
  // Initialize time from NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Setup MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);  // Increase for JSON messages
  
  // Initialize HA Light Manager
  lightManager.begin();
  
  // Connect REST API to light manager
  lightManager.setRestAPIClient(&restAPI);
  
  Serial.println("Setup complete!");
  
  // Discover all light entities from Home Assistant
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n=== Discovering light entities ===");
    totalLights = restAPI.getAllLights(lightEntityIds, lightFriendlyNames, MAX_LIGHTS);
    
    if (totalLights > 0) {
      Serial.print("✓ Discovered ");
      Serial.print(totalLights);
      Serial.println(" lights");
      
      // Find the preferred light's index
      for (int i = 0; i < totalLights; i++) {
        if (lightEntityIds[i] == HA_PREFERRED_LIGHT_ENTITY) {
          currentLightIndex = i;
          Serial.print("Using preferred light: ");
          Serial.println(lightFriendlyNames[i]);
          break;
        }
      }
      
      // Fetch initial state for current device
      Serial.println("\n=== Fetching light state via REST API ===");
      LightState initialState;
      if (restAPI.getLightState(lightEntityIds[currentLightIndex], &initialState)) {
        lightManager.updateFromRestAPI(&initialState);
        Serial.println("✓ Initial light state loaded from HA");
      } else {
        Serial.println("⚠ Failed to get initial light state, will retry");
      }
    } else {
      Serial.println("⚠ No lights discovered");
    }
  }
}

unsigned long lastUpdate = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long lastRestAPIpoll = 0;

void loop() {
  // Update LVGL tick (required for LVGL timing system)
  lv_tick_inc(5);
  
  // Handle LVGL tasks
  lv_timer_handler();
  
  // Handle input
  handleEncoderInput();
  handleTouchSwipe();
  
  // Update clock every second (when in clock mode)
  if (currentMode == MODE_CLOCK && millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    updateClock();
  }
  
  // Reconnect MQTT if needed (every 5 seconds)
  if (millis() - lastMQTTReconnect > 5000) {
    lastMQTTReconnect = millis();
    if (WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
    }
  }
  
  // Poll REST API for light state updates (every 10 seconds)
  if (millis() - lastRestAPIpoll > 10000) {
    lastRestAPIpoll = millis();
    if (WiFi.status() == WL_CONNECTED && lightManager.isLightDiscovered() && totalLights > 0) {
      LightState polledState;
      if (restAPI.getLightState(lightEntityIds[currentLightIndex], &polledState)) {
        lightManager.updateFromRestAPI(&polledState);
      }
    }
  }
  
  // MQTT loop
  mqttClient.loop();
  
  // HA Light Manager loop (for debounced publishing)
  lightManager.loop();
  
  // LED Ring update
  if (currentMode == MODE_CLOCK) {
    // In clock mode, call update to maintain pulsing animation
    ledRing.update(0, 128);  // Parameters don't matter for clock mode
  } else {
    ledRing.update(lightManager.getHue(), lightManager.getBrightness());
  }
  
  // Check idle timeout - return to clock
  if (currentMode != MODE_CLOCK && inputManager.isIdle()) {
    Serial.println("Idle timeout - returning to clock");
    switchToMode(MODE_CLOCK);
  }
  
  delay(5);
}
