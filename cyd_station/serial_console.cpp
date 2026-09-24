#include "serial_console.h"
#include "config_store.h"
#include "net.h"
#include "ui.h"
#include "app.h"
#include "slideshow.h"
#include "touch_xpt2046.h"
#include <WiFi.h>

static String s_line;

static void printHelp() {
  Serial.println(F(
    "\n--- CYD 情報ステーション / シリアルコマンド ---\n"
    "  help                  このヘルプ\n"
    "  status                接続状態・メモリ・モードを表示\n"
    "  wifi                  保存済みSSIDを表示\n"
    "  wifi <SSID> <PASS>    WiFi設定を保存して再接続 (空白を含む場合は \"...\")\n"
    "  scan                  周囲のAPをスキャン\n"
    "  host <name>           mDNSホスト名 (既定 cyd-station)\n"
    "  tz <POSIX TZ>         タイムゾーン (既定 JST-9)\n"
    "  ntp <server>          NTPサーバ (既定 ntp.nict.jp)\n"
    "  rotate <1|3>          画面の向き\n"
    "  invert <on|off>       色反転\n"
    "  clock <on|off>        時計表示\n"
    "  slide <sec>           スライドショー間隔 (2-3600)\n"
    "  mode <clock|image|slideshow>\n"
    "  sd                    SDを再マウントしてAlbum/を再スキャン\n"
    "  calib                 タッチパネルの3点校正\n"
    "  touchraw              タッチ生値を5秒間表示 (デバッグ用)\n"
    "  reboot                再起動\n"
    "  erase                 設定を初期化して再起動\n"));
}

static void printStatus() {
  Serial.println(F("\n--- status ---"));
  Serial.printf("  WiFi      : %s\n", net_isConnected() ? "connected" : "disconnected");
  Serial.printf("  SSID      : %s\n", g_cfg.ssid.length() ? g_cfg.ssid.c_str() : "(未設定)");
  Serial.printf("  IP        : %s\n", net_ip().c_str());
  Serial.printf("  RSSI      : %d dBm\n", (int)WiFi.RSSI());
  Serial.printf("  mDNS      : %s.local\n", g_cfg.hostname.c_str());
  Serial.printf("  NTP       : %s (%s, TZ=%s)\n",
                net_ntpSynced() ? "synced" : "not synced", g_cfg.ntp.c_str(), g_cfg.tz.c_str());
  {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &t);
    Serial.printf("  localtime : %s\n", b);
  }
  Serial.printf("  SD        : %s (Album %d files)\n",
                sd_available() ? "mounted" : "none", slideshow_count());
  Serial.printf("  mode      : %d (slideshow=%d)\n", (int)app_mode(), (int)slideshow_running());
  Serial.printf("  image     : %u bytes\n", (unsigned)g_jpgLen);
  Serial.printf("  heap      : %u free / %u largest block\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  Serial.printf("  uptime    : %lu s\n", (unsigned long)(millis() / 1000));
  Serial.printf("  touch cal : swap=%d x %d..%d y %d..%d\n",
                (int)g_cfg.calib.swapxy, (int)g_cfg.calib.xmin, (int)g_cfg.calib.xmax,
                (int)g_cfg.calib.ymin, (int)g_cfg.calib.ymax);
}

// Splits a command line, honouring "double quoted" arguments.
static int tokenize(const String& line, String* out, int maxTok) {
  int n = 0;
  size_t i = 0;
  while (i < line.length() && n < maxTok) {
    while (i < line.length() && isspace((unsigned char)line[i])) ++i;
    if (i >= line.length()) break;
    String tok;
    if (line[i] == '"') {
      ++i;
      while (i < line.length() && line[i] != '"') tok += line[i++];
      if (i < line.length()) ++i;
    } else {
      while (i < line.length() && !isspace((unsigned char)line[i])) tok += line[i++];
    }
    out[n++] = tok;
  }
  return n;
}

static bool parseOnOff(const String& s, bool& out) {
  if (s == "on"  || s == "1" || s == "true")  { out = true;  return true; }
  if (s == "off" || s == "0" || s == "false") { out = false; return true; }
  return false;
}

static void doTouchRaw() {
  Serial.println(F("[touchraw] 5秒間、画面を押してください"));
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    int16_t rx, ry, sx, sy;
    if (touch_readRaw(rx, ry)) {
      if (touch_read(sx, sy)) Serial.printf("  raw=(%4d,%4d)  screen=(%3d,%3d)\n", rx, ry, sx, sy);
      else                    Serial.printf("  raw=(%4d,%4d)\n", rx, ry);
    }
    delay(120);
  }
  Serial.println(F("[touchraw] 終了"));
}

