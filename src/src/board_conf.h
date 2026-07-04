#ifndef BOARD_CONF_H
#define BOARD_CONF_H

#ifdef KNOMI

// ESP32 C3 Mini (ESP32-2424S012) pin mapping
// Profile from github.com/fbiego/esp32-c3-mini

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define RES_H SCREEN_WIDTH
#define RES_V SCREEN_HEIGHT
#define OFFSET_X 0
#define OFFSET_Y 0
#define RGB_ORDER false

// I2C (touch)
#define I2C_SDA CST816S_SDA_PIN
#define I2C_SCL CST816S_SCL_PIN
#define TP_INT  CST816S_IRQ_PIN
#define TP_RST  CST816S_RST_PIN

// Touch controller
#define CST816S_ADDR    0x15
#define CST816S_SDA_PIN 4
#define CST816S_SCL_PIN 5
#define CST816S_IRQ_PIN 0
#define CST816S_RST_PIN 1

// Display SPI (LovyanGFX naming)
#define SPI_HOST SPI2_HOST

#define SCLK GC9A01_SCLK_PIN
#define MOSI GC9A01_MOSI_PIN
#define MISO -1
#define DC   GC9A01_DC_PIN
#define CS   GC9A01_CS_PIN
#define RST  GC9A01_RST_PIN

#define GC9A01_MOSI_PIN 7
#define GC9A01_SCLK_PIN 6
#define GC9A01_CS_PIN   10
#define GC9A01_DC_PIN   2
#define GC9A01_RST_PIN  -1

// Backlight
#define BL      3
#define LCD_BL_PIN BL

#endif

#endif