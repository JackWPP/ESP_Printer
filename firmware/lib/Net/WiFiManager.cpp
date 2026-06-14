#include "WiFiManager.h"

namespace timeprint {

namespace {
const char kDefaultSsid[] = "xgx";
const char kDefaultPass[] = "1234567890";
constexpr uint32_t kStaAttemptTimeoutMs = 12000;
}  // namespace

void TimePrintWiFiManager::begin() {
#if defined(ARDUINO_ARCH_ESP32)
  jsonMutex_ = xSemaphoreCreateMutex();

  loadFromNvs();

  if (credentialCount_ == 0) {
    usingDefault_ = true;
    credentials_[0] = kDefaultSsid;
    passwords_[0] = kDefaultPass;
    credentialCount_ = 1;
    Serial.printf("[WiFi] NVS 无凭据，使用默认 SSID: %s\n", kDefaultSsid);
  }

  if (tryStaSequential()) return;
  startAp();
  lastScanMs_ = millis() - kScanIntervalMs;
#endif
}

void TimePrintWiFiManager::loop() {
#if defined(ARDUINO_ARCH_ESP32)
  if (apMode_) dns_.processNextRequest();
  scanTick();
#endif
}

void TimePrintWiFiManager::resetWiFi() {
#if defined(ARDUINO_ARCH_ESP32)
  prefs_.begin("timeprint", false);
  for (int i = 0; i < kMaxCredentials; ++i) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    if (prefs_.isKey(ssidKey.c_str())) prefs_.remove(ssidKey.c_str());
    if (prefs_.isKey(passKey.c_str())) prefs_.remove(passKey.c_str());
  }
  if (prefs_.isKey("count")) prefs_.remove("count");
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

bool TimePrintWiFiManager::isStaConnected() const {
#if defined(ARDUINO_ARCH_ESP32)
  return !apMode_ && WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

String TimePrintWiFiManager::getScanJson() {
  String result;
  if (jsonMutex_ && xSemaphoreTake(jsonMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    result = scanJson_;
    xSemaphoreGive(jsonMutex_);
  } else {
    result = "{\"scanning\":true,\"aps\":[]}";
  }
  return result;
}

void TimePrintWiFiManager::scanTick() {
#if defined(ARDUINO_ARCH_ESP32)
  switch (scanState_) {
  case SCAN_IDLE:
    if (millis() - lastScanMs_ >= kScanIntervalMs) {
      int r = WiFi.scanNetworks(true, false);
      if (r == WIFI_SCAN_RUNNING) {
        scanState_ = SCAN_RUNNING;
        Serial.println("[WiFi] 后台扫描已启动");
      } else {
        lastScanMs_ = millis();
      }
    }
    break;
  case SCAN_RUNNING: {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      buildScanJson(n);
      WiFi.scanDelete();
      lastScanMs_ = millis();
      scanState_ = SCAN_IDLE;
      Serial.printf("[WiFi] 后台扫描完成，%d 个网络\n", n);
    } else if (n == WIFI_SCAN_FAILED) {
      WiFi.scanDelete();
      lastScanMs_ = millis();
      scanState_ = SCAN_IDLE;
      Serial.println("[WiFi] 后台扫描失败");
    }
    break;
  }
  }
#endif
}

void TimePrintWiFiManager::buildScanJson(int n) {
  String json = "{\"scanning\":false,\"aps\":[";
  int count = n < kMaxScanResults ? n : kMaxScanResults;
  for (int i = 0; i < count; i++) {
    if (i) json += ',';
    json += "{\"ssid\":\"";
    json += jsonEscape(WiFi.SSID(i));
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"channel\":";
    json += String(WiFi.channel(i));
    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
    json += '}';
  }
  json += "]}";

  if (jsonMutex_ && xSemaphoreTake(jsonMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    scanJson_ = json;
    xSemaphoreGive(jsonMutex_);
  }
}

String TimePrintWiFiManager::jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

String TimePrintWiFiManager::credentialSsid(int index) const {
  if (index < 0 || index >= credentialCount_) return String();
  return credentials_[index];
}

void TimePrintWiFiManager::loadFromNvs() {
  credentialCount_ = 0;
  usingDefault_ = false;
  for (int i = 0; i < kMaxCredentials; ++i) {
    credentials_[i] = String();
    passwords_[i] = String();
  }
#if defined(ARDUINO_ARCH_ESP32)
  prefs_.begin("timeprint", true);
  int count = prefs_.getInt("count", 0);
  if (count > kMaxCredentials) count = kMaxCredentials;
  for (int i = 0; i < count; ++i) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    String ssid = prefs_.getString(ssidKey.c_str(), "");
    if (ssid.length() == 0) continue;
    credentials_[credentialCount_] = ssid;
    passwords_[credentialCount_] = prefs_.getString(passKey.c_str(), "");
    ++credentialCount_;
  }
  prefs_.end();
#endif
}

bool TimePrintWiFiManager::appendCredential(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;
  if (credentialCount_ >= kMaxCredentials) return false;

  if (usingDefault_) {
    credentials_[0] = kDefaultSsid;
    passwords_[0] = kDefaultPass;
    writePassForIndex(0, kDefaultPass);
    usingDefault_ = false;
  }

  for (int i = 0; i < credentialCount_; ++i) {
    if (credentials_[i] == ssid) {
      passwords_[i] = pass;
      writePassForIndex(i, pass);
      return true;
    }
  }

  credentials_[credentialCount_] = ssid;
  passwords_[credentialCount_] = pass;
  writePassForIndex(credentialCount_, pass);
  ++credentialCount_;
  return true;
}

bool TimePrintWiFiManager::removeCredential(int index) {
  if (index < 0 || index >= credentialCount_) return false;
  for (int i = index; i < credentialCount_ - 1; ++i) {
    credentials_[i] = credentials_[i + 1];
    passwords_[i] = passwords_[i + 1];
  }
  credentials_[credentialCount_ - 1] = String();
  passwords_[credentialCount_ - 1] = String();
  --credentialCount_;
  return true;
}

void TimePrintWiFiManager::clearAll() {
  for (int i = 0; i < credentialCount_; ++i) {
    credentials_[i] = String();
  }
  credentialCount_ = 0;
  usingDefault_ = false;
}

bool TimePrintWiFiManager::saveCredentials(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;
  if (!appendCredential(ssid, pass)) return false;
  commitAndRestart();
  return true;
}

void TimePrintWiFiManager::commitAndRestart() {
#if defined(ARDUINO_ARCH_ESP32)
  prefs_.begin("timeprint", false);
  int prevCount = prefs_.getInt("count", 0);
  if (prevCount > kMaxCredentials) prevCount = kMaxCredentials;
  for (int i = 0; i < credentialCount_; ++i) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    prefs_.putString(ssidKey.c_str(), credentials_[i]);
    if (passwords_[i].length() > 0) prefs_.putString(passKey.c_str(), passwords_[i]);
  }
  for (int i = credentialCount_; i < prevCount; ++i) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    if (prefs_.isKey(ssidKey.c_str())) prefs_.remove(ssidKey.c_str());
    if (prefs_.isKey(passKey.c_str())) prefs_.remove(passKey.c_str());
  }
  prefs_.putInt("count", credentialCount_);
  prefs_.end();
  delay(100);
  ESP.restart();
#endif
}

