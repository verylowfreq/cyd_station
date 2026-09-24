// config_store.h - persistent settings in NVS (Preferences)
#pragma once
#include <Arduino.h>

struct TouchCalib {
  int16_t xmin = 300;
  int16_t xmax = 3800;
  int16_t ymin = 300;
  int16_t ymax = 3800;
  bool    swapxy = true;   // landscape: the XPT2046 X axis runs down the screen
};

struct AppConfig {
  String   ssid;
  String   pass;
  String   hostname     = "cyd-station";
  String   tz           = "JST-9";        // POSIX TZ
  String   ntp          = "ntp.nict.jp";
  uint8_t  rotation     = 1;              // 1 or 3 (landscape)
  bool     invert       = false;
  bool     clockVisible = true;
  uint16_t slideSec     = 10;
  TouchCalib calib;
};

extern AppConfig g_cfg;

void config_begin();
void config_save();
void config_erase();
