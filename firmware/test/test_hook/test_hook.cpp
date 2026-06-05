#include <unity.h>

#include "PhysicalHook.h"

using namespace timeprint;

void setUp(void) {}
void tearDown(void) {}

void test_idle_never_fires(void) {
  EdgeHook hook(false, 30);
  TEST_ASSERT_FALSE(hook.update(true, 0));
  TEST_ASSERT_FALSE(hook.update(true, 100));
}

void test_fires_once_on_stable_active(void) {
  EdgeHook hook(false, 30);
  TEST_ASSERT_FALSE(hook.update(true, 0));
  TEST_ASSERT_FALSE(hook.update(false, 10));
  TEST_ASSERT_FALSE(hook.update(false, 30));
  TEST_ASSERT_TRUE(hook.update(false, 41));
  TEST_ASSERT_FALSE(hook.update(false, 60));
}

void test_bounce_does_not_multifire(void) {
  EdgeHook hook(false, 30);
  hook.update(true, 0);
  hook.update(false, 5);
  hook.update(true, 8);
  hook.update(false, 12);
  TEST_ASSERT_FALSE(hook.update(false, 30));
  TEST_ASSERT_TRUE(hook.update(false, 43));
  TEST_ASSERT_FALSE(hook.update(false, 100));
}

void test_refires_after_release(void) {
  EdgeHook hook(false, 30);
  hook.update(true, 0);
  hook.update(false, 10);
  TEST_ASSERT_TRUE(hook.update(false, 45));
  hook.update(true, 50);
  TEST_ASSERT_FALSE(hook.update(true, 90));
  hook.update(false, 100);
  TEST_ASSERT_TRUE(hook.update(false, 140));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_never_fires);
  RUN_TEST(test_fires_once_on_stable_active);
  RUN_TEST(test_bounce_does_not_multifire);
  RUN_TEST(test_refires_after_release);
  return UNITY_END();
}
