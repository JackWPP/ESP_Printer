#pragma once

#include <ArduinoJson.h>

#include "TimerCore.h"

namespace timeprint {

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
using String = std::string;
#endif

class IDeviceConfigStore {
 public:
  virtual ~IDeviceConfigStore() {}
  virtual bool saveWiFiCredentials(const String& ssid, const String& pass) = 0;
};

class ITemplateStore {
 public:
  virtual ~ITemplateStore() {}
  virtual void setTemplateMessage(const char* message) = 0;
};

class CommandAdapter {
 public:
  CommandAdapter(TimerCore* core, IDeviceConfigStore* configStore = nullptr,
                 ITemplateStore* templateStore = nullptr);

  bool handle(JsonDocument& doc);

 private:
  TimerCore* core_;
  IDeviceConfigStore* configStore_;
  ITemplateStore* templateStore_;
};

}  // namespace timeprint
