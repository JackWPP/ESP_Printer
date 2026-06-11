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

  void begin();
  void loop();
  void resetWiFi();
  bool isApMode() const { return apMode_; }
  const String& apSsid() const { return apSsid_; }
  String ipString() const;

  // 多凭据 API
  int  credentialCount() const { return credentialCount_; }
  String credentialSsid(int index) const;        // 密码不回显
  bool hasDefaultCredential() const { return usingDefault_; }

  // 写操作（不重启，留给上层在事务结束时统一 commit）
  void loadFromNvs();
  bool appendCredential(const String& ssid, const String& pass);
  bool removeCredential(int index);
  void clearAll();
  void commitAndRestart();

  // 兼容旧的 IDeviceConfigStore（等价 appendCredential + commit）
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
  // 内存中缓存所有凭据的 SSID 和密码，避免 commitAndRestart 中嵌套 Preferences
  String credentials_[kMaxCredentials];
  String passwords_[kMaxCredentials];
  int credentialCount_ = 0;
  bool usingDefault_ = false;

  void startAp();
  bool tryStaSequential();
  bool startSta(const String& ssid, const String& pass);
  String readPassForIndex(int index);
  void writePassForIndex(int index, const String& pass);
  String makeApSsid() const;
};

}  // namespace timeprint
