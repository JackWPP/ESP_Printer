#include <unity.h>

#include "TimerCore.h"

using namespace timeprint;

#define ASSERT_STATE(expected, actual) \
  TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

struct RecordingListener : public ITimerCoreListener {
  int stateChanges = 0;
  State lastState = State::Idle;
  int ticks = 0;
  TickInfo lastTick{};
  int slips = 0;
  SlipData lastSlip{};
  int timeUps = 0;

  void onStateChanged(State, State to) override {
    stateChanges++;
    lastState = to;
  }

  void onTick(const TickInfo& info) override {
    ticks++;
    lastTick = info;
  }

  void onPrintSlip(const SlipData& slip) override {
    slips++;
    lastSlip = slip;
  }

  void onFeedback(Feedback fb) override {
    if (fb == Feedback::TimeUp) timeUps++;
  }
};

static TimerCore core;
static RecordingListener rec;

void setUp(void) {
  core = TimerCore();
  rec = RecordingListener();
  core.addListener(&rec);
}

void tearDown(void) {}

void test_initial_state_is_idle(void) {
  ASSERT_STATE(State::Idle, core.state());
  TEST_ASSERT_EQUAL_UINT32(0, core.plannedSeconds());
}

void test_set_and_start(void) {
  core.setTime(25);
  TEST_ASSERT_EQUAL_UINT32(1500, core.plannedSeconds());
  core.start();
  ASSERT_STATE(State::Running, core.state());
  TEST_ASSERT_EQUAL_UINT32(0, core.elapsedSeconds());
}

void test_start_ignored_without_time(void) {
  core.start();
  ASSERT_STATE(State::Idle, core.state());
}

void test_countdown(void) {
  core.setTime(1);
  core.start();
  for (int i = 0; i < 30; ++i) core.tick();
  ASSERT_STATE(State::Running, core.state());
  TEST_ASSERT_EQUAL_UINT32(30, core.elapsedSeconds());
  TEST_ASSERT_EQUAL_UINT32(30, core.remainingSeconds());
}

void test_auto_stop_at_zero(void) {
  core.setTime(1);
  core.start();
  for (int i = 0; i < 59; ++i) core.tick();
  ASSERT_STATE(State::Running, core.state());
  TEST_ASSERT_EQUAL_UINT32(1, core.remainingSeconds());
  core.tick();
  ASSERT_STATE(State::Idle, core.state());
  TEST_ASSERT_EQUAL_INT(1, rec.timeUps);
  TEST_ASSERT_EQUAL_INT(1, rec.slips);
  TEST_ASSERT_EQUAL_UINT32(60, rec.lastSlip.plannedSeconds);
  TEST_ASSERT_EQUAL_UINT32(60, rec.lastSlip.actualSeconds);
}

void test_pause_freezes_then_resume(void) {
  core.setTime(1);
  core.start();
  core.tick();
  core.tick();
  core.pause();
  ASSERT_STATE(State::Paused, core.state());
  core.tick();
  TEST_ASSERT_EQUAL_UINT32(2, core.elapsedSeconds());
  core.resume();
  ASSERT_STATE(State::Running, core.state());
}

void test_stop_emits_slip_and_returns_idle(void) {
  core.setTime(1);
  core.start();
  for (int i = 0; i < 30; ++i) core.tick();
  core.stop();
  ASSERT_STATE(State::Idle, core.state());
  TEST_ASSERT_EQUAL_INT(1, rec.slips);
  TEST_ASSERT_EQUAL_UINT32(60, rec.lastSlip.plannedSeconds);
  TEST_ASSERT_EQUAL_UINT32(30, rec.lastSlip.actualSeconds);
  TEST_ASSERT_EQUAL_UINT32(0, rec.lastSlip.overrunSeconds);
}

void test_reset_clears(void) {
  core.setTime(5);
  core.start();
  core.tick();
  core.reset();
  ASSERT_STATE(State::Idle, core.state());
  TEST_ASSERT_EQUAL_UINT32(0, core.plannedSeconds());
  TEST_ASSERT_EQUAL_UINT32(0, core.elapsedSeconds());
}

void test_settime_ignored_while_running(void) {
  core.setTime(1);
  core.start();
  core.setTime(10);
  TEST_ASSERT_EQUAL_UINT32(60, core.plannedSeconds());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_is_idle);
  RUN_TEST(test_set_and_start);
  RUN_TEST(test_start_ignored_without_time);
  RUN_TEST(test_countdown);
  RUN_TEST(test_auto_stop_at_zero);
  RUN_TEST(test_pause_freezes_then_resume);
  RUN_TEST(test_stop_emits_slip_and_returns_idle);
  RUN_TEST(test_reset_clears);
  RUN_TEST(test_settime_ignored_while_running);
  return UNITY_END();
}
