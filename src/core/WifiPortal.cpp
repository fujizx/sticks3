#include "WifiPortal.h"

#include <WiFi.h>
#include <WiFiManager.h>

#include "AppLog.h"

namespace {
constexpr uint32_t kReconnectIntervalMs = 30000;
constexpr uint16_t kConfigPortalTimeoutSec = 180;
}

void WifiPortal::begin(const String &deviceName) {
  active_ = true;
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(true);

  WiFiManager wm;
  wm.setConfigPortalTimeout(kConfigPortalTimeoutSec);
  wm.setConnectTimeout(15);

  const String apName = deviceName + "-Setup";
  LOGI("wifi", "connecting, portal ap=%s", apName.c_str());

  if (wm.autoConnect(apName.c_str())) {
    rememberConnection();
    LOGI("wifi", "connected ssid=%s ip=%s", lastSsid_.c_str(), lastIp_.c_str());
    return;
  }

  LOGW("wifi", "not connected, continue offline");
}

void WifiPortal::loop(bool keepTrying) {
  if (!active_) return;
  if (WiFi.status() == WL_CONNECTED) {
    rememberConnection();
    return;
  }
  if (!keepTrying) return;

  const uint32_t now = millis();
  if (now - lastReconnectMs_ < kReconnectIntervalMs) return;
  lastReconnectMs_ = now;

  LOGW("wifi", "reconnect");
  WiFi.reconnect();
}

void WifiPortal::shutdown() {
  if (!active_) return;
  if (WiFi.status() == WL_CONNECTED) rememberConnection();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  active_ = false;
  LOGI("wifi", "radio off");
}

bool WifiPortal::connected() const {
  return active_ && WiFi.status() == WL_CONNECTED;
}

bool WifiPortal::active() const {
  return active_;
}

bool WifiPortal::everConnected() const {
  return everConnected_;
}

String WifiPortal::ip() const {
  return connected() ? WiFi.localIP().toString() : lastIp_;
}

String WifiPortal::ssid() const {
  return connected() ? WiFi.SSID() : lastSsid_;
}

int8_t WifiPortal::rssi() const {
  return connected() ? WiFi.RSSI() : lastRssi_;
}

String WifiPortal::statusText() const {
  if (connected()) return "Connected";
  if (!active_ && everConnected_) return "Off after NTP";
  if (!active_) return "Off";
  return "Connecting";
}

void WifiPortal::resetWifiSettings() {
  WiFiManager wm;
  wm.resetSettings();
  WiFi.disconnect(true, true);
  active_ = false;
  everConnected_ = false;
  lastIp_ = "-";
  lastSsid_ = "-";
  lastRssi_ = 0;
  LOGW("wifi", "saved wifi settings cleared");
}

void WifiPortal::rememberConnection() {
  everConnected_ = true;
  lastIp_ = WiFi.localIP().toString();
  lastSsid_ = WiFi.SSID();
  lastRssi_ = WiFi.RSSI();
}
