#pragma once
#if defined(VARIO_SCREEN_HOST)
#include "screen_host_panel.h"
#else
#include <Adafruit_GFX.h>
#include <SPI.h>

#if defined(VARIO_LARGE_TFT)
#include <TFT_eSPI.h>
// Keep the Adafruit_GFX text/canvas interface while letting TFT_eSPI handle
// controller-specific SPI pixels (ILI9488 requires 18-bit transfers).
class DisplayPanel : public Adafruit_GFX {
 public:
  static constexpr uint8_t DefaultRotation = 0;
  static constexpr uint32_t SpiFrequency = 20000000;
  DisplayPanel() : Adafruit_GFX(320, 480) {}
  void begin(uint32_t) { panel_.init(); panel_.setSwapBytes(true); }
  void setRotation(uint8_t r) override {
    Adafruit_GFX::setRotation(r); panel_.setRotation(r);
    _width = panel_.width(); _height = panel_.height();
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { panel_.drawPixel(x,y,c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override { panel_.fillRect(x,y,w,h,c); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { panel_.drawFastHLine(x,y,w,c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { panel_.drawFastVLine(x,y,h,c); }
  void drawRGBBitmap(int16_t x, int16_t y, uint16_t *pixels, int16_t w, int16_t h) { panel_.pushImage(x,y,w,h,pixels); }
 private:
  TFT_eSPI panel_;
};
#else
#include <Adafruit_ILI9341.h>
// Retain the earlier module profile as an explicitly separate build target.
class DisplayPanel : public Adafruit_ILI9341 {
 public:
  static constexpr uint8_t DefaultRotation = 1;
  static constexpr uint32_t SpiFrequency = 10000000;
  DisplayPanel() : Adafruit_ILI9341(&SPI, 26, 27, 25) {}
  void setRotation(uint8_t r) override {
    Adafruit_ILI9341::setRotation(r);
    _width = (r & 1) ? 240 : 320; _height = (r & 1) ? 320 : 240;
  }
};
#endif
#endif
