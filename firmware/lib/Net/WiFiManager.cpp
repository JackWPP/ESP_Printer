#include "WiFiManager.h"

namespace timeprint {

void TimePrintWiFiManager::begin() {
#if defined(ARDUINO_ARCH_ESP32)
  prefs_.begin("timeprint", false);
  String ssid = prefs_.isKey("ssid") ? prefs_.getString("ssid", "") : "";
  String pass = prefs_.isKey("pass") ? prefs_.getString("pass", "") : "";
  if (ssid.length() > 0 && startSta(ssid, pass)) return;
  startAp();
#endif
}

void TimePrintWiFiManager::loop() {
#if defined(ARDUINO_ARCH_ESP32)
  if (apMode_) dns_.processNextRequest();
#endif
}

void TimePrintWiFiManager::resetWiFi() {
#if defined(ARDUINO_ARCH_ESP32)
  prefs_.begin("timeprint", false);
  prefs_.remove("ssid");
  prefs_.remove("pass");
  delay(100);
  ESP.restart();
#endif
}

String TimePrintWiFiManager::ipString() const {
#if defined(ARDUINO_ARCH_ESP32)
  return apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
#else
  return String();
#endif
}

bool TimePrintWiFiManager::saveCredentials(const String& ssid, const String& pass) {
#if defined(ARDUINO_ARCH_ESP32)
  if (ssid.length() == 0) return false;
  prefs_.begin("timeprint", false);
  prefs_.putString("ssid", ssid);
  prefs_.putString("pass", pass);
  delay(100);
  ESP.restart();
#endif
  return true;
}

void TimePrintWiFiManager::startAp() {
#if defined(ARDUINO_ARCH_ESP32)
  apMode_ = true;
  apSsid_ = makeApSsid();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid_.c_str());
  dns_.start(53, "*", WiFi.softAPIP());
  Serial.printf("[WiFi] AP %s 地址 %s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
#endif
}

bool TimePrintWiFiManager::startSta(const String& ssid, const String& pass) {
#if defined(ARDUINO_ARCH_ESP32)
  apMode_ = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WiFi] 正在连接 %s", ssid.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] STA 已连接，地址 %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[WiFi] STA 连接失败，回退到 AP 模式");
#endif
  return false;
}

String TimePrintWiFiManager::makeApSsid() const {
#if defined(ARDUINO_ARCH_ESP32)
  uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned>(mac & 0xFFFF));
  return String("TimePrint-") + suffix;
#else
  return String("TimePrint");
#endif
}

}  // namespace timeprint
