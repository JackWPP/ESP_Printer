#include "WebControlAdapter.h"

#include "WebAssets.h"

namespace timeprint {

WebControlAdapter::WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi)
    : core_(core),
      wifi_(wifi)
      ,
      commandAdapter_(core, wifi)
#if defined(ARDUINO_ARCH_ESP32)
      ,
      server_(80),
      ws_("/ws")
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
  ws_.cleanupClients();
  if (millis() - lastBroadcastMs_ >= 1000) {
    lastBroadcastMs_ = millis();
    broadcastStatus();
  }
#endif
}

bool WebControlAdapter::handleCommand(JsonDocument& doc) {
  if (!commandAdapter_.handle(doc)) return false;
  broadcastStatus();
  return true;
}

String WebControlAdapter::statusJson() const {
  JsonDocument doc;
  doc["state"] = core_ ? stateToJsonString(core_->state()) : "unknown";
  doc["planned_s"] = core_ ? core_->plannedSeconds() : 0;
  doc["remaining_s"] = core_ ? core_->remainingSeconds() : 0;
  doc["elapsed_s"] = core_ ? core_->elapsedSeconds() : 0;
  doc["overtime_s"] = core_ ? core_->overrunSeconds() : 0;
  doc["wifi_mode"] = wifi_ && wifi_->isApMode() ? "ap" : "sta";
  doc["ip"] = wifi_ ? wifi_->ipString() : "";
  String out;
  serializeJson(doc, out);
  return out;
}

void WebControlAdapter::broadcastStatus() {
#if defined(ARDUINO_ARCH_ESP32)
  ws_.textAll(statusJson());
#endif
}

void WebControlAdapter::onStateChanged(State, State) {
  broadcastStatus();
}

void WebControlAdapter::onTick(const TickInfo&) {
  broadcastStatus();
}

void WebControlAdapter::onFeedback(Feedback) {
  broadcastStatus();
}

void WebControlAdapter::registerRoutes() {
#if defined(ARDUINO_ARCH_ESP32)
  ws_.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void*,
                    uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text(statusJson());
      return;
    }
    if (type != WS_EVT_DATA) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err || !handleCommand(doc)) {
      client->text("{\"error\":\"无效命令\"}");
    }
  });
  server_.addHandler(&ws_);

  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });
  server_.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", statusJson());
  });
  server_.on(
      "/api/cmd", HTTP_POST, [](AsyncWebServerRequest*) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err || !handleCommand(doc)) {
          request->send(400, "application/json", "{\"error\":\"无效命令\"}");
          return;
        }
        request->send(200, "application/json", statusJson());
      });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
      request->send_P(200, "text/html; charset=utf-8", kIndexHtml);
      return;
    }
    request->send(404, "text/plain", "未找到");
  });
#endif
}

}  // namespace timeprint
