// net.h - WiFi, NTP and mDNS
#pragma once
#include <Arduino.h>

void   net_begin();
void   net_loop();
void   net_applyWifi();      // (re)connect using the stored credentials
bool   net_isConnected();
bool   net_ntpSynced();
String net_ip();
void   net_applyTime();      // re-apply TZ / NTP server from the config
