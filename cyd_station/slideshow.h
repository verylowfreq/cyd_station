// slideshow.h - microSD access and the Album/ slideshow
#pragma once
#include <Arduino.h>

// SPI3 (VSPI) is dedicated to the SD card: SCK 18 / MISO 19 / MOSI 23 / CS 5.
bool sd_begin();
bool sd_available();
bool sd_remount();

bool   sd_saveLastImage(const uint8_t* data, size_t len);
size_t sd_loadLastImage(uint8_t* buf, size_t cap);   // 0 when unavailable

void slideshow_scan();            // (re)builds the Album/ file list
int  slideshow_count();
bool slideshow_running();
void slideshow_start();
void slideshow_stop();
void slideshow_next();
void slideshow_redraw();      // repaint the current slide
void slideshow_loop();
