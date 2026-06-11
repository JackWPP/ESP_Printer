#include <unity.h>

#include "PhysicalHook.h"

using namespace timeprint;

void setUp(void) {}
void tearDown(void) {}

void test_idle_never_fires(void) {
  EdgeHook hook(false, 100);
  TEST_ASSERT_FALSE(hook.update(true, 0));
  TEST_ASSERT_FALSE(hook.update(true, 500));
}

void test_fires_once_on_rising_edge(void) {
  EdgeHook hook(true, 100);
  TEST_ASSERT_TRUE(hook.update(true, 0));    // 触发
  TEST_ASSERT_FALSE(hook.update(true, 50));  // Alarmed，不重复
  TEST_ASSERT_FALSE(hook.update(true, 200)); // 仍在 Alarmed
}

void test_no_refire_while_alarm_active(void) {
  // 报警持续 60 秒（人没关），只触发一次
  EdgeHook hook(true, 1000);
  TEST_ASSERT_TRUE(hook.update(true, 0));
  for (uint32_t t = 100; t <= 60000; t += 100) {
    TEST_ASSERT_FALSE(hook.update(true, t));
  }
}

void test_rearm_after_silence(void) {
  // 报警 → 人关掉 → 静默 → 重新就绪
  EdgeHook hook(true, 500);
  TEST_ASSERT_TRUE(hook.update(true, 0));      // 触发，Alarmed
  TEST_ASSERT_FALSE(hook.update(true, 1000));  // 仍在报警
  TEST_ASSERT_FALSE(hook.update(false, 2000)); // 人关了 → WaitSilence
  TEST_ASSERT_FALSE(hook.update(false, 2400)); // 静默中 (400ms < 500ms)
  TEST_ASSERT_FALSE(hook.update(false, 2500)); // 静默到期 (500ms >= 500ms)，回 Armed
  TEST_ASSERT_TRUE(hook.update(true, 2600));   // 新报警 → 再触发
}

void test_brief_gap_stays_alarmed(void) {
  // 报警中间有短暂间隙（< silenceMs），不算停止
  EdgeHook hook(true, 2000);
  TEST_ASSERT_TRUE(hook.update(true, 0));       // 触发
  TEST_ASSERT_FALSE(hook.update(false, 500));   // 短暂间隙
  TEST_ASSERT_FALSE(hook.update(true, 600));    // 报警又来了
  TEST_ASSERT_FALSE(hook.update(true, 5000));   // 还在报警
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_never_fires);
  RUN_TEST(test_fires_once_on_rising_edge);
  RUN_TEST(test_no_refire_while_alarm_active);
  RUN_TEST(test_rearm_after_silence);
  RUN_TEST(test_brief_gap_stays_alarmed);
  return UNITY_END();
}
