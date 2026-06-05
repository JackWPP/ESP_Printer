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
  return (elapsedSeconds_ < plannedSeconds_) ? (plannedSeconds_ - elapsedSeconds_) : 0;
}

uint32_t TimerCore::overrunSeconds() const {
  return (elapsedSeconds_ > plannedSeconds_) ? (elapsedSeconds_ - plannedSeconds_) : 0;
}

void TimerCore::setTime(uint32_t minutes) {
  if (state_ != State::Idle) return;  // 计时中不允许改,避免行为意外
  plannedSeconds_ = minutes * 60u;
  emitTick();  // 让 UI 立刻反映新的预设值
}

void TimerCore::start() {
  if (state_ != State::Idle) return;
  if (plannedSeconds_ == 0) return;  // 没设时间则忽略
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
  if (state_ == State::Paused) {
    transitionTo(pausedFrom_);
  }
}

void TimerCore::stop() {
  if (state_ == State::Running || state_ == State::Overtime || state_ == State::Paused) {
    transitionTo(State::Finished);
    emitFeedback(Feedback::Stop);
    emitSlip();             // 在 Finished 态发出便签
    transitionTo(State::Idle);  // 立刻回 Idle;实际打印由 adapter 异步处理
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
  if (state_ != State::Running && state_ != State::Overtime) return;  // 暂停/待机不推进
  elapsedSeconds_++;
  if (state_ == State::Running && elapsedSeconds_ >= plannedSeconds_) {
    transitionTo(State::Overtime);
    emitFeedback(Feedback::TimeUp);  // 红 -> 白
  }
  emitTick();
}

void TimerCore::transitionTo(State next) {
  if (next == state_) return;
  State prev = state_;
  state_ = next;
  for (int i = 0; i < listenerCount_; ++i) {
    if (listeners_[i]) listeners_[i]->onStateChanged(prev, next);
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

}  // namespace timeprint
