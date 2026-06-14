#include "WebControlAdapter.h"

#include "PrintTemplate.h"
#include "PrinterConfig.h"
#include "WebAssets.h"

extern uint32_t focusCount;

namespace timeprint {

WebControlAdapter::WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi,
                                     TimerCorePrinterBridge* bridge)
    : core_(core),
      wifi_(wifi),
      bridge_(bridge),
      commandAdapter_(core, wifi, this)
#if defined(ARDUINO_ARCH_ESP32)
      ,
      server_(80)
#endif
{
}

void WebControlAdapter::begin() {
#if defined(ARDUINO_ARCH_ESP32)
  registerRoutes();
  server_.begin();
  Serial.println("[Web] Web 服务已启动");
#endif
}

void WebControlAdapter::loop() {
#if defined(ARDUINO_ARCH_ESP32)
  server_.handleClient();
  if (wifi_) wifi_->loop();
#endif
}

bool WebControlAdapter::handleCommand(JsonDocument& doc) {
  if (!commandAdapter_.handle(doc)) return false;
  return true;
}

String WebControlAdapter::statusJson() const {
  JsonDocument doc;
  doc["state"] = core_ ? stateToJsonString(core_->state()) : "unknown";
  doc["planned_s"] = core_ ? core_->plannedSeconds() : 0;
  doc["remaining_s"] = core_ ? core_->remainingSeconds() : 0;
  doc["elapsed_s"] = core_ ? core_->elapsedSeconds() : 0;
  doc["wifi_mode"] = wifi_ && wifi_->isApMode() ? "ap" : "sta";
  doc["ip"] = wifi_ ? wifi_->ipString() : "";
  doc["focus_count"] = focusCount;
#if defined(ARDUINO_ARCH_ESP32)
  struct tm t;
  if (getLocalTime(&t)) {
    char timeBuf[20];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    doc["current_time"] = timeBuf;
  }
#endif
  String out;
  serializeJson(doc, out);
  return out;
}

void WebControlAdapter::onStateChanged(State, State) {}
void WebControlAdapter::onTick(const TickInfo&) {}
void WebControlAdapter::onFeedback(Feedback) {}

void WebControlAdapter::setTemplateMessage(const char* message) {
  if (bridge_) bridge_->setTemplateMessage(message);
}

void WebControlAdapter::sendJson(int code, const String& body) {
#if defined(ARDUINO_ARCH_ESP32)
  server_.send(code, "application/json", body);
#endif
}

