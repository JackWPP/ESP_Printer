#pragma once

#include <stddef.h>
#include <stdint.h>

namespace timeprint {

enum class State : uint8_t {
  Idle,
  Running,
  Overtime,
  Paused,
  Finished
};

enum class Feedback : uint8_t {
  Start,
  TimeUp,
  Stop,
  Reset
};

struct TickInfo {
  uint32_t plannedSeconds;
  uint32_t elapsedSeconds;
  uint32_t remainingSeconds;
  uint32_t overrunSeconds;
};

struct SlipData {
  uint32_t plannedSeconds;
  uint32_t actualSeconds;
  uint32_t overrunSeconds;
};

class ITimerCoreListener {
 public:
  virtual ~ITimerCoreListener() {}
  virtual void onStateChanged(State from, State to) {}
  virtual void onTick(const TickInfo& info) {}
  virtual void onPrintSlip(const SlipData& slip) {}
  virtual void onFeedback(Feedback fb) {}
};

class TimerCore {
 public:
  static const int kMaxListeners = 4;

  TimerCore();

  bool addListener(ITimerCoreListener* listener);

  void setTime(uint32_t minutes);
  void start();
  void pause();
  void resume();
  void stop();
  void reset();
  void tick();

  State state() const { return state_; }
  uint32_t plannedSeconds() const { return plannedSeconds_; }
  uint32_t elapsedSeconds() const { return elapsedSeconds_; }
  uint32_t remainingSeconds() const;
  uint32_t overrunSeconds() const;

 private:
  void transitionTo(State next);
  void emitTick();
  void emitFeedback(Feedback fb);
  void emitSlip();

  State state_;
  State pausedFrom_;
  uint32_t plannedSeconds_;
  uint32_t elapsedSeconds_;
  ITimerCoreListener* listeners_[kMaxListeners];
  int listenerCount_;
};

const char* stateToString(State state);
const char* stateToJsonString(State state);

}  // namespace timeprint
