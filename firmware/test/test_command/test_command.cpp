#include <unity.h>

#include "CommandAdapter.h"
#include "TimerCore.h"

using namespace timeprint;

struct FakeConfigStore : public IDeviceConfigStore {
  int saves = 0;
  String ssid;
  String pass;
  bool result = true;

  bool saveWiFiCredentials(const String& nextSsid, const String& nextPass) override {
    saves++;
    ssid = nextSsid;
    pass = nextPass;
    return result;
  }
};

static TimerCore core;
static FakeConfigStore config;
static CommandAdapter* adapter = nullptr;

void setUp(void) {
  core = TimerCore();
  config = FakeConfigStore();
  adapter = new CommandAdapter(&core, &config);
}

void tearDown(void) {
  delete adapter;
  adapter = nullptr;
}

static bool handleJson(const char* json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  TEST_ASSERT_FALSE(err);
  return adapter->handle(doc);
}

void test_set_and_start_commands_drive_timer_core(void) {
  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"set\",\"minutes\":25}"));
  TEST_ASSERT_EQUAL_UINT32(1500, core.plannedSeconds());

  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"start\"}"));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Running), static_cast<int>(core.state()));
}

void test_pause_resume_stop_reset_commands(void) {
  handleJson("{\"cmd\":\"set\",\"minutes\":1}");
  handleJson("{\"cmd\":\"start\"}");
  core.tick();

  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"pause\"}"));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Paused), static_cast<int>(core.state()));

  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"resume\"}"));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Running), static_cast<int>(core.state()));

  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"stop\"}"));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Idle), static_cast<int>(core.state()));

  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"reset\"}"));
  TEST_ASSERT_EQUAL_UINT32(0, core.plannedSeconds());
}

void test_config_command_saves_wifi_credentials(void) {
  TEST_ASSERT_TRUE(handleJson("{\"cmd\":\"config\",\"data\":{\"ssid\":\"lab\",\"pass\":\"secret\"}}"));
  TEST_ASSERT_EQUAL_INT(1, config.saves);
  TEST_ASSERT_EQUAL_STRING("lab", config.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secret", config.pass.c_str());
}

void test_rejects_unknown_or_malformed_commands(void) {
  TEST_ASSERT_FALSE(handleJson("{\"cmd\":\"bogus\"}"));
  TEST_ASSERT_FALSE(handleJson("{\"minutes\":25}"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_set_and_start_commands_drive_timer_core);
  RUN_TEST(test_pause_resume_stop_reset_commands);
  RUN_TEST(test_config_command_saves_wifi_credentials);
  RUN_TEST(test_rejects_unknown_or_malformed_commands);
  return UNITY_END();
}
