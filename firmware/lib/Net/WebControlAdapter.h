#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "CommandAdapter.h"
#include "TimerCore.h"
#include "WiFiManager.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <ESPAsyncWebServer.h>
#endif

namespace timeprint {

class WebControlAdapter : public ITimerCoreListener {
 public:
  WebControlAdapter(TimerCore* core, TimePrintWiFiManager* wifi);

  void begin();
  void loop();
  bool handleCommand(JsonDocument& doc);
  String statusJson() const;
  void broadcastStatus();

  void onStateChanged(State from, State to) override;
  void onTick(const TickInfo& info) override;
  void onFeedback(Feedback fb) override;

 private:
  TimerCore* core_;
  TimePrintWiFiManager* wifi_;
  CommandAdapter commandAdapter_;
  uint32_t lastBroadcastMs_ = 0;

#if defined(ARDUINO_ARCH_ESP32)
  AsyncWebServer server_;
  AsyncWebSocket ws_;
#endif

  void registerRoutes();
};

}  // namespace timeprint
