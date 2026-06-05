#include "CommandAdapter.h"

#include <string.h>

namespace timeprint {

CommandAdapter::CommandAdapter(TimerCore* core, IDeviceConfigStore* configStore)
    : core_(core), configStore_(configStore) {}

bool CommandAdapter::handle(JsonDocument& doc) {
  if (!core_ || !doc["cmd"].is<const char*>()) return false;

  const char* command = doc["cmd"];
  if (strcmp(command, "set") == 0) {
    core_->setTime(doc["minutes"] | 0);
  } else if (strcmp(command, "start") == 0) {
    core_->start();
  } else if (strcmp(command, "pause") == 0) {
    core_->pause();
  } else if (strcmp(command, "resume") == 0) {
    core_->resume();
  } else if (strcmp(command, "stop") == 0) {
    core_->stop();
  } else if (strcmp(command, "reset") == 0) {
    core_->reset();
  } else if (strcmp(command, "config") == 0) {
    if (!configStore_) return false;
    JsonVariant data = doc["data"];
    const char* ssid = data["ssid"] | "";
    const char* pass = data["pass"] | "";
    return configStore_->saveWiFiCredentials(String(ssid), String(pass));
  } else {
    return false;
  }

  return true;
}

}  // namespace timeprint
