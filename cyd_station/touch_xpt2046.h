// touch_xpt2046.h - software-SPI (bit-bang) driver for the XPT2046 touch controller.
//
// ESP32 only has two general purpose SPI hosts (SPI2/SPI3) but the CYD carries
// three SPI devices.  The LCD (SPI3) and the microSD slot (SPI2) both need the
// throughput, so the touch controller - a slow, <=2MHz part - is bit-banged.
#pragma once
#include <Arduino.h>

void touch_begin();

// True while a finger is on the panel.  x/y are screen coordinates for the
// current rotation.
bool touch_read(int16_t& x, int16_t& y);

// Returns true exactly once per press (rising edge), with the press position.
bool touch_tapped(int16_t& x, int16_t& y);

// Raw (uncalibrated) 12bit values; false if nothing is touched.
bool touch_readRaw(int16_t& rx, int16_t& ry);

// 3-point calibration helper: feeds raw samples measured at known screen
// points and stores the result into g_cfg.calib.
void touch_calibrateFrom(int16_t rx0, int16_t ry0, int16_t sx0, int16_t sy0,
                         int16_t rx1, int16_t ry1, int16_t sx1,
                         int16_t rx2, int16_t ry2, int16_t sy2,
                         int16_t screenW, int16_t screenH);