static void execute(const String& line) {
  String a[8];
  int n = tokenize(line, a, 8);
  if (n == 0) return;
  String cmd = a[0];
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    printHelp();

  } else if (cmd == "status") {
    printStatus();

  } else if (cmd == "wifi") {
    if (n == 1) {
      Serial.printf("  SSID=%s PASS=%s\n",
                    g_cfg.ssid.length() ? g_cfg.ssid.c_str() : "(未設定)",
                    g_cfg.pass.length() ? "********" : "(なし)");
    } else if (n >= 2) {
      g_cfg.ssid = a[1];
      g_cfg.pass = (n >= 3) ? a[2] : "";
      config_save();
      Serial.printf("  saved: SSID=%s\n", g_cfg.ssid.c_str());
      net_applyWifi();
    }

  } else if (cmd == "scan") {
    Serial.println(F("  scanning..."));
    int found = WiFi.scanNetworks();
    for (int i = 0; i < found; ++i) {
      Serial.printf("  %2d) %-32s %4d dBm %s\n", i + 1,
                    WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
    }
    WiFi.scanDelete();

  } else if (cmd == "host" && n >= 2) {
    g_cfg.hostname = a[1];
    config_save();
    Serial.printf("  hostname=%s.local (要再起動)\n", g_cfg.hostname.c_str());

  } else if (cmd == "tz" && n >= 2) {
    g_cfg.tz = a[1];
    config_save();
    net_applyTime();
    Serial.printf("  TZ=%s\n", g_cfg.tz.c_str());

  } else if (cmd == "ntp" && n >= 2) {
    g_cfg.ntp = a[1];
    config_save();
    net_applyTime();
    Serial.printf("  NTP=%s\n", g_cfg.ntp.c_str());

  } else if (cmd == "rotate" && n >= 2) {
    int r = a[1].toInt();
    if (r != 1 && r != 3) { Serial.println(F("  rotate 1 or 3")); return; }
    g_cfg.rotation = (uint8_t)r;
    config_save();
    lcd.setRotation(g_cfg.rotation);
    app_redraw();
    Serial.printf("  rotation=%d (タッチ校正 calib を再実行してください)\n", r);

  } else if (cmd == "invert" && n >= 2) {
    bool v;
    if (!parseOnOff(a[1], v)) { Serial.println(F("  invert on|off")); return; }
    g_cfg.invert = v;
    config_save();
    lcd.invertDisplay(v);
    Serial.printf("  invert=%s\n", v ? "on" : "off");

  } else if (cmd == "clock" && n >= 2) {
    bool v;
    if (!parseOnOff(a[1], v)) { Serial.println(F("  clock on|off")); return; }
    g_cfg.clockVisible = v;
    config_save();
    app_redraw();
    Serial.printf("  clock=%s\n", v ? "on" : "off");

  } else if (cmd == "slide" && n >= 2) {
    long s = a[1].toInt();
    if (s < 2 || s > 3600) { Serial.println(F("  slide 2..3600")); return; }
    g_cfg.slideSec = (uint16_t)s;
    config_save();
    Serial.printf("  slide=%d sec\n", (int)g_cfg.slideSec);

  } else if (cmd == "mode" && n >= 2) {
    if (a[1] == "clock")          { slideshow_stop(); app_setMode(MODE_CLOCK); }
    else if (a[1] == "image") {
      if (!app_hasImage()) { Serial.println(F("  表示できる画像がありません")); return; }
      slideshow_stop();
      app_setMode(MODE_IMAGE);
    }
    else if (a[1] == "slideshow") {
      if (slideshow_count() == 0) slideshow_scan();
      if (slideshow_count() == 0) { Serial.println(F("  Album/ に画像がありません")); return; }
      app_setMode(MODE_SLIDESHOW);
      slideshow_start();
    }
    else Serial.println(F("  mode clock|image|slideshow"));

  } else if (cmd == "sd") {
    sd_remount();
    Serial.printf("  SD=%s, Album=%d files\n", sd_available() ? "ok" : "none", slideshow_count());

  } else if (cmd == "calib") {
    ui_runTouchCalibration();

  } else if (cmd == "touchraw") {
    doTouchRaw();

  } else if (cmd == "reboot") {
    Serial.println(F("  rebooting..."));
    delay(200);
    ESP.restart();

  } else if (cmd == "erase") {
    config_erase();
    Serial.println(F("  設定を初期化しました。再起動します。"));
    delay(200);
    ESP.restart();

  } else {
    Serial.printf("  unknown command: %s  (help で一覧)\n", cmd.c_str());
  }
}

void console_begin() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("CYD 情報ステーション - 'help' でコマンド一覧"));
}

void console_loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = s_line;
      s_line = "";
      line.trim();
      if (line.length()) execute(line);
      continue;
    }
    if (s_line.length() < 160) s_line += c;
  }
}
