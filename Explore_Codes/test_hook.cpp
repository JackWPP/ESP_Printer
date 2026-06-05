#include <unity.h>
#include "PhysicalHook.h"

using namespace timeprint;

// 全部按低有效(active-low)+ 30ms 去抖测试

void test_idle_never_fires(void) {
  EdgeHook h(false, 30);
  TEST_ASSERT_FALSE(h.update(true, 0));    // 高电平 = 非激活
  TEST_ASSERT_FALSE(h.update(true, 100));
}

void test_fires_once_on_stable_active(void) {
  EdgeHook h(false, 30);
  TEST_ASSERT_FALSE(h.update(true, 0));    // 基线:非激活
  TEST_ASSERT_FALSE(h.update(false, 10));  // 进入激活,尚未稳定
  TEST_ASSERT_FALSE(h.update(false, 30));  // 稳定 20ms < 30
  TEST_ASSERT_TRUE(h.update(false, 41));   // 稳定 31ms -> 触发
  TEST_ASSERT_FALSE(h.update(false, 60));  // 保持激活,不重复触发
}

void test_bounce_does_not_multifire(void) {
  EdgeHook h(false, 30);
  h.update(true, 0);
  h.update(false, 5);   // 抖动
  h.update(true, 8);
  h.update(false, 12);
  TEST_ASSERT_FALSE(h.update(false, 30));  // 距最后变化(12)仅 18ms
  TEST_ASSERT_TRUE(h.update(false, 43));   // 稳定 31ms -> 仅触发一次
  TEST_ASSERT_FALSE(h.update(false, 100));
}

void test_refires_after_release(void) {
  EdgeHook h(false, 30);
  h.update(true, 0);
  h.update(false, 10);
  TEST_ASSERT_TRUE(h.update(false, 45));   // 触发
  h.update(true, 50);                      // 释放(回到非激活)
  TEST_ASSERT_FALSE(h.update(true, 90));   // 稳定非激活 -> 提交,不触发
  h.update(false, 100);                    // 再次激活
  TEST_ASSERT_TRUE(h.update(false, 140));  // 稳定 -> 再次触发
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_never_fires);
  RUN_TEST(test_fires_once_on_stable_active);
  RUN_TEST(test_bounce_does_not_multifire);
  RUN_TEST(test_refires_after_release);
  return UNITY_END();
}
