// TFT_eSPI User Setup for IoTeikXgo 1.28" ESP32-S3 Round Display
// Place this file in: Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
// Or create User_Setups/Setup_GC9A01_ESP32_S3.h and include it

#define USER_SETUP_INFO "User_Setup"

// Driver
#define GC9A01_DRIVER

// ESP32-S3 Pin Configuration
#define TFT_MISO -1   // Not connected
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS   9
#define TFT_DC   8
#define TFT_RST  14
#define TFT_BL   2    // Backlight

// Display Resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Fonts
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes
#define LOAD_FONT8  // Font 8. Large 75 pixel font, needs ~3256 bytes
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts

#define SMOOTH_FONT

// SPI Frequency
#define SPI_FREQUENCY  40000000  // 40MHz
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
