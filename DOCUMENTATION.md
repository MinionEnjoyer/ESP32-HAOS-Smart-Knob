# ESP32 Round Clock - HAOS Smart Knob

## Hardware Overview

**Device:** ESP32-S3 Round Display (1.28" GC9A01)
- **MCU:** ESP32-S3 (Dual Core, 240MHz)
- **Memory:** 8MB PSRAM, 16MB Flash
- **Display:** 1.28" Round GC9A01 240x240 (SPI)
- **Touch:** CST816D Capacitive Touch (I2C)
- **Rotary Encoder:** 5-position with push button
- **LED Ring:** 5x WS2812 RGB LEDs

## Pin Configuration

### Display (GC9A01 - SPI)
- **SCLK:** GPIO 10
- **MOSI:** GPIO 11
- **DC:** GPIO 3
- **CS:** GPIO 9
- **RST:** GPIO 14
- **Backlight:** GPIO 46 (PWM)
- **Power:** GPIO 1, GPIO 2 (must be HIGH)

### Touch (CST816D - I2C)
- **SDA:** GPIO 6
- **SCL:** GPIO 7
- **INT:** GPIO 5
- **RST:** GPIO 13

### Rotary Encoder
- **Pin A:** GPIO 45
- **Pin B:** GPIO 42
- **Switch:** GPIO 41

### LED Ring (WS2812)
- **Data:** GPIO 48
- **Count:** 5 LEDs

## Software Configuration

### WiFi Settings
Edit `Config.h`:
```cpp
#define WIFI_SSID "*insert your WiFi SSID*"
#define WIFI_PASSWORD "*insert your WiFi password*"
```

### MQTT Broker
```cpp
#define MQTT_SERVER "*insert your MQTT broker IP*"
#define MQTT_PORT 1883
#define MQTT_USERNAME "*insert your MQTT username*"
#define MQTT_PASSWORD "*insert your MQTT password*"
#define MQTT_CLIENT_ID "ESP32_Round_Clock"
#define MQTT_DISCOVERY_PREFIX "homeassistant"
```

### Home Assistant REST API
```cpp
#define HA_SERVER "*insert your Home Assistant IP*"
#define HA_PORT 8123
#define HA_ACCESS_TOKEN "*insert your Home Assistant long-lived access token*"
```

### Light Entity
```cpp
#define HA_PREFERRED_LIGHT_ENTITY "*insert your preferred light entity ID*"  // Example: light.bedroom_lamp
```

## Required Arduino Libraries

Install via Arduino Library Manager:
- **LovyanGFX** (1.2.19) - Display driver
- **lvgl** (8.3.x) - GUI library
- **Adafruit_NeoPixel** - LED ring
- **PubSubClient** - MQTT
- **ArduinoJson** (7.4.3) - JSON parsing

**Important:** Copy `lv_conf.h` to Arduino libraries folder before compiling.

## Compilation & Upload

### Prerequisites
- **Arduino CLI** or Arduino IDE
- **Python** with esptool installed (`pip install esptool`)
- **ESP32 Board Package** version 3.3.7

### Board Settings
- **Board:** ESP32S3 Dev Module
- **Flash Size:** 16MB
- **PSRAM:** OPI PSRAM
- **Partition Scheme:** Huge APP (3MB No OTA)
- **Upload Speed:** 921600

### Step 1: Compile Firmware

**Using Arduino CLI:**
```powershell
arduino-cli compile `
  --fqbn esp32:esp32:esp32s3:JTAGAdapter=default,PSRAM=opi,FlashMode=qio,FlashSize=16M,LoopCore=1,EventsCore=1,USBMode=hwcdc,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,PartitionScheme=huge_app,CPUFreq=240,UploadSpeed=921600,DebugLevel=none,EraseFlash=none `
  --output-dir "./build" `
  "ESP32_Round_Clock.ino"
```

**Expected Output:**
```
Sketch uses ~1.4MB (46%) of program storage space. Maximum is 3145728 bytes.
Global variables use ~84KB (25%) of dynamic memory.
```

### Step 2: Copy boot_app0.bin (First Time Only)

On Windows:
```powershell
Copy-Item "$env:LOCALAPPDATA\Arduino15\packages\esp32\hardware\esp32\*\tools\partitions\boot_app0.bin" `
  -Destination "./build/boot_app0.bin" -Force
```

On Linux/Mac:
```bash
cp ~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin ./build/
```

### Step 3: Upload to ESP32

**Using esptool:**
```powershell
python -m esptool --chip esp32s3 --port COM4 --baud 921600 `
  --before default-reset --after hard-reset write-flash `
  --flash-mode dio --flash-freq 80m --flash-size 16MB `
  0x0 ./build/ESP32_Round_Clock.ino.bootloader.bin `
  0x8000 ./build/ESP32_Round_Clock.ino.partitions.bin `
  0xe000 ./build/boot_app0.bin `
  0x10000 ./build/ESP32_Round_Clock.ino.bin
```

Replace `COM4` with your serial port (e.g., `/dev/ttyUSB0` on Linux).

### Step 4: Monitor Serial Output

```bash
python -m serial.tools.miniterm COM4 115200
```

**Exit miniterm:** Press `Ctrl+]`

## Features

### Clock Mode
- Displays current time (HH:MM format)
- Shows date (Day, Mon DD)
- Connection status indicator
- Pulsing white LED ring

### Device Selection
- Swipe left from clock to browse all available lights
- Rotate encoder to scroll through devices
- Swipe left again to enter control mode for selected device

### Light Control Modes

#### Power Control
- Toggle light on/off with encoder button press
- LED ring reflects light state

#### Brightness Control
- Swipe up/down to switch between modes
- Rotate encoder to adjust brightness
- LED ring shows brightness level as bar graph
- Auto-publishes to Home Assistant via REST API

#### Color Control
- Swipe up from brightness mode
- Rotate encoder to change hue (0-360°)
- LED ring displays current color
- Auto-publishes to Home Assistant via REST API

### Auto-Idle
- Returns to clock after 15 seconds of inactivity
- Touch or encoder interaction wakes device

## Home Assistant Integration

### How It Works
The device uses **REST API** (not MQTT) to control lights because most lights (Zigbee/Z-Wave/WiFi) don't publish to MQTT topics.

**API Endpoints Used:**
- `GET /api/states` - Discover all light entities
- `GET /api/states/{entity_id}` - Fetch current state
- `POST /api/services/light/turn_on` - Control light
- `POST /api/services/light/turn_off` - Turn off light

**Polling:** Device polls every 10 seconds to sync state changes made elsewhere.

### Creating Long-Lived Access Token

1. Open Home Assistant
2. Click your profile (bottom left)
3. Scroll to "Long-Lived Access Tokens"
4. Click "CREATE TOKEN"
5. Name it "ESP32_Round_Clock"
6. Copy token immediately (you can't see it again!)
7. Paste into `Config.h` as `HA_ACCESS_TOKEN`

### Supported Light Entities
Works with ANY Home Assistant light entity:
- Zigbee lights (via ZHA, Zigbee2MQTT, etc.)
- Z-Wave lights
- WiFi smart bulbs (Tuya, TP-Link, etc.)
- Helper Groups
- MQTT lights (legacy)

## Troubleshooting

### Display Not Working
- Check power pins (GPIO 1, 2 must be HIGH)
- Verify SPI connections
- Check backlight PWM (GPIO 46)

### Touch Not Responding
- Verify I2C connections (SDA=6, SCL=7)
- Check touch INT and RST pins

### WiFi Connection Failed
- Verify SSID and password in Config.h
- Check 2.4GHz WiFi band (ESP32 doesn't support 5GHz)
- Monitor serial output at 115200 baud

### Light Control Not Working - HTTP GET Failed
This error indicates the ESP32 cannot reach Home Assistant's REST API.

**Solutions:**
1. **Check Network Connectivity:**
   ```bash
   # From a computer, verify HA is accessible
   curl http://YOUR_HA_IP:8123/api/
   ```

2. **Verify HA Access Token:**
   - Token may have expired - check expiration in HA
   - Regenerate token if needed
   - Update Config.h with new token

3. **Verify Light Entity ID:**
   - Check entity exists in Home Assistant
   - Test API endpoint: `http://YOUR_HA_IP:8123/api/states/light.your_light`
   - Monitor serial output for detailed API error messages

### MQTT Connection Issues
- MQTT is only used for status publishing (optional)
- Light control uses REST API, not MQTT
- Check broker IP, port, username, password

## Serial Monitor Output

**Successful Startup:**
```
=== WiFi Setup ===
✓ WiFi CONNECTED!
=== Discovering light entities ===
✓ Discovered X lights
✓ Initial light state loaded from HA
```

**When Controlling Light:**
```
Using REST API to send command
✓ Light command sent successfully
```

## File Structure

```
ESP32_Round_Clock/
├── ESP32_Round_Clock.ino    # Main application
├── Config.h                  # WiFi, MQTT, HA settings
├── HARestAPIClient.h         # Home Assistant REST API client
├── HALightManager.h          # Light state management
├── LEDRingController.h       # WS2812 LED control
├── InputManager.h            # Encoder & touch handling
├── CST816D.h/cpp            # Touch driver
├── lv_conf.h                # LVGL configuration
├── User_Setup.h             # Display configuration
└── README.md                # This file
```

## License

MIT License - Free to use and modify

## Credits

- Hardware: Elecrow ESP32 Display 1.28" Rotary Screen
- Libraries: LovyanGFX, LVGL, Adafruit NeoPixel, PubSubClient, ArduinoJson
