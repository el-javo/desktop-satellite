#pragma once

// TFT_eSPI User Setup for ST7789 240x240 (SPI, no CS)

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// No CS pin on this module
#define TFT_CS  -1

// SPI pins
#define TFT_MOSI 23
#define TFT_SCLK 18

// Control pins
#define TFT_DC   2
#define TFT_RST  4

// Optional: backlight pin (not used by TFT_eSPI core)
// #define TFT_BL 19

// SPI frequency
#define SPI_FREQUENCY  10000000

// Color order
#define TFT_RGB_ORDER TFT_RGB
