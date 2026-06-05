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
  AlarmDetector detector(100, 50);
  EdgeHook hook(true, 40);
  int fires = 0;
  uint32_t t = 0;

  for (; t <= 300; t += 5) {
    int sample = ((t / 5) % 2) ? 2400 : 1700;
    if (hook.update(detector.update(sample, t), t)) fires++;
  }

  TEST_ASSERT_EQUAL_INT(1, fires);

  for (; t <= 600; t += 5) hook.update(detector.update(2000, t), t);

  for (; t <= 900; t += 5) {
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
