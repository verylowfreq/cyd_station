/*
  CYD 情報ステーション  -  Cheap Yellow Display (ESP32-2432S028) desk info station

    - NTP synchronised clock (year / month / day / weekday / time / seconds)
    - WiFi credentials configured over USB serial
    - mDNS  : http://cyd-station.local/
    - HTTP  : JPEG upload from anywhere + browser crop UI
    - microSD slideshow of /Album
    - tap the screen for the menu, permanent status bar at the bottom

  Board : ESP32 Dev Module, Partition Scheme = "Huge APP (3MB No OTA/1MB SPIFFS)"
  See README.md for wiring and usage.
*/

#include "LGFX_CYD.hpp"
#include "app.h"
#include "config_store.h"
#include "ui.h"
#include "net.h"
#include "web_server.h"
#include "serial_console.h"
#include "slideshow.h"
#include "touch_xpt2046.h"

// ---- global image buffer -------------------------------------------------
uint8_t* g_jpgBuf = nullptr;
size_t   g_jpgCap = 0;
size_t   g_jpgLen = 0;

static const size_t JPG_BUF_SIZE = 100 * 1024;   // ~100KB is plenty for 320x240

static AppMode s_mode    = MODE_CLOCK;
static bool    s_imageOk = false;   // did the last JPEG decode succeed?

AppMode app_mode()     { return s_mode; }
bool    app_hasImage() { return g_jpgBuf && g_jpgLen > 0; }

void app_redraw() {
  switch (s_mode) {
    case MODE_IMAGE:
      if (app_hasImage()) {
        s_imageOk = lcd.drawJpg(g_jpgBuf, g_jpgLen, 0, 0, lcd.width(), lcd.height(), 0, 0);
        if (!s_imageOk) {
          ui_showMessage("画像を表示できません", "ベースラインJPEGのみ対応です", nullptr);
        }
      } else {
        // Defensive: every path into MODE_IMAGE checks app_hasImage() first.
        ui_showMessage("画像がありません", "http://<IP>/ からアップロードできます", nullptr);
        lcd.fillRect(0, CONTENT_H, lcd.width(), STATUSBAR_H, 0x000000U);
      }
      break;

    case MODE_SLIDESHOW:
      slideshow_redraw();
      break;

    case MODE_CLOCK:
    default:
      ui_clearContent(0x000000U);
      if (!g_cfg.clockVisible) {
        lcd.setFont(&fonts::efontJA_16);
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(0x606060U, 0x000000U);
        lcd.drawString("画面をタップでメニュー", lcd.width() / 2, CONTENT_H / 2);
        lcd.setTextDatum(textdatum_t::top_left);
      }
      break;
  }
  ui_drawClock(true);
  ui_drawStatusBar(true);
}

void app_setMode(AppMode m) {
  s_mode = m;
  ui_dismissOverlay();   // an HTTP/serial mode change must not leave a ghost menu
  app_redraw();
}

bool app_commitImage(size_t len, bool saveToSd) {
  if (!g_jpgBuf || len == 0 || len > g_jpgCap) return false;
  g_jpgLen = len;

  if (saveToSd && sd_available()) {
    if (sd_saveLastImage(g_jpgBuf, g_jpgLen)) Serial.println("[sd] saved /last.jpg");
    else                                      Serial.println("[sd] failed to save /last.jpg");
  }
  app_setMode(MODE_IMAGE);
  return s_imageOk;
}

// -------------------------------------------------------------------- setup
static void ledsOff() {
  // The on-board RGB LED is active low: drive all three high or it glows.
  pinMode(CYD_LED_R, OUTPUT);
  pinMode(CYD_LED_G, OUTPUT);
  pinMode(CYD_LED_B, OUTPUT);
  digitalWrite(CYD_LED_R, HIGH);
  digitalWrite(CYD_LED_G, HIGH);
  digitalWrite(CYD_LED_B, HIGH);
}

void setup() {
  ledsOff();
  console_begin();
  config_begin();

  ui_begin();
  ui_splash();
  touch_begin();

  // Allocate the JPEG buffer before WiFi comes up: afterwards the heap is too
  // fragmented to hand out a 100KB contiguous block.
  g_jpgBuf = (uint8_t*)malloc(JPG_BUF_SIZE);
  g_jpgCap = g_jpgBuf ? JPG_BUF_SIZE : 0;
  Serial.printf("[mem] jpeg buffer %u bytes %s\n",
                (unsigned)g_jpgCap, g_jpgBuf ? "ok" : "FAILED");

  sd_begin();
  if (g_jpgBuf && sd_available()) {
    size_t n = sd_loadLastImage(g_jpgBuf, g_jpgCap);
    if (n) {
      g_jpgLen = n;
      Serial.printf("[sd] restored /last.jpg (%u bytes)\n", (unsigned)n);
    }
  }

  net_begin();
  web_begin();

  // Boot into "image + clock" when /last.jpg was restored from the card,
  // otherwise show the clock on its own.
  s_mode = app_hasImage() ? MODE_IMAGE : MODE_CLOCK;
  if (g_cfg.ssid.length() == 0) {
    ui_showMessage("WiFi未設定",
                   "USBシリアル(115200)で",
                   "wifi <SSID> <PASS> を実行");
    ui_drawStatusBar(true);
    delay(2500);
  }
  app_redraw();
}

// --------------------------------------------------------------------- loop
void loop() {
  console_loop();
  web_loop();
  net_loop();
  slideshow_loop();

  int16_t tx, ty;
  if (touch_tapped(tx, ty)) {
    ui_handleTap(tx, ty);
  }

  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 250) {
    lastTick = millis();
    ui_drawClock(false);
    ui_drawStatusBar(false);
  }

  ui_loopOverlay();
  ui_loopToast();
  delay(2);
}
