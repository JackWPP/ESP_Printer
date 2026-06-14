#include "WebControlAdapter.h"

#include "PrintTemplate.h"
#include "PrinterConfig.h"
#include "WebAssets.h"

namespace timeprint {

WebControlAdapter::WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi,
                                     TimerCorePrinterBridge* bridge)
    : core_(core),
      wifi_(wifi),
      bridge_(bridge),
      commandAdapter_(core, wifi, this)
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

void WebControlAdapter::setTemplateMessage(const char* message) {
  if (bridge_) bridge_->setTemplateMessage(message);
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

  // WiFi 列表
  server_.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleWifiGet(request);
  });
  server_.on(
      "/api/wifi", HTTP_POST, [](AsyncWebServerRequest*) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
        handleWifiAdd(request, data, len);
      });

  // 打印机
  server_.on("/api/printer", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handlePrinterGet(request);
  });
  server_.on(
      "/api/printer/test", HTTP_POST, [](AsyncWebServerRequest*) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
        handlePrinterTest(request, data, len);
      });

  // 模板列表
  server_.on("/api/templates", HTTP_GET, [this](AsyncWebServerRequest* request) {
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
    sendJson(request, 200, out);
  });

  // 模板预览
  server_.on("/api/preview", HTTP_GET, [this](AsyncWebServerRequest* request) {
    const char* id = request->getParam("template")
                         ? request->getParam("template")->value().c_str()
                         : "default";
    const PrintTemplate* tpl = findTemplate(id);
    JsonDocument doc;
    doc["id"] = tpl->id;
    doc["name"] = tpl->name;
    doc["message"] = tpl->message;
    doc["planned_s"] = core_ ? core_->plannedSeconds() : 0;
    doc["remaining_s"] = core_ ? core_->remainingSeconds() : 0;
    doc["elapsed_s"] = core_ ? core_->elapsedSeconds() : 0;
    doc["overtime_s"] = core_ ? core_->overrunSeconds() : 0;
    String out;
    serializeJson(doc, out);
    sendJson(request, 200, out);
  });

  // 捕获 /api/wifi/N 的 DELETE：AsyncWebServer 用通配，需要扫 path
  server_.on(
      "/api/wifi/0", HTTP_DELETE, [this](AsyncWebServerRequest* request) { handleWifiDelete(request, 0); });
  server_.on(
      "/api/wifi/1", HTTP_DELETE, [this](AsyncWebServerRequest* request) { handleWifiDelete(request, 1); });
  server_.on(
      "/api/wifi/2", HTTP_DELETE, [this](AsyncWebServerRequest* request) { handleWifiDelete(request, 2); });
  server_.on(
      "/api/wifi/3", HTTP_DELETE, [this](AsyncWebServerRequest* request) { handleWifiDelete(request, 3); });
  server_.on(
      "/api/wifi/4", HTTP_DELETE, [this](AsyncWebServerRequest* request) { handleWifiDelete(request, 4); });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
      request->send_P(200, "text/html; charset=utf-8", kIndexHtml);
      return;
    }
    request->send(404, "text/plain", "未找到");
  });
#endif
}

void WebControlAdapter::sendJson(AsyncWebServerRequest* request, int code, const String& body) {
#if defined(ARDUINO_ARCH_ESP32)
  AsyncWebServerResponse* resp = request->beginResponse(code, "application/json", body);
  resp->addHeader("Cache-Control", "no-store");
  request->send(resp);
#endif
}

void WebControlAdapter::handleWifiGet(AsyncWebServerRequest* request) {
#if defined(ARDUINO_ARCH_ESP32)
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
  sendJson(request, 200, out);
#endif
}

void WebControlAdapter::handleWifiAdd(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
#if defined(ARDUINO_ARCH_ESP32)
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    sendJson(request, 400, "{\"error\":\"JSON 解析失败\"}");
    return;
  }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (strlen(ssid) == 0) {
    sendJson(request, 400, "{\"error\":\"ssid 不能为空\"}");
    return;
  }
  if (!wifi_->saveCredentials(String(ssid), String(pass))) {
    sendJson(request, 500, "{\"error\":\"保存失败（已满？NVS 错误？）\"}");
    return;
  }
  // saveCredentials 会触发 ESP.restart()，下面这行通常来不及返回
  sendJson(request, 200, "{\"ok\":true,\"restart\":true}");
#endif
}

void WebControlAdapter::handleWifiDelete(AsyncWebServerRequest* request, int index) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!wifi_ || index < 0 || index >= wifi_->credentialCount()) {
    sendJson(request, 400, "{\"error\":\"索引无效\"}");
    return;
  }
  String ssid = wifi_->credentialSsid(index);
  wifi_->removeCredential(index);
  wifi_->commitAndRestart();
  // 通常 ESP.restart() 已经触发，构造一个应答但用户收不到
  String body = String("{\"ok\":true,\"deleted\":\"") + ssid + "\"}";
  sendJson(request, 200, body);
#endif
}

void WebControlAdapter::handlePrinterGet(AsyncWebServerRequest* request) {
#if defined(ARDUINO_ARCH_ESP32)
  JsonDocument doc;
  doc["rx_pin"] = PRINTER_RX_PIN;
  doc["tx_pin"] = PRINTER_TX_PIN;
  doc["baud"] = 115200;
  doc["ready_timeout_ms"] = PRINTER_READY_TIMEOUT_MS;
  // 通过 HPD482Printer::ready() 判活；activePrinter_ 是 Printer 基类指针
  // 这里只暴露它是 stub 还是 hpd482，因为 Printer 接口没 ready()。
  doc["active"] = activePrinter_ ? activePrinter_->name() : "none";
  String out;
  serializeJson(doc, out);
  sendJson(request, 200, out);
#endif
}

void WebControlAdapter::handlePrinterTest(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!activePrinter_) {
    sendJson(request, 500, "{\"error\":\"没有可用打印机\"}");
    return;
  }
  // testPage() 内部有串口 I/O，可能阻塞数百毫秒；AsyncWebServer 的
  // body handler 在 async 线程里调用，阻塞太久会导致客户端看到连接断开。
  // HPD482Printer::testPage() 内部已做就绪检查，未就绪时直接跳过，
  // 阻塞时间 = sendCommand("AT+CN=32") 的 3s 超时 + 几条 printText 的等待，
  // 实际上几乎不可能超过 ~1s（打印机就绪时），所以这里直接调用。
  activePrinter_->testPage();
  sendJson(request, 200, "{\"ok\":true}");
#endif
}

}  // namespace timeprint
