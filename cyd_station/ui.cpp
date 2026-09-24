#include "ui.h"
#include "app.h"
#include "net.h"
#include "slideshow.h"
#include "config_store.h"
#include "touch_xpt2046.h"
#include <time.h>

LGFX_CYD lcd;

// ---- colours -------------------------------------------------------------
static const uint32_t C_BG      = 0x000000U;
static const uint32_t C_FG      = 0xFFFFFFU;
static const uint32_t C_ACCENT  = 0x33CCFFU;
static const uint32_t C_DIM     = 0x808080U;
static const uint32_t C_BAR_BG  = 0x202830U;
static const uint32_t C_PANEL   = 0x182030U;
static const uint32_t C_BORDER  = 0x4080C0U;
static const uint32_t C_NG      = 0xD04040U;

static const char* WDAY_JA[7] = { "日", "月", "火", "水", "木", "金", "土" };

// ---- overlay state -------------------------------------------------------
enum Overlay : uint8_t { OV_NONE = 0, OV_MENU, OV_QR };
static Overlay  s_overlay   = OV_NONE;
static uint32_t s_overlayAt = 0;
static const uint32_t OVERLAY_TIMEOUT_MS = 10000;

// ---- toast state ---------------------------------------------------------
static char     s_toast[64] = {0};
static uint32_t s_toastUntil = 0;

// ---- menu geometry -------------------------------------------------------
static const int MENU_X = 20, MENU_Y = 6, MENU_W = 280, MENU_H = 210;
static const int MENU_ITEM_H = 33;
static const int MENU_ITEM_Y = MENU_Y + 38;
static const int MENU_ITEMS  = 5;

void ui_begin() {
  lcd.init();
  lcd.setRotation(g_cfg.rotation);
  lcd.invertDisplay(g_cfg.invert);
  lcd.setBrightness(200);
  lcd.fillScreen(C_BG);
  lcd.setTextWrap(false);
}

void ui_splash() {
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(C_ACCENT, C_BG);
  lcd.setFont(&fonts::efontJA_24);
  lcd.drawString("CYD 情報ステーション", lcd.width() / 2, lcd.height() / 2 - 16);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("起動中...", lcd.width() / 2, lcd.height() / 2 + 20);
  lcd.setTextDatum(textdatum_t::top_left);
}

void ui_clearContent(uint32_t color) {
  lcd.fillRect(0, 0, lcd.width(), CONTENT_H, color);
}

void ui_showMessage(const char* title, const char* l1, const char* l2) {
  ui_clearContent(C_BG);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::efontJA_24);
  lcd.setTextColor(C_ACCENT, C_BG);
  lcd.drawString(title, lcd.width() / 2, 60);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextColor(C_FG, C_BG);
  if (l1) lcd.drawString(l1, lcd.width() / 2, 110);
  if (l2) lcd.drawString(l2, lcd.width() / 2, 136);
  lcd.setTextDatum(textdatum_t::top_left);
}

// ---------------------------------------------------------------- status bar
// The bar is permanent on the clock-only screen, but on an image it would eat
// 16 rows of the picture, so there it only appears while the menu/QR is open.
static bool statusBarVisible() {
  return app_mode() == MODE_CLOCK || s_overlay != OV_NONE;
}

