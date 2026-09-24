// ui.h - all drawing: clock, status bar, menu, QR code, toast
#pragma once
#include <Arduino.h>
#include "LGFX_CYD.hpp"

extern LGFX_CYD lcd;

static const int STATUSBAR_H = 16;
#define CONTENT_H (lcd.height() - STATUSBAR_H)

void ui_begin();
void ui_splash();

void ui_drawStatusBar(bool force);
void ui_drawClock(bool force);        // honours the current mode and clock visibility
void ui_clearContent(uint32_t color);
void ui_showMessage(const char* title, const char* l1, const char* l2);

void ui_toast(const char* msg);
void ui_loopToast();

bool ui_overlayVisible();             // menu or QR is on screen
void ui_openMenu();
void ui_closeOverlay();               // close and repaint the current mode
void ui_dismissOverlay();             // drop the overlay without repainting
void ui_handleTap(int16_t x, int16_t y);
void ui_loopOverlay();                // auto-close timer

void ui_showQr();
void ui_runTouchCalibration();
