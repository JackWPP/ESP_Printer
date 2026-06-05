#include "TimerCore.h"

namespace timeprint {

TimerCore::TimerCore()
    : state_(State::Idle),
      pausedFrom_(State::Running),
      plannedSeconds_(0),
      elapsedSeconds_(0),
      listenerCount_(0) {
  for (int i = 0; i < kMaxListeners; ++i) listeners_[i] = nullptr;
}

bool TimerCore::addListener(ITimerCoreListener* listener) {
  if (listener == nullptr || listenerCount_ >= kMaxListeners) return false;
  listeners_[listenerCount_++] = listener;
  return true;
}

uint32_t TimerCore::remainingSeconds() const {
  return elapsedSeconds_ < plannedSeconds_ ? plannedSeconds_ - elapsedSeconds_ : 0;
}

uint32_t TimerCore::overrunSeconds() const {
  return elapsedSeconds_ > plannedSeconds_ ? elapsedSeconds_ - plannedSeconds_ : 0;
}

void TimerCore::setTime(uint32_t minutes) {
  if (state_ != State::Idle) return;
  plannedSeconds_ = minutes * 60u;
  elapsedSeconds_ = 0;
  emitTick();
}

void TimerCore::start() {
  if (state_ != State::Idle || plannedSeconds_ == 0) return;
  elapsedSeconds_ = 0;
  transitionTo(State::Running);
  emitFeedback(Feedback::Start);
  emitTick();
}

void TimerCore::pause() {
  if (state_ == State::Running || state_ == State::Overtime) {
    pausedFrom_ = state_;
    transitionTo(State::Paused);
  }
}

void TimerCore::resume() {
  if (state_ == State::Paused) transitionTo(pausedFrom_);
}

void TimerCore::stop() {
  if (state_ == State::Running || state_ == State::Overtime || state_ == State::Paused) {
    transitionTo(State::Finished);
    emitFeedback(Feedback::Stop);
    emitSlip();
    transitionTo(State::Idle);
  }
}

void TimerCore::reset() {
  plannedSeconds_ = 0;
  elapsedSeconds_ = 0;
  emitFeedback(Feedback::Reset);
  transitionTo(State::Idle);
  emitTick();
}

void TimerCore::tick() {
  if (state_ != State::Running && state_ != State::Overtime) return;
  elapsedSeconds_++;
  if (state_ == State::Running && elapsedSeconds_ >= plannedSeconds_) {
    transitionTo(State::Overtime);
    emitFeedback(Feedback::TimeUp);
  }
  emitTick();
}

void TimerCore::transitionTo(State next) {
  if (next == state_) return;
  State previous = state_;
  state_ = next;
  for (int i = 0; i < listenerCount_; ++i) {
    if (listeners_[i]) listeners_[i]->onStateChanged(previous, next);
  }
}

void TimerCore::emitTick() {
  TickInfo info{plannedSeconds_, elapsedSeconds_, remainingSeconds(), overrunSeconds()};
  for (int i = 0; i < listenerCount_; ++i) {
    if (listeners_[i]) listeners_[i]->onTick(info);
  }
}

void TimerCore::emitFeedback(Feedback fb) {
  for (int i = 0; i < listenerCount_; ++i) {
    if (listeners_[i]) listeners_[i]->onFeedback(fb);
  }
}

void TimerCore::emitSlip() {
  SlipData slip{plannedSeconds_, elapsedSeconds_, overrunSeconds()};
  for (int i = 0; i < listenerCount_; ++i) {
    if (listeners_[i]) listeners_[i]->onPrintSlip(slip);
  }
}

const char* stateToString(State state) {
  switch (state) {
    case State::Idle:
      return "IDLE";
    case State::Running:
      return "RUNNING";
    case State::Overtime:
      return "OVERTIME";
    case State::Paused:
      return "PAUSED";
    case State::Finished:
      return "FINISHED";
  }
  return "UNKNOWN";
}

const char* stateToJsonString(State state) {
  switch (state) {
    case State::Idle:
      return "idle";
    case State::Running:
      return "running";
    case State::Overtime:
      return "overtime";
    case State::Paused:
      return "paused";
    case State::Finished:
      return "finished";
  }
  return "unknown";
}

}  // namespace timeprint
