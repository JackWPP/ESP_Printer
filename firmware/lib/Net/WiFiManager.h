#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>
#endif

namespace timeprint {

class TimePrintWiFiManager {
 public:
  void begin();
  void loop();
  void resetWiFi();
  bool isApMode() const { return apMode_; }
  const String& apSsid() const { return apSsid_; }
  String ipString() const;
  bool saveCredentials(const String& ssid, const String& pass);

 private:
#if defined(ARDUINO_ARCH_ESP32)
  Preferences prefs_;
  DNSServer dns_;
#endif
  bool apMode_ = true;
  String apSsid_;

  void startAp();
  bool startSta(const String& ssid, const String& pass);
  String makeApSsid() const;
};

}  // namespace timeprint
