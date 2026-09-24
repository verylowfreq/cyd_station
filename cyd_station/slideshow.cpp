#include "slideshow.h"
#include "ui.h"          // pulls in LGFX_CYD.hpp, which includes SPI.h/SD.h first
#include "app.h"
#include "config_store.h"
#include <algorithm>
#include <vector>

static SPIClass sdSPI(VSPI);          // SPI3 - dedicated to the SD card (18/19/23/5 are its IO_MUX pins)
static bool s_sdOk = false;

static std::vector<String> s_files;
static int      s_index    = 0;
static bool     s_running  = false;
static uint32_t s_lastStep = 0;

static const char* ALBUM_DIRS[] = { "/Album", "/album", "/ALBUM" };

static const char* MSG_BAD_IMAGE_T = "表示できない画像";
static const char* MSG_BAD_IMAGE_B = "ベースラインJPEGのみ対応です";

bool sd_available() { return s_sdOk; }

bool sd_begin() {
  sdSPI.begin(CYD_SD_SCLK, CYD_SD_MISO, CYD_SD_MOSI, CYD_SD_CS);
  s_sdOk = SD.begin(CYD_SD_CS, sdSPI, 20000000);
  if (!s_sdOk) {
    // Some cards need a slower first handshake.
    s_sdOk = SD.begin(CYD_SD_CS, sdSPI, 4000000);
  }
  Serial.printf("[sd] %s\n", s_sdOk ? "mounted" : "not present");
  if (s_sdOk) slideshow_scan();
  return s_sdOk;
}

bool sd_remount() {
  if (s_sdOk) { SD.end(); s_sdOk = false; }
  s_files.clear();
  s_index = 0;
  return sd_begin();
}

static bool hasJpegExt(const String& name) {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".jpg") || n.endsWith(".jpeg");
}

void slideshow_scan() {
  s_files.clear();
  s_index = 0;
  if (!s_sdOk) return;

  File dir;
  const char* album = nullptr;
  for (size_t i = 0; i < sizeof(ALBUM_DIRS) / sizeof(ALBUM_DIRS[0]); ++i) {
    dir = SD.open(ALBUM_DIRS[i]);
    if (dir && dir.isDirectory()) { album = ALBUM_DIRS[i]; break; }
    if (dir) dir.close();
    dir = File();
  }
  if (!album) {
    Serial.println("[slideshow] /Album not found");
    return;
  }

  while (s_files.size() < 256) {
    File f = dir.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      // File::name() is the bare name on ESP32 core 3.x, so rebuild the path.
      String name = f.name();
      if (!name.startsWith("/")) name = String(album) + "/" + name;
      if (hasJpegExt(name)) s_files.push_back(name);
    }
    f.close();
  }
  dir.close();

  std::sort(s_files.begin(), s_files.end(),
            [](const String& a, const String& b) { return strcmp(a.c_str(), b.c_str()) < 0; });
  Serial.printf("[slideshow] %d image(s) found\n", (int)s_files.size());
}

int  slideshow_count()   { return (int)s_files.size(); }
bool slideshow_running() { return s_running; }

static void showIndex(int i) {
  if (s_files.empty()) return;
  const String& path = s_files[i];
  // Images are expected to be 320x240.  Anything larger is cropped to the
  // top-left 320x240 because maxWidth/maxHeight clip the decode.
  bool ok = lcd.drawJpgFile(SD, path.c_str(), 0, 0, lcd.width(), lcd.height(), 0, 0);
  if (!ok) {
    // tjpgd cannot decode progressive JPEG.
    Serial.printf("[slideshow] decode failed (progressive JPEG?): %s\n", path.c_str());
    // No toast here: its expiry calls app_redraw(), which would re-enter showIndex().
    ui_showMessage(MSG_BAD_IMAGE_T, MSG_BAD_IMAGE_B, path.c_str());
  }
  ui_drawStatusBar(true);
  ui_drawClock(true);
}

void slideshow_start() {
  if (!s_sdOk) return;
  if (s_files.empty()) slideshow_scan();
  if (s_files.empty()) { ui_toast("Album/ に画像がありません"); return; }
  s_running  = true;
  s_lastStep = millis();
  showIndex(s_index);
}

void slideshow_stop() { s_running = false; }

void slideshow_redraw() {
  if (!s_files.empty()) showIndex(s_index);
}

void slideshow_next() {
  if (s_files.empty()) return;
  s_index = (s_index + 1) % s_files.size();
  s_lastStep = millis();
  showIndex(s_index);
}

void slideshow_loop() {
  if (!s_running) return;
  if (ui_overlayVisible()) return;   // never paint over an open menu / QR code
  if (millis() - s_lastStep < (uint32_t)g_cfg.slideSec * 1000UL) return;
  slideshow_next();
}

// ---------------------------------------------------------------- last image
static const char* LAST_PATH = "/last.jpg";

bool sd_saveLastImage(const uint8_t* data, size_t len) {
  if (!s_sdOk || !data || !len) return false;
  File f = SD.open(LAST_PATH, FILE_WRITE);
  if (!f) return false;
  size_t w = f.write(data, len);
  f.close();
  return w == len;
}

size_t sd_loadLastImage(uint8_t* buf, size_t cap) {
  if (!s_sdOk || !buf) return 0;
  File f = SD.open(LAST_PATH, FILE_READ);
  if (!f) return 0;
  size_t len = f.size();
  if (len == 0 || len > cap) { f.close(); return 0; }
  size_t r = f.read(buf, len);
  f.close();
  return (r == len) ? len : 0;
}
