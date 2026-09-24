// LGFX_CYD.hpp - LovyanGFX board configuration for ESP32-2432S028 (Cheap Yellow Display)
//
//   LCD  : ILI9341 320x240 on SPI2(HSPI)  SCK 14 / MOSI 13 / MISO 12 / CS 15 / DC 2 / BL 21
//          (14/12/13/15 are SPI2's native IO_MUX pins, so no GPIO matrix hop)
//   TOUCH: XPT2046 is NOT configured here. It is driven by a software-SPI (bit-bang)
//          driver in touch_xpt2046.cpp, because SPI3 is dedicated to the microSD slot.
#pragma once

#define LGFX_USE_V1
// SPI.h / SD.h must come BEFORE LovyanGFX.hpp: the fs::FS overloads of
// drawJpgFile() are only compiled in when FS_H is already defined.
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <LovyanGFX.hpp>

// ---- pin map -------------------------------------------------------------
#define CYD_TFT_SCLK 14
#define CYD_TFT_MOSI 13
#define CYD_TFT_MISO 12
#define CYD_TFT_CS   15
#define CYD_TFT_DC    2
#define CYD_TFT_RST  -1
#define CYD_TFT_BL   21

#define CYD_SD_SCLK  18
#define CYD_SD_MISO  19
#define CYD_SD_MOSI  23
#define CYD_SD_CS     5

#define CYD_TP_CLK   25
#define CYD_TP_MOSI  32
#define CYD_TP_MISO  39
#define CYD_TP_CS    33
#define CYD_TP_IRQ   36

#define CYD_LED_R     4
#define CYD_LED_G    16
#define CYD_LED_B    17

class LGFX_CYD : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;

public:
  LGFX_CYD(void)
  {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;   // HSPI. 14/12/13/15 are its IO_MUX pins.
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = CYD_TFT_SCLK;
      cfg.pin_mosi    = CYD_TFT_MOSI;
      cfg.pin_miso    = CYD_TFT_MISO;
      cfg.pin_dc      = CYD_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs           = CYD_TFT_CS;
      cfg.pin_rst          = CYD_TFT_RST;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;   // overridden at runtime from the stored config
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;   // SPI2 is used by the panel only
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = CYD_TFT_BL;
      cfg.invert      = false;
      cfg.freq        = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};