bool TimePrintWiFiManager::tryStaSequential() {
#if defined(ARDUINO_ARCH_ESP32)
  for (int i = 0; i < credentialCount_; ++i) {
    if (startSta(credentials_[i], passwords_[i])) {
      Serial.printf("[WiFi] 已用凭据 [%d] %s 连接\n", i, credentials_[i].c_str());
      return true;
    }
  }
#endif
  return false;
}

String TimePrintWiFiManager::readPassForIndex(int index) {
#if defined(ARDUINO_ARCH_ESP32)
  if (usingDefault_ && index == 0) return String(kDefaultPass);
  String key = "pass_" + String(index);
  prefs_.begin("timeprint", true);
  String pass = prefs_.getString(key.c_str(), "");
  prefs_.end();
  return pass;
#else
  return String();
#endif
}

void TimePrintWiFiManager::writePassForIndex(int index, const String& pass) {
#if defined(ARDUINO_ARCH_ESP32)
  String key = "pass_" + String(index);
  prefs_.begin("timeprint", false);
  prefs_.putString(key.c_str(), pass);
  prefs_.end();
#endif
}

void TimePrintWiFiManager::startAp() {
#if defined(ARDUINO_ARCH_ESP32)
  apMode_ = true;
  apSsid_ = makeApSsid();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.softAP(apSsid_.c_str(), nullptr, 1, 0, 4);
  dns_.start(53, "*", WiFi.softAPIP());
  Serial.printf("[WiFi] AP %s 地址 %s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
#endif
}

bool TimePrintWiFiManager::startSta(const String& ssid, const String& pass) {
#if defined(ARDUINO_ARCH_ESP32)
  apMode_ = false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WiFi] 正在连接 %s", ssid.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kStaAttemptTimeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] STA 已连接，地址 %s\n", WiFi.localIP().toString().c_str());
    configTime(8 * 3600, 0, "pool.ntp.org", "ntp.aliyun.com");
    Serial.println("[NTP] 已启动 NTP 同步 (UTC+8)");
    return true;
  }
  Serial.printf("[WiFi] STA %s 连接失败\n", ssid.c_str());
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