void ui_drawStatusBar(bool force) {
  static String lastLeft, lastMid, lastRight;

  if (!statusBarVisible()) {
    // Forget the cache so the bar is fully repainted when it comes back.
    lastLeft = ""; lastMid = ""; lastRight = "";
    return;
  }

  // No clock here - the time is on the main screen. The bar only speaks up
  // while it is not synchronised yet.
  bool   ntp  = net_ntpSynced();
  String left = ntp ? String() : String("時刻未同期");
  String mid  = net_isConnected() ? net_ip() : String("WiFi未接続");
  String right;
  if (sd_available()) right += "SD ";
  // rounded to 8KB so ordinary heap churn does not repaint the bar 4x/second
  right += String((int)(ESP.getFreeHeap() / 8192) * 8) + "K";

  if (!force && left == lastLeft && mid == lastMid && right == lastRight) return;
  lastLeft = left; lastMid = mid; lastRight = right;

  const int y = lcd.height() - STATUSBAR_H;
  lcd.fillRect(0, y, lcd.width(), STATUSBAR_H, C_BAR_BG);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setTextColor(ntp ? C_FG : C_NG, C_BAR_BG);
  lcd.drawString(left.c_str(), 4, y);

  lcd.setTextColor(C_FG, C_BAR_BG);
  lcd.setTextDatum(textdatum_t::top_center);
  lcd.drawString(mid.c_str(), lcd.width() / 2, y);

  lcd.setTextColor(C_DIM, C_BAR_BG);
  lcd.setTextDatum(textdatum_t::top_right);
  lcd.drawString(right.c_str(), lcd.width() - 4, y);
  lcd.setTextDatum(textdatum_t::top_left);
}

