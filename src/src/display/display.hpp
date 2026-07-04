#pragma once

#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01      _panel_instance;
  lgfx::Bus_SPI           _bus_instance;
  lgfx::Light_PWM         _light_instance;
  lgfx::Touch_CST816S     _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 10000000;
      cfg.spi_3wire   = true;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 2;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs      = 10;
      cfg.pin_rst     = -1;
      cfg.pin_busy    = -1;

      cfg.memory_width     = 240;
      cfg.memory_height    = 240;
      cfg.panel_width      = 240;
      cfg.panel_height     = 240;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable    = false;
      cfg.invert      = true;
      cfg.rgb_order   = false;
      cfg.dlen_16bit  = false;
      cfg.bus_shared  = false;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();

      cfg.pin_bl      = 3;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 1;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();

      cfg.x_min      = 0;
      cfg.x_max      = 240;
      cfg.y_min      = 0;
      cfg.y_max      = 240;
      cfg.pin_int    = 0;
      cfg.pin_rst    = 1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port   = 0;
      cfg.i2c_addr   = 0x15;
      cfg.pin_sda    = 4;
      cfg.pin_scl    = 5;
      cfg.freq       = 400000;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
