#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "CommandAdapter.h"
#include "PrintTemplate.h"
#include "Printer.h"
#include "TimerCore.h"
#include "TimerCorePrinterBridge.h"
#include "WiFiManager.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <ESPAsyncWebServer.h>
#endif

namespace timeprint {

class WebControlAdapter : public ITimerCoreListener, public ITemplateStore {
 public:
  WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi, TimerCorePrinterBridge* bridge);

  void begin();
  void loop();
  void setActivePrinter(Printer* printer) { activePrinter_ = printer; }
  void setTemplateMessage(const char* message) override;
  bool handleCommand(JsonDocument& doc);
  String statusJson() const;
  void broadcastStatus();

  void onStateChanged(State from, State to) override;
  void onTick(const TickInfo& info) override;
  void onFeedback(Feedback fb) override;

 private:
  TimerCore* core_;
  TimePrintWiFiManager* wifi_;
  TimerCorePrinterBridge* bridge_;
  Printer* activePrinter_ = nullptr;
  CommandAdapter commandAdapter_;
  uint32_t lastBroadcastMs_ = 0;

#if defined(ARDUINO_ARCH_ESP32)
  AsyncWebServer server_;
  AsyncWebSocket ws_;
#endif

  void registerRoutes();
  void handleWifiGet(AsyncWebServerRequest* request);
  void handleWifiAdd(AsyncWebServerRequest* request, uint8_t* data, size_t len);
  void handleWifiDelete(AsyncWebServerRequest* request, int index);
  void handlePrinterGet(AsyncWebServerRequest* request);
  void handlePrinterTest(AsyncWebServerRequest* request, uint8_t* data, size_t len);
  void sendJson(AsyncWebServerRequest* request, int code, const String& body);
};

}  // namespace timeprint
