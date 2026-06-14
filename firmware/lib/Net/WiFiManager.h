#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>
#endif

#include "CommandAdapter.h"

namespace timeprint {

class TimePrintWiFiManager : public IDeviceConfigStore {
 public:
  static const int kMaxCredentials = 5;
  static const int kMaxScanResults = 20;

  void begin();
  void loop();
  void resetWiFi();
  bool isApMode() const { return apMode_; }
  bool isStaConnected() const;
  const String& apSsid() const { return apSsid_; }
  String ipString() const;

  String getScanJson();

  int  credentialCount() const { return credentialCount_; }
  String credentialSsid(int index) const;
  bool hasDefaultCredential() const { return usingDefault_; }

  void loadFromNvs();
  bool appendCredential(const String& ssid, const String& pass);
  bool removeCredential(int index);
  void clearAll();
  void commitAndRestart();

  bool saveCredentials(const String& ssid, const String& pass);
  bool saveWiFiCredentials(const String& ssid, const String& pass) override {
    return saveCredentials(ssid, pass);
  }

 private:
#if defined(ARDUINO_ARCH_ESP32)
  Preferences prefs_;
  DNSServer dns_;
#endif
  bool apMode_ = true;
  String apSsid_;
  String credentials_[kMaxCredentials];
  String passwords_[kMaxCredentials];
  int credentialCount_ = 0;
  bool usingDefault_ = false;

  enum ScanState { SCAN_IDLE, SCAN_RUNNING };
  ScanState scanState_ = SCAN_IDLE;
  uint32_t lastScanMs_ = 0;
  static const uint32_t kScanIntervalMs = 10000;

  String scanJson_ = "{\"scanning\":true,\"aps\":[]}";
  SemaphoreHandle_t jsonMutex_ = nullptr;

  void scanTick();
  void buildScanJson(int n);
  static String jsonEscape(const String& s);

  void startAp();
  bool tryStaSequential();
  bool startSta(const String& ssid, const String& pass);
  String readPassForIndex(int index);
  void writePassForIndex(int index, const String& pass);
  String makeApSsid() const;
};

}  // namespace timeprint
