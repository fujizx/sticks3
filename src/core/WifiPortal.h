#pragma once

#include <Arduino.h>

class WifiPortal {
 public:
  void begin(const String &deviceName);
  void loop(bool keepTrying = true);
  void shutdown();
  bool connected() const;
  bool active() const;
  bool everConnected() const;
  String ip() const;
  String ssid() const;
  int8_t rssi() const;
  String statusText() const;
  void resetWifiSettings();

 private:
  void rememberConnection();

  uint32_t lastReconnectMs_ = 0;
  bool active_ = false;
  bool everConnected_ = false;
  String lastIp_ = "-";
  String lastSsid_ = "-";
  int8_t lastRssi_ = 0;
};