// --------------------------------------------------------------------- clock
static void formatNow(char* date, size_t dlen, char* tm, size_t tlen) {
  if (!net_ntpSynced()) {
    // Before the first sync the RTC sits at the epoch; showing 1970 is worse
    // than admitting the clock is not set yet.
    snprintf(date, dlen, "時刻未同期");
    snprintf(tm, tlen, "--:--:--");
    return;
  }
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  snprintf(date, dlen, "%04d年%d月%d日(%s)",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, WDAY_JA[t.tm_wday % 7]);
  snprintf(tm, tlen, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
}

static void drawClockFull(bool force) {
  static String lastDate, lastHM, lastSS;
  char date[48], tm[16];
  formatNow(date, sizeof(date), tm, sizeof(tm));

  if (force) { lastDate = ""; lastHM = ""; lastSS = ""; }

  if (lastDate != date) {
    lastDate = date;
    lcd.setFont(&fonts::efontJA_24);
    lcd.setTextDatum(textdatum_t::top_center);
    lcd.setTextColor(C_ACCENT, C_BG);
    lcd.fillRect(0, 28, lcd.width(), 30, C_BG);
    lcd.drawString(date, lcd.width() / 2, 30);
  }

  // "HH:MM:" and "SS" are drawn separately at fixed positions so the once-a-
  // second repaint only touches the two seconds glyphs instead of the whole
  // field.  setTextColor(fg, bg) paints each glyph's background, so nothing has
  // to be cleared first - that is what made it flicker.
  String hm(tm);            // "HH:MM:"
  String ss(tm);            // "SS"
  hm.remove(6);
  ss.remove(0, 6);

  lcd.setFont(&fonts::Font7);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setTextColor(C_FG, C_BG);

  const int totalW = lcd.textWidth("88:88:88");
  const int hmW    = lcd.textWidth("88:88:");   // 7-segment digits are fixed width
  const int x0     = (lcd.width() - totalW) / 2;
  const int y0     = 130 - lcd.fontHeight() / 2;

  if (lastHM != hm) { lastHM = hm; lcd.drawString(hm.c_str(), x0, y0); }
  if (lastSS != ss) { lastSS = ss; lcd.drawString(ss.c_str(), x0 + hmW, y0); }

  lcd.setTextDatum(textdatum_t::top_left);
}

static void drawClockOverlay(bool force) {
  static String lastDate, lastHM, lastSS;
  char date[48], tm[16];
  formatNow(date, sizeof(date), tm, sizeof(tm));

  if (force) { lastDate = ""; lastHM = ""; lastSS = ""; }

  // 168 = 200 - 32: two fullwidth efontJA_16 characters of dead space trimmed
  // off the right hand edge.
  const int bx = 6, by = 6, bw = 168, bh = 48;

  // The panel and the date line are only repainted when the date changes (or
  // when the image underneath was just redrawn); the per-second update below
  // touches nothing but the glyphs it writes.
  if (lastDate != date) {
    lastDate = date;
    lastHM = ""; lastSS = "";        // the box repaint wiped the time as well
    lcd.fillRoundRect(bx, by, bw, bh, 6, C_BG);
    lcd.drawRoundRect(bx, by, bw, bh, 6, C_BORDER);
    lcd.setFont(&fonts::efontJA_16);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.drawString(date, bx + 8, by + 4);
  }

  String hm(tm);            // "HH:MM:"
  String ss(tm);            // "SS"
  hm.remove(6);
  ss.remove(0, 6);

  lcd.setFont(&fonts::efontJA_24);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setTextColor(C_FG, C_BG);

  const int hmW = lcd.textWidth("88:88:");   // digits are fixed width in efont
  const int tx  = bx + 8;
  const int ty  = by + 21;

  if (lastHM != hm) { lastHM = hm; lcd.drawString(hm.c_str(), tx, ty); }
  if (lastSS != ss) { lastSS = ss; lcd.drawString(ss.c_str(), tx + hmW, ty); }
}

void ui_drawClock(bool force) {
  if (s_overlay != OV_NONE) return;
  if (!g_cfg.clockVisible) return;
  if (app_mode() == MODE_CLOCK) drawClockFull(force);
  else                          drawClockOverlay(force);
}

// --------------------------------------------------------------------- toast
void ui_toast(const char* msg) {
  strncpy(s_toast, msg, sizeof(s_toast) - 1);
  s_toast[sizeof(s_toast) - 1] = 0;
  s_toastUntil = millis() + 2500;

  const int h = 26, y = CONTENT_H - h - 6;
  lcd.fillRoundRect(10, y, lcd.width() - 20, h, 5, C_PANEL);
  lcd.drawRoundRect(10, y, lcd.width() - 20, h, 5, C_BORDER);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(C_FG, C_PANEL);
  lcd.drawString(s_toast, lcd.width() / 2, y + h / 2);
  lcd.setTextDatum(textdatum_t::top_left);
}

void ui_loopToast() {
  if (!s_toastUntil) return;
  if ((int32_t)(millis() - s_toastUntil) < 0) return;
  s_toastUntil = 0;
  if (s_overlay == OV_NONE) app_redraw();
}

// ---------------------------------------------------------------------- menu
static bool slideshowSelectable() { return sd_available() && slideshow_count() > 0; }

static void menuItemLabel(int i, char* out, size_t len, bool* enabled) {
  *enabled = true;
  switch (i) {
    case 0:
      // Switches between "image + clock" and "clock only".
      if (app_mode() == MODE_IMAGE) {
        snprintf(out, len, "時計のみ表示");
      } else {
        *enabled = app_hasImage();
        snprintf(out, len, "%s", *enabled ? "画像+時計を表示" : "画像なし");
      }
      break;
    case 1:
      snprintf(out, len, "時計表示: %s", g_cfg.clockVisible ? "ON" : "OFF");
      break;
    case 2:
      snprintf(out, len, "接続用QRコードを表示");
      break;
    case 3:
      *enabled = slideshowSelectable();
      if (!*enabled)                snprintf(out, len, "スライドショー (SDなし)");
      else if (slideshow_running()) snprintf(out, len, "スライドショー: 停止");
      else                          snprintf(out, len, "スライドショー: 開始 (%d枚)", slideshow_count());
      break;
    default:
      snprintf(out, len, "閉じる");
      break;
  }
}

static void drawMenu() {
  lcd.fillRoundRect(MENU_X, MENU_Y, MENU_W, MENU_H, 8, C_PANEL);
  lcd.drawRoundRect(MENU_X, MENU_Y, MENU_W, MENU_H, 8, C_BORDER);

  lcd.setFont(&fonts::efontJA_24);
  lcd.setTextDatum(textdatum_t::top_center);
  lcd.setTextColor(C_ACCENT, C_PANEL);
  lcd.drawString("メニュー", MENU_X + MENU_W / 2, MENU_Y + 10);

  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(textdatum_t::middle_left);
  for (int i = 0; i < MENU_ITEMS; ++i) {
    char label[64];
    bool enabled;
    menuItemLabel(i, label, sizeof(label), &enabled);
    int y = MENU_ITEM_Y + i * MENU_ITEM_H;
    lcd.drawRoundRect(MENU_X + 10, y, MENU_W - 20, MENU_ITEM_H - 6, 4, enabled ? C_BORDER : C_DIM);
    lcd.setTextColor(enabled ? C_FG : C_DIM, C_PANEL);
    lcd.drawString(label, MENU_X + 22, y + (MENU_ITEM_H - 6) / 2);
  }
  lcd.setTextDatum(textdatum_t::top_left);
}

void ui_openMenu() {
  s_overlay   = OV_MENU;
  s_overlayAt = millis();
  drawMenu();
  ui_drawStatusBar(true);   // on an image the bar only shows while the menu is up
}

void ui_closeOverlay() {
  s_overlay = OV_NONE;
  app_redraw();
}

void ui_dismissOverlay() { s_overlay = OV_NONE; }

bool ui_overlayVisible() { return s_overlay != OV_NONE; }

void ui_showQr() {
  s_overlay   = OV_QR;
  s_overlayAt = millis();
  ui_clearContent(C_BG);

  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(textdatum_t::top_left);

  if (!net_isConnected()) {
    lcd.setTextColor(C_NG, C_BG);
    lcd.drawString("WiFi未接続です", 12, 40);
    lcd.setTextColor(C_FG, C_BG);
    lcd.drawString("USBシリアル(115200)で", 12, 70);
    lcd.drawString("wifi <SSID> <PASS>", 12, 92);
    lcd.drawString("を実行してください", 12, 114);
    ui_drawStatusBar(true);
    return;
  }

  String url  = "http://" + net_ip() + "/";
  String mdns = "http://" + g_cfg.hostname + ".local/";

  // Centred column: title, QR, then the two URLs. Keeps a margin on every side
  // of the 320x224 content area.
  const int qrSize = 126;
  const int qrX    = (lcd.width() - qrSize) / 2;
  const int qrY    = 24;

  lcd.setTextDatum(textdatum_t::top_center);
  lcd.setTextColor(C_ACCENT, C_BG);
  lcd.drawString("スマホ・PCから接続", lcd.width() / 2, 4);

  // A quiet zone is required for the code to scan; lcd.qrcode() does not draw
  // one on its own, and the page background here is black.
  lcd.fillRect(qrX - 8, qrY - 8, qrSize + 16, qrSize + 16, 0xFFFFFFU);
  lcd.qrcode(url.c_str(), qrX, qrY, qrSize, 3);

  lcd.setTextColor(C_FG, C_BG);
  lcd.drawString(url.c_str(), lcd.width() / 2, qrY + qrSize + 8);
  lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString(mdns.c_str(), lcd.width() / 2, qrY + qrSize + 28);
  lcd.drawString("画面タップで戻る", lcd.width() / 2, qrY + qrSize + 50);
  lcd.setTextDatum(textdatum_t::top_left);
  ui_drawStatusBar(true);
}

void ui_loopOverlay() {
  if (s_overlay == OV_NONE) return;
  if (millis() - s_overlayAt > OVERLAY_TIMEOUT_MS) ui_closeOverlay();
}

void ui_handleTap(int16_t x, int16_t y) {
  if (s_overlay == OV_QR) { ui_closeOverlay(); return; }

  if (s_overlay == OV_NONE) {
    if (y < CONTENT_H) ui_openMenu();
    return;
  }

  // menu
  s_overlayAt = millis();
  if (x < MENU_X || x > MENU_X + MENU_W || y < MENU_ITEM_Y) {
    ui_closeOverlay();
    return;
  }
  int idx = (y - MENU_ITEM_Y) / MENU_ITEM_H;
  if (idx < 0 || idx >= MENU_ITEMS) { ui_closeOverlay(); return; }

  switch (idx) {
    case 0:
      if (app_mode() == MODE_IMAGE) {
        s_overlay = OV_NONE;
        app_setMode(MODE_CLOCK);
      } else if (app_hasImage()) {
        slideshow_stop();
        s_overlay = OV_NONE;
        app_setMode(MODE_IMAGE);
      } else {
        drawMenu();
      }
      break;
    case 1:
      // Apply immediately and get out of the way so the result is visible.
      g_cfg.clockVisible = !g_cfg.clockVisible;
      config_save();
      ui_closeOverlay();
      break;
    case 2:
      ui_showQr();
      break;
    case 3:
      if (!slideshowSelectable()) { drawMenu(); break; }
      if (slideshow_running()) {
        slideshow_stop();
        s_overlay = OV_NONE;
        app_setMode(app_hasImage() ? MODE_IMAGE : MODE_CLOCK);
      } else {
        s_overlay = OV_NONE;
        app_setMode(MODE_SLIDESHOW);
        slideshow_start();
      }
      break;
    default:
      ui_closeOverlay();
      break;
  }
}

// -------------------------------------------------------------- calibration
static bool waitForTap(int16_t& rx, int16_t& ry, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  // wait for the previous press to be released first
  while (millis() - t0 < timeoutMs) {
    int16_t a, b;
    if (!touch_readRaw(a, b)) break;
    delay(20);
  }
  while (millis() - t0 < timeoutMs) {
    int16_t a, b;
    if (touch_readRaw(a, b)) {
      delay(60);                       // let the finger settle
      if (touch_readRaw(a, b)) { rx = a; ry = b; return true; }
    }
    delay(20);
  }
  return false;
}

static void drawTarget(int16_t x, int16_t y, const char* text) {
  ui_clearContent(C_BG);
  lcd.drawCircle(x, y, 10, C_ACCENT);
  lcd.drawCircle(x, y, 3, C_ACCENT);
  lcd.drawFastHLine(x - 16, y, 32, C_ACCENT);
  lcd.drawFastVLine(x, y - 16, 32, C_ACCENT);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(C_FG, C_BG);
  lcd.drawString(text, lcd.width() / 2, CONTENT_H / 2);
  lcd.setTextDatum(textdatum_t::top_left);
}

void ui_runTouchCalibration() {
  const int16_t P_X0 = 24, P_Y0 = 24;
  const int16_t P_X1 = lcd.width() - 24;
  const int16_t P_Y2 = CONTENT_H   - 24;

  int16_t rx0, ry0, rx1, ry1, rx2, ry2;
  s_overlay = OV_NONE;

  drawTarget(P_X0, P_Y0, "左上の印を押してください (1/3)");
  if (!waitForTap(rx0, ry0, 20000)) goto timeout;

  drawTarget(P_X1, P_Y0, "右上の印を押してください (2/3)");
  if (!waitForTap(rx1, ry1, 20000)) goto timeout;

  drawTarget(P_X0, P_Y2, "左下の印を押してください (3/3)");
  if (!waitForTap(rx2, ry2, 20000)) goto timeout;

  touch_calibrateFrom(rx0, ry0, P_X0, P_Y0,
                      rx1, ry1, P_X1,
                      rx2, ry2, P_Y2,
                      lcd.width(), lcd.height());
  config_save();

  Serial.printf("[calib] swapxy=%d x:%d..%d y:%d..%d\n",
                (int)g_cfg.calib.swapxy, (int)g_cfg.calib.xmin, (int)g_cfg.calib.xmax,
                (int)g_cfg.calib.ymin, (int)g_cfg.calib.ymax);

  ui_showMessage("校正完了", "設定を保存しました", nullptr);
  delay(1200);
  app_redraw();
  return;

timeout:
  ui_showMessage("校正中止", "タイムアウトしました", nullptr);
  delay(1500);
  app_redraw();
}
