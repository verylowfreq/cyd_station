#include "net.h"
#include "config_store.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_sntp.h>
#include <time.h>

static volatile bool s_ntpSynced = false;
static bool     s_mdnsUp    = false;
static volatile bool s_wantMdns = false;
static uint32_t s_lastRetry = 0;
static const uint32_t RETRY_MS = 15000;

static void onTimeSync(struct timeval* tv) {
  (void)tv;
  s_ntpSynced = true;
}

static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
      // MDNS.begin() is started from net_loop(), not from this event task.
      s_wantMdns = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[wifi] disconnected");
      break;
    default:
      break;
  }
}

void net_applyTime() {
  // esp_sntp_* is the current API on ESP32 core 3.x / IDF5; the old sntp_*
  // names are deprecated.
  esp_sntp_set_time_sync_notification_cb(onTimeSync);
  configTzTime(g_cfg.tz.c_str(), g_cfg.ntp.c_str(), "pool.ntp.org", "time.cloudflare.com");
}

void net_begin() {
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_cfg.hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  net_applyTime();
  net_applyWifi();
}

void net_applyWifi() {
  if (g_cfg.ssid.length() == 0) {
    Serial.println("[wifi] no credentials stored - use: wifi <SSID> <PASS>");
    return;
  }
  Serial.printf("[wifi] connecting to \"%s\"...\n", g_cfg.ssid.c_str());
  WiFi.disconnect();
  WiFi.setHostname(g_cfg.hostname.c_str());
  WiFi.begin(g_cfg.ssid.c_str(), g_cfg.pass.c_str());
  s_lastRetry = millis();
}

void net_loop() {
  if (s_wantMdns && !s_mdnsUp && WiFi.status() == WL_CONNECTED) {
    s_wantMdns = false;
    if (MDNS.begin(g_cfg.hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      s_mdnsUp = true;
      Serial.printf("[mdns] http://%s.local/\n", g_cfg.hostname.c_str());
    } else {
      Serial.println("[mdns] start failed");
    }
  }
  if (g_cfg.ssid.length() == 0) return;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - s_lastRetry < RETRY_MS) return;
  s_lastRetry = millis();
  WiFi.reconnect();
}

bool net_isConnected() { return WiFi.status() == WL_CONNECTED; }

bool net_ntpSynced() {
  // The callback fires on every successful sync; time() is the fallback for a
  // clock that was already set before this boot's callback registration.
  return s_ntpSynced || time(nullptr) > 1600000000;
}

String net_ip() { return WiFi.localIP().toString(); }