void WebControlAdapter::registerRoutes() {
#if defined(ARDUINO_ARCH_ESP32)
  server_.on("/", HTTP_GET, [this]() {
    server_.send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });
  server_.on("/setup", HTTP_GET, [this]() {
    server_.send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });
  server_.on("/api/status", HTTP_GET, [this]() {
    sendJson(200, statusJson());
  });
  server_.on("/api/cmd", HTTP_POST, [this]() {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server_.arg("plain"));
    if (err || !handleCommand(doc)) {
      sendJson(400, "{\"error\":\"无效命令\"}");
      return;
    }
    sendJson(200, statusJson());
  });

  server_.on("/api/wifi", HTTP_GET, [this]() {
    JsonDocument doc;
    doc["max"] = TimePrintWiFiManager::kMaxCredentials;
    doc["default_used"] = wifi_ && wifi_->hasDefaultCredential();
    JsonArray items = doc["items"].to<JsonArray>();
    if (wifi_) {
      int n = wifi_->credentialCount();
      for (int i = 0; i < n; ++i) {
        JsonObject o = items.add<JsonObject>();
        o["index"] = i;
        o["ssid"] = wifi_->credentialSsid(i);
      }
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
  });
  server_.on("/api/wifi", HTTP_POST, [this]() {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server_.arg("plain"));
    if (err) { sendJson(400, "{\"error\":\"JSON 解析失败\"}"); return; }
    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["pass"] | "";
    if (strlen(ssid) == 0) { sendJson(400, "{\"error\":\"ssid 不能为空\"}"); return; }
    if (!wifi_->saveCredentials(String(ssid), String(pass))) {
      sendJson(500, "{\"error\":\"保存失败\"}");
      return;
    }
    sendJson(200, "{\"ok\":true,\"restart\":true}");
  });

  server_.on("/api/wifi/scan", HTTP_GET, [this]() {
    if (!wifi_) { sendJson(200, "{\"scanning\":true,\"aps\":[]}"); return; }
    String body = wifi_->getScanJson();
    server_.send(200, "application/json", body);
  });

  server_.on("/api/printer", HTTP_GET, [this]() {
    JsonDocument doc;
    doc["rx_pin"] = PRINTER_RX_PIN;
    doc["tx_pin"] = PRINTER_TX_PIN;
    doc["baud"] = 115200;
    doc["active"] = activePrinter_ ? activePrinter_->name() : "none";
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
  });
  server_.on("/api/printer/test", HTTP_POST, [this]() {
    if (!activePrinter_) { sendJson(500, "{\"error\":\"没有可用打印机\"}"); return; }
    activePrinter_->testPage();
    sendJson(200, "{\"ok\":true}");
  });

  server_.on("/api/templates", HTTP_GET, [this]() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < kTemplateCount; ++i) {
      JsonObject o = arr.add<JsonObject>();
      o["id"] = kTemplates[i].id;
      o["name"] = kTemplates[i].name;
      o["message"] = kTemplates[i].message;
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
  });

  server_.on("/api/messages", HTTP_GET, [this]() {
    JsonDocument doc;
    JsonArray defaults = doc["defaults"].to<JsonArray>();
    for (int i = 0; i < kTemplateCount; ++i) {
      JsonObject o = defaults.add<JsonObject>();
      o["id"] = kTemplates[i].id;
      o["name"] = kTemplates[i].name;
      o["message"] = kTemplates[i].message;
    }
    JsonArray custom = doc["custom"].to<JsonArray>();
#if defined(ARDUINO_ARCH_ESP32)
    Preferences msgPrefs;
    msgPrefs.begin("timeprint", true);
    int customCount = msgPrefs.getInt("msg_count", 0);
    for (int i = 0; i < customCount; ++i) {
      String key = "msg_" + String(i);
      String msg = msgPrefs.getString(key.c_str(), "");
      if (msg.length() > 0) {
        JsonObject o = custom.add<JsonObject>();
        o["message"] = msg;
      }
    }
    msgPrefs.end();
#endif
    doc["selected"] = "default";
    doc["mode"] = "fixed";
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
  });
  server_.on("/api/messages", HTTP_POST, [this]() {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server_.arg("plain"));
    if (err) { sendJson(400, "{\"error\":\"JSON 解析失败\"}"); return; }
    const char* message = doc["message"] | "";
    if (strlen(message) == 0) { sendJson(400, "{\"error\":\"消息不能为空\"}"); return; }
#if defined(ARDUINO_ARCH_ESP32)
    Preferences msgPrefs;
    msgPrefs.begin("timeprint", false);
    int count = msgPrefs.getInt("msg_count", 0);
    if (count >= 10) { msgPrefs.end(); sendJson(400, "{\"error\":\"已满\"}"); return; }
    String key = "msg_" + String(count);
    msgPrefs.putString(key.c_str(), message);
    msgPrefs.putInt("msg_count", count + 1);
    msgPrefs.end();
#endif
    sendJson(200, "{\"ok\":true}");
  });

  server_.onNotFound([this]() {
    if (server_.method() == HTTP_GET) {
      server_.send_P(200, "text/html; charset=utf-8", kIndexHtml);
      return;
    }
    server_.send(404, "text/plain", "未找到");
  });
#endif
}

}  // namespace timeprint
