#include "config_store.h"
#include <Preferences.h>

AppConfig g_cfg;

static Preferences prefs;
static const char* NS = "cyd";

void config_begin() {
  prefs.begin(NS, true);
  g_cfg.ssid         = prefs.getString("ssid", g_cfg.ssid);
  g_cfg.pass         = prefs.getString("pass", g_cfg.pass);
  g_cfg.hostname     = prefs.getString("host", g_cfg.hostname);
  g_cfg.tz           = prefs.getString("tz",   g_cfg.tz);
  g_cfg.ntp          = prefs.getString("ntp",  g_cfg.ntp);
  g_cfg.rotation     = prefs.getUChar("rot",   g_cfg.rotation);
  g_cfg.invert       = prefs.getBool("inv",    g_cfg.invert);
  g_cfg.clockVisible = prefs.getBool("clk",    g_cfg.clockVisible);
  g_cfg.slideSec     = prefs.getUShort("slide", g_cfg.slideSec);
  g_cfg.calib.xmin   = prefs.getShort("cxmin", g_cfg.calib.xmin);
  g_cfg.calib.xmax   = prefs.getShort("cxmax", g_cfg.calib.xmax);
  g_cfg.calib.ymin   = prefs.getShort("cymin", g_cfg.calib.ymin);
  g_cfg.calib.ymax   = prefs.getShort("cymax", g_cfg.calib.ymax);
  g_cfg.calib.swapxy = prefs.getBool("cswap",  g_cfg.calib.swapxy);
  prefs.end();

  if (g_cfg.rotation != 1 && g_cfg.rotation != 3) g_cfg.rotation = 1;
  if (g_cfg.slideSec < 2) g_cfg.slideSec = 2;
}

void config_save() {
  prefs.begin(NS, false);
  prefs.putString("ssid", g_cfg.ssid);
  prefs.putString("pass", g_cfg.pass);
  prefs.putString("host", g_cfg.hostname);
  prefs.putString("tz",   g_cfg.tz);
  prefs.putString("ntp",  g_cfg.ntp);
  prefs.putUChar("rot",   g_cfg.rotation);
  prefs.putBool("inv",    g_cfg.invert);
  prefs.putBool("clk",    g_cfg.clockVisible);
  prefs.putUShort("slide", g_cfg.slideSec);
  prefs.putShort("cxmin", g_cfg.calib.xmin);
  prefs.putShort("cxmax", g_cfg.calib.xmax);
  prefs.putShort("cymin", g_cfg.calib.ymin);
  prefs.putShort("cymax", g_cfg.calib.ymax);
  prefs.putBool("cswap",  g_cfg.calib.swapxy);
  prefs.end();
}

void config_erase() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}
