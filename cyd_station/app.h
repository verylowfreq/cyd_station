// app.h - application wide state shared between the modules
#pragma once
#include <Arduino.h>

enum AppMode : uint8_t {
  MODE_CLOCK = 0,
  MODE_IMAGE,
  MODE_SLIDESHOW,
};

// The single JPEG buffer.  Allocated once in setup() *before* WiFi starts, and
// never freed: after WiFi is up the heap is too fragmented to hand out a
// 100KB contiguous block.
extern uint8_t* g_jpgBuf;
extern size_t   g_jpgCap;
extern size_t   g_jpgLen;   // bytes currently held

AppMode app_mode();
void    app_setMode(AppMode m);
bool    app_hasImage();

// Redraws whatever the current mode shows (used after closing the menu/QR).
void app_redraw();

// Takes ownership of the freshly received JPEG already sitting in g_jpgBuf
// (length len), shows it and optionally writes it to SD as /last.jpg.
bool app_commitImage(size_t len, bool saveToSd);
