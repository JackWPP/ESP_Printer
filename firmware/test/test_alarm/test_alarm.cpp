#include <unity.h>

#include "AlarmDetector.h"
#include "PhysicalHook.h"

using namespace timeprint;

void setUp(void) {}
void tearDown(void) {}

void test_steady_not_oscillating(void) {
  AlarmDetector detector(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) detector.update(2000, t);
  TEST_ASSERT_FALSE(detector.isOscillating());
  TEST_ASSERT_TRUE(detector.lastP2P() < 100);
}

void test_oscillation_detected(void) {
  AlarmDetector detector(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int sample = ((t / 5) % 2) ? 2400 : 1700;
    detector.update(sample, t);
  }
  TEST_ASSERT_TRUE(detector.isOscillating());
  TEST_ASSERT_TRUE(detector.lastP2P() >= 100);
}

void test_small_noise_below_threshold(void) {
  AlarmDetector detector(300, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int sample = ((t / 5) % 2) ? 2050 : 2000;
    detector.update(sample, t);
  }
  TEST_ASSERT_FALSE(detector.isOscillating());
}

void test_returns_to_idle_after_alarm(void) {
  AlarmDetector detector(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int sample = ((t / 5) % 2) ? 2400 : 1700;
    detector.update(sample, t);
  }
  TEST_ASSERT_TRUE(detector.isOscillating());
  for (uint32_t t = 125; t <= 320; t += 5) detector.update(2000, t);
  TEST_ASSERT_FALSE(detector.isOscillating());
}

void test_chain_fires_once_per_alarm(void) {
  // cooldown=500ms，确保一段报警（~300ms 振荡）只触发一次
  AlarmDetector detector(100, 50);
  EdgeHook hook(true, 500);
  int fires = 0;
  uint32_t t = 0;

  // 第一段报警：持续振荡 300ms
  for (; t <= 300; t += 5) {
    int sample = ((t / 5) % 2) ? 2400 : 1700;
    if (hook.update(detector.update(sample, t), t)) fires++;
  }
  // 第一段报警只触发一次
  TEST_ASSERT_EQUAL_INT(1, fires);

  // 长静默期（远超 cooldown）
  for (; t <= 2000; t += 5) hook.update(detector.update(2000, t), t);

  // 第二段报警：cooldown 已过，应再触发一次
  for (; t <= 2300; t += 5) {
    int sample = ((t / 5) % 2) ? 2400 : 1700;
    if (hook.update(detector.update(sample, t), t)) fires++;
  }
  TEST_ASSERT_EQUAL_INT(2, fires);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_steady_not_oscillating);
  RUN_TEST(test_oscillation_detected);
  RUN_TEST(test_small_noise_below_threshold);
  RUN_TEST(test_returns_to_idle_after_alarm);
  RUN_TEST(test_chain_fires_once_per_alarm);
  return UNITY_END();
}
