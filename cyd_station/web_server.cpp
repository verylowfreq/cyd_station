#include "web_server.h"
#include "web_ui.h"
#include "app.h"
#include "net.h"
#include "ui.h"
#include "slideshow.h"
#include "config_store.h"
#include <WebServer.h>

static WebServer server(80);

// Upload state.  The body is streamed straight into g_jpgBuf so no second
// copy of the image ever exists in RAM.
static size_t s_rxLen    = 0;
static bool   s_overflow = false;

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", UPLOAD_HTML);
}

static void handleStatus() {
  const char* modeName = "clock";
  switch (app_mode()) {
    case MODE_IMAGE:     modeName = "image"; break;
    case MODE_SLIDESHOW: modeName = "slideshow"; break;
    default: break;
  }
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"ip\":\"%s\",\"hostname\":\"%s\",\"ntp\":%s,\"sd\":%s,\"album\":%d,"
           "\"mode\":\"%s\",\"slideshow\":%s,\"image\":%u,\"heap\":%u,\"uptime\":%lu}",
           net_ip().c_str(), g_cfg.hostname.c_str(),
           net_ntpSynced() ? "true" : "false",
           sd_available() ? "true" : "false",
           slideshow_count(), modeName,
           slideshow_running() ? "true" : "false",
           (unsigned)g_jpgLen, (unsigned)ESP.getFreeHeap(),
           (unsigned long)(millis() / 1000));
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

static void handleImage() {
  if (!app_hasImage()) { server.send(404, "text/plain", "no image"); return; }
  server.setContentLength(g_jpgLen);
  server.send(200, "image/jpeg", "");
  server.client().write(g_jpgBuf, g_jpgLen);
}

static void handleMode() {
  String m = server.arg("m");
  if (m == "clock") {
    slideshow_stop();
    app_setMode(MODE_CLOCK);
  } else if (m == "image") {
    if (!app_hasImage()) { server.send(409, "text/plain", "no image"); return; }
    slideshow_stop();
    app_setMode(MODE_IMAGE);
  } else if (m == "slideshow") {
    if (!sd_available()) { server.send(409, "text/plain", "no sd"); return; }
    if (slideshow_count() == 0) slideshow_scan();
    if (slideshow_count() == 0) { server.send(409, "text/plain", "no images in Album/"); return; }
    app_setMode(MODE_SLIDESHOW);
    slideshow_start();
  } else {
    server.send(400, "text/plain", "m must be clock|image|slideshow");
    return;
  }
  server.send(200, "text/plain", "ok");
}

static void handleRescan() {
  sd_remount();
  server.send(200, "text/plain", String(slideshow_count()));
}

static void handleUploadData() {
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      s_rxLen    = 0;
      s_overflow = false;
      g_jpgLen   = 0;                 // the old image is being overwritten
      Serial.printf("[http] upload start: %s\n", up.filename.c_str());
      break;

    case UPLOAD_FILE_WRITE:
      if (s_overflow) break;
      if (s_rxLen + up.currentSize > g_jpgCap) { s_overflow = true; break; }
      memcpy(g_jpgBuf + s_rxLen, up.buf, up.currentSize);
      s_rxLen += up.currentSize;
      break;

    case UPLOAD_FILE_END:
      Serial.printf("[http] upload end: %u bytes%s\n",
                    (unsigned)s_rxLen, s_overflow ? " (OVERFLOW)" : "");
      break;

    default:
      s_overflow = true;
      break;
  }
}

static void handleUploadDone() {
  if (s_overflow) {
    char m[64];
    snprintf(m, sizeof(m), "image too large (max %u bytes)", (unsigned)g_jpgCap);
    server.send(413, "text/plain", m);
    return;
  }
  if (s_rxLen < 4 || g_jpgBuf[0] != 0xFF || g_jpgBuf[1] != 0xD8) {
    server.send(400, "text/plain", "not a JPEG");
    return;
  }
  slideshow_stop();
  bool ok = app_commitImage(s_rxLen, true);
  if (!ok) { server.send(500, "text/plain", "decode failed"); return; }
  server.send(200, "text/plain", "ok");
}

static void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

void web_begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/rescan", HTTP_POST, handleRescan);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[http] server started on :80");
}

void web_loop() {
  server.handleClient();
}
