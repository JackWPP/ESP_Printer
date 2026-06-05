#include <unity.h>
#include "AlarmDetector.h"
#include "PhysicalHook.h"

using namespace timeprint;

void test_steady_not_oscillating(void) {
  AlarmDetector d(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) d.update(2000, t);
  TEST_ASSERT_FALSE(d.isOscillating());
  TEST_ASSERT_TRUE(d.lastP2P() < 100);
}

void test_oscillation_detected(void) {
  AlarmDetector d(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int s = ((t / 5) % 2) ? 2400 : 1700;  // 交替跳变
    d.update(s, t);
  }
  TEST_ASSERT_TRUE(d.isOscillating());
  TEST_ASSERT_TRUE(d.lastP2P() >= 100);
}

void test_small_noise_below_threshold(void) {
  AlarmDetector d(300, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int s = ((t / 5) % 2) ? 2050 : 2000;  // p2p≈50 < 300
    d.update(s, t);
  }
  TEST_ASSERT_FALSE(d.isOscillating());
}

void test_returns_to_idle_after_alarm(void) {
  AlarmDetector d(100, 50);
  for (uint32_t t = 0; t <= 120; t += 5) {
    int s = ((t / 5) % 2) ? 2400 : 1700;
    d.update(s, t);
  }
  TEST_ASSERT_TRUE(d.isOscillating());
  for (uint32_t t = 125; t <= 320; t += 5) d.update(2000, t);
  TEST_ASSERT_FALSE(d.isOscillating());
}

// 全链路:报警整段只触发一次,结束后能再次触发
void test_chain_fires_once_per_alarm(void) {
  AlarmDetector d(100, 50);
  EdgeHook hook(/*activeHigh=*/true, /*debounceMs=*/40);
  int fires = 0;
  uint32_t t = 0;
  for (; t <= 300; t += 5) {
    int s = ((t / 5) % 2) ? 2400 : 1700;
    if (hook.update(d.update(s, t), t)) fires++;
  }
  TEST_ASSERT_EQUAL_INT(1, fires);                 // 第一次报警:触发一次
  for (; t <= 600; t += 5) hook.update(d.update(2000, t), t);  // 报警结束
  for (; t <= 900; t += 5) {
    int s = ((t / 5) % 2) ? 2400 : 1700;
    if (hook.update(d.update(s, t), t)) fires++;
  }
  TEST_ASSERT_EQUAL_INT(2, fires);                 // 第二次报警:再次触发
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
